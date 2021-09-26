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
using namespace boost;
    
int const CdFramesPerSecond = 75;
int const CdSamplesPerFrame = 588;
int const CdSamplesPerSecond = CdFramesPerSecond * CdSamplesPerFrame;
    
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
        return std::make_tuple(minutes, seconds, frames);
    }
    
    bool operator<(const Time& time) const { return samples < time.samples; }
    bool operator>(const Time& time) const { return time < *this; }
    bool operator==(const Time& time) const { return !(time < *this || time > *this); }
    bool operator!=(const Time& time) const { return !(*this == time); }
    
    Time operator+(const Time& time) const { return samples + time.samples; }
    Time operator-(const Time& time) const { return samples - time.samples; }
};
    
std::ostream& operator<<(std::ostream& o, const Time& t);
    
std::string to_string(const Time& t);
    
class File {
public:
    std::string path;
    std::string fileType;
};
    
class Disc;
class Track;
class Index;
    
class Index {
    friend class Track;
    int _file;
    Disc* _disc;
public:
    File& file() const;
    void setFile(const File& file);
    Time begin;
    int index;
    std::vector<std::string> comments;
};

class Track {
    friend class Disc;
    using IndexCollection = std::vector<Index>;
    
    IndexCollection _indexes;
    Disc* _disc;
public:
    using IndexIterator = IndexCollection::iterator;
    using ConstIndexIterator = IndexCollection::const_iterator;
    
    std::string dataType;
    int number;
    optional<std::string> performer;
    optional<std::string> title;
    optional<std::string> songwriter;
    optional<std::string> isrc;
    optional<std::string> flags;
    optional<Time> pregap;
    optional<Time> postgap;
    std::vector<std::string> comments;
    
    IndexIterator indexesBegin() { return _indexes.begin(); }
    IndexIterator indexesEnd() { return _indexes.end(); }
    ConstIndexIterator indexesCbegin() const { return _indexes.cbegin(); }
    ConstIndexIterator indexesCend() const { return _indexes.cend(); }
    Index& addIndex() {
        _indexes.push_back(Index());
        auto result = _indexes.rbegin();
        result->_disc = _disc;
        return *result;
    }
};

class ParseError : public std::runtime_error {
    using runtime_error::runtime_error;
};
    
class Disc {
    using TrackCollection = std::vector<Track>;
    using FileCollection = std::vector<File>;
    
    TrackCollection _tracks;
    FileCollection _files;
public:
    using FileIterator = FileCollection::iterator;
    using ConstFileIterator = FileCollection::const_iterator;
    using TrackIterator = TrackCollection::iterator;
    using ConstTrackIterator = TrackCollection::const_iterator;
    
    std::vector<std::string> comments;
    optional<std::string> catalog;
    optional<std::string> cdTextFile;
    optional<std::string> performer;
    optional<std::string> title;
    optional<std::string> songwriter;
    
    Disc(std::istream& input) throw(ParseError);
    
    Disc() = default;
    Disc(const Disc& disc) = delete;
    Disc& operator=(const Disc& disc) = delete;
    
    TrackIterator tracksBegin() { return _tracks.begin(); }
    TrackIterator tracksEnd() { return _tracks.end(); }
    ConstTrackIterator tracksCbegin() const { return _tracks.cbegin(); }
    ConstTrackIterator tracksCend() const { return _tracks.cend(); }
    Track& addTrack() {
        _tracks.push_back(Track());
        auto result = _tracks.rbegin();
        result->_disc = this;
        return *result;
    }
    
    FileIterator filesBegin() { return _files.begin(); }
    FileIterator filesEnd() { return _files.end(); }
    ConstFileIterator filesCbegin() const { return _files.cbegin(); }
    ConstFileIterator filesCend() const { return _files.cend(); }
    File& addFile() {
        _files.push_back(File());
        return *_files.rbegin();
    }
};
    
std::ostream& operator<<(std::ostream& o, const Disc& disc);

struct SplitInputSegment {
    std::string inputFile;
    Time begin;
    optional<Time> end;
};
    
struct SplitOutput {
    std::string outputFile;
    std::vector<SplitInputSegment> inputSegments;
};

struct Split {
    std::vector<SplitOutput> outputFiles;
    std::shared_ptr<Disc> outputSheet;
};
    
class SplitGenerator {
public:
    virtual Split split(const Disc& disc) const = 0;
};
    
class GapsAppendedSplitGenerator : public SplitGenerator {
public:
    using OutputFileNameHandler = std::function<std::string(const optional<const Track&> track)>; // boost::none means HTOA
    using InputFileDurationHandler = std::function<Time(const std::string& fileName)>;
    
private:
    OutputFileNameHandler _outputFileNameHandler;
    InputFileDurationHandler _inputFileDurationHandler;
    
public:
    GapsAppendedSplitGenerator(OutputFileNameHandler outputFileNameHandler, InputFileDurationHandler inputFileDurationHandler)
    : _outputFileNameHandler(outputFileNameHandler)
    , _inputFileDurationHandler(inputFileDurationHandler) {}
    
    virtual Split split(const Disc& disc) const override;
};
    
}

#endif /* CueParse_h */
