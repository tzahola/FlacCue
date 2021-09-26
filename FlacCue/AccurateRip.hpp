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
        } else if (trackLengths.empty()) {
            throw InvalidTOCException("The TOC must not be empty!");
        } else if (!firstTrackOffset.isFrameBoundary()) {
            throw InvalidTOCException((boost::format("First track offset '%1%' is not a frame boundary!") % firstTrackOffset).str());
        }
        
        TableOfContents result;
        result._entries.reserve(trackLengths.size() + 1);
        result._entries.push_back({ 1, firstTrackOffset });
        for (auto trackLength : trackLengths) {
            if (!trackLength.isFrameBoundary()) {
                throw InvalidTOCException((boost::format("Track length '%1%' is not a frame boundary!") % trackLength).str());
            } else if (trackLength < Time(0, 4, 0)) {
                throw InvalidTOCException((boost::format("Forbidden track length of '%1%'! (less than 4 seconds)") % trackLength).str());
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
    
    cue::Time trackLengthAt(int track) const {
        assert(track >= 0 && track < numberOfEntries() - 1);
        return (*this)[track + 1].startOffset - (*this)[track].startOffset;
    }
    
    cue::Time totalLength() const {
        return _entries.crbegin()->startOffset - _entries.cbegin()->startOffset;
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
    using Checksums = std::vector<TrackCRC>;
    
    const TableOfContents _toc;
    
    class V1ChecksumGenerator {
        const int32_t _minimumOffset;
        const int32_t _maximumOffset;
        
        uint32_t _sampleIndex;
        int _baseChecksumCalculationTrack;
        int _derivedChecksumsCalculationTrack;
        std::queue<uint32_t> _offsetCalculationSamples;
        std::vector<uint32_t> _offsetCalculationSums;
        std::vector<Checksums> _checksums;
        
        std::vector<uint32_t> _firstSampleIndexes;
        std::vector<uint32_t> _firstSampleMultipliers;
        std::vector<uint32_t> _lastSampleIndexes;
    public:
        V1ChecksumGenerator(const TableOfContents& toc,
                            const std::vector<uint32_t>& firstSampleIndexes,
                            const std::vector<uint32_t>& firstSampleMultipliers,
                            const std::vector<uint32_t>& lastSampleIndexes,
                            int32_t minimumOffset,
                            int32_t maximumOffset)
        : _firstSampleIndexes(firstSampleIndexes),
        _firstSampleMultipliers(firstSampleMultipliers),
        _lastSampleIndexes(lastSampleIndexes),
        _minimumOffset(minimumOffset),
        _maximumOffset(maximumOffset) {
            
            _sampleIndex = (uint32_t)toc[0].startOffset.samples;
            _baseChecksumCalculationTrack = 0;
            _derivedChecksumsCalculationTrack = 0;
            
            auto numberOfTracks = toc.numberOfEntries() - 1;
            
            _checksums.resize(numberOfTracks);
            for (auto& checksums : _checksums) {
                checksums.push_back(0);
            }
            
            _offsetCalculationSums.resize(numberOfTracks, 0);
        }
        
        const TrackCRC& checksumWithOffset(int track, int32_t offset) const {
            assert(offset >= _minimumOffset && offset <= _maximumOffset);
            return _checksums[track].at(offset - _minimumOffset);
        }
        
        void processSamples(int32_t const * const buffer[2], uint32_t count) {
        
            for (auto i = 0; i < count; ++i) {
                uint32_t samples = (((uint16_t)buffer[1][i] << 16) | (uint16_t)buffer[0][i]);
                
                auto numberOfTracks = _checksums.size();
                
                if (_sampleIndex >= _firstSampleIndexes[_baseChecksumCalculationTrack] + _minimumOffset) {
                    
                    if (_sampleIndex < _firstSampleIndexes[_baseChecksumCalculationTrack] + _maximumOffset) {
                        _offsetCalculationSamples.push(samples);
                    }
                    
                    if (_sampleIndex <= _lastSampleIndexes[_baseChecksumCalculationTrack] + _minimumOffset) {
                        uint32_t multiplier = _firstSampleMultipliers[_baseChecksumCalculationTrack] + ((_sampleIndex - _minimumOffset) - _firstSampleIndexes[_baseChecksumCalculationTrack]);
                        _checksums[_baseChecksumCalculationTrack][0] += multiplier * samples;
                        _offsetCalculationSums[_baseChecksumCalculationTrack] += samples;
                    }
                }
                
                if (_sampleIndex > _lastSampleIndexes[_derivedChecksumsCalculationTrack] + _minimumOffset &&
                    _sampleIndex <= _lastSampleIndexes[_derivedChecksumsCalculationTrack] + _maximumOffset) {
                    assert(!_offsetCalculationSamples.empty());
                    
                    uint32_t multiplier = _firstSampleMultipliers[_derivedChecksumsCalculationTrack] + (_lastSampleIndexes[_derivedChecksumsCalculationTrack] - _firstSampleIndexes[_derivedChecksumsCalculationTrack]);
                    _checksums[_derivedChecksumsCalculationTrack].push_back(
                        *(_checksums[_derivedChecksumsCalculationTrack].end() - 1) -
                        _offsetCalculationSums[_derivedChecksumsCalculationTrack] -
                        ((_firstSampleMultipliers[_derivedChecksumsCalculationTrack] - 1) * _offsetCalculationSamples.front()) +
                        multiplier * samples);
                    
                    _offsetCalculationSums[_derivedChecksumsCalculationTrack] -= _offsetCalculationSamples.front();
                    _offsetCalculationSums[_derivedChecksumsCalculationTrack] += samples;
                    _offsetCalculationSamples.pop();
                }
                
                if (_sampleIndex == _lastSampleIndexes[_derivedChecksumsCalculationTrack] + _maximumOffset) {
                    ++_derivedChecksumsCalculationTrack;
                }
                
                if (_baseChecksumCalculationTrack < numberOfTracks - 1 &&
                    _sampleIndex == _firstSampleIndexes[_baseChecksumCalculationTrack + 1] - 1 + _minimumOffset) {
                    ++_baseChecksumCalculationTrack;
                }
                
                ++_sampleIndex;
            }
        }
    };
    
    class V2ChecksumGenerator {
        uint32_t _sampleIndex;
        int _track;
        std::vector<TrackCRC> _checksums;
        
        std::vector<uint32_t> _firstSampleIndexes;
        std::vector<uint32_t> _firstSampleMultipliers;
        std::vector<uint32_t> _lastSampleIndexes;
    public:
        V2ChecksumGenerator(const TableOfContents& toc,
                            const std::vector<uint32_t>& firstSampleIndexes,
                            const std::vector<uint32_t>& firstSampleMultipliers,
                            const std::vector<uint32_t>& lastSampleIndexes)
        : _firstSampleIndexes(firstSampleIndexes),
        _firstSampleMultipliers(firstSampleMultipliers),
        _lastSampleIndexes(lastSampleIndexes) {
            auto numberOfTracks = toc.numberOfEntries() - 1;
            _track = 0;
            _checksums = std::vector<uint32_t>(numberOfTracks, 0);
            _sampleIndex = (uint32_t)toc[0].startOffset.samples;
        }
        
        TrackCRC checksum(int track) const {
            return _checksums[track];
        }
        
        void processSamples(int32_t const * const buffer[2], uint32_t count) {
            
            for (auto i = 0; i < count; ++i) {
                if (_sampleIndex >= _firstSampleIndexes[_track] && _sampleIndex <= _lastSampleIndexes[_track]) {
                    uint32_t samples = (((uint16_t)buffer[1][i] << 16) | (uint16_t)buffer[0][i]);
                    uint32_t multiplier = _firstSampleMultipliers[_track] + _sampleIndex - _firstSampleIndexes[_track];
                    _checksums[_track] += fold((uint64_t)multiplier * (uint64_t)samples);
                }
                
                auto numberOfTracks = _checksums.size();
                if (_track < numberOfTracks - 1 &&
                    _sampleIndex == _firstSampleIndexes[_track + 1] - 1) {
                    ++_track;
                }
                
                ++_sampleIndex;
            }
        }
    };
    
    int numberOfTracks() const {
        return _toc.numberOfEntries() - 1;
    };
    
    V1ChecksumGenerator _v1ChecksumGenerator;
    V1ChecksumGenerator _v1Frame450ChecksumGenerator;
    V2ChecksumGenerator _v2ChecksumGenerator;
    int32_t _samplesProcessed;
    
    static std::vector<uint32_t> calculateFirstSampleIndexesForV1Checksum(const TableOfContents& toc) {
        auto numberOfTracks = toc.numberOfEntries() - 1;
        std::vector<uint32_t> firstSampleIndexes;
        for (auto track = 0; track < numberOfTracks; ++track) {
            bool isFirstTrack = track == 0;
            uint32_t firstSampleIndex = (uint32_t)toc[track].startOffset.samples + (isFirstTrack ? (cue::CdSamplesPerFrame * 5 - 1) : 0);
            firstSampleIndexes.push_back(firstSampleIndex);
        }
        return firstSampleIndexes;
    }
    
    static std::vector<uint32_t> calculateLastSampleIndexesForV1Checksum(const TableOfContents& toc) {
        auto numberOfTracks = toc.numberOfEntries() - 1;
        std::vector<uint32_t> lastSampleIndexes;
        for (auto track = 0; track < numberOfTracks; ++track) {
            bool isLastTrack = track == numberOfTracks - 1;
            uint32_t lastSampleIndex =  (uint32_t)(toc[track + 1].startOffset.samples - 1) - (isLastTrack ? cue::CdSamplesPerFrame * 5 : 0);
            lastSampleIndexes.push_back(lastSampleIndex);
        }
        return lastSampleIndexes;
    }
    
    static std::vector<uint32_t> calculateFirstSampleMultipliersForV1Checksum(const TableOfContents& toc) {
        auto numberOfTracks = toc.numberOfEntries() - 1;
        std::vector<uint32_t> firstSampleMultipliers;
        for (auto track = 0; track < numberOfTracks; ++track) {
            bool isFirstTrack = track == 0;
            uint32_t firstSampleMultiplier = isFirstTrack ? cue::CdSamplesPerFrame * 5 : 1;
            firstSampleMultipliers.push_back(firstSampleMultiplier);
        }
        return firstSampleMultipliers;
    }
    
    static std::vector<uint32_t> calculateFirstSampleIndexesForV1Frame450Checksum(const TableOfContents& toc) {
        std::vector<uint32_t> firstSampleIndexes;
        for (auto track = 0; track < toc.numberOfEntries() - 1; ++track) {
            firstSampleIndexes.push_back((uint32_t)toc[track].startOffset.samples + cue::CdSamplesPerFrame * 450);
        }
        return firstSampleIndexes;
    }
    
    static std::vector<uint32_t> calculateLastSampleIndexesForV1Frame450Checksum(const TableOfContents& toc) {
        std::vector<uint32_t> lastSampleIndexes;
        for (auto track = 0; track < toc.numberOfEntries() - 1; ++track) {
            lastSampleIndexes.push_back((uint32_t)toc[track].startOffset.samples + cue::CdSamplesPerFrame * 451 - 1);
        }
        return lastSampleIndexes;
    }
    
    static std::vector<uint32_t> calculateFirstSampleMultipliersForV1Frame450Checksum(const TableOfContents& toc) {
        return std::vector<uint32_t>(toc.numberOfEntries() - 1, 1);
    }
    
    static std::vector<uint32_t> calculateFirstSampleIndexesForV2Checksum(const TableOfContents& toc) {
        return calculateFirstSampleIndexesForV1Checksum(toc);
    }
    
    static std::vector<uint32_t> calculateLastSampleIndexesForV2Checksum(const TableOfContents& toc) {
        return calculateLastSampleIndexesForV1Checksum(toc);
    }
    
    static std::vector<uint32_t> calculateFirstSampleMultipliersForV2Checksum(const TableOfContents& toc) {
        return calculateFirstSampleMultipliersForV1Checksum(toc);
    }
    
    void ensureDone() const {
        if (_samplesProcessed != _toc.totalLength().samples) {
            throw std::runtime_error("Received samples (" + std::to_string(_samplesProcessed) + ") "
                                     "less than indicated by the TOC (" + std::to_string(_toc.totalLength().samples) + ")");
        }
    }
public:
    const std::string accurateRipDataURL;
    const int32_t _minimumOffset;
    const int32_t _maximumOffset;
    
    int32_t minimumOffset() const { return _minimumOffset; }
    int32_t maximumOffset() const { return _maximumOffset; }
    
    TrackCRC v1ChecksumWithOffset(int track, int32_t offset) const {
        ensureDone();
        return _v1ChecksumGenerator.checksumWithOffset(track, offset);
    }
    
    bool hasV1Frame450Checksum(int track) const {
        return !(_toc.trackLengthAt(track) < Time(0, 0, 451));
    }
    
    TrackCRC v1Frame450ChecksumWithOffset(int track, int32_t offset) const {
        ensureDone();
        if (!hasV1Frame450Checksum(track)) {
            throw std::runtime_error("Track " + std::to_string(track) + " is too short for Frame450 checksum!");
        }
        return _v1Frame450ChecksumGenerator.checksumWithOffset(track, offset);
    }
    
    TrackCRC v2Checksum(int track) const {
        ensureDone();
        return _v2ChecksumGenerator.checksum(track);
    }
    
    ChecksumGenerator(const TableOfContents& toc, int32_t minimumOffset = -2939, int32_t maximumOffset = 2940)
    : _toc(toc),
    accurateRipDataURL(calculateARDataURL(toc)),
    _minimumOffset(minimumOffset),
    _maximumOffset(maximumOffset),
    _v1ChecksumGenerator(toc,
                         calculateFirstSampleIndexesForV1Checksum(toc),
                         calculateFirstSampleMultipliersForV1Checksum(toc),
                         calculateLastSampleIndexesForV1Checksum(toc),
                         minimumOffset,
                         maximumOffset),
    _v1Frame450ChecksumGenerator(toc,
                                 calculateFirstSampleIndexesForV1Frame450Checksum(toc),
                                 calculateFirstSampleMultipliersForV1Frame450Checksum(toc),
                                 calculateLastSampleIndexesForV1Frame450Checksum(toc),
                                 minimumOffset,
                                 maximumOffset),
    _v2ChecksumGenerator(toc,
                         calculateFirstSampleIndexesForV2Checksum(toc),
                         calculateFirstSampleMultipliersForV2Checksum(toc),
                         calculateLastSampleIndexesForV2Checksum(toc)),
    _samplesProcessed(0) {
        assert(_minimumOffset <= _maximumOffset);
        assert(_minimumOffset >= -(cue::CdSamplesPerFrame * 5 - 1)); // Backward offset cannot be larger than five frames - 1 samples
        assert(_maximumOffset <= cue::CdSamplesPerFrame * 5); // Forward offset cannot be larger than five frames
    }
    
    void processSamples(int32_t const * const buffer[2], uint32_t count) {
        _samplesProcessed += count;
        if (_samplesProcessed > _toc.totalLength().samples) {
            throw std::runtime_error("Received more samples (" + std::to_string(_samplesProcessed) + ") "
                                     "than the TOC indicated (" + std::to_string(_toc.totalLength().samples) + ")");
        }
        _v1ChecksumGenerator.processSamples(buffer, count);
        _v1Frame450ChecksumGenerator.processSamples(buffer, count);
        _v2ChecksumGenerator.processSamples(buffer, count);
    }
};
    
}

#endif /* AccurateRip_h */
