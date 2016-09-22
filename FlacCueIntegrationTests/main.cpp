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
    auto tmp = strdup(path.c_str());
    auto dir = dirname(tmp);
    std::string result(dir);
    free(tmp);
    return result;
}

static cue::Time readFileLength(const std::string& path) {
    FLACLambdaReader reader;
    auto initStatus = reader.init(path);
    if (initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        std::cerr << "Error while opening " << path << std::endl;
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

unsigned int levenshtein_distance(const std::string& s1, const std::string& s2) {
    auto len1 = s1.size();
    auto len2 = s2.size();
    std::vector<unsigned int> col(len2 + 1);
    std::vector<unsigned int> prevCol(len2 + 1);
    
    for (unsigned int i = 0; i < prevCol.size(); ++i) {
        prevCol[i] = i;
    }
    
    for (unsigned int i = 0; i < len1; i++) {
        col[0] = i + 1;
        for (unsigned int j = 0; j < len2; ++j) {
            col[j + 1] = std::min({ prevCol[1 + j] + 1, col[j] + 1, prevCol[j] + (s1[i] == s2[j] ? 0 : 1) });
        }
        col.swap(prevCol);
    }
    
    return prevCol[len2];
}

static inline bool hasSuffix(const std::string& str, const std::string& suffix) {
    return
    (str.size() >= suffix.size()) &&
    (str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
}

template<typename T> static inline T center(const T& t, unsigned width, typename T::const_reference fill) {
    if (t.size() >= width) return t;
    
    T result;
    while (result.size() < (width - t.size()) / 2) result += fill;
    result += t;
    while (result.size() < width) result += fill;
    return result;
}

static inline std::string msfString(const cue::Time& time) {
    auto msf = time.cueTime();
    return (boost::format("%1%:%2%:%3%") %
            boost::io::group(std::setw(2), std::setfill('0'), std::get<0>(msf)) %
            boost::io::group(std::setw(2), std::setfill('0'), std::get<1>(msf)) %
            boost::io::group(std::setw(2), std::setfill('0'), std::get<2>(msf))).str();
}

template<typename TStream1, typename TStream2> class MultiplexedOutputStream {
    TStream1& _stream1;
    TStream2& _stream2;
public:
    MultiplexedOutputStream(TStream1& stream1, TStream2& stream2) : _stream1(stream1), _stream2(stream2) {}
    template<typename T> MultiplexedOutputStream& operator<<(const T& t) {
        _stream1 << t;
        _stream2 << t;
        return *this;
    }
    MultiplexedOutputStream& operator<<(std::ostream&(*f)(std::ostream&)) {
        _stream1 << f;
        _stream2 << f;
        return *this;
    }
};

std::string filenameMostSimiliarTo(const std::string& filename, const std::vector<std::string>& filenames) {
    return *std::min_element(filenames.begin(), filenames.end(), [&](const std::string& a, const std::string& b) {
        return levenshtein_distance(a, filename) < levenshtein_distance(b, filename);
    });
}

std::vector<std::string> filesInDir(const std::string& dirPath) {
    std::vector<std::string> result;
    auto dir = opendir(dirPath.c_str());
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_REG) {
            result.push_back(std::string(ent->d_name));
        }
    }
    closedir(dir);
    return result;
}

int main(int argc, const char * argv[]) {
    if (argc < 2) {
        std::cerr << "No path specified!" << std::endl;
    }
    
    for (auto i = 1; i < argc; ++i) {
        std::string path = argv[i];
        std::cerr << "Processing: " << path << std::endl;
        
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
            auto filesInCueDir = filesInDir(cueDir);
            std::vector<std::string> flacFiles;
            std::copy_if(filesInCueDir.begin(), filesInCueDir.end(), std::back_inserter(flacFiles), [](const std::string& file) {
                return hasSuffix(file, ".flac");
            });
            
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
            std::cerr << "Unknown file: " << path << std::endl;
            abort();
        }
        
        auto filesInCueDir = filesInDir(cueDir);
        std::vector<std::string> flacFilesInCueDir;
        std::copy_if(filesInCueDir.begin(), filesInCueDir.end(), std::back_inserter(flacFilesInCueDir), [](const std::string& file) {
            return hasSuffix(file, ".flac");
        });
        
        int maxTrackNumber = max_element(disc->tracksCbegin(), disc->tracksCend(), [](const cue::Track& a, const cue::Track& b) {
            return a.number < b.number;
        })->number;
        int trackNumberDigits = (int)ceil(log10(maxTrackNumber));
        
        std::unordered_map<std::string, cue::Time> inputFileLengths;
        for_each(disc->filesCbegin(), disc->filesCend(), [&](const cue::File& file) {
            auto realFilename = filenameMostSimiliarTo(file.path, flacFilesInCueDir);
            inputFileLengths[file.path] = readFileLength(cueDir + "/" + realFilename);
        });
        
        std::vector<cue::Time> trackOffsets;
        cue::File const* currentFile = nullptr;
        cue::Time currentFileBeginTime = 0;
        for_each(disc->tracksCbegin(), disc->tracksCend(), [&](const cue::Track& track) {
            for_each(track.indexesCbegin(), track.indexesCend(), [&](const cue::Index& index) {
                if (&index.file() != currentFile) {
                    currentFileBeginTime = currentFileBeginTime + (currentFile == nullptr ? 0 : inputFileLengths[currentFile->path]);
                    currentFile = &index.file();
                }
                if (index.index == 1) {
                    trackOffsets.push_back(currentFileBeginTime + index.begin);
                }
            });
        });
        trackOffsets.push_back(currentFileBeginTime + inputFileLengths[currentFile->path]);
        
        auto toc = accuraterip::TableOfContents::CreateFromTrackOffsets(trackOffsets);
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
        
        bool isSplittedDifferent = false;
        for (auto outputSegment : split.outputFiles) {
            if (outputSegment.inputSegments.size() != 1) {
                isSplittedDifferent = true;
                break;
            }
            
            auto inputSegment = outputSegment.inputSegments[0];
            if (inputSegment.begin != cue::Time(0)) {
                isSplittedDifferent = true;
                break;
            }
            
            if (inputSegment.end != boost::none && inputSegment.end != inputFileLengths[inputSegment.inputFile]) {
                isSplittedDifferent = true;
                break;
            }
        }
        
        auto outputDir = cueDir;
        if (isSplittedDifferent) {
            outputDir = outputDir + "/converted";
            std::cerr << "Creating canonicalized disc in '" << outputDir << "'"<< std::endl;
            struct stat outputDirStat;
            if (stat(outputDir.c_str(), &outputDirStat) != 0 && errno == ENOENT) {
                if (mkdir(outputDir.c_str(), 0700) != 0) {
                    std::cerr << "Failed to create '" << outputDir << "'!" << std::endl;
                    abort();
                }
            } else if (!S_ISDIR(outputDirStat.st_mode)) {
                std::cerr << "'" << outputDir << "' already exists and it's a file!" << std::endl;
                abort();
            }
        } else {
            std::cerr << "Input is already in canonical format" << std::endl;
        }
        
        bool hasHTOA = disc->tracksCbegin()->indexesCbegin()->index == 0;
        for (auto i = 0; i < split.outputFiles.size(); ++i) {
            auto outputFile = split.outputFiles[i];
            
            FLAC::Encoder::File currentDestinationWriter;
            if (isSplittedDifferent) {
                currentDestinationWriter.set_channels(2);
                currentDestinationWriter.set_sample_rate(44100);
                currentDestinationWriter.set_bits_per_sample(16);
                currentDestinationWriter.init(outputDir + "/" + outputFile.outputFile);
            }
            
            for (auto inputSegment : outputFile.inputSegments) {
                FLACLambdaReader reader;
                auto realFilename = filenameMostSimiliarTo(inputSegment.inputFile, flacFilesInCueDir);
                reader.init(cueDir + "/" + realFilename);
                reader.process_until_end_of_metadata();
                
                long remainingSamples = (inputSegment.end.value_or(inputFileLengths[inputSegment.inputFile]) - inputSegment.begin).samples;
                
                reader.writeCallback = [&](const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]) {
                    auto samplesToBeWritten = std::min<long>(remainingSamples, frame->header.blocksize);
                    if (isSplittedDifferent) {
                        currentDestinationWriter.process(buffer, (unsigned int)samplesToBeWritten);
                    }
                    
                    if (!hasHTOA || i > 0) {
                        checksumGenerator.processSamples(buffer, (uint32_t)samplesToBeWritten);
                    }
                    
                    static_assert(sizeof(FLAC__int32) == sizeof(int32_t), "");
                    
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
            
            if (isSplittedDifferent) {
                currentDestinationWriter.finish();
            }
        }

        auto album = fallback(split.outputSheet->title, boost::optional<std::string>(""));
        auto albumArtist = fallback(split.outputSheet->performer,
                                    split.outputSheet->songwriter,
                                    split.outputSheet->tracksCbegin()->performer,
                                    split.outputSheet->tracksCbegin()->songwriter,
                                    boost::optional<std::string>(""));
        
        if (isSplittedDifferent) {
            auto cueFile = outputDir + "/" + albumArtist + " - " + album + ".cue";
            std::cerr << "Writing canonical cuesheet to '" << cueFile << "'" << std::endl;
            std::ofstream cueOutput(cueFile);
            cueOutput << *split.outputSheet;
            cueOutput.close();
        }
        
        auto numberOfTracks = trackOffsets.size() - 1;
        
        auto accurateRipLogFile = outputDir + "/" + albumArtist + " - " + album + ".arlog";
        std::cerr << "Writing AccurateRip log to '" << accurateRipLogFile << "'" << std::endl;
        std::ofstream accurateRipLogFileStream(accurateRipLogFile);
        MultiplexedOutputStream<decltype(std::cout), decltype(accurateRipLogFileStream)>accurateRipLogStream(std::cout, accurateRipLogFileStream);
        
        accurateRipLogStream << "Audio checksums:" << std::endl;
        accurateRipLogStream << " #         TOC           V1    V1_Fr450    V2" << std::endl;
        for (auto i = 0; i < numberOfTracks; ++i) {
            accurateRipLogStream
            << (boost::format("%1%: %2% %3% %4% %5%") %
                boost::io::group(std::setw(2), std::setfill('0'), i + 1) %
                (msfString(toc[i].startOffset) + '-' + msfString(toc[i+1].startOffset - cue::Time(0,0,1))) %
                boost::io::group(std::setw(8), std::setfill('0'), std::setbase(16), checksumGenerator.v1ChecksumWithOffset(i, 0)) %
                boost::io::group(std::setw(8), std::setfill('0'), std::setbase(16), checksumGenerator.v1Frame450ChecksumWithOffset(i, 0)) %
                boost::io::group(std::setw(8), std::setfill('0'), std::setbase(16), checksumGenerator.v2Checksum(i))).str()
            << std::endl;
        }
        accurateRipLogStream << std::endl;
        
        accurateRipLogStream << "AccurateRip data URL: " << checksumGenerator.accurateRipDataURL << std::endl;
        
        std::unique_ptr<accuraterip::Data> arData = nullptr;
        
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
            std::cerr
            << "Failed to download AccurateRip data!"
            << std::endl
            << downloadedHeaders.str()
            << std::endl;
        } else {
            std::cerr << "Downloaded AccurateRip data." << std::endl;
            arData.reset(new accuraterip::Data(downloadedData));
        }
        
        accurateRipLogStream << "AccurateRip data contains " << arData->discs.size() << " discs." << std::endl << std::endl;
        for (auto i = 0; i < arData->discs.size(); ++i) {
            accurateRipLogStream << "Data from AccurateRip disc " << (i + 1) << ":" << std::endl;
            accurateRipLogStream << " #  Checksum  Fr450   Count Matches" << std::endl;
            auto disc = arData->discs[i];
            for (auto i = 0; i < disc.tracks.size(); ++i) {
                auto track = disc.tracks[i];
                accurateRipLogStream
                << (boost::format("%1%: %2% %3% %4%") %
                    boost::io::group(std::setw(2), std::setfill('0'), i + 1) %
                    boost::io::group(std::setw(8), std::setfill('0'), std::setbase(16), track.crc) %
                    boost::io::group(std::setw(8), std::setfill('0'), std::setbase(16), track.frame450CRC) %
                    center(std::to_string(track.count), 5, ' ')).str();
                
                for (auto offset = checksumGenerator.minimumOffset(); offset <= checksumGenerator.maximumOffset(); ++offset) {
                    if (checksumGenerator.v1ChecksumWithOffset(i, offset) == track.crc) {
                        accurateRipLogStream << " V1";
                        if (offset != 0) {
                            accurateRipLogStream << "(offset=" << offset << ")";
                        }
                    }
                    
                    if (checksumGenerator.v1Frame450ChecksumWithOffset(i, offset) == track.frame450CRC) {
                        accurateRipLogStream << " V1_Fr450";
                        if (offset != 0) {
                            accurateRipLogStream << "(offset=" << offset << ")";
                        }
                    }
                }
                if (checksumGenerator.v2Checksum(i) == track.crc) {
                    accurateRipLogStream << " V2";
                }
                accurateRipLogStream << std::endl;
            }
            accurateRipLogStream << std::endl;
        }
        accurateRipLogFileStream.close();
    }
    
    return 0;
}
