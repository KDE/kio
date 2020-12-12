/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2017 Friedrich W. H. Kossebau <kossebau@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/JobTracker>
#include <KJobTrackerInterface>
#include <KJob>
#include <KFile>

#include <QTest>
#include <QEventLoop>
#include <QTimer>

// widget is shown with hardcoded delay of 500 ms by KWidgetJobTracker
static const int testJobRunningTime = 600;

class TestJob : public KJob
{
    Q_OBJECT
public:
    void start() override { QTimer::singleShot(testJobRunningTime, this, &TestJob::doEmit); }

private Q_SLOTS:
    void doEmit() { emitResult(); }
};


class KDynamicJobTrackerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testNoCrashWithoutQWidgetsPossible();
};

void KDynamicJobTrackerTest::testNoCrashWithoutQWidgetsPossible()
{
    // dummy call: need to use some symbol from KIOWidgets so linkers do not drop linking to it
    KFile::isDefaultView(KFile::Default);

    // simply linking to KIOWidgets results in KDynamicJobTracker installing itself as KIO's jobtracker
    KJobTrackerInterface* jobtracker = KIO::getJobTracker();
    QCOMPARE(jobtracker->metaObject()->className(), "KDynamicJobTracker");

    TestJob *job = new TestJob;

    jobtracker->registerJob(job);

    job->start();
    QEventLoop loop;
    connect(job, &KJob::result, &loop, &QEventLoop::quit);
    loop.exec();
    // if we are here, no crash has happened due to QWidgets tried to be used -> success
}

// GUILESS, so QWidgets are not possible
QTEST_GUILESS_MAIN(KDynamicJobTrackerTest)

#include "kdynamicjobtrackernowidgetstest.moc"
