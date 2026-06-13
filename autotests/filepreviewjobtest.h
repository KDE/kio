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

    void testCachedThumbnailReturnsFreshThumbnail();
    void testCachedThumbnailReturnsStaleThumbnail();
    void testCachedThumbnailDetectsSizeMismatch();
    void testCachedThumbnailMissReturnsNull();
    void testCachedThumbnailWrongSizeBucket();
    void testCachedThumbnailRejectsIneligibleItems();

private:
    // Member (not a free function) so it can reach the private path helper this
    // test is a friend of.
    static QString cacheThumbnailPath(const QUrl &url, const QSize &size, qreal dpr);
    static void writeCachedThumbnail(const QUrl &url, const QSize &size, qreal dpr, qint64 mtimeSecs, qint64 fileSize);
};

#endif
