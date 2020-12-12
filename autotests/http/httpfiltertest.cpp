/*
    SPDX-FileCopyrightText: 2002-2005 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <QSignalSpy>
#include <QTest>

#include <KFilterDev>
#include <KFilterBase>
#include <QFile>
#include <QDebug>
#include <QDir>
#include <zlib.h>
#include "httpfilter.h"

class HTTPFilterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void test_deflateWithZlibHeader();
    void test_httpFilterGzip();

private:
    void test_block_write(const QString &fileName, const QByteArray &data);
    void test_block_read(const QString &fileName);
    void test_getch(const QString &fileName);
    void test_textstream(const QString &fileName);
    void test_readall(const QString &fileName, const QString &mimeType, const QByteArray &expectedData);

protected Q_SLOTS:
    void slotFilterOutput(const QByteArray &data);

private:
    QString pathgz;
    QByteArray testData;
    QByteArray m_filterOutput;
};

QTEST_MAIN(HTTPFilterTest)

void HTTPFilterTest::initTestCase()
{
    qRegisterMetaType<KCompressionDevice::CompressionType>();
    const QString currentdir = QDir::currentPath();
    pathgz = currentdir + "/test.gz";

    testData = "hello world\n";

    // Create the gz file

    KFilterDev dev(pathgz);
    QVERIFY(dev.open(QIODevice::WriteOnly));
    const int ret = dev.write(testData);
    QCOMPARE(ret, testData.size());
    dev.close();
}

static void getCompressedData(QByteArray &data, QByteArray &compressedData)
{
    data = "Hello world, this is a test for deflate, from bug 114830 / 117683";
    compressedData.resize(long(data.size() * 1.1f) + 12L); // requirements of zlib::compress2
    unsigned long out_bufferlen = compressedData.size();
    const int ret = compress2((Bytef *)compressedData.data(), &out_bufferlen, (const Bytef *)data.constData(), data.size(), 1);
    QCOMPARE(ret, Z_OK);
    compressedData.resize(out_bufferlen);
}

void HTTPFilterTest::test_deflateWithZlibHeader()
{
    QByteArray data, deflatedData;
    getCompressedData(data, deflatedData);

    {
        HTTPFilterDeflate filter;
        QSignalSpy spyOutput(&filter, &HTTPFilterBase::output);
        QSignalSpy spyError(&filter, &HTTPFilterBase::error);
        filter.slotInput(deflatedData);
        QCOMPARE(spyOutput.count(), 2);
        QCOMPARE(spyOutput[0][0].toByteArray(), data);
        QCOMPARE(spyOutput[1][0].toByteArray(), QByteArray());
        QCOMPARE(spyError.count(), 0);
    }
    {
        // Now a test for giving raw deflate data to HTTPFilter
        HTTPFilterDeflate filter;
        QSignalSpy spyOutput(&filter, &HTTPFilterBase::output);
        QSignalSpy spyError(&filter, &HTTPFilterBase::error);
        QByteArray rawDeflate = deflatedData.mid(2); // remove CMF+FLG
        rawDeflate.chop(4); // remove trailing Adler32.
        filter.slotInput(rawDeflate);
        QCOMPARE(spyOutput.count(), 2);
        QCOMPARE(spyOutput[0][0].toByteArray(), data);
        QCOMPARE(spyOutput[1][0].toByteArray(), QByteArray());
        QCOMPARE(spyError.count(), 0);
    }
}

void HTTPFilterTest::test_httpFilterGzip()
{
    QFile file(pathgz);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray compressed = file.readAll();

    // Test sending the whole data in one go
    {
        HTTPFilterGZip filter;
        QSignalSpy spyOutput(&filter, &HTTPFilterBase::output);
        QSignalSpy spyError(&filter, &HTTPFilterBase::error);
        filter.slotInput(compressed);
        QCOMPARE(spyOutput.count(), 2);
        QCOMPARE(spyOutput[0][0].toByteArray(), testData);
        QCOMPARE(spyOutput[1][0].toByteArray(), QByteArray());
        QCOMPARE(spyError.count(), 0);
    }

    // Test sending the data byte by byte
    {
        m_filterOutput.clear();
        HTTPFilterGZip filter;
        QSignalSpy spyOutput(&filter, &HTTPFilterBase::output);
        connect(&filter, &HTTPFilterBase::output, this, &HTTPFilterTest::slotFilterOutput);
        QSignalSpy spyError(&filter, &HTTPFilterBase::error);
        for (int i = 0; i < compressed.size(); ++i) {
            //qDebug() << "sending byte number" << i << ":" << (uchar)compressed[i];
            filter.slotInput(QByteArray(compressed.constData() + i, 1));
            QCOMPARE(spyError.count(), 0);
        }
        QCOMPARE(m_filterOutput, testData);
        QCOMPARE(spyOutput[spyOutput.count() - 1][0].toByteArray(), QByteArray()); // last one was empty
    }
}

void HTTPFilterTest::slotFilterOutput(const QByteArray &data)
{
    m_filterOutput += data;
}

#include "httpfiltertest.moc"
