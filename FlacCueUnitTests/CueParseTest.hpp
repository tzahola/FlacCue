//
//  CueParseTest.hpp
//  FlacCue
//
//  Created by Tamás Zahola on 28/01/16.
//  Copyright © 2016 Tamás Zahola. All rights reserved.
//

#ifndef CueParseTest_h
#define CueParseTest_h

#include "FlacCue.h"

#include <iostream>
#include <sstream>
#include <boost/test/unit_test.hpp>
#include <boost/optional/optional_io.hpp>

#include "TestUtils.hpp"

BOOST_AUTO_TEST_SUITE(CueParseTest)

BOOST_AUTO_TEST_CASE(EmptySheet) {
    std::istringstream cueSheetStream("");
    cue::Disc disc(cueSheetStream);
    
    BOOST_CHECK_EQUAL(disc.tracksEnd() - disc.tracksBegin(), 0);
    BOOST_CHECK_EQUAL(disc.comments.size(), 0);
    BOOST_CHECK_EQUAL(disc.filesEnd() - disc.filesBegin(), 0);
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
    
    BOOST_CHECK_EQUAL(disc.tracksEnd() - disc.tracksBegin(), 3);
    BOOST_CHECK_EQUAL(disc.filesEnd() - disc.filesBegin(), 1);
    
    auto file0 = disc.filesBegin();
    BOOST_CHECK_EQUAL(file0->path, "testFile");
    BOOST_CHECK_EQUAL(file0->fileType, "WAVE");
    
    auto track0 = disc.tracksBegin();
    BOOST_CHECK_EQUAL(track0->indexesEnd() - track0->indexesBegin(), 1);
    BOOST_CHECK_EQUAL(&track0->indexesBegin()->file(), &*file0);
    BOOST_CHECK_EQUAL(track0->indexesBegin()->begin, cue::Time(0,0,0));
    
    auto track1 = track0 + 1;
    BOOST_CHECK_EQUAL(track1->indexesEnd() - track1->indexesBegin(), 1);
    BOOST_CHECK_EQUAL(&track1->indexesBegin()->file(), &*file0);
    BOOST_CHECK_EQUAL(track1->indexesBegin()->begin, cue::Time(1,0,0));
    
    auto track2 = track1 + 1;
    BOOST_CHECK_EQUAL(track2->indexesEnd() - track2->indexesBegin(), 1);
    BOOST_CHECK_EQUAL(&track2->indexesBegin()->file(), &*file0);
    BOOST_CHECK_EQUAL(track2->indexesBegin()->begin, cue::Time(2,0,0));
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
    
    BOOST_CHECK_EQUAL(disc.tracksEnd() - disc.tracksBegin(), 3);
    BOOST_CHECK_EQUAL(disc.filesEnd() - disc.filesBegin(), 1);
    
    auto file0 = disc.filesBegin();
    BOOST_CHECK_EQUAL(file0->path, "testFile");
    BOOST_CHECK_EQUAL(file0->fileType, "WAVE");
    
    auto track0 = disc.tracksBegin();
    BOOST_CHECK_EQUAL(track0->indexesEnd() - track0->indexesBegin(), 2);
    {
        auto index0 = track0->indexesBegin();
        BOOST_CHECK_EQUAL(&index0->file(), &*file0);
        BOOST_CHECK_EQUAL(index0->begin, cue::Time(0,0,0));
        
        auto index1 = index0 + 1;
        BOOST_CHECK_EQUAL(&index1->file(), &*file0);
        BOOST_CHECK_EQUAL(index1->begin, cue::Time(0,2,0));
    }
    
    auto track1 = track0 + 1;
    BOOST_CHECK_EQUAL(track1->indexesEnd() - track1->indexesBegin(), 1);
    {
        auto index0 = track1->indexesBegin();
        BOOST_CHECK_EQUAL(&index0->file(), &*file0);
        BOOST_CHECK_EQUAL(index0->begin, cue::Time(1,0,0));
    }
    
    auto track2 = track1 + 1;
    {
        auto index0 = track2->indexesBegin();
        BOOST_CHECK_EQUAL(&index0->file(), &*file0);
        BOOST_CHECK_EQUAL(index0->begin, cue::Time(2,0,0));
        
        auto index1 = index0 + 1;
        BOOST_CHECK_EQUAL(&index1->file(), &*file0);
        BOOST_CHECK_EQUAL(index1->begin, cue::Time(2,2,0));
    }
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
    
    BOOST_CHECK_EQUAL(disc.tracksEnd() - disc.tracksBegin(), 3);
    BOOST_CHECK_EQUAL(disc.filesEnd() - disc.filesBegin(), 3);
    
    auto file0 = disc.filesBegin();
    BOOST_CHECK_EQUAL(file0->path, "testFile1");
    BOOST_CHECK_EQUAL(file0->fileType, "WAVE");
    
    auto file1 = file0 + 1;
    BOOST_CHECK_EQUAL(file1->path, "testFile2");
    BOOST_CHECK_EQUAL(file1->fileType, "WAVE");
    
    auto file2 = file1 + 1;
    BOOST_CHECK_EQUAL(file2->path, "testFile3");
    BOOST_CHECK_EQUAL(file2->fileType, "WAVE");
    
    auto track0 = disc.tracksBegin();
    BOOST_CHECK_EQUAL(track0->indexesEnd() - track0->indexesBegin(), 1);
    {
        auto index0 = track0->indexesBegin();
        BOOST_CHECK_EQUAL(&index0->file(), &*file0);
        BOOST_CHECK_EQUAL(index0->begin, cue::Time(0,0,0));
    }
    
    auto track1 = track0 + 1;
    BOOST_CHECK_EQUAL(track1->indexesEnd() - track1->indexesBegin(), 1);
    {
        auto index0 = track1->indexesBegin();
        BOOST_CHECK_EQUAL(&index0->file(), &*file1);
        BOOST_CHECK_EQUAL(index0->begin, cue::Time(0,0,0));
    }
    
    auto track2 = track1 + 1;
    BOOST_CHECK_EQUAL(track2->indexesEnd() - track2->indexesBegin(), 1);
    {
        auto index0 = track2->indexesBegin();
        BOOST_CHECK_EQUAL(&index0->file(), &*file2);
        BOOST_CHECK_EQUAL(index0->begin, cue::Time(0,0,0));
    }
}

BOOST_AUTO_TEST_CASE(MultipleFilesGapsAppended) {
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
    
    BOOST_CHECK_EQUAL(disc.tracksEnd() - disc.tracksBegin(), 3);
    BOOST_CHECK_EQUAL(disc.filesEnd() - disc.filesBegin(), 3);
    
    auto file0 = disc.filesBegin();
    BOOST_CHECK_EQUAL(file0->path, "testFile1");
    BOOST_CHECK_EQUAL(file0->fileType, "WAVE");
    
    auto file1 = file0 + 1;
    BOOST_CHECK_EQUAL(file1->path, "testFile2");
    BOOST_CHECK_EQUAL(file1->fileType, "WAVE");
    
    auto file2 = file1 + 1;
    BOOST_CHECK_EQUAL(file2->path, "testFile3");
    BOOST_CHECK_EQUAL(file2->fileType, "WAVE");
    
    auto track0 = disc.tracksBegin();
    BOOST_CHECK_EQUAL(track0->indexesEnd() - track0->indexesBegin(), 1);
    {
        auto index0 = track0->indexesBegin();
        BOOST_CHECK_EQUAL(&index0->file(), &*file0);
        BOOST_CHECK_EQUAL(index0->begin, cue::Time(0,0,0));
    }
    
    auto track1 = track0 + 1;
    BOOST_CHECK_EQUAL(track1->indexesEnd() - track1->indexesBegin(), 2);
    {
        auto index0 = track1->indexesBegin();
        BOOST_CHECK_EQUAL(&index0->file(), &*file0);
        BOOST_CHECK_EQUAL(index0->begin, cue::Time(1,0,0));
        
        auto index1 = index0 + 1;
        BOOST_CHECK_EQUAL(&index1->file(), &*file1);
        BOOST_CHECK_EQUAL(index1->begin, cue::Time(0,0,0));
    }
    
    auto track2 = track1 + 1;
    BOOST_CHECK_EQUAL(track2->indexesEnd() - track2->indexesBegin(), 2);
    {
        auto index0 = track2->indexesBegin();
        BOOST_CHECK_EQUAL(&index0->file(), &*file1);
        BOOST_CHECK_EQUAL(index0->begin, cue::Time(0,55,0));
        
        auto index1 = index0 + 1;
        BOOST_CHECK_EQUAL(&index1->file(), &*file2);
        BOOST_CHECK_EQUAL(index1->begin, cue::Time(0,0,0));
    }
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
        "  TRACK 02 AUDIO\n"
        "    POSTGAP 00:00:01\n"
        "    INDEX 01 00:15:00\n"
        "  TRACK 03 AUDIO\n"
        "    INDEX 00 00:55:00\n"
        "FILE testFile3 WAVE\n"
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
    "  TRACK 02 AUDIO\n"
    "    POSTGAP 00:00:01\n"
    "    INDEX 01 00:15:00\n"
    "  TRACK 03 AUDIO\n"
    "    INDEX 00 00:55:00\n"
    "FILE \"testFile3\" WAVE\n"
    "    INDEX 01 00:00:00\n";
    
    std::istringstream stream(cueSheet);
    cue::Disc disc(stream);
    
    BOOST_CHECK_EQUAL((std::stringstream() << disc).str(), cueSheet);
}

BOOST_AUTO_TEST_SUITE_END()

#endif /* CueParseTest_h */
