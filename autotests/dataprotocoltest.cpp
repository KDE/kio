/*
    SPDX-FileCopyrightText: 2002, 2003 Leo Savernik <l.savernik@aon.at>
    SPDX-FileCopyrightText: 2012 Rolf Eike Beer <kde@opensource.sf-tec.de>
*/

#ifdef DATAKIOSLAVE
#  undef DATAKIOSLAVE
#endif
#ifndef TESTKIO
#  define TESTKIO
#endif

#include "dataprotocoltest.h"

#include <QTest>
#include <QDebug>


QTEST_MAIN(DataProtocolTest)

class TestSlave
{
public:
    TestSlave()
    {
    }
    virtual ~TestSlave()
    {
    }

    void mimeType(const QString &type)
    {
        QCOMPARE(type, mime_type_expected);
    }

    void totalSize(KIO::filesize_t /*bytes*/)
    {
//    qDebug() << "content size: " << bytes << " bytes";
    }

    void setMetaData(const QString &key, const QString &value)
    {
        KIO::MetaData::Iterator it = attributes_expected.find(key);
        QVERIFY(it != attributes_expected.end());
        QCOMPARE(value, it.value());
        attributes_expected.erase(it);
    }

    void setAllMetaData(const KIO::MetaData &md)
    {
        KIO::MetaData::ConstIterator it = md.begin();
        KIO::MetaData::ConstIterator end = md.end();

        for (; it != end; ++it) {
            KIO::MetaData::Iterator eit = attributes_expected.find(it.key());
            QVERIFY(eit != attributes_expected.end());
            QCOMPARE(it.value(), eit.value());
            attributes_expected.erase(eit);
        }
    }

    void sendMetaData()
    {
        // check here if attributes_expected contains any excess keys
        KIO::MetaData::ConstIterator it = attributes_expected.constBegin();
        KIO::MetaData::ConstIterator end = attributes_expected.constEnd();
        for (; it != end; ++it) {
            qDebug() << "Metadata[\"" << it.key()
                     << "\"] was expected but not defined";
        }
        QVERIFY(attributes_expected.isEmpty());
    }

    void data(const QByteArray &a)
    {
        if (a.isEmpty()) {
//    qDebug() << "<no more data>";
        } else {
            QCOMPARE(a, content_expected);
        }/*end if*/
    }

    void dispatch_data(const QByteArray &a)
    {
        data(a);
    }

    void finished()
    {
    }

    void dispatch_finished()
    {
    }

    void ref() {}
    void deref() {}

private:
    // -- testcase related members
    QString mime_type_expected;   // expected MIME type
    /** contains all attributes and values the testcase has to set */
    KIO::MetaData attributes_expected;
    /** contains the content as it is expected to be returned */
    QByteArray content_expected;

public:
    /**
     * sets the MIME type that this testcase is expected to return
     */
    void setExpectedMimeType(const QString &mime_type)
    {
        mime_type_expected = mime_type;
    }

    /**
     * sets all attribute-value pairs the testcase must deliver.
     */
    void setExpectedAttributes(const KIO::MetaData &attres)
    {
        attributes_expected = attres;
    }

    /**
     * sets content as expected to be delivered by the testcase.
     */
    void setExpectedContent(const QByteArray &content)
    {
        content_expected = content;
    }
};

#include "dataprotocol.cpp" // we need access to static data & functions

void runTest(const QByteArray &mimetype, const QStringList &metalist, const QByteArray &content, const QUrl &url)
{
    DataProtocol kio_data;

    kio_data.setExpectedMimeType(mimetype);
    MetaData exp_attrs;
    for (const QString &meta : metalist) {
        const QStringList metadata = meta.split(QLatin1Char('='));
        Q_ASSERT(metadata.count() == 2);
        exp_attrs[metadata[0]] = metadata[1];
    }
    kio_data.setExpectedAttributes(exp_attrs);
    kio_data.setExpectedContent(content);

    // check that mimetype(url) returns the same value as the complete parsing
    kio_data.mimetype(url);

    kio_data.get(url);
}

void DataProtocolTest::runAllTests()
{
    QFETCH(QByteArray, expected_mime_type);
    QFETCH(QString, metadata);
    QFETCH(QByteArray, exp_content);
    QFETCH(QByteArray, url);

    const QStringList metalist = metadata.split(QLatin1Char('\n'));

    runTest(expected_mime_type, metalist, exp_content, QUrl(url));
}

void DataProtocolTest::runAllTests_data()
{
    QTest::addColumn<QByteArray>("expected_mime_type");
    QTest::addColumn<QString>("metadata");
    QTest::addColumn<QByteArray>("exp_content");
    QTest::addColumn<QByteArray>("url");

    const QByteArray textplain = "text/plain";
    const QString usascii = QStringLiteral("charset=us-ascii");
    const QString csutf8 = QStringLiteral("charset=utf-8");
    const QString cslatin1 = QStringLiteral("charset=iso-8859-1");
    const QString csiso7 = QStringLiteral("charset=iso-8859-7");

    QTest::newRow("escape resolving") <<
                                      textplain <<
                                      usascii <<
                                      QByteArray("blah blah") <<
                                      QByteArray("data:,blah%20blah");

    QTest::newRow("MIME type, escape resolving") <<
            QByteArray("text/html") <<
            usascii <<
            QByteArray("<div style=\"border:thin orange solid;padding:1ex;background-color:yellow;color:black\">Rich <b>text</b></div>") <<
            QByteArray("data:text/html,<div%20style=\"border:thin%20orange%20solid;"
                       "padding:1ex;background-color:yellow;color:black\">Rich%20<b>text</b>"
                       "</div>");

    QTest::newRow("whitespace test I") <<
                                       QByteArray("text/css") <<
                                       QStringLiteral("charset=iso-8859-15") <<
                                       QByteArray(" body { color: yellow; background:darkblue; font-weight:bold }") <<
                                       QByteArray("data:text/css  ;  charset =  iso-8859-15 , body { color: yellow; "
                                               "background:darkblue; font-weight:bold }");

    QTest::newRow("out of spec argument order, base64 decoding, whitespace test II") <<
            textplain <<
            QStringLiteral("charset=iso-8859-1") <<
            QByteArray("paaaaaaaasd!!\n") <<
            QByteArray("data: ;  base64 ; charset =  \"iso-8859-1\" ,cGFhYWFhYWFhc2QhIQo=");

    QTest::newRow("arbitrary keys, reserved names as keys, whitespace test III") <<
            textplain <<
            QString::fromLatin1("base64=nospace\n"
                                "key=onespaceinner\n"
                                "key2=onespaceouter\n"
                                "charset=utf8\n"
                                "<<empty>>=") <<
            QByteArray("Die, Allied Schweinehund (C) 1990 Wolfenstein 3D") <<
            QByteArray("data: ;base64=nospace;key = onespaceinner; key2=onespaceouter ;"
                       " charset = utf8 ; <<empty>>= ,Die, Allied Schweinehund "
                       "(C) 1990 Wolfenstein 3D");

    QTest::newRow("string literal with escaped chars, testing delimiters within string") <<
            textplain <<
            QStringLiteral("fortune-cookie=Master Leep say: \"Rabbit is humble, Rabbit is gentle; follow the Rabbit\"\ncharset=us-ascii") <<
            QByteArray("(C) 1997 Shadow Warrior ;-)") <<
            QByteArray("data:;fortune-cookie=\"Master Leep say: \\\"Rabbit is humble, "
                       "Rabbit is gentle; follow the Rabbit\\\"\",(C) 1997 Shadow Warrior "
                       ";-)");

    QTest::newRow("escaped charset") <<
                                     textplain <<
                                     QStringLiteral("charset=iso-8859-7") <<
                                     QByteArray("test") <<
                                     QByteArray("data:text/plain;charset=%22%5cis%5co%5c-%5c8%5c8%5c5%5c9%5c-%5c7%22,test");

    // the "greenbytes" tests are taken from http://greenbytes.de/tech/tc/datauri/
    QTest::newRow("greenbytes-simple") <<
                                       textplain <<
                                       usascii <<
                                       QByteArray("test") <<
                                       QByteArray("data:,test");

    QTest::newRow("greenbytes-simplewfrag") <<
                                            textplain <<
                                            usascii <<
                                            QByteArray("test") <<
                                            QByteArray("data:,test#foo");

    QTest::newRow("greenbytes-simple-utf8-dec") <<
            textplain <<
            csutf8 <<
            QByteArray("test \xc2\xa3 pound sign") <<
            QByteArray("data:text/plain;charset=utf-8,test%20%c2%a3%20pound%20sign");

    QTest::newRow("greenbytes-simple-iso8859-1-dec") <<
            textplain <<
            cslatin1 <<
            QByteArray("test \xc2\xa3 pound sign") <<
            QByteArray("data:text/plain;charset=iso-8859-1,test%20%a3%20pound%20sign");

    QTest::newRow("greenbytes-simple-iso8859-7-dec") <<
            textplain <<
            csiso7 <<
            QByteArray("test \xce\xa3 sigma") <<
            QByteArray("data:text/plain;charset=iso-8859-7,test%20%d3%20sigma");

    QTest::newRow("greenbytes-simple-utf8-dec-dq") <<
            textplain <<
            csutf8 <<
            QByteArray("test \xc2\xa3 pound sign") <<
            QByteArray("data:text/plain;charset=%22utf-8%22,test%20%c2%a3%20pound%20sign");

    QTest::newRow("greenbytes-simple-iso8859-1-dec-dq") <<
            textplain <<
            cslatin1 <<
            QByteArray("test \xc2\xa3 pound sign") <<
            QByteArray("data:text/plain;charset=%22iso-8859-1%22,test%20%a3%20pound%20sign");

    QTest::newRow("greenbytes-simple-iso8859-7-dec-dq") <<
            textplain <<
            csiso7 <<
            QByteArray("test \xce\xa3 sigma") <<
            QByteArray("data:text/plain;charset=%22iso-8859-7%22,test%20%d3%20sigma");

    QTest::newRow("greenbytes-simple-utf8-dec-dq-escaped") <<
            textplain <<
            csutf8 <<
            QByteArray("test \xc2\xa3 pound sign") <<
            QByteArray("data:text/plain;charset=%22%5cu%5ct%5cf%5c-%5c8%22,test%20%c2%a3%20pound%20sign");

    QTest::newRow("greenbytes-simple-iso8859-1-dec-dq-escaped") <<
            textplain <<
            cslatin1 <<
            QByteArray("test \xc2\xa3 pound sign") <<
            QByteArray("data:text/plain;charset=%22%5ci%5cs%5co%5c-%5c8%5c8%5c5%5c9%5c-%5c1%22,test%20%a3%20pound%20sign");

    QTest::newRow("greenbytes-simple-iso8859-7-dec-dq-escaped") <<
            textplain <<
            csiso7 <<
            QByteArray("test \xce\xa3 sigma") <<
            QByteArray("data:text/plain;charset=%22%5ci%5cs%5co%5c-%5c8%5c8%5c5%5c9%5c-%5c7%22,test%20%d3%20sigma");

    QTest::newRow("greenbytes-simplefrag") <<
                                           QByteArray("text/html") <<
                                           usascii <<
                                           QByteArray("<p>foo</p>") <<
                                           QByteArray("data:text/html,%3Cp%3Efoo%3C%2Fp%3E#bar");

    QTest::newRow("greenbytes-svg") <<
                                    QByteArray("image/svg+xml") <<
                                    usascii <<
                                    QByteArray("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n"
                                            "  <circle cx=\"100\" cy=\"100\" r=\"25\" stroke=\"black\" stroke-width=\"1\" fill=\"green\"/>\n"
                                            "</svg>\n") <<
                                    QByteArray("data:image/svg+xml,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20version%3D%221.1"
                                            "%22%3E%0A%20%20%3Ccircle%20cx%3D%22100%22%20cy%3D%22100%22%20r%3D%2225%22%20stroke%3D%22black%22%20"
                                            "stroke-width%3D%221%22%20fill%3D%22green%22%2F%3E%0A%3C%2Fsvg%3E%0A#bar");

    QTest::newRow("greenbytes-ext-simple") <<
                                           QByteArray("image/svg+xml") <<
                                           QStringLiteral("foo=bar\ncharset=us-ascii") <<
                                           QByteArray("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n"
                                                   "  <circle cx=\"100\" cy=\"100\" r=\"25\" stroke=\"black\" stroke-width=\"1\" fill=\"green\"/>\n"
                                                   "</svg>\n") <<
                                           QByteArray("data:image/svg+xml;foo=bar,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20version%3D%221.1"
                                                   "%22%3E%0A%20%20%3Ccircle%20cx%3D%22100%22%20cy%3D%22100%22%20r%3D%2225%22%20stroke%3D%22black%22%20"
                                                   "stroke-width%3D%221%22%20fill%3D%22green%22%2F%3E%0A%3C%2Fsvg%3E%0A");

    QTest::newRow("greenbytes-ext-simple-qs") <<
            QByteArray("image/svg+xml") <<
            QStringLiteral("foo=bar,bar\ncharset=us-ascii") <<
            QByteArray("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n"
                       "  <circle cx=\"100\" cy=\"100\" r=\"25\" stroke=\"black\" stroke-width=\"1\" fill=\"green\"/>\n"
                       "</svg>\n") <<
            QByteArray("data:image/svg+xml;foo=%22bar,bar%22,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20"
                       "version%3D%221.1%22%3E%0A%20%20%3Ccircle%20cx%3D%22100%22%20cy%3D%22100%22%20r%3D%2225%22%20stroke%3D%22black"
                       "%22%20stroke-width%3D%221%22%20fill%3D%22green%22%2F%3E%0A%3C%2Fsvg%3E%0A");
}

