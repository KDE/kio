/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2010, 2011 Rolf Eike Beer <kde@opensource.sf-tec.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "httpheaderdispositiontest.h"

#include <QTest>

#include <QByteArray>

#include <parsinghelpers.h>

#include <parsinghelpers.cpp>

// QT5 TODO QTEST_GUILESS_MAIN(HeaderDispositionTest)
QTEST_MAIN(HeaderDispositionTest)

static void runTest(const QString &header, const QByteArray &result)
{
    QMap<QString, QString> parameters = contentDispositionParser(header);

    QList<QByteArray> results = result.split('\n');
    if (result.isEmpty()) {
        results.clear();
    }

    for (const QByteArray &ba : qAsConst(results)) {
        QList<QByteArray> values = ba.split('\t');
        const QString key(QString::fromLatin1(values.takeFirst()));

        QVERIFY(parameters.contains(key));

        const QByteArray val = values.takeFirst();
        QVERIFY(values.isEmpty());

        QCOMPARE(parameters[key], QString::fromUtf8(val.constData(), val.length()));
    }

    QCOMPARE(parameters.count(), results.count());
}

void HeaderDispositionTest::runAllTests_data()
{
    QTest::addColumn<QString>("header");
    QTest::addColumn<QByteArray>("result");

    // http://greenbytes.de/tech/tc2231/
    QTest::newRow("greenbytes-inlonly") << "inline" <<
                                        QByteArray("type\tinline");
    QTest::newRow("greenbytes-inlonlyquoted") << "\"inline\"" <<
            QByteArray();
    QTest::newRow("greenbytes-inlwithasciifilename") << "inline; filename=\"foo.html\"" <<
            QByteArray("type\tinline\n"
                       "filename\tfoo.html");
    QTest::newRow("greenbytes-inlwithfnattach") << "inline; filename=\"Not an attachment!\"" <<
            QByteArray("type\tinline\n"
                       "filename\tNot an attachment!");
    QTest::newRow("greenbytes-inlwithasciifilenamepdf") << "inline; filename=\"foo.pdf\"" <<
            QByteArray("type\tinline\n"
                       "filename\tfoo.pdf");
    QTest::newRow("greenbytes-attonly") << "attachment" <<
                                        QByteArray("type\tattachment");
    QTest::newRow("greenbytes-attonlyquoted") << "\"attachment\"" <<
            QByteArray();
    QTest::newRow("greenbytes-attonlyucase") << "ATTACHMENT" <<
            QByteArray("type\tattachment");
    QTest::newRow("greenbytes-attwithasciifilename") << "attachment; filename=\"foo.html\"" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo.html");
    QTest::newRow("greenbytes-attwithasciifnescapedchar") << "attachment; filename=\"f\\oo.html\"" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo.html");
    QTest::newRow("greenbytes-attwithasciifnescapedquote") << "attachment; filename=\"\\\"quoting\\\" tested.html\"" <<
            QByteArray("type\tattachment\n"
                       "filename\t\"quoting\" tested.html");
    QTest::newRow("greenbytes-attwithquotedsemicolon") << "attachment; filename=\"Here's a semicolon;.html\"" <<
            QByteArray("type\tattachment\n"
                       "filename\tHere's a semicolon;.html");
    QTest::newRow("greenbytes-attwithfilenameandextparam") << "attachment; foo=\"bar\"; filename=\"foo.html\"" <<
            QByteArray("type\tattachment\n"
                       "foo\tbar\n"
                       "filename\tfoo.html");
    QTest::newRow("greenbytes-attwithfilenameandextparamescaped") << "attachment; foo=\"\\\"\\\\\";filename=\"foo.html\"" <<
            QByteArray("type\tattachment\n"
                       "foo\t\"\\\n"
                       "filename\tfoo.html");
    QTest::newRow("greenbytes-attwithasciifilenameucase") << "attachment; FILENAME=\"foo.html\"" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo.html");
// specification bug in RfC 2616, legal through RfC 2183 and 6266
    QTest::newRow("greenbytes-attwithasciifilenamenq") << "attachment; filename=foo.html" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo.html");
    QTest::newRow("greenbytes-attwithasciifilenamenqws") << "attachment; filename=foo bar.html" <<
            QByteArray("type\tattachment");
    QTest::newRow("greenbytes-attwithfntokensq") << "attachment; filename='foo.bar'" <<
            QByteArray("type\tattachment\n"
                       "filename\t'foo.bar'");
    QTest::newRow("greenbytes-attwithisofnplain-x") << QStringLiteral("attachment; filename=\"foo-\xe4.html\"") <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo-ä.html");
    QTest::newRow("greenbytes-attwithisofnplain") << QString::fromLatin1("attachment; filename=\"foo-ä.html\"") <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo-Ã¤.html");
    QTest::newRow("greenbytes-attwithfnrawpctenca") << "attachment; filename=\"foo-%41.html\"" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo-%41.html");
    QTest::newRow("greenbytes-attwithfnusingpct") << "attachment; filename=\"50%.html\"" <<
            QByteArray("type\tattachment\n"
                       "filename\t50%.html");
    QTest::newRow("greenbytes-attwithfnrawpctencaq") << "attachment; filename=\"foo-%\\41.html\"" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo-%41.html");
    QTest::newRow("greenbytes-attwithnamepct") << "attachment; name=\"foo-%41.html\"" <<
            QByteArray("type\tattachment\n"
                       "name\tfoo-%41.html");
    QTest::newRow("greenbytes-attwithfilenamepctandiso") << QStringLiteral("attachment; filename=\"\xe4-%41.html\"") <<
            QByteArray("type\tattachment\n"
                       "filename\tä-%41.html");
    QTest::newRow("greenbytes-attwithfnrawpctenclong") << "attachment; filename=\"foo-%c3%a4-%e2%82%ac.html\"" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo-%c3%a4-%e2%82%ac.html");
    QTest::newRow("greenbytes-attwithasciifilenamews1") << "attachment; filename =\"foo.html\"" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo.html");
    QTest::newRow("greenbytes-attwith2filenames") << "attachment; filename=\"foo.html\"; filename=\"bar.html\"" <<
            QByteArray("type\tattachment");
    QTest::newRow("greenbytes-attfnbrokentoken") << "attachment; filename=foo[1](2).html" <<
            QByteArray("type\tattachment");
    QTest::newRow("greenbytes-attmissingdisposition") << "filename=foo.html" <<
            QByteArray();
    QTest::newRow("greenbytes-attmissingdisposition2") << "x=y; filename=foo.html" <<
            QByteArray();
    QTest::newRow("greenbytes-attmissingdisposition3") << "\"foo; filename=bar;baz\"; filename=qux" <<
            QByteArray();
    QTest::newRow("greenbytes-attmissingdisposition4") << "filename=foo.html, filename=bar.html" <<
            QByteArray();
    QTest::newRow("greenbytes-emptydisposition") << "; filename=foo.html" <<
            QByteArray();
    QTest::newRow("greenbytes-attbrokenquotedfn") << "attachment; filename=\"foo.html\".txt" <<
            QByteArray("type\tattachment");
    QTest::newRow("greenbytes-attbrokenquotedfn2") << "attachment; filename=\"bar" <<
            QByteArray("type\tattachment");
    QTest::newRow("greenbytes-attbrokenquotedfn3") << "attachment; filename=foo\"bar;baz\"qux" <<
            QByteArray("type\tattachment");
    QTest::newRow("greenbytes-attreversed") << "filename=foo.html; attachment" <<
                                            QByteArray();
    QTest::newRow("greenbytes-attconfusedparam") << "attachment; xfilename=foo.html" <<
            QByteArray("type\tattachment\n"
                       "xfilename\tfoo.html");
    QTest::newRow("greenbytes-attabspath") << "attachment; filename=\"/foo.html\"" <<
                                           QByteArray("type\tattachment\n"
                                                   "filename\tfoo.html");
#ifdef Q_OS_WIN
    QTest::newRow("greenbytes-attabspath") << "attachment; filename=\"\\\\foo.html\"" <<
                                           QByteArray("type\tattachment\n"
                                                   "filename\tfoo.html");
#else // Q_OS_WIN
    QTest::newRow("greenbytes-attabspath") << "attachment; filename=\"\\\\foo.html\"" <<
                                           QByteArray("type\tattachment\n"
                                                   "filename\t\\foo.html");
#endif // Q_OS_WIN
    QTest::newRow("greenbytes-") << "attachment; creation-date=\"Wed, 12 Feb 1997 16:29:51 -0500\"" <<
                                 QByteArray("type\tattachment\n"
                                            "creation-date\tWed, 12 Feb 1997 16:29:51 -0500");
    QTest::newRow("greenbytes-") << "attachment; modification-date=\"Wed, 12 Feb 1997 16:29:51 -0500\"" <<
                                 QByteArray("type\tattachment\n"
                                            "modification-date\tWed, 12 Feb 1997 16:29:51 -0500");
    QTest::newRow("greenbytes-dispext") << "foobar" <<
                                        QByteArray("type\tfoobar");
    QTest::newRow("greenbytes-dispextbadfn") << "attachment; example=\"filename=example.txt\"" <<
            QByteArray("type\tattachment\n"
                       "example\tfilename=example.txt");
    QTest::newRow("greenbytes-attwithisofn2231iso") << "attachment; filename*=iso-8859-1''foo-%E4.html" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo-ä.html");
    QTest::newRow("greenbytes-attwithfn2231utf8") << "attachment; filename*=UTF-8''foo-%c3%a4-%e2%82%ac.html" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo-ä-€.html");
    QTest::newRow("greenbytes-attwithfn2231noc") << "attachment; filename*=''foo-%c3%a4-%e2%82%ac.html" <<
            QByteArray("type\tattachment");
// it's not filename, but "filename "
    QTest::newRow("greenbytes-attwithfn2231ws1") << "attachment; filename *=UTF-8''foo-%c3%a4.html" <<
            QByteArray("type\tattachment");
    QTest::newRow("greenbytes-attwithfn2231ws2") << "attachment; filename*= UTF-8''foo-%c3%a4.html" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo-ä.html");
    QTest::newRow("greenbytes-attwithfn2231ws3") << "attachment; filename* =UTF-8''foo-%c3%a4.html" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo-ä.html");
// argument must not be enclosed in double quotes
    QTest::newRow("greenbytes-attwithfn2231quot") << "attachment; filename*=\"UTF-8''foo-%c3%a4.html\"" <<
            QByteArray("type\tattachment");
    QTest::newRow("greenbytes-attwithfn2231dpct") << "attachment; filename*=UTF-8''A-%2541.html" <<
            QByteArray("type\tattachment\n"
                       "filename\tA-%41.html");
#ifdef Q_OS_WIN
    QTest::newRow("greenbytes-attwithfn2231abspathdisguised") << "attachment; filename*=UTF-8''%5cfoo.html" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo.html");
#else // Q_OS_WIN
    QTest::newRow("greenbytes-attwithfn2231abspathdisguised") << "attachment; filename*=UTF-8''%5cfoo.html" <<
            QByteArray("type\tattachment\n"
                       "filename\t\\foo.html");
#endif // Q_OS_WIN
    QTest::newRow("greenbytes-attfncont") << "attachment; filename*0=\"foo.\"; filename*1=\"html\"" <<
                                          QByteArray("type\tattachment\n"
                                                  "filename\tfoo.html");
    QTest::newRow("greenbytes-attfncontenc") << "attachment; filename*0*=UTF-8''foo-%c3%a4; filename*1=\".html\"" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo-ä.html");
// no leading zeros
    QTest::newRow("greenbytes-attfncontlz") << "attachment; filename*0=\"foo\"; filename*01=\"bar\"" <<
                                            QByteArray("type\tattachment\n"
                                                    "filename\tfoo");
    QTest::newRow("greenbytes-attfncontnc") << "attachment; filename*0=\"foo\"; filename*2=\"bar\"" <<
                                            QByteArray("type\tattachment\n"
                                                    "filename\tfoo");
// first element must have number 0
    QTest::newRow("greenbytes-attfnconts1") << "attachment; filename*1=\"foo.\"; filename*2=\"html\"" <<
                                            QByteArray("type\tattachment");
// we must not rely on element ordering
    QTest::newRow("greenbytes-attfncontord") << "attachment; filename*1=\"bar\"; filename*0=\"foo\"" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoobar");
// specifying both param and param* is allowed, param* should be taken
    QTest::newRow("greenbytes-attfnboth") << "attachment; filename=\"foo-ae.html\"; filename*=UTF-8''foo-%c3%a4.html" <<
                                          QByteArray("type\tattachment\n"
                                                  "filename\tfoo-ä.html");
// specifying both param and param* is allowed, param* should be taken
    QTest::newRow("greenbytes-attfnboth2") << "attachment; filename*=UTF-8''foo-%c3%a4.html; filename=\"foo-ae.html\"" <<
                                           QByteArray("type\tattachment\n"
                                                   "filename\tfoo-ä.html");
    QTest::newRow("greenbytes-attnewandfn") << "attachment; foobar=x; filename=\"foo.html\"" <<
                                            QByteArray("type\tattachment\n"
                                                    "filename\tfoo.html\n"
                                                    "foobar\tx");
// invalid argument, should be ignored
    QTest::newRow("greenbytes-attrfc2047token") << "attachment; filename==?ISO-8859-1?Q?foo-=E4.html?=" <<
            QByteArray("type\tattachment");
    QTest::newRow("space_before_value") << "attachment; filename= \"foo.html\"" <<
                                        QByteArray("type\tattachment\n"
                                                "filename\tfoo.html");
// no character set given but 8 bit characters
    QTest::newRow("8bit_in_ascii") << "attachment; filename*=''foo-%c3%a4.html" <<
                                   QByteArray("type\tattachment");
// there may not be gaps in numbering
    QTest::newRow("continuation013") << "attachment; filename*0=\"foo.\"; filename*1=\"html\"; filename*3=\"bar\"" <<
                                     QByteArray("type\tattachment\n"
                                             "filename\tfoo.html");
// "wrong" element ordering and encoding
    QTest::newRow("reversed_continuation+encoding") << "attachment; filename*1=\"html\"; filename*0*=us-ascii''foo." <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo.html");
// unknown charset
    QTest::newRow("unknown_charset") << "attachment; filename*=unknown''foo" <<
                                     QByteArray("type\tattachment");
// no apostrophs
    QTest::newRow("encoding-no-apostrophs") << "attachment; filename*=foo" <<
                                            QByteArray("type\tattachment");
// only one apostroph
    QTest::newRow("encoding-one-apostroph") << "attachment; filename*=us-ascii'foo" <<
                                            QByteArray("type\tattachment");
// duplicate filename, both should be ignored and parsing should stop
    QTest::newRow("duplicate-filename") << "attachment; filename=foo; filename=bar; foo=bar" <<
                                        QByteArray("type\tattachment");
// garbage after closing quote, parsing should stop there
    QTest::newRow("garbage_after_closing_quote") << "attachment; filename*=''foo; bar=\"f\"oo; baz=foo" <<
            QByteArray("type\tattachment\n"
                       "filename\tfoo");
// trailing whitespace should be ignored
    QTest::newRow("whitespace_after_value") << "attachment; filename=\"foo\" ; bar=baz" <<
                                            QByteArray("type\tattachment\n"
                                                    "filename\tfoo\n"
                                                    "bar\tbaz");
// invalid syntax for type
    QTest::newRow("invalid_type1") << "filename=foo.html" <<
                                   QByteArray();
// invalid syntax for type
    QTest::newRow("invalid_type2") << "inline{; filename=\"foo\"" <<
                                   QByteArray();
    QTest::newRow("invalid_type3") << "foo bar; filename=\"foo\"" <<
                                   QByteArray();
    QTest::newRow("invalid_type4") << "foo\tbar; filename=\"foo\"" <<
                                   QByteArray();
// missing closing quote, so parameter is broken
    QTest::newRow("no_closing_quote") << "attachment; filename=\"bar" <<
                                      QByteArray("type\tattachment");
// we ignore any path given in the header and use only the filename
    QTest::newRow("full_path_given") << "attachment; filename=\"/etc/shadow\"" <<
                                     QByteArray("type\tattachment\n"
                                             "filename\tshadow");
// we ignore any path given in the header and use only the filename even if there is an error later
    QTest::newRow("full_path_and_parse_error") << "attachment; filename=\"/etc/shadow\"; foo=\"baz\"; foo=\"bar\"" <<
            QByteArray("type\tattachment\n"
                       "filename\tshadow");
// control characters are forbidden in the quoted string
    QTest::newRow("control_character_in_value") << "attachment; filename=\"foo\003\"" <<
            QByteArray("type\tattachment");
// duplicate keys must be ignored
    QTest::newRow("duplicate_with_continuation") << "attachment; filename=\"bar\"; filename*0=\"foo.\"; filename*1=\"html\"" <<
            QByteArray("type\tattachment");

// percent encoding, invalid first character
    QTest::newRow("percent-first-char-invalid") << "attachment; filename*=UTF-8''foo-%o5.html" <<
            QByteArray("type\tattachment");
// percent encoding, invalid second character
    QTest::newRow("percent-second-char-invalid") << "attachment; filename*=UTF-8''foo-%5o.html" <<
            QByteArray("type\tattachment");
// percent encoding, both characters invalid
    QTest::newRow("greenbytes-attwithfn2231nbadpct2") << "attachment; filename*=UTF-8''foo-%oo.html" <<
            QByteArray("type\tattachment");
// percent encoding, invalid second character
    QTest::newRow("percent-second-char-missing") << "attachment; filename*=UTF-8''foo-%f.html" <<
            QByteArray("type\tattachment");
// percent encoding, too short value
    QTest::newRow("percent-short-encoding-at-end") << "attachment; filename*=UTF-8''foo-%f" <<
            QByteArray("type\tattachment");
}

#if 0
// currently unclear if our behaviour is only accidentally correct
// invalid syntax
{  "inline; attachment; filename=foo.html",
    "type\tinline"
},
// invalid syntax
{
    "attachment; inline; filename=foo.html",
    "type\tattachment"
},

// deactivated for now: failing due to missing implementation
{
    "attachment; filename=\"foo-&#xc3;&#xa4;.html\"",
    "type\tattachment\n"
    "filename\tfoo-Ã¤.html"
},
// deactivated for now: not the same utf, no idea if the expected value is actually correct
{
    "attachment; filename*=UTF-8''foo-a%cc%88.html",
    "type\tattachment\n"
    "filename\tfoo-ä.html"
}

// deactivated for now: only working to missing implementation
// string is not valid iso-8859-1 so filename should be ignored
//"attachment; filename*=iso-8859-1''foo-%c3%a4-%e2%82%ac.html",
//"type\tattachment",

// deactivated for now: only working to missing implementation
// should not be decoded
//"attachment; filename=\"=?ISO-8859-1?Q?foo-=E4.html?=\"",
//"type\tattachment\n"
//"filename\t=?ISO-8859-1?Q?foo-=E4.html?=",

};
#endif

void HeaderDispositionTest::runAllTests()
{
    QFETCH(QString, header);
    QFETCH(QByteArray, result);

    runTest(header, result);
}
