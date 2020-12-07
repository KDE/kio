/*
    SPDX-FileCopyrightText: 2008 Peter Penz <peter.penz@gmx.at>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef URLNAVIGATORTEST_H
#define URLNAVIGATORTEST_H

#include <QObject>
#include <kiofilewidgets_export.h>
class KUrlNavigator;

class KUrlNavigatorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testHistorySizeAndIndex();
    void testGoBack();
    void testGoForward();
    void testHistoryInsert();

    void bug251553_goUpFromArchive();

    void testUrlParsing_data();
    void testUrlParsing();

    void testRelativePaths();

    void testFixUrlPath_data();
    void testFixUrlPath();

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(4, 5)
    void testButtonUrl_data();
    void testButtonUrl();
#endif
    void testButtonText();

    void testInitWithRedundantPathSeparators();


private:
    KUrlNavigator *m_navigator;
};

#endif
