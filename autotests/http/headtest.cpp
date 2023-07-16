/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/MimetypeJob>
#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class HeadTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testMimeType();
    void testMimeType_data();
};

void HeadTest::testMimeType_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expectedMimeType");

    QTest::addRow("html") << "http://localhost:5000/mime/html"
                          << "text/html";
    QTest::addRow("calendar") << "http://localhost:5000/mime/calendar"
                              << "text/calendar";
}

void HeadTest::testMimeType()
{
    QFETCH(QString, url);
    QFETCH(QString, expectedMimeType);

    auto *job = KIO::mimetype(QUrl(url));

    QSignalSpy mimeTypeFoundSpy(job, &KIO::MimetypeJob::mimeTypeFound);
    mimeTypeFoundSpy.wait();
    QCOMPARE(mimeTypeFoundSpy.count(), 1);

    auto args = mimeTypeFoundSpy.first();
    QCOMPARE(args[1], expectedMimeType);

    QSignalSpy spy(job, &KJob::finished);
    spy.wait();
    QVERIFY(spy.size());
    QCOMPARE(job->mimetype(), expectedMimeType);
}

QTEST_GUILESS_MAIN(HeadTest)

#include "headtest.moc"
