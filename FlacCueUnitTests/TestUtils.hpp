//
//  TestUtils.hpp
//  FlacCue
//
//  Created by Tamás Zahola on 28/01/16.
//  Copyright © 2016 Tamás Zahola. All rights reserved.
//

#ifndef TestUtils_h
#define TestUtils_h

#include <iostream>
#include <boost/test/unit_test.hpp>
#include <boost/iterator/zip_iterator.hpp>

#include "FlacCue.h"

namespace std {
    std::ostream& operator<<(std::ostream& os, const std::nullopt_t& _) {
        return os << "std::nullopt";
    }

    template<typename T>
    std::ostream& operator<<(std::ostream& os, const std::optional<T>& t) {
        if (t) {
            return os << *t;
        } else {
            return os << std::nullopt;
        }
    }
}

static void checkFilesEqual(const cue::File& file1, const cue::File& file2) {
    BOOST_CHECK_EQUAL(file1.path, file2.path);
    BOOST_CHECK_EQUAL(file1.fileType, file2.fileType);
}

static void checkIndexesEqual(const cue::Index& index1, const cue::Index& index2) {
    BOOST_CHECK_EQUAL_COLLECTIONS(index1.comments.begin(),
                                  index1.comments.end(),
                                  index2.comments.begin(),
                                  index2.comments.end());
    BOOST_CHECK_EQUAL(index1.index, index2.index);
    checkFilesEqual(index1.file(), index2.file());
}

static void checkTracksEqual(const cue::Track& track1, const cue::Track& track2) {
    BOOST_CHECK_EQUAL_COLLECTIONS(track1.comments.begin(),
                                  track1.comments.end(),
                                  track2.comments.begin(),
                                  track2.comments.end());
    BOOST_CHECK_EQUAL(track1.dataType, track2.dataType);
    BOOST_CHECK_EQUAL(track1.flags, track2.flags);
    BOOST_CHECK_EQUAL(track1.isrc, track2.isrc);
    
    BOOST_CHECK_EQUAL(track1.indexesCend() - track1.indexesCbegin(), track2.indexesCend() - track2.indexesCbegin());
    std::for_each(boost::make_zip_iterator(boost::make_tuple(track1.indexesCbegin(), track2.indexesCbegin())),
                  boost::make_zip_iterator(boost::make_tuple(track1.indexesCend(), track2.indexesCend())),
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
    BOOST_CHECK_EQUAL(d1.filesCend() - d1.filesCbegin(), d2.filesCend() - d2.filesCbegin());
    std::for_each(boost::make_zip_iterator(boost::make_tuple(d1.filesCbegin(), d2.filesCbegin())),
                  boost::make_zip_iterator(boost::make_tuple(d1.filesCend(), d2.filesCend())),
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
    
    BOOST_CHECK_EQUAL(d1.tracksCend() - d1.tracksCbegin(), d2.tracksCend() - d2.tracksCbegin());
    std::for_each(boost::make_zip_iterator(boost::make_tuple(d1.tracksCbegin(), d2.tracksCbegin())),
                  boost::make_zip_iterator(boost::make_tuple(d1.tracksCend(), d2.tracksCend())),
                  [](const boost::tuple<const cue::Track&, const cue::Track&>& tracks) {
                      checkTracksEqual(boost::get<0>(tracks), boost::get<1>(tracks));
                  });
}

#endif /* TestUtils_h */
