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

using TOC = std::vector<cue::Time>;

static inline uint32_t fold(uint64_t x) {
    return (uint32_t)(x >> 32) + (uint32_t)x;
}

template<typename T> static inline int sumDigits(T x) {
    static_assert(std::is_integral<T>(), "sumDigits can only be used with integral types!");
    return x == 0 ? 0 : ((x % 10) + sumDigits(x / 10));
}

class InvalidTOCException : std::runtime_error {
public:
    using runtime_error::runtime_error;
};
    
const int32_t OffsetFindingMinimumOffset = -1000;
const int32_t OffsetFindingMaximumOffset = 1000;
const int32_t OffsetFindingNumberOfOffsets = OffsetFindingMaximumOffset - OffsetFindingMinimumOffset + 1;
    
class ChecksumGenerator {
    TOC _toc;
    uint32_t _arDiscSampleIndex;
    uint32_t _arTrackSampleIndex;
    int _currentTrack;
    
    using Frame450Checksums = std::array<TrackCRC, OffsetFindingNumberOfOffsets>;
    std::vector<Frame450Checksums> _v1Frame450Checksums;
    std::array<uint32_t, OffsetFindingNumberOfOffsets - 1> _offsetCalculationSamples;
    uint32_t _offsetCalculationSum;
public:
    std::vector<TrackCRC> v1Checksums;
    std::vector<TrackCRC> v2Checksums;
    std::string accurateRipDataURL;
    
    const TrackCRC& v1Frame450ChecksumWithOffset(int track, int32_t offset) const {
        return _v1Frame450Checksums[track].at(offset - OffsetFindingMinimumOffset);
    }
    
    int finishedFiles() const {
        return _currentTrack;
    }
    
    ChecksumGenerator(const TOC& toc) : _toc(toc) {
        
        for (auto tocEntry : toc) {
            if (!tocEntry.isFrameBoundary()) {
                throw InvalidTOCException((boost::format("TOC entry '%1%' is not a frame boundary!") % tocEntry).str());
            }
        }
        
        uint32_t arDiscId1 = 0;
        uint32_t arDiscId2 = 0;
        uint32_t cddbDiscId = 0;
        for (auto i = 0; i < toc.size(); ++i) {
            uint32_t startOffset = (uint32_t)(toc[i].samples / cue::CdSamplesPerFrame);
            arDiscId1 += startOffset;
            arDiscId2 += std::max<uint32_t>(startOffset, 1) * (i + 1);
            auto isLeadOutTrack = (i == toc.size() - 1);
            if (!isLeadOutTrack) {
                cddbDiscId += sumDigits(startOffset / cue::CdFramesPerSecond + 2);
            }
        }
        
        cddbDiscId = ((cddbDiscId % 255) << 24) +
        (((uint32_t)(toc.rbegin()->samples / cue::CdSamplesPerSecond) - (uint32_t)(toc.begin()->samples / cue::CdSamplesPerSecond)) << 8) +
        (uint32_t)(toc.size() - 1);
        
        std::string arTrackCountString = (std::stringstream() << std::setw(3) << std::setfill('0') << (toc.size() - 1)).str();
        std::string arDiscId1String = (std::stringstream() << std::setw(8) << std::setfill('0') << std::setbase(16) << arDiscId1).str();
        std::string arDiscId2String = (std::stringstream() << std::setw(8) << std::setfill('0') << std::setbase(16) << arDiscId2).str();
        std::string cddbDiscIdString = (std::stringstream() << std::setw(8) << std::setfill('0') << std::setbase(16) << cddbDiscId).str();
        accurateRipDataURL = (boost::format("http://www.accuraterip.com/accuraterip/%1%/%2%/%3%/dBAR-%4%-%5%-%6%-%7%.bin")
                              % arDiscId1String[7]
                              % arDiscId1String[6]
                              % arDiscId1String[5]
                              % arTrackCountString
                              % arDiscId1String
                              % arDiscId2String
                              % cddbDiscIdString).str();
        _arDiscSampleIndex = (uint32_t)_toc[0].samples;
        _arTrackSampleIndex = 0;
        _currentTrack = 0;
        
        v1Checksums.resize(_toc.size() - 1, 0);
        v2Checksums.resize(_toc.size() - 1, 0);
        
        _v1Frame450Checksums.resize(_toc.size() - 1);
        std::for_each(_v1Frame450Checksums.begin(),
                      _v1Frame450Checksums.end(),
                      std::bind(&Frame450Checksums::fill, std::placeholders::_1, 0));
        
        _offsetCalculationSum = 0;
    }
    
    void processSamples(int32_t const * const buffer[2], uint32_t count) {
        for (auto i = 0; i < count; ++i) {
            assert(_currentTrack < _toc.size() - 1);
            if (_arDiscSampleIndex >= (_toc.begin()->samples + cue::CdSamplesPerFrame * 5 - 1) &&
                _arDiscSampleIndex < (_toc.rbegin()->samples - cue::CdSamplesPerFrame * 5)) {
                uint32_t samples = (((uint16_t)buffer[1][i] << 16) | (uint16_t)buffer[0][i]);
                
                v1Checksums[_currentTrack] += (_arTrackSampleIndex + 1) * samples;
                v2Checksums[_currentTrack] += fold((uint64_t)(_arTrackSampleIndex + 1) * (uint64_t)samples);
                
                auto leftmostOffsettedFrame450FirstSampleIndex = 450 * cue::CdSamplesPerFrame + OffsetFindingMinimumOffset;
                auto rightmostOffsettedFrame450LastSampleIndex = 451 * cue::CdSamplesPerFrame + OffsetFindingMaximumOffset - 1;
                
                if (_arTrackSampleIndex >= leftmostOffsettedFrame450FirstSampleIndex &&
                    _arTrackSampleIndex <= rightmostOffsettedFrame450LastSampleIndex) {
                    if (_arTrackSampleIndex < leftmostOffsettedFrame450FirstSampleIndex + cue::CdSamplesPerFrame) {
                        uint32_t multiplier = _arTrackSampleIndex - leftmostOffsettedFrame450FirstSampleIndex + 1;
                        _v1Frame450Checksums[_currentTrack][0] += multiplier * samples;
                        _offsetCalculationSum += samples;
                    } else {
                        auto crcIndex = _arTrackSampleIndex - leftmostOffsettedFrame450FirstSampleIndex - cue::CdSamplesPerFrame + 1;
                        _v1Frame450Checksums[_currentTrack][crcIndex] = _v1Frame450Checksums[_currentTrack].at(crcIndex - 1) - _offsetCalculationSum + samples * cue::CdSamplesPerFrame;
                        _offsetCalculationSum = _offsetCalculationSum - _offsetCalculationSamples.at(crcIndex - 1) + samples;
                    }
                    if (_arTrackSampleIndex <= rightmostOffsettedFrame450LastSampleIndex - cue::CdSamplesPerFrame) {
                        _offsetCalculationSamples[_arTrackSampleIndex - leftmostOffsettedFrame450FirstSampleIndex] = samples;
                    }
                }
            }
            ++_arTrackSampleIndex;
            ++_arDiscSampleIndex;
            if (_arDiscSampleIndex == _toc[_currentTrack + 1].samples) {
                _currentTrack++;
                _arTrackSampleIndex = 0;
                _offsetCalculationSum = 0;
            }
        }
    }
};
    
}

#endif /* AccurateRip_h */
