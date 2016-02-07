//
//  GapsAppendedSplitTest.hpp
//  FlacCue
//
//  Created by Tamás Zahola on 28/01/16.
//  Copyright © 2016 Tamás Zahola. All rights reserved.
//

#ifndef GapsAppendedSplitTest_h
#define GapsAppendedSplitTest_h

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <boost/test/unit_test.hpp>
#include <boost/optional/optional_io.hpp>

#include "TestUtils.hpp"

struct GapsAppendedSplitTestFixture {
    std::vector<std::string> testFileNames;
    std::unordered_map<std::string, cue::Time> testFileDurations;
    cue::GapsAppendedSplitGenerator::OutputFileNameHandler testOutputFileNameHandler;
    cue::GapsAppendedSplitGenerator::InputFileDurationHandler testInputFileDurationHandler;
    
    GapsAppendedSplitTestFixture()
    : testFileNames({
        "outputFileHTOA",
        "outputFile1",
        "outputFile2",
        "outputFile3",
        "outputFile4",
        "outputFile5"
    })
    , testFileDurations({
        {"inputFile1", cue::Time(1,0,0)},
        {"inputFile2", cue::Time(2,0,0)},
        {"inputFile3", cue::Time(3,0,0)},
        {"inputFile4", cue::Time(4,0,0)},
        {"inputFile5", cue::Time(5,0,0)}
    }) {
        testOutputFileNameHandler = [&](const boost::optional<const cue::Track&> track) {
            return testFileNames[track == boost::none ? 0 : track.value().number];
        };
        testInputFileDurationHandler = [&](const std::string& fileName) {
            return testFileDurations[fileName];
        };
    }
};

BOOST_FIXTURE_TEST_SUITE(GapsAppendedSplitTest, GapsAppendedSplitTestFixture)

BOOST_AUTO_TEST_CASE(EmptySheet) {
    std::istringstream cueSheetStream("");
    cue::Disc disc(cueSheetStream);
    cue::GapsAppendedSplitGenerator splitter(testOutputFileNameHandler, testInputFileDurationHandler);
    auto split = splitter.split(disc);
    
    BOOST_CHECK_EQUAL(split.outputFiles.size(), 0);
    BOOST_CHECK_EQUAL(split.outputSheet->filesEnd() - split.outputSheet->filesBegin(), 0);
    BOOST_CHECK_EQUAL(split.outputSheet->tracksEnd() - split.outputSheet->tracksBegin(), 0);
}

BOOST_AUTO_TEST_CASE(SingleFileImage) {
    std::string cueSheet =
    "FILE inputFile1 WAVE\n"
    "  TRACK 01 AUDIO\n"
    "    INDEX 01 00:00:00\n"
    "  TRACK 02 AUDIO\n"
    "    INDEX 01 00:10:00\n"
    "  TRACK 03 AUDIO\n"
    "    INDEX 01 00:20:00";
    std::istringstream cueSheetStream(cueSheet);
    cue::Disc disc(cueSheetStream);
    cue::GapsAppendedSplitGenerator splitter(testOutputFileNameHandler, testInputFileDurationHandler);
    auto split = splitter.split(disc);
    
    BOOST_CHECK_EQUAL(split.outputFiles.size(), 3);
    {
        auto output = split.outputFiles[0];
        BOOST_CHECK_EQUAL(output.outputFile, "outputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments.size(), 1);
        BOOST_CHECK_EQUAL(output.inputSegments[0].inputFile, "inputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments[0].begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(output.inputSegments[0].end, cue::Time(0,10,0));
    }
    {
        auto output = split.outputFiles[1];
        BOOST_CHECK_EQUAL(output.outputFile, "outputFile2");
        BOOST_CHECK_EQUAL(output.inputSegments.size(), 1);
        BOOST_CHECK_EQUAL(output.inputSegments[0].inputFile, "inputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments[0].begin, cue::Time(0,10,0));
        BOOST_CHECK_EQUAL(output.inputSegments[0].end, cue::Time(0,20,0));
    }
    {
        auto output = split.outputFiles[2];
        BOOST_CHECK_EQUAL(output.outputFile, "outputFile3");
        BOOST_CHECK_EQUAL(output.inputSegments.size(), 1);
        BOOST_CHECK_EQUAL(output.inputSegments[0].inputFile, "inputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments[0].begin, cue::Time(0,20,0));
        BOOST_CHECK_EQUAL(output.inputSegments[0].end, boost::none);
    }
    
    BOOST_CHECK_EQUAL(split.outputSheet->filesCend() - split.outputSheet->filesCbegin(), 3);
    auto file1 = split.outputSheet->filesCbegin();
    auto file2 = file1 + 1;
    auto file3 = file2 + 1;
    BOOST_CHECK_EQUAL(file1->path, "outputFile1");
    BOOST_CHECK_EQUAL(file1->fileType, "WAVE");
    BOOST_CHECK_EQUAL(file2->path, "outputFile2");
    BOOST_CHECK_EQUAL(file2->fileType, "WAVE");
    BOOST_CHECK_EQUAL(file3->path, "outputFile3");
    BOOST_CHECK_EQUAL(file3->fileType, "WAVE");
    
    BOOST_CHECK_EQUAL(split.outputSheet->tracksCend() - split.outputSheet->tracksCbegin(), 3);
    auto track1 = split.outputSheet->tracksCbegin();
    {
        auto index1 = track1->indexesCbegin();
        BOOST_CHECK_EQUAL(index1->begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(&index1->file(), &*file1);
        BOOST_CHECK(index1 + 1 == track1->indexesCend());
    }
    auto track2 = track1 + 1;
    {
        auto index1 = track2->indexesCbegin();
        BOOST_CHECK_EQUAL(index1->begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(&index1->file(), &*file2);
        BOOST_CHECK(index1 + 1 == track2->indexesCend());
    }
    auto track3 = track2 + 1;
    {
        auto index1 = track3->indexesCbegin();
        BOOST_CHECK_EQUAL(index1->begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(&index1->file(), &*file3);
        BOOST_CHECK(index1 + 1 == track3->indexesCend());
    }
}

BOOST_AUTO_TEST_CASE(SingleFileImageWithGaps) {
    std::string cueSheet =
    "FILE inputFile1 WAVE\n"
    "  TRACK 01 AUDIO\n"
    "    INDEX 00 00:00:00\n"
    "    INDEX 01 00:10:00\n"
    "  TRACK 02 AUDIO\n"
    "    INDEX 00 00:20:00\n"
    "    INDEX 01 00:30:00\n"
    "  TRACK 03 AUDIO\n"
    "    INDEX 00 00:40:00\n"
    "    INDEX 01 00:50:00";
    std::istringstream cueSheetStream(cueSheet);
    cue::Disc disc(cueSheetStream);
    cue::GapsAppendedSplitGenerator splitter(testOutputFileNameHandler, testInputFileDurationHandler);
    auto split = splitter.split(disc);
    
    BOOST_CHECK_EQUAL(split.outputFiles.size(), 4);
    {
        auto output = split.outputFiles[0];
        BOOST_CHECK_EQUAL(output.outputFile, "outputFileHTOA");
        BOOST_CHECK_EQUAL(output.inputSegments.size(), 1);
        BOOST_CHECK_EQUAL(output.inputSegments[0].inputFile, "inputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments[0].begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(output.inputSegments[0].end, cue::Time(0,10,0));
    }
    {
        auto output = split.outputFiles[1];
        BOOST_CHECK_EQUAL(output.outputFile, "outputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments.size(), 2);
        BOOST_CHECK_EQUAL(output.inputSegments[0].inputFile, "inputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments[0].begin, cue::Time(0,10,0));
        BOOST_CHECK_EQUAL(output.inputSegments[0].end, cue::Time(0,20,0));
        BOOST_CHECK_EQUAL(output.inputSegments[1].inputFile, "inputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments[1].begin, cue::Time(0,20,0));
        BOOST_CHECK_EQUAL(output.inputSegments[1].end, cue::Time(0,30,0));
    }
    {
        auto output = split.outputFiles[2];
        BOOST_CHECK_EQUAL(output.outputFile, "outputFile2");
        BOOST_CHECK_EQUAL(output.inputSegments.size(), 2);
        BOOST_CHECK_EQUAL(output.inputSegments[0].inputFile, "inputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments[0].begin, cue::Time(0,30,0));
        BOOST_CHECK_EQUAL(output.inputSegments[0].end, cue::Time(0,40,0));
        BOOST_CHECK_EQUAL(output.inputSegments[1].inputFile, "inputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments[1].begin, cue::Time(0,40,0));
        BOOST_CHECK_EQUAL(output.inputSegments[1].end, cue::Time(0,50,0));
    }
    {
        auto output = split.outputFiles[3];
        BOOST_CHECK_EQUAL(output.outputFile, "outputFile3");
        BOOST_CHECK_EQUAL(output.inputSegments.size(), 1);
        BOOST_CHECK_EQUAL(output.inputSegments[0].inputFile, "inputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments[0].begin, cue::Time(0,50,0));
        BOOST_CHECK_EQUAL(output.inputSegments[0].end, boost::none);
    }
    
    BOOST_CHECK_EQUAL(split.outputSheet->filesCend() - split.outputSheet->filesCbegin(), 4);
    auto file1 = split.outputSheet->filesCbegin();
    auto file2 = file1 + 1;
    auto file3 = file2 + 1;
    auto file4 = file3 + 1;
    BOOST_CHECK_EQUAL(file1->path, "outputFileHTOA");
    BOOST_CHECK_EQUAL(file1->fileType, "WAVE");
    BOOST_CHECK_EQUAL(file2->path, "outputFile1");
    BOOST_CHECK_EQUAL(file2->fileType, "WAVE");
    BOOST_CHECK_EQUAL(file3->path, "outputFile2");
    BOOST_CHECK_EQUAL(file3->fileType, "WAVE");
    BOOST_CHECK_EQUAL(file4->path, "outputFile3");
    BOOST_CHECK_EQUAL(file4->fileType, "WAVE");
    
    auto track1 = split.outputSheet->tracksCbegin();
    {
        BOOST_CHECK_EQUAL(track1->indexesCend() - track1->indexesCbegin(), 2);
        auto index1 = track1->indexesCbegin();
        BOOST_CHECK_EQUAL(index1->begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(&index1->file(), &*file1);
        auto index2 = index1 + 1;
        BOOST_CHECK_EQUAL(index2->begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(&index2->file(), &*file2);
    }
    auto track2 = track1 + 1;
    {
        auto index1 = track2->indexesCbegin();
        BOOST_CHECK_EQUAL(index1->begin, cue::Time(0,10,0));
        BOOST_CHECK_EQUAL(&index1->file(), &*file2);
        auto index2 = index1 + 1;
        BOOST_CHECK_EQUAL(index2->begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(&index2->file(), &*file3);
        
        BOOST_CHECK(index2 + 1 == track2->indexesCend());
    }
    auto track3 = track2 + 1;
    {
        auto index1 = track3->indexesCbegin();
        BOOST_CHECK_EQUAL(index1->begin, cue::Time(0,10,0));
        BOOST_CHECK_EQUAL(&index1->file(), &*file3);
        auto index2 = index1 + 1;
        BOOST_CHECK_EQUAL(index2->begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(&index2->file(), &*file4);
        
        BOOST_CHECK(index2 + 1 == track3->indexesCend());
    }
    
    BOOST_CHECK(track3 + 1 == split.outputSheet->tracksCend());
}

BOOST_AUTO_TEST_CASE(MultipleFilesGapsAppended) {
    std::string cueSheet =
    "FILE outputFile1 WAVE\n"
    "  TRACK 01 AUDIO\n"
    "    INDEX 01 00:00:00\n"
    "  TRACK 02 AUDIO\n"
    "    INDEX 00 00:10:00\n"
    "FILE outputFile2 WAVE\n"
    "    INDEX 01 00:00:00\n"
    "  TRACK 03 AUDIO\n"
    "    INDEX 00 00:10:00\n"
    "FILE outputFile3 WAVE\n"
    "    INDEX 01 00:00:00";
    
    std::istringstream cueSheetStream(cueSheet);
    cue::Disc disc(cueSheetStream);
    cue::GapsAppendedSplitGenerator splitter(testOutputFileNameHandler, testInputFileDurationHandler);
    auto split = splitter.split(disc);
    
    BOOST_CHECK_EQUAL(split.outputFiles.size(), 3);
    {
        auto output = split.outputFiles[0];
        BOOST_CHECK_EQUAL(output.outputFile, "outputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments.size(), 2);
        BOOST_CHECK_EQUAL(output.inputSegments[0].inputFile, "outputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments[0].begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(output.inputSegments[0].end, cue::Time(0,10,0));
        BOOST_CHECK_EQUAL(output.inputSegments[1].inputFile, "outputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments[1].begin, cue::Time(0,10,0));
        BOOST_CHECK_EQUAL(output.inputSegments[1].end, boost::none);
    }
    {
        auto output = split.outputFiles[1];
        BOOST_CHECK_EQUAL(output.outputFile, "outputFile2");
        BOOST_CHECK_EQUAL(output.inputSegments.size(), 2);
        BOOST_CHECK_EQUAL(output.inputSegments[0].inputFile, "outputFile2");
        BOOST_CHECK_EQUAL(output.inputSegments[0].begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(output.inputSegments[0].end, cue::Time(0,10,0));
        BOOST_CHECK_EQUAL(output.inputSegments[1].inputFile, "outputFile2");
        BOOST_CHECK_EQUAL(output.inputSegments[1].begin, cue::Time(0,10,0));
        BOOST_CHECK_EQUAL(output.inputSegments[1].end, boost::none);
    }
    {
        auto output = split.outputFiles[2];
        BOOST_CHECK_EQUAL(output.outputFile, "outputFile3");
        BOOST_CHECK_EQUAL(output.inputSegments.size(), 1);
        BOOST_CHECK_EQUAL(output.inputSegments[0].inputFile, "outputFile3");
        BOOST_CHECK_EQUAL(output.inputSegments[0].begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(output.inputSegments[0].end, boost::none);
    }
    
    checkDiscsAreEqual(disc, *split.outputSheet);
}


BOOST_AUTO_TEST_CASE(MultipleFilesGapsPrepended) {
    std::string cueSheet =
    "FILE inputFile1 WAVE\n"
    "  TRACK 01 AUDIO\n"
    "    INDEX 00 00:00:00\n"
    "    INDEX 01 00:10:00\n"
    "FILE inputFile2 WAVE\n"
    "  TRACK 02 AUDIO\n"
    "    INDEX 00 00:00:00\n"
    "    INDEX 01 00:10:00\n"
    "FILE inputFile3 WAVE\n"
    "  TRACK 03 AUDIO\n"
    "    INDEX 00 00:00:00\n"
    "    INDEX 01 00:10:00";
    std::istringstream cueSheetStream(cueSheet);
    cue::Disc disc(cueSheetStream);
    cue::GapsAppendedSplitGenerator splitter(testOutputFileNameHandler, testInputFileDurationHandler);
    auto split = splitter.split(disc);
    
    BOOST_CHECK_EQUAL(split.outputFiles.size(), 4);
    {
        auto output = split.outputFiles[0];
        BOOST_CHECK_EQUAL(output.outputFile, "outputFileHTOA");
        BOOST_CHECK_EQUAL(output.inputSegments.size(), 1);
        BOOST_CHECK_EQUAL(output.inputSegments[0].inputFile, "inputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments[0].begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(output.inputSegments[0].end, cue::Time(0,10,0));
    }
    {
        auto output = split.outputFiles[1];
        BOOST_CHECK_EQUAL(output.outputFile, "outputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments.size(), 2);
        BOOST_CHECK_EQUAL(output.inputSegments[0].inputFile, "inputFile1");
        BOOST_CHECK_EQUAL(output.inputSegments[0].begin, cue::Time(0,10,0));
        BOOST_CHECK_EQUAL(output.inputSegments[0].end, boost::none);
        BOOST_CHECK_EQUAL(output.inputSegments[1].inputFile, "inputFile2");
        BOOST_CHECK_EQUAL(output.inputSegments[1].begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(output.inputSegments[1].end, cue::Time(0,10,0));
    }
    {
        auto output = split.outputFiles[2];
        BOOST_CHECK_EQUAL(output.outputFile, "outputFile2");
        BOOST_CHECK_EQUAL(output.inputSegments.size(), 2);
        BOOST_CHECK_EQUAL(output.inputSegments[0].inputFile, "inputFile2");
        BOOST_CHECK_EQUAL(output.inputSegments[0].begin, cue::Time(0,10,0));
        BOOST_CHECK_EQUAL(output.inputSegments[0].end, boost::none);
        BOOST_CHECK_EQUAL(output.inputSegments[1].inputFile, "inputFile3");
        BOOST_CHECK_EQUAL(output.inputSegments[1].begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(output.inputSegments[1].end, cue::Time(0,10,0));
    }
    {
        auto output = split.outputFiles[3];
        BOOST_CHECK_EQUAL(output.outputFile, "outputFile3");
        BOOST_CHECK_EQUAL(output.inputSegments.size(), 1);
        BOOST_CHECK_EQUAL(output.inputSegments[0].inputFile, "inputFile3");
        BOOST_CHECK_EQUAL(output.inputSegments[0].begin, cue::Time(0,10,0));
        BOOST_CHECK_EQUAL(output.inputSegments[0].end, boost::none);
    }
    
    BOOST_CHECK_EQUAL(split.outputSheet->filesCend() - split.outputSheet->filesCbegin(), 4);
    auto file1 = split.outputSheet->filesCbegin();
    auto file2 = file1 + 1;
    auto file3 = file2 + 1;
    auto file4 = file3 + 1;
    BOOST_CHECK_EQUAL(file1->path, "outputFileHTOA");
    BOOST_CHECK_EQUAL(file1->fileType, "WAVE");
    BOOST_CHECK_EQUAL(file2->path, "outputFile1");
    BOOST_CHECK_EQUAL(file2->fileType, "WAVE");
    BOOST_CHECK_EQUAL(file3->path, "outputFile2");
    BOOST_CHECK_EQUAL(file3->fileType, "WAVE");
    BOOST_CHECK_EQUAL(file4->path, "outputFile3");
    BOOST_CHECK_EQUAL(file4->fileType, "WAVE");
    
    auto track1 = split.outputSheet->tracksCbegin();
    {
        BOOST_CHECK_EQUAL(track1->indexesCend() - track1->indexesCbegin(), 2);
        auto index1 = track1->indexesCbegin();
        BOOST_CHECK_EQUAL(index1->begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(&index1->file(), &*file1);
        auto index2 = index1 + 1;
        BOOST_CHECK_EQUAL(index2->begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(&index2->file(), &*file2);
    }
    auto track2 = track1 + 1;
    {
        auto index1 = track2->indexesCbegin();
        BOOST_CHECK_EQUAL(index1->begin, testFileDurations["inputFile1"] - cue::Time(0,10,0));
        BOOST_CHECK_EQUAL(&index1->file(), &*file2);
        auto index2 = index1 + 1;
        BOOST_CHECK_EQUAL(index2->begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(&index2->file(), &*file3);
        
        BOOST_CHECK(index2 + 1 == track2->indexesCend());
    }
    auto track3 = track2 + 1;
    {
        auto index1 = track3->indexesCbegin();
        BOOST_CHECK_EQUAL(index1->begin, testFileDurations["inputFile2"] - cue::Time(0,10,0));
        BOOST_CHECK_EQUAL(&index1->file(), &*file3);
        auto index2 = index1 + 1;
        BOOST_CHECK_EQUAL(index2->begin, cue::Time(0,0,0));
        BOOST_CHECK_EQUAL(&index2->file(), &*file4);
        
        BOOST_CHECK(index2 + 1 == track3->indexesCend());
    }
    
    BOOST_CHECK(track3 + 1 == split.outputSheet->tracksCend());
}

BOOST_AUTO_TEST_SUITE_END()

#endif /* GapsAppendedSplitTest_h */
