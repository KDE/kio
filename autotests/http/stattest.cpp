/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/StatJob>

#include <QSignalSpy>
#include <QTest>

class StatTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testStatSource();
    void testStatDest();
};

void StatTest::testStatSource()
{
    auto job = KIO::stat(QUrl("http://localhost:5000/bla"), KIO::StatJob::SourceSide);

    QSignalSpy finishedSpy(job, &KJob::finished);
    finishedSpy.wait();
    QVERIFY(finishedSpy.count());

    const KIO::UDSEntry result = job->statResult();
    QCOMPARE(result.stringValue(KIO::UDSEntry::UDS_NAME), "bla");
    QCOMPARE(job->error(), KJob::NoError);
}

void StatTest::testStatDest()
{
    auto job = KIO::stat(QUrl("http://localhost:5000/bla"), KIO::StatJob::DestinationSide);

    QSignalSpy finishedSpy(job, &KJob::finished);
    finishedSpy.wait();
    QVERIFY(finishedSpy.count());
    QCOMPARE(job->error(), KIO::ERR_DOES_NOT_EXIST);
}

QTEST_GUILESS_MAIN(StatTest)

#include "stattest.moc"
