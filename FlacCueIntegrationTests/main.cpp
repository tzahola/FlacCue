//
//  main.cpp
//  FlacCue
//
//  Created by Tamás Zahola on 30/12/15.
//  Copyright © 2015 Tamás Zahola. All rights reserved.
//


#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <libgen.h>
#include <sys/stat.h>
#include <dirent.h>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <iomanip>
#include <FLAC++/all.h>
#include <algorithm>
#include <unordered_map>
#include <tuple>
#include <math.h>

extern "C" {
    #include <curl/curl.h>
}

#include "FlacCue.h"

class CURLException : std::runtime_error {
public:
    using runtime_error::runtime_error;
};

class CURLDownloader {
public:
    using DataCallback = std::function<size_t(void const * buffer, size_t size, size_t count)>;
    using StatusCode = long;
    
private:
    CURL* _curl;
    DataCallback _writeCallback;
    DataCallback _headerCallback;
    
    static size_t callWriteCallback(void const * buffer, size_t size, size_t count, void * context) {
        return ((CURLDownloader*)context)->_writeCallback(buffer, size, count);
    }
    
    static size_t callHeaderCallback(void const * buffer, size_t size, size_t count, void * context) {
        return ((CURLDownloader*)context)->_headerCallback(buffer, size, count);
    }
    
public:
    CURLDownloader(std::string url, DataCallback writeCallback, DataCallback headerCallback)
    : _writeCallback(writeCallback)
    , _headerCallback(headerCallback) {
        _curl = curl_easy_init();
        curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(_curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(_curl, CURLOPT_HEADERFUNCTION, callHeaderCallback);
        curl_easy_setopt(_curl, CURLOPT_HEADERDATA, this);
        curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, callWriteCallback);
        curl_easy_setopt(_curl, CURLOPT_WRITEDATA, this);
    }
    
    StatusCode perform() {
        auto curlResult = curl_easy_perform(_curl);
        if (curlResult != CURLE_OK) {
            throw CURLException(curl_easy_strerror(curlResult));
        } else {
            long responseCode;
            curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE, &responseCode);
            return (StatusCode)responseCode;
        }
    }
    
    CURLDownloader(const CURLDownloader&) = delete;
    CURLDownloader& operator=(const CURLDownloader&) = delete;
    
    ~CURLDownloader() {
        curl_easy_cleanup(_curl);
    }
};

class FLACLambdaReader : public FLAC::Decoder::File {
public:
    std::function<::FLAC__StreamDecoderWriteStatus(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[])> writeCallback;
    std::function<void(::FLAC__StreamDecoderErrorStatus status)> errorCallback;
    
protected:
    virtual ::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]) override {
        return writeCallback(frame, buffer);
    }
    
    virtual void error_callback(::FLAC__StreamDecoderErrorStatus status) override {
        return errorCallback(status);
    }
};

static inline std::string dirname(std::string const path) {
    auto tmp = dirname(const_cast<char*>(path.c_str()));
    std::string result(tmp);
    free(tmp);
    return result;
}

static cue::Time readFileLength(const std::string& path) {
    FLACLambdaReader reader;
    auto initStatus = reader.init(path);
    if (initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        std::cout << "Error while opening " << path << std::endl;
        abort();
    }
    reader.process_until_end_of_metadata();
    cue::Time result = reader.get_total_samples();
    reader.finish();
    return result;
}

template<typename T> T fallback(const boost::optional<T>& lastResort) {
    return lastResort.value();
}

template<typename T, typename ... Targs> T fallback(const boost::optional<T>& x, const boost::optional<Targs>&... args) {
    if (x == boost::none) {
        return fallback(args...);
    } else {
        return x.value();
    }
}

static inline bool hasSuffix(const std::string& str, const std::string& suffix) {
    return
    (str.size() >= suffix.size()) &&
    (str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
}

int main(int argc, const char * argv[]) {
    std::string path = "/Users/TamasZahola/Music/Inbox/Kerri Chandler - Computer Games/Computer Games CD1.cue";
    
    struct stat pathStat;
    stat(path.c_str(), &pathStat);
    
    std::string cueDir;
    std::shared_ptr<cue::Disc> disc = nullptr;
    if (S_ISREG(pathStat.st_mode)) {
        cueDir = dirname(path);
        std::ifstream input(path);
        disc = std::make_shared<cue::Disc>(input);
        input.close();
    } else if (S_ISDIR(pathStat.st_mode)) {
        disc = std::make_shared<cue::Disc>();
        cueDir = path;
        std::vector<std::string> flacFiles;
        
        auto dir = opendir(path.c_str());
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_REG) {
                std::string fileName(ent->d_name);
                if (hasSuffix(fileName, ".flac")) {
                    flacFiles.push_back(fileName);
                }
            }
        }
        closedir(dir);
        
        std::sort(flacFiles.begin(), flacFiles.end());
        for (auto fileName : flacFiles) {
            auto& file = disc->addFile();
            file.path = fileName;
            file.fileType = "WAVE";
            auto& track = disc->addTrack();
            track.number = (int)(disc->tracksCend() - disc->tracksCbegin());
            track.dataType = "AUDIO";
            auto& index = track.addIndex();
            index.index = 1;
            index.begin = 0;
            index.setFile(file);
        }
    } else {
        abort();
    }
    
//    auto outputDir = cueDir + "/converted";
//    struct stat outputDirStat;
//    if (stat(outputDir.c_str(), &outputDirStat) != 0 && errno == ENOENT) {
//        if (mkdir(outputDir.c_str(), 0700) != 0) {
//            abort();
//        }
//    } else if (!S_ISDIR(outputDirStat.st_mode)) {
//        abort();
//    }
    
    int maxTrackNumber = max_element(disc->tracksCbegin(), disc->tracksCend(), [](const cue::Track& a, const cue::Track& b) {
        return a.number < b.number;
    })->number;
    int trackNumberDigits = (int)ceil(log10(maxTrackNumber));
    
    std::unordered_map<std::string, cue::Time> inputFileLengths;
    for_each(disc->filesCbegin(), disc->filesCend(), [&](const cue::File& file) {
        inputFileLengths[file.path] = readFileLength(cueDir + "/" + file.path);
    });
    
    accuraterip::TOC toc;
    cue::File const* currentFile = nullptr;
    cue::Time currentFileBeginTime = 0;
    for_each(disc->tracksCbegin(), disc->tracksCend(), [&](const cue::Track& track) {
        for_each(track.indexesCbegin(), track.indexesCend(), [&](const cue::Index& index) {
            if (&index.file() != currentFile) {
                currentFileBeginTime = currentFileBeginTime + (currentFile == nullptr ? 0 : inputFileLengths[currentFile->path]);
                currentFile = &index.file();
            }
            if (index.index == 1) {
                toc.push_back(currentFileBeginTime + index.begin);
            }
        });
    });
    toc.push_back(currentFileBeginTime + inputFileLengths[currentFile->path]);
    
    accuraterip::ChecksumGenerator checksumGenerator(toc);
    
    cue::GapsAppendedSplitGenerator splitter([&](const boost::optional<const cue::Track&> track) -> std::string {
        if (track == boost::none) {
            return (boost::format("%1% - HTOA.flac") % boost::io::group(std::setw(trackNumberDigits), std::setfill('0'), 0)).str();
        } else {
            auto artist = fallback(track->performer, track->songwriter, disc->performer, disc->songwriter, boost::optional<std::string>(""));
            auto title = track->title.value_or("");
            return (boost::format("%1% - %2% - %3%.flac")
                    % boost::io::group(std::setw(trackNumberDigits), std::setfill('0'), track->number)
                   % artist
                   % title).str();
        }
    }, [&](const std::string& fileName) -> cue::Time {
        return inputFileLengths[fileName];
    });
    
    auto split = splitter.split(*disc);
    
    for (auto i = 0; i < split.outputFiles.size(); ++i) {
        auto outputFile = split.outputFiles[i];
        
//        FLAC::Encoder::File currentDestinationWriter;
//        currentDestinationWriter.set_channels(2);
//        currentDestinationWriter.set_sample_rate(44100);
//        currentDestinationWriter.set_bits_per_sample(16);
//        currentDestinationWriter.init(outputDir + "/" + outputFile.outputFile);
        
        for (auto inputSegment : outputFile.inputSegments) {
            FLACLambdaReader reader;
            reader.init(cueDir + "/" + inputSegment.inputFile);
            reader.process_until_end_of_metadata();
            
            auto inputSegmentEnd = inputSegment.end == boost::none ? inputFileLengths[inputSegment.inputFile] : inputSegment.end.value();
            long remainingSamples = (inputSegmentEnd - inputSegment.begin).samples;
            
            reader.writeCallback = [&](const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]) {
                auto samplesToBeWritten = std::min<long>(remainingSamples, frame->header.blocksize);
//                currentDestinationWriter.process(buffer, (unsigned int)samplesToBeWritten);
                
                checksumGenerator.processSamples(buffer, (uint32_t)samplesToBeWritten);
                static_assert(sizeof(FLAC__int32) == sizeof(uint32_t), "");
                
                remainingSamples -= samplesToBeWritten;
                return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
            };
            
            reader.seek_absolute(inputSegment.begin.samples);
            while (remainingSamples != 0) {
                assert(remainingSamples > 0);
                reader.process_single();
            }
            
            reader.finish();
        }
        
//        currentDestinationWriter.finish();
    }
    
//    std::ofstream cueOutput(outputDir + "/converted.cue");
//    cueOutput << split.outputSheet;
//    cueOutput.close();
    
    std::cout << "Downloading " << checksumGenerator.accurateRipDataURL << " ..." << std::endl;
    std::stringstream downloadedData(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
    std::stringstream downloadedHeaders(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
    CURLDownloader downloader(checksumGenerator.accurateRipDataURL, [&](void const * buffer, size_t size, size_t count) -> size_t {
        downloadedData.write((const char*)buffer, size * count);
        return downloadedData.bad() ? 0 : count;
    }, [&](void const * buffer, size_t size, size_t count) -> size_t {
        downloadedHeaders.write((const char*)buffer, size * count);
        return downloadedHeaders.bad() ? 0 : count;
    });
    auto statusCode = downloader.perform();
    if (statusCode != 200) {
        std::cout
        << "Failed to download AccurateRip data!"
        << std::endl
        << downloadedHeaders.str()
        << std::endl;
        abort();
    } else {
        std::cout << "Downloaded successfully." << std::endl;
    }
    
    accuraterip::Data arData(downloadedData);
    
    std::cout << " #     V1       V2    Results:" << std::endl;
    for (auto i = 0; i < checksumGenerator.v1Checksums.size(); ++i) {
        std::cout << (boost::format("%1%: ") % boost::io::group(std::setw(2), std::setfill('0'), i + 1)).str();
        
        auto arCrcV1 = checksumGenerator.v1Checksums[i];
        auto arCrcV2 = checksumGenerator.v2Checksums[i];
        
        std::ios::fmtflags flags = std::cout.flags();
        {
            std::cout
            << boost::io::group(std::setw(8), std::setfill('0'), std::setbase(16), arCrcV1) << " "
            << boost::io::group(std::setw(8), std::setfill('0'), std::setbase(16), arCrcV2) << " ";
        }
        std::cout.flags(flags);
        
        bool didMatchAnyChecksum = false;
        bool didMatchAnyOffset = false;
        for (auto j = 0; j < arData.discs.size(); ++j) {
            if (arCrcV1 == arData.discs[j].tracks[i].crc) {
                didMatchAnyChecksum = true;
                std::cout << (boost::format("V1(disc=%1%, count=%2%)") % j % arData.discs[j].tracks[i].count).str() << " ";
            }
            if (arCrcV2 == arData.discs[j].tracks[i].crc) {
                didMatchAnyChecksum = true;
                std::cout << (boost::format("V2(disc=%1%, count=%2%)") % j % arData.discs[j].tracks[i].count).str() << " ";
            }
            
            for (auto offset = accuraterip::OffsetFindingMinimumOffset; offset <= accuraterip::OffsetFindingMaximumOffset; ++offset) {
                if (checksumGenerator.v1Frame450ChecksumWithOffset(i, offset) == arData.discs[j].tracks[i].frame450CRC) {
                    didMatchAnyOffset = true;
                    std::cout
                    << (boost::format("V1_Frame450(offset=%1%, disc=%2%, count=%3%)")
                        % offset
                        % j
                        % arData.discs[j].tracks[i].count).str()
                    << " ";
                }
            }
        }
        if (didMatchAnyChecksum) {
            std::cout << "OK";
        } else if (didMatchAnyOffset) {
            std::cout << "No match, but offset";
        } else {
            std::cout << "No match, no offset";
        }
        std::cout << std::endl;
    }
    
    return 0;
}
