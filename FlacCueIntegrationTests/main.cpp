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
#include <boost/format.hpp>
#include <iomanip>
#include <FLAC++/all.h>
#include <algorithm>

#include "FlacCue.h"

using namespace cue;
using namespace boost;

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

static inline string dirname(string const path) {
    auto tmp = dirname(const_cast<char*>(path.c_str()));
    string result(tmp);
    free(tmp);
    return result;
}

int main(int argc, const char * argv[]) {
    string cuePath = "/Users/TamasZahola/Music/Inbox/DRHCD001 - Kerri Chandler - Computer Games/Computer Games CD1.cue";

    string cueDir = dirname(cuePath);
    auto outputDir = cueDir + "/converted";
    struct stat outputDirStat;
    if (stat(outputDir.c_str(), &outputDirStat) != 0 && errno == ENOENT) {
        if (mkdir(outputDir.c_str(), 0700) != 0) {
            abort();
        }
    } else if (!S_ISDIR(outputDirStat.st_mode)) {
        abort();
    }
    
    try {
        auto input = ifstream(cuePath);
        Disc disc(input);
        input.close();
        GapsAppendedSplitGenerator splitter;
        auto split = splitter.split(disc);
        
        cout << disc << endl;
        
        for (auto outputFile : split.outputFiles) {
            FLAC::Encoder::File currentDestinationWriter;
            currentDestinationWriter.set_channels(2);
            currentDestinationWriter.set_sample_rate(44100);
            currentDestinationWriter.set_bits_per_sample(16);
            currentDestinationWriter.init(outputDir + "/" + outputFile.outputFile);
            
            for (auto inputSegment : outputFile.inputSegments) {
                FLACLambdaReader reader;
                reader.init(cueDir + "/" + inputSegment.inputFile);
                reader.process_until_end_of_metadata();
                
                long remainingSamples;
                if (inputSegment.end == none) {
                    remainingSamples = reader.get_total_samples() - inputSegment.begin.samples;
                } else {
                    remainingSamples = inputSegment.end.value().samples - inputSegment.begin.samples;
                }
                
                reader.writeCallback =
                [&currentDestinationWriter, &remainingSamples]
                (const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]) {
                    auto samplesToBeWritten = std::min<long>(remainingSamples, frame->header.blocksize);
                    currentDestinationWriter.process(buffer, (unsigned int)samplesToBeWritten);
                    remainingSamples -= samplesToBeWritten;
                    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
                };
                
                reader.seek_absolute(inputSegment.begin.samples);
                while (remainingSamples != 0) {
                    reader.process_single();
                }
                
                reader.finish();
            }
            
            currentDestinationWriter.finish();
        }
    } catch (ParseError const parseError) {
        cout << parseError.what() << endl;
    }
    
    return 0;
}
