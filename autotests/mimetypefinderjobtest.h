/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef MIMETYPEFINDERJOBTEST_H
#define MIMETYPEFINDERJOBTEST_H

#include <QObject>

class MimeTypeFinderJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void determineMimeType_data();
    void determineMimeType();

    void invalidUrl();
    void nonExistingFile();

    void httpUrlWithKIO();
    void killHttp();
    void ftpUrlWithKIO();
};

#endif /* MIMETYPEFINDERJOBTEST_H */
