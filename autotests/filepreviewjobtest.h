/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Meven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef FILEPREVIEWJOBTEST_H
#define FILEPREVIEWJOBTEST_H

#include <QObject>

class FilePreviewJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testTimeoutTimerStoppedOnFinish();
    void testCacheSizeValidation_data();
    void testCacheSizeValidation();
    void testGeneratedImageDevicePixelRatio_data();
    void testGeneratedImageDevicePixelRatio();
    void testSupportedDevicePixelRatio_data();
    void testSupportedDevicePixelRatio();

    void testCachedPreviewReturnsFreshThumbnail();
    void testCachedPreviewServesAStaleThumbnail();
    void testCachedPreviewMissReturnsNull();
    void testCachedPreviewRejectsIneligibleItems();
    void testCacheDirMatchesWhatIsWritten_data();
    void testCacheDirMatchesWhatIsWritten();
    void testCachedPreviewFoundAtDevicePixelRatio_data();
    void testCachedPreviewFoundAtDevicePixelRatio();
    void testNoCacheBucketLeavesTheCacheAlone();
    void testAMissedCacheLookupKeepsItsPlaceInTheQueue();

    void testCachedOnlyServesTheCacheAndMakesNothing();
    void testCachedOnlyServesAnItemWhoseTypeIsNotDetermined();
    void testCachedOnlySkipsATypeNoEnabledThumbnailerClaims();
    void testCachedOnlyTakesASmallerBucketWhenTheOneAskedForIsEmpty();
    void testCachedOnlyReachesTheSmallestBucketAtEveryRatio_data();
    void testCachedOnlyReachesTheSmallestBucketAtEveryRatio();
};

#endif
