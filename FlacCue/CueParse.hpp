//
//  CueParse.h
//  FlacCue
//
//  Created by Tamás Zahola on 10/01/16.
//  Copyright © 2016 Tamás Zahola. All rights reserved.
//

#ifndef CueParse_h
#define CueParse_h

#include <iostream>
#include <tuple>
#include <string>
#include <vector>
#include <boost/optional.hpp>

namespace cue {
using namespace std;
using namespace boost;
    
int const CdFramesPerSecond = 75;
int const CdSamplesPerFrame = 588;
    
struct Time {
    long long samples;

    Time(long long samples = 0) : samples(samples) {}
    Time(std::tuple<int,int,int> cueTime) : Time(get<0>(cueTime), get<1>(cueTime), get<2>(cueTime)) {}
    Time(int minutes, int seconds, int frames)
    : samples(((minutes * 60 + seconds) * CdFramesPerSecond + frames) * CdSamplesPerFrame) {}
    
    bool isFrameBoundary() const {
        return (samples % CdSamplesPerFrame) == 0;
    }
    
    std::tuple<int,int,int> cueTime() const {
        auto frames = samples / CdSamplesPerFrame;
        auto seconds = frames / CdFramesPerSecond;
        auto minutes = seconds / 60;
        frames -= seconds * CdFramesPerSecond;
        seconds -= minutes * 60;
        return make_tuple(minutes, seconds, frames);
    }
    
    bool operator<(const Time& time) const {
        return samples < time.samples;
    }
    
    bool operator>(const Time& time) const {
        return time < *this;
    }
    
    bool operator==(const Time& time) const {
        return !(time < *this || time > *this);
    }
};
    
ostream& operator<<(ostream& o, const Time& t);
    
struct File {
    string path;
    string fileType;
};

struct Source {
    int file;
    Time begin;
    optional<Time> end;
};
    
struct Index {
    int index;
    vector<Source> sources;
    vector<string> comments;
};

struct Track {
    string dataType;
    int number;
    optional<string> performer;
    optional<string> title;
    optional<string> songwriter;
    optional<string> isrc;
    optional<string> flags;
    optional<Time> pregap;
    optional<Time> postgap;
    vector<Index> indexes;
    vector<string> comments;
};

class ParseError : public runtime_error {
    using runtime_error::runtime_error;
};
    
struct Disc {
    vector<File> files;
    vector<string> comments;
    optional<string> catalog;
    optional<string> cdTextFile;
    optional<string> performer;
    optional<string> title;
    optional<string> songwriter;
    vector<Track> tracks;
    
    Disc() = default;
    Disc(istream& input) throw(ParseError);
};
    
ostream& operator<<(ostream& o, const Disc& disc);

struct SplitInputSegment {
    string inputFile;
    Time begin;
    optional<Time> end;
};
    
struct SplitOutput {
    string outputFile;
    vector<SplitInputSegment> inputSegments;
};

struct Split {
    vector<SplitOutput> outputFiles;
    Disc outputSheet;
};
    
class SplitGenerator {
public:
    virtual Split split(const Disc& disc) const = 0;
};
    
class GapsAppendedSplitGenerator : public SplitGenerator {
public:
    virtual Split split(const Disc& disc) const override;
};
    
}

#endif /* CueParse_h */
