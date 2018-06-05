/* This file is part of the KDE project
   Copyright 2017 Friedrich W. H. Kossebau <kossebau@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
 */

#include <KIO/JobTracker>
#include <KJobTrackerInterface>
#include <KJob>
#include <KFile>

#include <QtTest>
#include <QEventLoop>

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
