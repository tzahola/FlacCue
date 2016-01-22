//
//  main.cpp
//  FlacCueTests
//
//  Created by Tamás Zahola on 18/01/16.
//  Copyright © 2016 Tamás Zahola. All rights reserved.
//

#define BOOST_TEST_MODULE CueParseTest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <sstream>
#include <boost/optional/optional_io.hpp>
#include <boost/iterator/zip_iterator.hpp>

#include "FlacCue.h"

BOOST_AUTO_TEST_SUITE(CueParseTest)

BOOST_AUTO_TEST_CASE(EmptySheet) {
    std::istringstream cueSheetStream("");
    cue::Disc disc(cueSheetStream);
    
    BOOST_CHECK_EQUAL(disc.tracks.size(), 0);
    BOOST_CHECK_EQUAL(disc.comments.size(), 0);
    BOOST_CHECK_EQUAL(disc.files.size(), 0);
}

BOOST_AUTO_TEST_CASE(SingleFileImage) {
    std::string cueSheet =
    "FILE testFile WAVE\n"
    "  TRACK 01 AUDIO\n"
    "    INDEX 01 00:00:00\n"
    "  TRACK 02 AUDIO\n"
    "    INDEX 01 01:00:00\n"
    "  TRACK 03 AUDIO\n"
    "    INDEX 01 02:00:00";
    std::istringstream cueSheetStream(cueSheet);
    cue::Disc disc(cueSheetStream);
    
    BOOST_CHECK_EQUAL(disc.tracks.size(), 3);
    BOOST_CHECK_EQUAL(disc.files.size(), 1);
    
    BOOST_CHECK_EQUAL(disc.files[0].path, "testFile");
    BOOST_CHECK_EQUAL(disc.files[0].fileType, "WAVE");
    
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].file, 0);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].begin, cue::Time(0,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].end, cue::Time(1,0,0));
    
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].file, 0);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].begin, cue::Time(1,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].end, cue::Time(2,0,0));
    
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].file, 0);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].begin, cue::Time(2,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].end, boost::none);
}

BOOST_AUTO_TEST_CASE(SingleFileImageWithGaps) {
    std::string cueSheet =
    "FILE testFile WAVE\n"
    "  TRACK 01 AUDIO\n"
    "    INDEX 00 00:00:00\n"
    "    INDEX 01 00:02:00\n"
    "  TRACK 02 AUDIO\n"
    "    INDEX 01 01:00:00\n"
    "  TRACK 03 AUDIO\n"
    "    INDEX 00 02:00:00\n"
    "    INDEX 01 02:02:00";
    std::istringstream cueSheetStream(cueSheet);
    cue::Disc disc(cueSheetStream);
    
    BOOST_CHECK_EQUAL(disc.tracks.size(), 3);
    BOOST_CHECK_EQUAL(disc.files.size(), 1);
    
    BOOST_CHECK_EQUAL(disc.files[0].path, "testFile");
    BOOST_CHECK_EQUAL(disc.files[0].fileType, "WAVE");
    
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes.size(), 2);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].file, 0);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].begin, cue::Time(0,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].end, cue::Time(0,2,0));
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[1].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[1].sources[0].file, 0);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[1].sources[0].begin, cue::Time(0,2,0));
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[1].sources[0].end, cue::Time(1,0,0));
    
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].file, 0);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].begin, cue::Time(1,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].end, cue::Time(2,0,0));
    
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes.size(), 2);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].file, 0);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].begin, cue::Time(2,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].end, cue::Time(2,2,0));
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[1].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[1].sources[0].file, 0);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[1].sources[0].begin, cue::Time(2,2,0));
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[1].sources[0].end, boost::none);
}

BOOST_AUTO_TEST_CASE(MultipleFiles) {
    std::string cueSheet =
    "FILE testFile1 WAVE\n"
    "  TRACK 01 AUDIO\n"
    "    INDEX 01 00:00:00\n"
    "FILE testFile2 WAVE\n"
    "  TRACK 02 AUDIO\n"
    "    INDEX 01 00:00:00\n"
    "FILE testFile3 WAVE\n"
    "  TRACK 03 AUDIO\n"
    "    INDEX 01 00:00:00";
    std::istringstream cueSheetStream(cueSheet);
    cue::Disc disc(cueSheetStream);
    
    BOOST_CHECK_EQUAL(disc.tracks.size(), 3);
    BOOST_CHECK_EQUAL(disc.files.size(), 3);
    
    BOOST_CHECK_EQUAL(disc.files[0].path, "testFile1");
    BOOST_CHECK_EQUAL(disc.files[0].fileType, "WAVE");
    BOOST_CHECK_EQUAL(disc.files[1].path, "testFile2");
    BOOST_CHECK_EQUAL(disc.files[1].fileType, "WAVE");
    BOOST_CHECK_EQUAL(disc.files[2].path, "testFile3");
    BOOST_CHECK_EQUAL(disc.files[2].fileType, "WAVE");
    
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].file, 0);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].begin, cue::Time(0,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].end, boost::none);
    
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].file, 1);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].begin, cue::Time(0,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].end, boost::none);
    
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].file, 2);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].begin, cue::Time(0,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].end, boost::none);
}

BOOST_AUTO_TEST_CASE(MultipleFilesNonCompliant) {
    std::string cueSheet =
    "FILE testFile1 WAVE\n"
    "  TRACK 01 AUDIO\n"
    "    INDEX 01 00:00:00\n"
    "  TRACK 02 AUDIO\n"
    "    INDEX 00 01:00:00\n"
    "FILE testFile2 WAVE\n"
    "    INDEX 01 00:00:00\n"
    "  TRACK 03 AUDIO\n"
    "    INDEX 00 00:55:00\n"
    "FILE testFile3 WAVE\n"
    "    INDEX 01 00:00:00";
    std::istringstream cueSheetStream(cueSheet);
    cue::Disc disc(cueSheetStream);
    
    BOOST_CHECK_EQUAL(disc.tracks.size(), 3);
    BOOST_CHECK_EQUAL(disc.files.size(), 3);
    
    BOOST_CHECK_EQUAL(disc.files[0].path, "testFile1");
    BOOST_CHECK_EQUAL(disc.files[0].fileType, "WAVE");
    BOOST_CHECK_EQUAL(disc.files[1].path, "testFile2");
    BOOST_CHECK_EQUAL(disc.files[1].fileType, "WAVE");
    BOOST_CHECK_EQUAL(disc.files[2].path, "testFile3");
    BOOST_CHECK_EQUAL(disc.files[2].fileType, "WAVE");
    
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].file, 0);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].begin, cue::Time(0,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].end, cue::Time(1,0,0));
    
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes.size(), 2);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].file, 0);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].begin, cue::Time(1,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].end, boost::none);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[1].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[1].sources[0].file, 1);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[1].sources[0].begin, cue::Time(0,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[1].sources[0].end, cue::Time(0,55,0));
    
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes.size(), 2);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].file, 1);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].begin, cue::Time(0,55,0));
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].end, boost::none);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[1].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[1].sources[0].file, 2);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[1].sources[0].begin, cue::Time(0,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[1].sources[0].end, boost::none);
}

BOOST_AUTO_TEST_CASE(MultipleFilesPerIndex) {
    std::string cueSheet =
    "FILE testFile1 WAVE\n"
    "  TRACK 01 AUDIO\n"
    "    INDEX 01 00:00:00\n"
    "FILE testFile2 WAVE\n"
    "FILE testFile3 WAVE\n"
    "  TRACK 02 AUDIO\n"
    "    INDEX 01 00:15:00\n"
    "  TRACK 03 AUDIO\n"
    "    INDEX 00 00:55:00\n"
    "FILE testFile4 WAVE\n"
    "    INDEX 01 00:00:00";
    std::istringstream cueSheetStream(cueSheet);
    cue::Disc disc(cueSheetStream);
    
    BOOST_CHECK_EQUAL(disc.tracks.size(), 3);
    BOOST_CHECK_EQUAL(disc.files.size(), 4);
    
    BOOST_CHECK_EQUAL(disc.files[0].path, "testFile1");
    BOOST_CHECK_EQUAL(disc.files[1].path, "testFile2");
    BOOST_CHECK_EQUAL(disc.files[2].path, "testFile3");
    BOOST_CHECK_EQUAL(disc.files[3].path, "testFile4");
    
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources.size(), 3);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].file, 0);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].begin, cue::Time(0,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[0].end, boost::none);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[1].file, 1);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[1].begin, cue::Time(0,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[1].end, boost::none);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[2].file, 2);
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[2].begin, cue::Time(0,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[0].indexes[0].sources[2].end, cue::Time(0,15,0));
    
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].file, 2);
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].begin, cue::Time(0,15,0));
    BOOST_CHECK_EQUAL(disc.tracks[1].indexes[0].sources[0].end, cue::Time(0,55,0));
    
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes.size(), 2);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].file, 2);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].begin, cue::Time(0,55,0));
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[0].sources[0].end, boost::none);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[1].sources.size(), 1);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[1].sources[0].file, 3);
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[1].sources[0].begin, cue::Time(0,0,0));
    BOOST_CHECK_EQUAL(disc.tracks[2].indexes[1].sources[0].end, boost::none);
}

static void checkIndexesEqual(const cue::Index& index1, const cue::Index& index2) {
    BOOST_CHECK_EQUAL_COLLECTIONS(index1.comments.begin(),
                                  index1.comments.end(),
                                  index2.comments.begin(),
                                  index2.comments.end());
    BOOST_CHECK_EQUAL(index1.index, index2.index);
    
    BOOST_CHECK_EQUAL(index1.sources.size(), index2.sources.size());
    std::for_each(boost::make_zip_iterator(boost::make_tuple(index1.sources.begin(), index2.sources.begin())),
                  boost::make_zip_iterator(boost::make_tuple(index1.sources.end(), index2.sources.end())),
                  [](const boost::tuple<const cue::Source&, const cue::Source&>& sources) {
                      auto source1 = boost::get<0>(sources);
                      auto source2 = boost::get<1>(sources);
                      BOOST_CHECK_EQUAL(source1.file, source2.file);
                      BOOST_CHECK_EQUAL(source1.begin, source2.begin);
                      BOOST_CHECK_EQUAL(source1.end, source2.end);
                  });
}

static void checkTracksEqual(const cue::Track& track1, const cue::Track& track2) {
    BOOST_CHECK_EQUAL_COLLECTIONS(track1.comments.begin(),
                                  track1.comments.end(),
                                  track2.comments.begin(),
                                  track2.comments.end());
    BOOST_CHECK_EQUAL(track1.dataType, track2.dataType);
    BOOST_CHECK_EQUAL(track1.flags, track2.flags);
    BOOST_CHECK_EQUAL(track1.isrc, track2.isrc);
    
    BOOST_CHECK_EQUAL(track1.indexes.size(), track2.indexes.size());
    std::for_each(boost::make_zip_iterator(boost::make_tuple(track1.indexes.begin(), track2.indexes.begin())),
                  boost::make_zip_iterator(boost::make_tuple(track1.indexes.end(), track2.indexes.end())),
                  [](const boost::tuple<const cue::Index&, const cue::Index&>& indexes) {
                      checkIndexesEqual(boost::get<0>(indexes), boost::get<1>(indexes));
                  });
    
    BOOST_CHECK_EQUAL(track1.number, track2.number);
    BOOST_CHECK_EQUAL(track1.performer, track2.performer);
    BOOST_CHECK_EQUAL(track1.postgap, track2.postgap);
    BOOST_CHECK_EQUAL(track1.pregap, track2.pregap);
    BOOST_CHECK_EQUAL(track1.songwriter, track2.songwriter);
    BOOST_CHECK_EQUAL(track1.title, track2.title);
}

static void checkDiscsAreEqual(const cue::Disc& d1, const cue::Disc& d2) {
    BOOST_CHECK_EQUAL(d1.files.size(), d2.files.size());
    std::for_each(boost::make_zip_iterator(boost::make_tuple(d1.files.begin(), d2.files.begin())),
                  boost::make_zip_iterator(boost::make_tuple(d1.files.end(), d2.files.end())),
                  [](const boost::tuple<const cue::File&, const cue::File&>& files) {
                      BOOST_CHECK_EQUAL(boost::get<0>(files).path, boost::get<1>(files).path);
                      BOOST_CHECK_EQUAL(boost::get<0>(files).fileType, boost::get<1>(files).fileType);
                  });
    
    BOOST_CHECK_EQUAL_COLLECTIONS(d1.comments.begin(), d1.comments.end(), d2.comments.begin(), d2.comments.end());
    BOOST_CHECK_EQUAL(d1.catalog, d2.catalog);
    BOOST_CHECK_EQUAL(d1.cdTextFile, d2.cdTextFile);
    BOOST_CHECK_EQUAL(d1.performer, d2.performer);
    BOOST_CHECK_EQUAL(d1.title, d2.title);
    BOOST_CHECK_EQUAL(d1.songwriter, d2.songwriter);
    
    BOOST_CHECK_EQUAL(d1.tracks.size(), d2.tracks.size());
    std::for_each(boost::make_zip_iterator(boost::make_tuple(d1.tracks.begin(), d2.tracks.begin())),
                  boost::make_zip_iterator(boost::make_tuple(d1.tracks.end(), d2.tracks.end())),
                  [](const boost::tuple<const cue::Track&, const cue::Track&>& tracks) {
                      checkTracksEqual(boost::get<0>(tracks), boost::get<1>(tracks));
                  });
}

BOOST_AUTO_TEST_CASE(SerializationThenDeserialization) {
    std::vector<std::string> exampleCueSheets = {
        "FILE testFile WAVE\n"
        "  TRACK 01 AUDIO\n"
        "    INDEX 01 00:00:00\n"
        "  TRACK 02 AUDIO\n"
        "    INDEX 01 01:00:00\n"
        "  TRACK 03 AUDIO\n"
        "    INDEX 01 02:00:00",
        
        "FILE testFile WAVE\n"
        "  TRACK 01 AUDIO\n"
        "    INDEX 00 00:00:00\n"
        "    INDEX 01 00:02:00\n"
        "  TRACK 02 AUDIO\n"
        "    INDEX 01 01:00:00\n"
        "  TRACK 03 AUDIO\n"
        "    INDEX 00 02:00:00\n"
        "    INDEX 01 02:02:00",
        
        "FILE testFile1 WAVE\n"
        "  TRACK 01 AUDIO\n"
        "    INDEX 01 00:00:00\n"
        "FILE testFile2 WAVE\n"
        "  TRACK 02 AUDIO\n"
        "    INDEX 01 00:00:00\n"
        "FILE testFile3 WAVE\n"
        "  TRACK 03 AUDIO\n"
        "    INDEX 01 00:00:00",
        
        "FILE testFile1 WAVE\n"
        "  TRACK 01 AUDIO\n"
        "    INDEX 01 00:00:00\n"
        "  TRACK 02 AUDIO\n"
        "    INDEX 00 01:00:00\n"
        "FILE testFile2 WAVE\n"
        "    INDEX 01 00:00:00\n"
        "  TRACK 03 AUDIO\n"
        "    INDEX 00 00:55:00\n"
        "FILE testFile3 WAVE\n"
        "    INDEX 01 00:00:00",
        
        "REM this is a comment\n"
        "FILE testFile1 WAVE\n"
        "  TRACK 01 AUDIO\n"
        "    PREGAP 00:03:00\n"
        "    INDEX 01 00:00:00\n"
        "FILE testFile2 WAVE\n"
        "FILE testFile3 WAVE\n"
        "  TRACK 02 AUDIO\n"
        "    POSTGAP 00:00:01\n"
        "    INDEX 01 00:15:00\n"
        "  TRACK 03 AUDIO\n"
        "    INDEX 00 00:55:00\n"
        "FILE testFile4 WAVE\n"
        "    INDEX 01 00:00:00\n",
    };
    
    for (auto cueSheet : exampleCueSheets) {
        std::istringstream stream(cueSheet);
        cue::Disc disc(stream);
        
        std::stringstream serializerStream;
        serializerStream << disc;
        std::string serialized1 = serializerStream.str();
        cue::Disc deserialized(serializerStream);
        
        checkDiscsAreEqual(disc, deserialized);
    }
}

BOOST_AUTO_TEST_CASE(Serialization) {
    std::string cueSheet =
    "REM this is a comment\n"
    "FILE \"testFile1\" WAVE\n"
    "  TRACK 01 AUDIO\n"
    "    PREGAP 00:03:00\n"
    "    INDEX 01 00:00:00\n"
    "FILE \"testFile2\" WAVE\n"
    "FILE \"testFile3\" WAVE\n"
    "  TRACK 02 AUDIO\n"
    "    POSTGAP 00:00:01\n"
    "    INDEX 01 00:15:00\n"
    "  TRACK 03 AUDIO\n"
    "    INDEX 00 00:55:00\n"
    "FILE \"testFile4\" WAVE\n"
    "    INDEX 01 00:00:00\n";
    
    std::istringstream stream(cueSheet);
    cue::Disc disc(stream);
    
    BOOST_CHECK_EQUAL((std::stringstream() << disc).str(), cueSheet);
}

BOOST_AUTO_TEST_SUITE_END()
