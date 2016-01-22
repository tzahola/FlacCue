//
//  CueParse.cpp
//  FlacCue
//
//  Created by Tamás Zahola on 10/01/16.
//  Copyright © 2016 Tamás Zahola. All rights reserved.
//

#include "CueParse.hpp"

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
    istream& input;
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
    
ostream& operator<<(ostream& o, const Time& t) {
    auto components = t.cueTime();
    return o
    << setfill('0') << setw(2) << get<0>(components) << ':'
    << setfill('0') << setw(2) << get<1>(components) << ':'
    << setfill('0') << setw(2) << get<2>(components);
}
    
Disc::Disc(istream& input) throw(ParseError) {
    
    struct CueCommandList result;
    CueCommandListInit(&result);
    char* error;
    
    struct CueParserExtra extra;
    CueParserReadCallbackContext context { input, false };
    extra.context  = &context;
    extra.readCallback = CueParserReadCallback;
    
    yyscan_t scanner;
    cue_lex_init(&scanner);
    cue_set_extra(&extra, scanner);
    auto status = cue_parse(scanner, &result, &error);
    cue_lex_destroy(scanner);
    
    if (status == 0) {
        CueCommandListItem* item = result.head;
        
        Index* lastIndex = nullptr;
        
        while (item != NULL) {
            auto command = item->command;
            switch (command.type) {
                case CueCommandTypeCatalog: this->catalog = command.catalog; break;
                case CueCommandTypeCdTextFile: this->cdTextFile = command.cdTextFile; break;
                case CueCommandTypeFile: {
                    this->files.push_back(File());
                    auto& file = *this->files.rbegin();
                    file.path = command.file.path;
                    file.fileType = command.file.fileType;
                    
                    if (lastIndex != nullptr) {
                        lastIndex->sources.rbegin()->end = none;
                        
                        Source source;
                        source.file = (int)this->files.size() - 1;
                        source.begin = 0;
                        source.end = none;
                        
                        lastIndex->sources.push_back(source);
                    }
                } break;
                case CueCommandTypeFlags: {
                    if (this->tracks.size() == 0) {
                        throw ParseError("FLAGS must be used within a TRACK!");
                    }
                    this->tracks.rbegin()->flags = command.flags;
                } break;
                case CueCommandTypeIndex: {
                    if (this->files.size() == 0) {
                        throw ParseError("INDEX can only be used after specifying a FILE!");
                    }
                    auto time = Time(command.index.time.minutes, command.index.time.seconds, command.index.time.frames);
                    if (lastIndex != nullptr) {
                        lastIndex->sources.rbegin()->end = time;
                        if (lastIndex->sources.rbegin()->begin == lastIndex->sources.rbegin()->end) {
                            lastIndex->sources.pop_back();
                        }
                    }
                    
                    Source source;
                    source.file = (int)this->files.size() - 1;
                    source.begin = time;
                    source.end = none;
                    
                    this->tracks.rbegin()->indexes.push_back(Index());
                    auto& index = *(this->tracks.rbegin()->indexes.rbegin());
                    index.index = command.index.index;
                    index.sources.push_back(source);
                    
                    lastIndex = &index;
                } break;
                case CueCommandTypeIsrc: {
                    if (this->tracks.size() == 0) {
                        throw ParseError("ISRC must be used within a TRACK!");
                    }
                    this->tracks.rbegin()->isrc = command.isrc;
                } break;
                case CueCommandTypePerformer:  {
                    if (this->tracks.size() == 0) {
                        this->performer = command.performer;
                    } else {
                        this->tracks.rbegin()->performer = command.performer;
                    }
                } break;
                case CueCommandTypePostgap: {
                    if (this->tracks.size() == 0) {
                        throw ParseError("POSTGAP must be used within a TRACK!");
                    }
                    this->tracks.rbegin()->postgap = Time(command.postGap.minutes, command.postGap.seconds, command.postGap.frames);
                } break;
                case CueCommandTypePregap: {
                    if (this->tracks.size() == 0) {
                        throw ParseError("PREGAP must be used within a TRACK!");
                    }
                    this->tracks.rbegin()->pregap = Time(command.preGap.minutes, command.preGap.seconds, command.preGap.frames);
                } break;
                case CueCommandTypeRem: {
                    if (this->tracks.size() == 0) {
                        this->comments.push_back(command.rem);
                    } else if (this->tracks.rbegin()->indexes.size() == 0) {
                        this->tracks.rbegin()->comments.push_back(command.rem);
                    } else {
                        this->tracks.rbegin()->indexes.rbegin()->comments.push_back(command.rem);
                    }
                } break;
                case CueCommandTypeSongwriter: {
                    if (this->tracks.size() == 0) {
                        this->songwriter = command.songwriter;
                    } else {
                        this->tracks.rbegin()->songwriter = command.songwriter;
                    }
                } break;
                case CueCommandTypeTitle: {
                    if (this->tracks.size() == 0) {
                        this->title = command.title;
                    } else {
                        this->tracks.rbegin()->title = command.title;
                    }
                } break;
                case CueCommandTypeTrack: {
                    Track track;
                    track.number = command.track.number;
                    track.dataType = command.track.dataType;
                    this->tracks.push_back(track);
                } break;
            }
            item = item->next;
        }
        CueCommandListDestroy(&result);
    } else {
        CueCommandListDestroy(&result);
        string errorString(error);
        free(error);
        throw ParseError(errorString);
    }
}
    
static string escape(const string& s) {
    auto result = s;
    replace_all(result, "\"", "\\\"");
    return '"' + result + '"';
}
    
ostream& operator<<(ostream& o, const Disc& disc) {
    for (auto comment : disc.comments) {
        o << "REM " << comment << endl;
    }
    if (disc.cdTextFile) {
        o << "CDTEXTFILE " << escape(disc.cdTextFile.value()) << endl;
    }
    if (disc.catalog) {
        o << "CATALOG " << disc.catalog.value() << endl;
    }
    if (disc.performer) {
        o << "PERFORMER " << escape(disc.performer.value()) << endl;
    }
    if (disc.songwriter) {
        o << "SONGWRITER " << escape(disc.songwriter.value()) << endl;
    }
    if (disc.title) {
        o << "TITLE " << escape(disc.title.value()) << endl;
    }
    
    File const * currentFile = nullptr;
    for (auto track : disc.tracks) {
        if (currentFile == nullptr) {
            currentFile = &disc.files[track.indexes.begin()->sources.begin()->file];
            o << "FILE " << escape(currentFile->path) << " " << currentFile->fileType << endl;
        }
        
        o << "  TRACK " << io::group(setw(2), setfill('0'), track.number) << " " << track.dataType << endl;
        
        for (auto comment : track.comments) {
            o << "    REM " << comment << endl;
        }
        if (track.performer) {
            o << "    PERFORMER " << escape(track.performer.value()) << endl;
        }
        if (track.songwriter) {
            o << "    SONGWRITER " << escape(track.songwriter.value()) << endl;
        }
        if (track.title) {
            o << "    TITLE " << escape(track.title.value()) << endl;
        }
        if (track.isrc) {
            o << "    ISRC " << track.isrc.value() << endl;
        }
        if (track.flags) {
            o << "    FLAGS " << track.flags.value() << endl;
        }
        if (track.pregap) {
            o << "    PREGAP " << track.pregap.value() << endl;
        }
        if (track.postgap) {
            o << "    POSTGAP " << track.postgap.value() << endl;
        }
        
        for (auto index : track.indexes) {
            auto currentSource = *index.sources.begin();
            if (currentFile == nullptr) {
                currentFile = &disc.files[currentSource.file];
                o << "FILE " << escape(currentFile->path) << " " << currentFile->fileType << endl;
            }
            o << "    INDEX " << io::group(setw(2), setfill('0'), index.index) << " " << currentSource.begin << endl;
            
            for (auto comment : index.comments) {
                o << "      REM " << comment << endl;
            }
            
            if (currentSource.end == none) {
                currentFile = nullptr;
            }
            
            for (auto source = index.sources.begin() + 1; source != index.sources.end(); ++source) {
                assert(currentSource.end == none);
                currentSource = *source;
                assert(currentSource.begin == 0);
                currentFile = &disc.files[currentSource.file];
                o << "FILE " << escape(currentFile->path) << " " << currentFile->fileType << endl;
                if (currentSource.end == none) {
                    currentFile = nullptr;
                }
            }
        }
    }
    return o;
}
    
Split GapsAppendedSplitGenerator::split(const cue::Disc &disc) const {
    Split result;
    
    int maxTrackNumber = max_element(disc.tracks.begin(), disc.tracks.end(), [](const Track& a, const Track& b) {
        return a.number < b.number;
    })->number;
    int trackNumberDigits = (int)ceil(log10(maxTrackNumber));
    
    auto firstTrack = disc.tracks[0];
    bool hasHTOA = firstTrack.indexes.begin()->index == 0;
    if (hasHTOA) {
        auto htoaIndex = *firstTrack.indexes.begin();
        
        string path = "00 - HTOA.flac";
        
        SplitOutput htoa;
        htoa.outputFile = path;
        for (auto source : htoaIndex.sources) {
            auto file = disc.files[source.file];
            
            SplitInputSegment inputSegment;
            inputSegment.inputFile = file.path;
            inputSegment.begin = source.begin;
            inputSegment.end = source.end;
            
            htoa.inputSegments.push_back(inputSegment);
        }
        
        result.outputFiles.push_back(htoa);
        
        File outputSheetFile;
        outputSheetFile.path = path;
        outputSheetFile.fileType = "WAVE";
        result.outputSheet.files.push_back(outputSheetFile);
        
        Source outputSheetSource;
        outputSheetSource.file = result.outputSheet.files.size() - 1;
        outputSheetSource.begin = Time(0,0,0);
        outputSheetSource.end = none;
        
        Index outputSheetIndex;
        outputSheetIndex.comments = htoaIndex.comments;
        outputSheetIndex.index = htoaIndex.index;
        outputSheetIndex.sources.push_back(outputSheetSource);
        
        Track outputSheetTrack;
        outputSheetTrack.comments = firstTrack.comments;
        outputSheetTrack.dataType = firstTrack.dataType;
        outputSheetTrack.flags = firstTrack.flags;
        outputSheetTrack.isrc = firstTrack.isrc;
        outputSheetTrack.number = firstTrack.number;
        outputSheetTrack.performer = firstTrack.performer;
        outputSheetTrack.songwriter = firstTrack.songwriter;
        outputSheetTrack.title = firstTrack.title;
        outputSheetTrack.indexes.push_back(outputSheetIndex);
        
        result.outputSheet.tracks.push_back(outputSheetTrack);
    }
    
    for (int i = 0; i < disc.tracks.size(); i++) {
        auto track = disc.tracks[i];
        
        SplitOutput currentOutput;
        
        string artist = track.performer.value_or(track.songwriter.value_or(disc.performer.value_or(disc.songwriter.value_or(""))));
        string title = track.title.value_or("");
        string path = (format("%1% - %2% - %3%.flac")
                       % io::group(setw(trackNumberDigits), setfill('0'), track.number)
                       % artist
                       % title).str();
        currentOutput.outputFile = path;
        
        File outputSheetFile;
        outputSheetFile.path = path;
        outputSheetFile.fileType = "WAVE";
        result.outputSheet.files.push_back(outputSheetFile);
        
        if (result.outputSheet.tracks.empty() || result.outputSheet.tracks.rbegin()->number != track.number) {
            Track outputSheetTrack;
            outputSheetTrack.comments = track.comments;
            outputSheetTrack.dataType = track.dataType;
            outputSheetTrack.flags = track.flags;
            outputSheetTrack.isrc = track.isrc;
            outputSheetTrack.number = track.number;
            outputSheetTrack.performer = track.performer;
            outputSheetTrack.songwriter = track.songwriter;
            outputSheetTrack.title = track.title;
            result.outputSheet.tracks.push_back(outputSheetTrack);
        }
        
        bool hasNextTrack = i + 1 < disc.tracks.size();
        
        for (int j = 0; j < track.indexes.size() + 1; j++) {
            Index index; kell a szamok hossza is...
            
            bool isNextTracksZeroIndex = false;
            if (j < track.indexes.size()) {
                index = track.indexes[j];
            } else {
                bool nextTrackHasZeroIndex =
                    j == track.indexes.size() &&
                    hasNextTrack &&
                    disc.tracks[i + 1].indexes[0].index == 0;
                
                if (nextTrackHasZeroIndex) {
                    index = disc.tracks[i + 1].indexes[0];
                    isNextTracksZeroIndex = true;
                }
            }
            
            if (index.index == 0 && !isNextTracksZeroIndex) {
                continue;
            }
            
            for (auto source : index.sources) {
                auto sourceFile = disc.files[source.file];
                
                SplitInputSegment inputSegment;
                inputSegment.inputFile = sourceFile.path;
                inputSegment.begin = source.begin;
                inputSegment.end = source.end;
                
                currentOutput.inputSegments.push_back(inputSegment);
            }
        }
        
        result.outputFiles.push_back(currentOutput);
    }
    
    
    return result;
}
    
};
