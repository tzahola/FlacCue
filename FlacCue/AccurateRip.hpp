//
//  AccurateRip.h
//  FlacCue
//
//  Created by Tamás Zahola on 29/01/16.
//  Copyright © 2016 Tamás Zahola. All rights reserved.
//

#ifndef AccurateRip_h
#define AccurateRip_h

#include <iostream>
#include <sstream>
#include <vector>
#include <array>
#include <iomanip>
#include <queue>
#include <boost/format.hpp>

#include "CueParse.hpp"

namespace accuraterip {
    
using TrackCRC = uint32_t;
using DiscID1 = uint32_t;
using DiscID2 = uint32_t;
using CDDBID = uint32_t;

class Track {
public:
    TrackCRC crc;
    TrackCRC frame450CRC;
    int count;
};

class Disc {
public:
    DiscID1 discId1;
    DiscID2 discId2;
    CDDBID cddbId;
    std::vector<Track> tracks;
};

class ParseError : public std::runtime_error {
    using runtime_error::runtime_error;
};

class Data {
    template<typename T> static std::istream& readLittleEndian(std::istream& is, T& x) {
        static_assert(std::is_integral<T>() && std::is_unsigned<T>(), "T must be an unsigned integral type!");
        uint8_t buf[sizeof(T)];
        is.read((char*)buf, sizeof(buf));
        for (int i = sizeof(buf) - 1; i >= 0; --i) {
            x = x << 8 | buf[i];
        }
        return is;
    }
    
    static std::istream& readDiscInfo(std::istream& is, uint8_t& trackCount, DiscID1& discId1, DiscID2& discId2, CDDBID& cddbId) {
        readLittleEndian(is, trackCount);
        readLittleEndian(is, discId1);
        readLittleEndian(is, discId2);
        readLittleEndian(is, cddbId);
        return is;
    }
    
    static std::istream& readTrackInfo(std::istream& is, uint8_t& count, TrackCRC& crc, TrackCRC& frame450CRC) {
        readLittleEndian(is, count);
        readLittleEndian(is, crc);
        readLittleEndian(is, frame450CRC);
        return is;
    }
public:
    std::vector<Disc> discs;
    
    Data(std::istream& stream) throw(ParseError) {
        uint8_t trackCount;
        DiscID1 discId1;
        DiscID2 discId2;
        CDDBID cddbId;
        while (readDiscInfo(stream, trackCount, discId1, discId2, cddbId)) {
            discs.push_back(Disc());
            auto disc = discs.rbegin();
            disc->discId1 = discId1;
            disc->discId2 = discId2;
            disc->cddbId = cddbId;
            
            uint8_t count;
            TrackCRC crc;
            TrackCRC frame450CRC;
            for (int i = 0; i < trackCount; ++i) {
                if (readTrackInfo(stream, count, crc, frame450CRC)) {
                    disc->tracks.push_back(Track());
                    auto track = disc->tracks.rbegin();
                    track->count = count;
                    track->crc = crc;
                    track->frame450CRC = frame450CRC;
                } else {
                    throw ParseError("Failed to read track info!");
                }
            }
        }
    }
};
    
class InvalidTOCException : std::runtime_error {
public:
    using runtime_error::runtime_error;
};
    
using TrackNumber = unsigned int;
constexpr TrackNumber LeadOutTrack = 0xAA;
using Time = cue::Time;

struct TableOfContentsEntry {
    TrackNumber trackNumber;
    Time startOffset;
};
    
class TableOfContents {
    std::vector<TableOfContentsEntry> _entries;
public:
    using ConstEntryIterator = decltype(_entries)::const_iterator;
    
    static TableOfContents CreateFromTrackLengths(const std::vector<Time>& trackLengths, const Time& firstTrackOffset = 0) throw(InvalidTOCException) {
        if (trackLengths.size() > 99) {
            throw InvalidTOCException((boost::format("A disc can contain at most 99 tracks. (instead of %1%)") % trackLengths.size()).str());
        }
        if (trackLengths.empty()) {
            throw InvalidTOCException("The TOC must not be empty!");
        }
        if (!firstTrackOffset.isFrameBoundary()) {
            throw InvalidTOCException((boost::format("First track offset '%1%' is not a frame boundary!") % firstTrackOffset).str());
        }
        
        TableOfContents result;
        result._entries.reserve(trackLengths.size() + 1);
        result._entries.push_back({ 1, firstTrackOffset });
        for (auto trackLength : trackLengths) {
            if (!trackLength.isFrameBoundary()) {
                throw InvalidTOCException((boost::format("Track length '%1%' is not a frame boundary!") % trackLength).str());
            }
            result._entries.push_back({
                result._entries.rbegin()->trackNumber + 1,
                result._entries.rbegin()->startOffset + trackLength
            });
        }
        result._entries.rbegin()->trackNumber = LeadOutTrack;
        
        return result;
    }
    
    static TableOfContents CreateFromTrackOffsets(const std::vector<Time>& trackOffsets) {
        if (trackOffsets.size() < 2) {
            throw InvalidTOCException("trackOffsets must contain at least 2 items! (the start offset of at least 1 track, and the lead-out track)");
        }
        std::vector<Time> trackLengths;
        for (auto trackOffset = trackOffsets.begin(); trackOffset != trackOffsets.end() - 1; ++trackOffset) {
            trackLengths.push_back(*(trackOffset + 1) - *trackOffset);
        }
        return CreateFromTrackLengths(trackLengths, trackOffsets[0]);
    }
    
    ConstEntryIterator begin() const { return _entries.cbegin(); }
    ConstEntryIterator end() const { return _entries.cend(); }
    
    const TableOfContentsEntry& operator[](int i) const {
        return _entries[i];
    }
    
    int numberOfEntries() const {
        return (int)_entries.size();
    }
};

static inline uint32_t fold(uint64_t x) {
    return (uint32_t)(x >> 32) + (uint32_t)x;
}

template<typename T> static inline int sumDigits(T x) {
    static_assert(std::is_integral<T>(), "sumDigits can only be used with integral types!");
    return x == 0 ? 0 : ((x % 10) + sumDigits(x / 10));
}
    
static uint32_t calculateCDDBDiscId(const TableOfContents& toc) {
    uint32_t cddbDiscId = 0;
    for (auto tocEntry : toc) {
        uint32_t startOffset = (uint32_t)(tocEntry.startOffset.samples / cue::CdSamplesPerFrame);
        if (tocEntry.trackNumber != LeadOutTrack) {
            cddbDiscId += sumDigits(startOffset / cue::CdFramesPerSecond + 2);
        }
    }
    return ((cddbDiscId % 255) << 24) +
    (((uint32_t)((toc.end() - 1)->startOffset.samples / cue::CdSamplesPerSecond) - (uint32_t)(toc.begin()->startOffset.samples / cue::CdSamplesPerSecond)) << 8) +
    (uint32_t)(toc.numberOfEntries() - 1);
}
    
static uint32_t calculateARDiscId1(const TableOfContents& toc) {
    uint32_t arDiscId1 = 0;
    for (auto tocEntry : toc) {
        uint32_t startOffset = (uint32_t)(tocEntry.startOffset.samples / cue::CdSamplesPerFrame);
        arDiscId1 += startOffset;
    }
    return arDiscId1;
}
    
static uint32_t calculateARDiscId2(const TableOfContents& toc) {
    uint32_t arDiscId2 = 0;
    for (auto tocEntry : toc) {
        uint32_t startOffset = (uint32_t)(tocEntry.startOffset.samples / cue::CdSamplesPerFrame);
        auto isLeadOutTrack = tocEntry.trackNumber == LeadOutTrack;
        arDiscId2 += std::max<uint32_t>(startOffset, 1) * (isLeadOutTrack ? toc.numberOfEntries() : tocEntry.trackNumber);
    }
    return arDiscId2;
}
    
static std::string calculateARDataURL(const TableOfContents& toc) {
    uint32_t arDiscId1 = calculateARDiscId1(toc);
    uint32_t arDiscId2 = calculateARDiscId2(toc);
    uint32_t cddbDiscId = calculateCDDBDiscId(toc);
    
    std::string arTrackCountString = (std::stringstream() << std::setw(3) << std::setfill('0') << (toc.numberOfEntries() - 1)).str();
    std::string arDiscId1String = (std::stringstream() << std::setw(8) << std::setfill('0') << std::setbase(16) << arDiscId1).str();
    std::string arDiscId2String = (std::stringstream() << std::setw(8) << std::setfill('0') << std::setbase(16) << arDiscId2).str();
    std::string cddbDiscIdString = (std::stringstream() << std::setw(8) << std::setfill('0') << std::setbase(16) << cddbDiscId).str();
    return (boost::format("http://www.accuraterip.com/accuraterip/%1%/%2%/%3%/dBAR-%4%-%5%-%6%-%7%.bin")
            % arDiscId1String[7]
            % arDiscId1String[6]
            % arDiscId1String[5]
            % arTrackCountString
            % arDiscId1String
            % arDiscId2String
            % cddbDiscIdString).str();
}

class ChecksumGenerator {
    const TableOfContents _toc;
    const int32_t _minimumOffset;
    const int32_t _maximumOffset;
    
    
    uint32_t _arDiscSampleIndex;
    uint32_t _arTrackSampleIndex;
    int _currentTrack;
    
    using Checksums = std::vector<TrackCRC>;
    std::queue<uint32_t> _offsetCalculationSamples;
    std::vector<uint32_t> _offsetCalculationSums;
    std::vector<Checksums> _v1Checksums;
    
    int numberOfTracks() const {
        return _toc.numberOfEntries() - 1;
    }
    
public:
    const std::string accurateRipDataURL;
    
    const TrackCRC& v1ChecksumWithOffset(int track, int32_t offset) const {
        assert(offset >= _minimumOffset && offset <= _maximumOffset);
        return _v1Checksums[track].at(offset - _minimumOffset);
    }
    
    ChecksumGenerator(const TableOfContents& toc, int32_t minimumOffset = -2000, int32_t maximumOffset = 2000)
    : _toc(toc), accurateRipDataURL(calculateARDataURL(toc)), _minimumOffset(minimumOffset), _maximumOffset(maximumOffset) {
        
        assert(_minimumOffset <= _maximumOffset);
        assert(_minimumOffset > -(cue::CdSamplesPerFrame * 5 - 1)); // Backward offset cannot be larger than five frames - 1 samples
        assert(_maximumOffset < cue::CdSamplesPerFrame * 5); // Forward offset cannot be larger than five frames
        
        _arTrackSampleIndex = 0;
        _currentTrack = -1;
        
        _v1Checksums.resize(numberOfTracks());
        for (auto& checksums : _v1Checksums) {
            checksums.push_back(0);
        }
        
        _offsetCalculationSums.resize(numberOfTracks(), 0);
    }
    
    uint32_t firstSampleIndexForTrack(int track) {
        return track == 0 ? (cue::CdSamplesPerFrame * 5 - 1) : 0;
    }
    
    uint32_t lastSampleIndexForTrack(int track) {
        assert(track <= numberOfTracks() - 1);
        if (track == -1) {
            return firstSampleIndexForTrack(0) + _minimumOffset - 1;
        } else {
            return (uint32_t)(_toc[track + 1].startOffset.samples - _toc[track].startOffset.samples - 1
                              - (track == numberOfTracks() - 1 ? cue::CdSamplesPerFrame * 5 : 0));
        }
    }
    
    void processSamples(int32_t const * const buffer[2], uint32_t count) {
        
        for (auto i = 0; i < count; ++i) {
            uint32_t samples = (((uint16_t)buffer[1][i] << 16) | (uint16_t)buffer[0][i]);
            
            if (_currentTrack != -1) {
                if (_arTrackSampleIndex < firstSampleIndexForTrack(_currentTrack) + (_maximumOffset - _minimumOffset)) {
                    if (_currentTrack != numberOfTracks()) {
                        _offsetCalculationSamples.push(samples);
                    }
                    
                    if (_currentTrack != 0) {
                        uint32_t previousTrackIndex = _currentTrack - 1;
                        _v1Checksums[previousTrackIndex].push_back(
                            *(_v1Checksums[previousTrackIndex].end() - 1) -
                            _offsetCalculationSums[previousTrackIndex] -
                            (firstSampleIndexForTrack(previousTrackIndex) * _offsetCalculationSamples.front()) +
                            samples * (lastSampleIndexForTrack(previousTrackIndex) + 1));
                        
                        _offsetCalculationSums[previousTrackIndex] -= _offsetCalculationSamples.front();
                        _offsetCalculationSums[previousTrackIndex] += samples;
                        _offsetCalculationSamples.pop();
                    }
                }
                
                if (_currentTrack != numberOfTracks()) {
                    _v1Checksums[_currentTrack][0] += (_arTrackSampleIndex + 1) * samples;
                    _offsetCalculationSums[_currentTrack] += samples;
                }
            }
            
            if (_currentTrack != numberOfTracks() && _arTrackSampleIndex == lastSampleIndexForTrack(_currentTrack)) {
                ++_currentTrack;
                _arTrackSampleIndex = firstSampleIndexForTrack(_currentTrack);
            } else {
                ++_arTrackSampleIndex;
            }
        }
    }
};
    
}

#endif /* AccurateRip_h */
