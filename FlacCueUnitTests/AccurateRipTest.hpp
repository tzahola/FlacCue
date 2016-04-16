//
//  AccurateRipTest.hpp
//  FlacCue
//
//  Created by Tamás Zahola on 07/02/16.
//  Copyright © 2016 Tamás Zahola. All rights reserved.
//

#ifndef AccurateRipTest_h
#define AccurateRipTest_h

#include <iostream>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <boost/test/unit_test.hpp>
#include <boost/optional/optional_io.hpp>

#include "TestUtils.hpp"


struct TestDisc {
    accuraterip::TableOfContents toc;
    std::vector<int32_t> channel0, channel1;
    
    cue::Time discLength() const {
        assert(channel0.size() == channel1.size());
        return cue::Time(channel0.size());
    }
    
    int numberOfTracks() const {
        return toc.numberOfEntries() - 1;
    }
    
    static TestDisc Create(int numberOfTracks, const cue::Time& firstTrackStartOffset = 0) {
        assert(numberOfTracks > 0);
        assert(firstTrackStartOffset.samples >= 0);
        
        TestDisc result;
        std::vector<cue::Time> trackLengths;
        uint32_t discLength = 0;
        for (auto i = 0; i < numberOfTracks; ++i) {
            auto cdTrackMinFrames = 4 * cue::CdFramesPerSecond; // 4 seconds is the min. track length according to the RedBook
            auto cdTrackMaxFrames = 30 * cue::CdFramesPerSecond; // 30 seconds (arbitrary)
            auto trackLength = (uint32_t)((rand() / (double)RAND_MAX) * (cdTrackMaxFrames - cdTrackMinFrames) + cdTrackMinFrames) * cue::CdSamplesPerFrame;
            trackLengths.push_back(cue::Time(trackLength));
            discLength += trackLength;
        }
        result.toc = accuraterip::TableOfContents::CreateFromTrackLengths(trackLengths, firstTrackStartOffset);
        
        result.channel0.resize(discLength);
        result.channel1.resize(discLength);
        
        std::generate(result.channel0.begin(), result.channel0.end(), [](){ return (int16_t)rand(); });
        std::generate(result.channel1.begin(), result.channel1.end(), [](){ return (int16_t)rand(); });
        
        return result;
    }
    
    uint32_t v1Checksum(int track) const {
        auto leadIn = toc[0].startOffset.samples;
        auto startIndex = toc[track].startOffset.samples - leadIn + (track == 0 ? (cue::Time(0,0,5).samples - 1) : 0);
        auto endIndex = toc[track + 1].startOffset.samples - leadIn - (track == (numberOfTracks() - 1) ? cue::Time(0,0,5).samples : 0);
        
        uint32_t checksum = 0;
        uint32_t multiplier = track == 0 ? (uint32_t)(cue::Time(0,0,5).samples - 1) : 0;
        for (uint32_t i = (uint32_t)startIndex; i < endIndex; ++i) {
            checksum += (multiplier + 1) * (((uint16_t)channel1[i] << 16) | (uint16_t)channel0[i]);
            ++multiplier;
        }
        
        return checksum;
    }
    
    TestDisc cloneWithOffset(int offset) const {
        TestDisc result;
        result.toc = toc;
        std::vector<int32_t> channel0Padding, channel1Padding;
        channel0Padding.resize(abs(offset));
        channel1Padding.resize(abs(offset));
        std::generate(channel0Padding.begin(), channel0Padding.end(), []() { return (int16_t)rand(); });
        std::generate(channel1Padding.begin(), channel1Padding.end(), []() { return (int16_t)rand(); });
        if (offset > 0) {
            result.channel0.insert(result.channel0.end(), channel0Padding.begin(), channel0Padding.end());
            result.channel0.insert(result.channel0.end(), channel0.begin(), channel0.end() - offset);
            result.channel1.insert(result.channel1.end(), channel1Padding.begin(), channel1Padding.end());
            result.channel1.insert(result.channel1.end(), channel1.begin(), channel1.end() - offset);
        } else {
            result.channel0.insert(result.channel0.end(), channel0.begin() - offset, channel0.end());
            result.channel0.insert(result.channel0.end(), channel0Padding.begin(), channel0Padding.end());
            result.channel1.insert(result.channel1.end(), channel1.begin() - offset, channel1.end());
            result.channel1.insert(result.channel1.end(), channel1Padding.begin(), channel1Padding.end());
        }
        
        assert(result.discLength() == discLength());
        
        return result;
    }
};

BOOST_AUTO_TEST_SUITE(AccurateRip)

static inline void checkTableOfContentsEntriesEqual(const accuraterip::TableOfContentsEntry& a, const accuraterip::TableOfContentsEntry& b) {
    BOOST_CHECK_EQUAL(a.trackNumber, b.trackNumber);
    BOOST_CHECK_EQUAL(a.startOffset, b.startOffset);
}

BOOST_AUTO_TEST_CASE(TableOfContentsFromTrackLengths) {
    std::vector<accuraterip::Time> trackLengths = {
        accuraterip::Time(0,0,1),
        accuraterip::Time(0,0,2),
        accuraterip::Time(0,0,3)
    };
    accuraterip::TableOfContents toc = accuraterip::TableOfContents::CreateFromTrackLengths(trackLengths, accuraterip::Time(0,0,1));
    
    BOOST_CHECK_EQUAL(toc.numberOfEntries(), 4);
    checkTableOfContentsEntriesEqual(toc[0], { 1, accuraterip::Time(0,0,1) });
    checkTableOfContentsEntriesEqual(toc[1], { 2, accuraterip::Time(0,0,2) });
    checkTableOfContentsEntriesEqual(toc[2], { 3, accuraterip::Time(0,0,4) });
    checkTableOfContentsEntriesEqual(toc[3], { accuraterip::LeadOutTrack, accuraterip::Time(0,0,7) });
}

BOOST_AUTO_TEST_CASE(TableOfContentsFromTrackLengthsWithDefaultFirstTrackOffset) {
    std::vector<accuraterip::Time> trackLengths = {
        accuraterip::Time(0,0,1),
        accuraterip::Time(0,0,2),
        accuraterip::Time(0,0,3)
    };
    accuraterip::TableOfContents toc = accuraterip::TableOfContents::CreateFromTrackLengths(trackLengths);
    
    BOOST_CHECK_EQUAL(toc.numberOfEntries(), 4);
    checkTableOfContentsEntriesEqual(toc[0], { 1, accuraterip::Time(0,0,0) });
    checkTableOfContentsEntriesEqual(toc[1], { 2, accuraterip::Time(0,0,1) });
    checkTableOfContentsEntriesEqual(toc[2], { 3, accuraterip::Time(0,0,3) });
    checkTableOfContentsEntriesEqual(toc[3], { accuraterip::LeadOutTrack, accuraterip::Time(0,0,6) });
}

BOOST_AUTO_TEST_CASE(TableOfContentsFromStartOffsets) {
    std::vector<accuraterip::Time> startOffsets = {
        accuraterip::Time(0,0,1),
        accuraterip::Time(0,0,2),
        accuraterip::Time(0,0,3)
    };
    accuraterip::TableOfContents toc = accuraterip::TableOfContents::CreateFromTrackOffsets(startOffsets);
    
    BOOST_CHECK_EQUAL(toc.numberOfEntries(), 3);
    checkTableOfContentsEntriesEqual(toc[0], { 1, accuraterip::Time(0,0,1) });
    checkTableOfContentsEntriesEqual(toc[1], { 2, accuraterip::Time(0,0,2) });
    checkTableOfContentsEntriesEqual(toc[2], { accuraterip::LeadOutTrack, accuraterip::Time(0,0,3) });
}

BOOST_AUTO_TEST_CASE(TableOfContentsErrors) {
    std::vector<accuraterip::Time> invalidStartOffsets = {
        accuraterip::Time(0,0,1),
        accuraterip::Time(0,0,2),
        accuraterip::Time(0,0,3) + 1
    };
    std::vector<accuraterip::Time> tooFewStartOffsets = {
        accuraterip::Time(0,0,1)
    };
    BOOST_CHECK_THROW(accuraterip::TableOfContents::CreateFromTrackOffsets(invalidStartOffsets),
                      accuraterip::InvalidTOCException);
    BOOST_CHECK_THROW(accuraterip::TableOfContents::CreateFromTrackOffsets(tooFewStartOffsets),
                      accuraterip::InvalidTOCException);
    BOOST_CHECK_THROW(accuraterip::TableOfContents::CreateFromTrackOffsets({}),
                      accuraterip::InvalidTOCException);
    
    std::vector<accuraterip::Time> invalidTrackLengths = {
        accuraterip::Time(0,0,1),
        accuraterip::Time(0,0,2) + 1,
        accuraterip::Time(0,0,3)
    };
    std::vector<accuraterip::Time> validTrackLengths = {
        accuraterip::Time(0,0,1),
        accuraterip::Time(0,0,2),
        accuraterip::Time(0,0,3)
    };
    accuraterip::Time invalidFirstTrackOffset = accuraterip::Time(0,0,1) + 1;
    accuraterip::Time validFirstTrackOffset = 0;
    BOOST_CHECK_THROW(accuraterip::TableOfContents::CreateFromTrackLengths(invalidTrackLengths, validFirstTrackOffset),
                      accuraterip::InvalidTOCException);
    BOOST_CHECK_THROW(accuraterip::TableOfContents::CreateFromTrackLengths(validTrackLengths, invalidFirstTrackOffset),
                      accuraterip::InvalidTOCException);
    BOOST_CHECK_THROW(accuraterip::TableOfContents::CreateFromTrackLengths(invalidTrackLengths, invalidFirstTrackOffset),
                      accuraterip::InvalidTOCException);
    BOOST_CHECK_THROW(accuraterip::TableOfContents::CreateFromTrackLengths({}),
                      accuraterip::InvalidTOCException);
}

BOOST_AUTO_TEST_CASE(ChecksumCalculation) {
    
    for (auto tracks = 1; tracks <= 5; ++tracks) {
        auto minOffset = -1000;
        auto maxOffset = 1000;
        auto offset = (uint32_t)((rand() / (double)RAND_MAX) * (maxOffset - minOffset) + minOffset);
        
        auto minHTOAFrames = 0;
        auto maxHTOAFrames = 2 * cue::CdFramesPerSecond;
        auto htoaFrames = (uint32_t)((rand() / (double)RAND_MAX) * (maxHTOAFrames - minHTOAFrames) + minHTOAFrames);
        auto testDisc = TestDisc::Create(tracks, htoaFrames * cue::CdSamplesPerFrame);
        auto testDiscWithOffset = testDisc.cloneWithOffset(offset);
        
        accuraterip::ChecksumGenerator checksumGenerator(testDisc.toc);
        int32_t* buffers[2] = { &testDisc.channel0[0], &testDisc.channel1[0] };
        checksumGenerator.processSamples(buffers, (uint32_t)testDisc.discLength().samples);
        
        accuraterip::ChecksumGenerator checksumGeneratorWithOffset(testDiscWithOffset.toc);
        int32_t* buffersWithOffset[2] = { &testDiscWithOffset.channel0[0], &testDiscWithOffset.channel1[0] };
        checksumGeneratorWithOffset.processSamples(buffersWithOffset, (uint32_t)testDiscWithOffset.discLength().samples);
        
        for (auto track = 0; track < tracks; ++track) {
            BOOST_CHECK_EQUAL(checksumGenerator.v1ChecksumWithOffset(track, 0), testDisc.v1Checksum(track));
            BOOST_CHECK_EQUAL(checksumGeneratorWithOffset.v1ChecksumWithOffset(track, offset), testDisc.v1Checksum(track));
            BOOST_CHECK_EQUAL(checksumGenerator.v1ChecksumWithOffset(track, -offset), checksumGeneratorWithOffset.v1ChecksumWithOffset(track, 0));
            BOOST_CHECK_EQUAL(checksumGenerator.v1ChecksumWithOffset(track, 0), checksumGeneratorWithOffset.v1ChecksumWithOffset(track, offset));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

#endif /* AccurateRipTest_h */
