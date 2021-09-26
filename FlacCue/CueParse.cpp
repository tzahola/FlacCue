//
//  CueParse.cpp
//  FlacCue
//
//  Created by Tamás Zahola on 10/01/16.
//  Copyright © 2016 Tamás Zahola. All rights reserved.
//

#include "CueParse.hpp"

#include <sstream>
#include <iomanip>
#include <math.h>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

extern "C" {
    #include "cue.h"
    #include "cue.tab.h"
        
    #define YYSTYPE CUE_STYPE
    #define YYLTYPE CUE_LTYPE
    #include "cue.lex.h"
}

namespace cue {

struct CueParserReadCallbackContext {
    std::istream& input;
    bool didReturnNewlineAfterEOF; // hack b/c FLEX handles <<EOF>> in a shitty way
};
    
static int CueParserReadCallback(void * context, void * buffer, int maxSize) {
    CueParserReadCallbackContext* callbackContext = (CueParserReadCallbackContext*)context;
    if (callbackContext->didReturnNewlineAfterEOF) {
        return 0;
    } else {
        callbackContext->input.read((char*)buffer, maxSize);
        int count = (int)callbackContext->input.gcount();
        if (count == 0) {
            callbackContext->didReturnNewlineAfterEOF = true;
            ((char*)buffer)[0] = '\n';
            return 1;
        } else {
            return count;
        }
    }
}
    
std::ostream& operator<<(std::ostream& o, const Time& t) {
    auto components = t.cueTime();
    return o
    << std::setfill('0') << std::setw(2) << std::get<0>(components) << ':'
    << std::setfill('0') << std::setw(2) << std::get<1>(components) << ':'
    << std::setfill('0') << std::setw(2) << std::get<2>(components);
}
    
std::string to_string(const Time& t) {
    std::ostringstream s;
    s << t;
    return s.str();
}
    
    
File& Index::file() const {
    return *(_disc->filesBegin() + _file);
}

void Index::setFile(const File& file) {
    _file = static_cast<int>(&file - &(*_disc->filesBegin()));
}
    
Disc::Disc(std::istream& input) throw(ParseError) {
    
    struct CueCommandList result;
    CueCommandListInit(&result);
    char* error;
    int errorLine;
    
    struct CueParserExtra extra;
    CueParserReadCallbackContext context { input, false };
    extra.context  = &context;
    extra.readCallback = CueParserReadCallback;
    
    yyscan_t scanner;
    cue_lex_init(&scanner);
    cue_set_extra(&extra, scanner);
    auto status = cue_parse(scanner, &result, &error, &errorLine);
    cue_lex_destroy(scanner);
    
    if (status == 0) {
        CueCommandListItem* item = result.head;
        
        while (item != NULL) {
            auto command = item->command;
            switch (command.type) {
                case CueCommandTypeCatalog: catalog = command.catalog; break;
                case CueCommandTypeCdTextFile: cdTextFile = command.cdTextFile; break;
                case CueCommandTypeFile: {
                    auto& file = addFile();
                    file.path = command.file.path;
                    file.fileType = command.file.fileType;
                } break;
                case CueCommandTypeFlags: {
                    if (tracksBegin() == tracksEnd()) {
                        throw ParseError("FLAGS must be used within a TRACK!");
                    }
                    (tracksEnd() - 1)->flags = command.flags;
                } break;
                case CueCommandTypeIndex: {
                    if (filesBegin() == filesEnd()) {
                        throw ParseError("INDEX can only be used after specifying a FILE!");
                    }
                    auto time = Time(command.index.time.minutes, command.index.time.seconds, command.index.time.frames);
                    
                    auto& index = (tracksEnd() - 1)->addIndex();
                    index.index = command.index.index;
                    index.begin = time;
                    index.setFile(*(filesEnd() - 1));
                } break;
                case CueCommandTypeIsrc: {
                    if (tracksBegin() == tracksEnd()) {
                        throw ParseError("ISRC must be used within a TRACK!");
                    }
                    (tracksEnd() - 1)->isrc = command.isrc;
                } break;
                case CueCommandTypePerformer:  {
                    if (tracksBegin() == tracksEnd()) {
                        performer = command.performer;
                    } else {
                        (tracksEnd() - 1)->performer = command.performer;
                    }
                } break;
                case CueCommandTypePostgap: {
                    if (tracksBegin() == tracksEnd()) {
                        throw ParseError("POSTGAP must be used within a TRACK!");
                    }
                    (tracksEnd() - 1)->postgap = Time(command.postGap.minutes, command.postGap.seconds, command.postGap.frames);
                } break;
                case CueCommandTypePregap: {
                    if (tracksBegin() == tracksEnd()) {
                        throw ParseError("PREGAP must be used within a TRACK!");
                    }
                    (tracksEnd() - 1)->pregap = Time(command.preGap.minutes, command.preGap.seconds, command.preGap.frames);
                } break;
                case CueCommandTypeRem: {
                    if (tracksBegin() == tracksEnd()) {
                        comments.push_back(command.rem);
                    } else {
                        auto& lastTrack = *(tracksEnd() - 1);
                        if (lastTrack.indexesBegin() == lastTrack.indexesEnd()) {
                            lastTrack.comments.push_back(command.rem);
                        } else {
                            auto& lastIndex = *(lastTrack.indexesEnd() - 1);
                            lastIndex.comments.push_back(command.rem);
                        }
                    }
                } break;
                case CueCommandTypeSongwriter: {
                    if (tracksBegin() == tracksEnd()) {
                        songwriter = command.songwriter;
                    } else {
                        (tracksEnd() - 1)->songwriter = command.songwriter;
                    }
                } break;
                case CueCommandTypeTitle: {
                    if (tracksBegin() == tracksEnd()) {
                        title = command.title;
                    } else {
                        (tracksEnd() - 1)->title = command.title;
                    }
                } break;
                case CueCommandTypeTrack: {
                    auto& track = addTrack();
                    track.number = command.track.number;
                    track.dataType = command.track.dataType;
                } break;
            }
            item = item->next;
        }
        CueCommandListDestroy(&result);
    } else {
        CueCommandListDestroy(&result);
        std::string errorString(error);
        free(error);
        throw ParseError("Line " + std::to_string(errorLine) + ": " + errorString);
    }
}

static std::string escape(const std::string& s) {
    auto result = s;
    replace_all(result, "\"", "\\\"");
    return '"' + result + '"';
}
    
std::ostream& operator<<(std::ostream& o, const Disc& disc) {
    for (auto comment : disc.comments) {
        o << "REM " << comment << std::endl;
    }
    if (disc.cdTextFile) {
        o << "CDTEXTFILE " << escape(disc.cdTextFile.value()) << std::endl;
    }
    if (disc.catalog) {
        o << "CATALOG " << disc.catalog.value() << std::endl;
    }
    if (disc.performer) {
        o << "PERFORMER " << escape(disc.performer.value()) << std::endl;
    }
    if (disc.songwriter) {
        o << "SONGWRITER " << escape(disc.songwriter.value()) << std::endl;
    }
    if (disc.title) {
        o << "TITLE " << escape(disc.title.value()) << std::endl;
    }
    
    File const * currentFile = nullptr;
    for (auto track = disc.tracksCbegin(); track != disc.tracksCend(); ++track) {
        auto firstIndexFile = &track->indexesCbegin()->file();
        if (currentFile != firstIndexFile) {
            currentFile = firstIndexFile;
            o << "FILE " << escape(currentFile->path) << " " << currentFile->fileType << std::endl;
        }
        
        o << "  TRACK " << io::group(std::setw(2), std::setfill('0'), track->number) << " " << track->dataType << std::endl;
        
        for (auto comment : track->comments) {
            o << "    REM " << comment << std::endl;
        }
        if (track->performer) {
            o << "    PERFORMER " << escape(track->performer.value()) << std::endl;
        }
        if (track->songwriter) {
            o << "    SONGWRITER " << escape(track->songwriter.value()) << std::endl;
        }
        if (track->title) {
            o << "    TITLE " << escape(track->title.value()) << std::endl;
        }
        if (track->isrc) {
            o << "    ISRC " << track->isrc.value() << std::endl;
        }
        if (track->flags) {
            o << "    FLAGS " << track->flags.value() << std::endl;
        }
        if (track->pregap) {
            o << "    PREGAP " << track->pregap.value() << std::endl;
        }
        if (track->postgap) {
            o << "    POSTGAP " << track->postgap.value() << std::endl;
        }
        
        for (auto index = track->indexesCbegin(); index != track->indexesCend(); ++index) {
            if (currentFile != &index->file()) {
                currentFile = &index->file();
                o << "FILE " << escape(currentFile->path) << " " << currentFile->fileType << std::endl;
            }
            o << "    INDEX " << io::group(std::setw(2), std::setfill('0'), index->index) << " " << index->begin << std::endl;
            
            for (auto comment : index->comments) {
                o << "      REM " << comment << std::endl;
            }
        }
    }
    return o;
}
    
Split GapsAppendedSplitGenerator::split(const cue::Disc &disc) const {
    Split result;
    result.outputSheet = std::make_shared<Disc>();
    result.outputSheet->comments = disc.comments;
    result.outputSheet->catalog = disc.catalog;
    result.outputSheet->cdTextFile = disc.cdTextFile;
    result.outputSheet->performer = disc.performer;
    result.outputSheet->title = disc.title;
    result.outputSheet->songwriter = disc.songwriter;
    
    auto firstTrack = disc.tracksCbegin();
    if (firstTrack == disc.tracksCend()) {
        return result;
    }
    
    bool hasHTOA = firstTrack->indexesCbegin()->index == 0;
    if (hasHTOA) {
        auto htoaIndex = firstTrack->indexesCbegin();
        auto& file = htoaIndex->file();
        auto nextIndex = htoaIndex + 1;
        auto nextIndexUsesTheSameFile = &nextIndex->file() == &file;
        
        SplitInputSegment inputSegment;
        inputSegment.inputFile = file.path;
        inputSegment.begin = htoaIndex->begin;
        if (htoaIndex->begin != 0) {
            throw std::runtime_error("HTOA (track 1, index 0) must start at 00:00:00 instead of " + to_string(htoaIndex->begin));
        }
        if (nextIndexUsesTheSameFile) {
            inputSegment.end = nextIndex->begin;
        } else {
            inputSegment.end = none;
        }
        
        SplitOutput htoa;
        htoa.outputFile = _outputFileNameHandler(none);
        htoa.inputSegments.push_back(inputSegment);
        
        result.outputFiles.push_back(htoa);
        
        auto& outputSheetTrack = result.outputSheet->addTrack();
        outputSheetTrack.comments = firstTrack->comments;
        outputSheetTrack.dataType = firstTrack->dataType;
        outputSheetTrack.flags = firstTrack->flags;
        outputSheetTrack.isrc = firstTrack->isrc;
        outputSheetTrack.number = firstTrack->number;
        outputSheetTrack.performer = firstTrack->performer;
        outputSheetTrack.songwriter = firstTrack->songwriter;
        outputSheetTrack.title = firstTrack->title;
        
        auto& outputSheetFile = result.outputSheet->addFile();
        outputSheetFile.path = htoa.outputFile;
        outputSheetFile.fileType = "WAVE";
        
        auto& outputSheetIndex = outputSheetTrack.addIndex();
        outputSheetIndex.comments = htoaIndex->comments;
        outputSheetIndex.index = htoaIndex->index;
        outputSheetIndex.setFile(outputSheetFile);
        outputSheetIndex.begin = 0;
    }
    
    for (auto track = firstTrack; track != disc.tracksCend(); ++track) {
        if (track->pregap.get_value_or(0) != 0 && track != firstTrack) {
            throw std::runtime_error("Track " + std::to_string(track->number) + " has non-zero pregap " + std::to_string(track->pregap.value().samples));
        }
        
        if (track->dataType != "AUDIO") {
            auto& outputSheetTrack = result.outputSheet->addTrack();
            outputSheetTrack.comments = track->comments;
            outputSheetTrack.dataType = track->dataType;
            outputSheetTrack.flags = track->flags;
            outputSheetTrack.isrc = track->isrc;
            outputSheetTrack.number = track->number;
            outputSheetTrack.performer = track->performer;
            outputSheetTrack.songwriter = track->songwriter;
            outputSheetTrack.title = track->title;
            outputSheetTrack.pregap = track->pregap;
            
            if (track->indexesCend() - track->indexesCbegin() != 1) {
                throw std::runtime_error("non-AUDIO tracks must have exactly 1 INDEX");
            }
            
            auto& index = *track->indexesCbegin();
            auto& outputSheetIndex = outputSheetTrack.addIndex();
            outputSheetIndex.begin = result.outputFiles.rbegin()->inputSegments.rbegin()->end.get();
            outputSheetIndex.comments = index.comments;
            outputSheetIndex.index = index.index;
        } else {
            SplitOutput currentOutput;
            currentOutput.outputFile = _outputFileNameHandler(*track);
            
            auto& outputSheetFile = result.outputSheet->addFile();
            outputSheetFile.path = currentOutput.outputFile;
            outputSheetFile.fileType = "WAVE";
            
            auto outputSheetHasNoTracks = result.outputSheet->tracksBegin() == result.outputSheet->tracksEnd();
            if (outputSheetHasNoTracks || (result.outputSheet->tracksEnd() - 1)->number != track->number) {
                auto& outputSheetTrack = result.outputSheet->addTrack();
                outputSheetTrack.comments = track->comments;
                outputSheetTrack.dataType = track->dataType;
                outputSheetTrack.flags = track->flags;
                outputSheetTrack.isrc = track->isrc;
                outputSheetTrack.number = track->number;
                outputSheetTrack.performer = track->performer;
                outputSheetTrack.songwriter = track->songwriter;
                outputSheetTrack.title = track->title;
                outputSheetTrack.pregap = track->pregap;
            }
            
            bool hasNextTrack = (track + 1) != disc.tracksCend();
            
            for (auto index = track->indexesCbegin(); ; ++index) {
                
                bool isNextTracksZeroIndex = false;
                if (index == track->indexesCend()) {
                    if (!hasNextTrack) {
                        break;
                    }
                    
                    auto nextTrack = track + 1;
                    if (nextTrack->dataType != "AUDIO") {
                        break;
                    }
                    
                    auto firstIndexOfNextTrack = nextTrack->indexesCbegin();
                    if (firstIndexOfNextTrack->index != 0) {
                        break;
                    }
                    
                    index = firstIndexOfNextTrack;
                    isNextTracksZeroIndex = true;
                    auto& outputSheetTrack = result.outputSheet->addTrack();
                    outputSheetTrack.comments = nextTrack->comments;
                    outputSheetTrack.dataType = nextTrack->dataType;
                    outputSheetTrack.flags = nextTrack->flags;
                    outputSheetTrack.isrc = nextTrack->isrc;
                    outputSheetTrack.number = nextTrack->number;
                    outputSheetTrack.performer = nextTrack->performer;
                    outputSheetTrack.songwriter = nextTrack->songwriter;
                    outputSheetTrack.title = nextTrack->title;
                }
                
                if (index->index == 0 && !isNextTracksZeroIndex) {
                    continue;
                }
                
                auto& file = index->file();
                
                SplitInputSegment inputSegment;
                inputSegment.inputFile = file.path;
                inputSegment.begin = index->begin;
                inputSegment.end = none;
                if (isNextTracksZeroIndex || (index + 1) != track->indexesCend()) {
                    auto nextIndex = index + 1;
                    if (&nextIndex->file() == &file) {
                        inputSegment.end = nextIndex->begin;
                    }
                } else if ((track + 1) != disc.tracksCend()) {
                    auto nextTrack = track + 1;
                    auto nextTracksFirstIndex = nextTrack->indexesCbegin();
                    if (&nextTracksFirstIndex->file() == &file) {
                        inputSegment.end = nextTracksFirstIndex->begin;
                    }
                }
                
                auto& outputSheetIndex = (result.outputSheet->tracksEnd() - 1)->addIndex();
                outputSheetIndex.comments = index->comments;
                outputSheetIndex.index = index->index;
                outputSheetIndex.setFile(outputSheetFile);
                outputSheetIndex.begin = 0;
                for (auto inputSegment : currentOutput.inputSegments) {
                    outputSheetIndex.begin = outputSheetIndex.begin + (inputSegment.end.value_or(_inputFileDurationHandler(inputSegment.inputFile)) - inputSegment.begin);
                }
                
                currentOutput.inputSegments.push_back(inputSegment);
                
                if (isNextTracksZeroIndex) {
                    break;
                }
            }
            
            result.outputFiles.push_back(currentOutput);
        }
    }
    
    std::vector<SplitOutput> consolidatedOutputFiles;
    for (auto outputFile : result.outputFiles) {
        SplitOutput consolidatedOutputFile;
        consolidatedOutputFile.outputFile = outputFile.outputFile;
        consolidatedOutputFile.inputSegments.push_back(*outputFile.inputSegments.begin());
        for (auto inputSegment = outputFile.inputSegments.begin() + 1; inputSegment != outputFile.inputSegments.end(); ++inputSegment) {
            if (inputSegment->inputFile == consolidatedOutputFile.inputSegments.rbegin()->inputFile &&
                inputSegment->begin == consolidatedOutputFile.inputSegments.rbegin()->end.value()) {
                consolidatedOutputFile.inputSegments.rbegin()->end = inputSegment->end;
            } else {
                consolidatedOutputFile.inputSegments.push_back(*inputSegment);
            }
        }
        consolidatedOutputFiles.push_back(consolidatedOutputFile);
    }
    result.outputFiles = consolidatedOutputFiles;
    
    return result;
}
    
};
