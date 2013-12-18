/* This file is part of the KDE libraries
    Copyright (C) 2011 Dawit Alemayehu <adawit@kde.org>

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

#include "httpauthenticationtest.h"

#include <qtest.h>

#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QtNetwork/QHostInfo>

#define ENABLE_HTTP_AUTH_NONCE_SETTER
#include "httpauthentication.cpp"

// QT5 TODO QTEST_GUILESS_MAIN(HTTPAuthenticationTest)
QTEST_MAIN(HTTPAuthenticationTest)

static void parseAuthHeader(const QByteArray& header,
                            QByteArray* bestOffer,
                            QByteArray* scheme,
                            QList<QByteArray>* result)
{
    const QList<QByteArray> authHeaders = KAbstractHttpAuthentication::splitOffers(QList<QByteArray>() << header);
    QByteArray chosenHeader = KAbstractHttpAuthentication::bestOffer(authHeaders);

    if (bestOffer) {
        *bestOffer = chosenHeader;
    }

    if (!scheme && !result) {
      return;
    }

    QByteArray authScheme;
    const QList<QByteArray> parseResult = parseChallenge(chosenHeader, &authScheme);

    if (scheme) {
        *scheme = authScheme;
    }

    if (result) {
        *result = parseResult;
    }
}

void HTTPAuthenticationTest::testHeaderParsing_data()
{
    QTest::addColumn<QByteArray>("header");
    QTest::addColumn<QByteArray>("resultScheme");
    QTest::addColumn<QByteArray>("resultValues");

     // Tests cases from http://greenbytes.de/tech/tc/httpauth/
    QTest::newRow("greenbytes-simplebasic") << QByteArray("Basic realm=\"foo\"") << QByteArray("Basic") << QByteArray("realm,foo");
    QTest::newRow("greenbytes-simplebasictok") << QByteArray("Basic realm=foo") << QByteArray("Basic") << QByteArray("realm,foo");
    QTest::newRow("greenbytes-simplebasiccomma") << QByteArray("Basic , realm=\"foo\"") << QByteArray("Basic") << QByteArray("realm,foo");
    // there must be a space after the scheme
    QTest::newRow("greenbytes-simplebasiccomma2") << QByteArray("Basic, realm=\"foo\"") << QByteArray() << QByteArray();
    // we accept scheme without any parameters to maintain compatibility with too simple minded servers out there
    QTest::newRow("greenbytes-simplebasicnorealm") << QByteArray("Basic") << QByteArray("Basic") << QByteArray();
    QTest::newRow("greenbytes-simplebasicwsrealm") << QByteArray("Basic realm = \"foo\"") << QByteArray("Basic") << QByteArray("realm,foo");
    QTest::newRow("greenbytes-simplebasicrealmsqc") << QByteArray("Basic realm=\"\\f\\o\\o\"") << QByteArray("Basic") << QByteArray("realm,foo");
    QTest::newRow("greenbytes-simplebasicrealmsqc2") << QByteArray("Basic realm=\"\\\"foo\\\"\"") << QByteArray("Basic") << QByteArray("realm,\"foo\"");
    QTest::newRow("greenbytes-simplebasicnewparam1") << QByteArray("Basic realm=\"foo\", bar=\"xyz\"") << QByteArray("Basic") << QByteArray("realm,foo,bar,xyz");
    QTest::newRow("greenbytes-simplebasicnewparam2") << QByteArray("Basic bar=\"xyz\", realm=\"foo\"") << QByteArray("Basic") << QByteArray("bar,xyz,realm,foo");
    // a Basic challenge following an empty one
    QTest::newRow("greenbytes-multibasicempty") << QByteArray(",Basic realm=\"foo\"") << QByteArray("Basic") << QByteArray("realm,foo");
    QTest::newRow("greenbytes-multibasicunknown") << QByteArray("Basic realm=\"basic\", Newauth realm=\"newauth\"") << QByteArray("Basic") << QByteArray("realm,basic");
    QTest::newRow("greenbytes-multibasicunknown2") << QByteArray("Newauth realm=\"newauth\", Basic realm=\"basic\"") << QByteArray("Basic") << QByteArray("realm,basic");
    QTest::newRow("greenbytes-unknown") << QByteArray("Newauth realm=\"newauth\"") << QByteArray() << QByteArray();

    // Misc. test cases
    QTest::newRow("ntlm") << QByteArray("NTLM   ") << QByteArray("NTLM") << QByteArray();
    QTest::newRow("unterminated-quoted-value") << QByteArray("Basic realm=\"") << QByteArray("Basic") << QByteArray();
    QTest::newRow("spacing-and-tabs") << QByteArray("bAsic bar\t =\t\"baz\", realm =\t\"foo\"") << QByteArray("bAsic") << QByteArray("bar,baz,realm,foo");
    QTest::newRow("empty-fields") << QByteArray("Basic realm=foo , , ,  ,, bar=\"baz\"\t,") << QByteArray("Basic") << QByteArray("realm,foo,bar,baz");
    QTest::newRow("spacing") << QByteArray("Basic realm=foo, bar = baz") << QByteArray("Basic") << QByteArray("realm,foo,bar,baz");
    QTest::newRow("missing-comma-between-fields") << QByteArray("Basic realm=foo bar = baz") << QByteArray("Basic") << QByteArray("realm,foo");
    // quotes around text, every character needlessly quoted
    QTest::newRow("quote-excess") << QByteArray("Basic realm=\"\\\"\\f\\o\\o\\\"\"") << QByteArray("Basic") << QByteArray("realm,\"foo\"");
    // quotes around text, quoted backslashes
    QTest::newRow("quoted-backslash") << QByteArray("Basic realm=\"\\\"foo\\\\\\\\\"") << QByteArray("Basic") << QByteArray("realm,\"foo\\\\");
    // quotes around text, quoted backslashes, quote hidden behind them
    QTest::newRow("quoted-backslash-and-quote") << QByteArray("Basic realm=\"\\\"foo\\\\\\\"\"") << QByteArray("Basic") << QByteArray("realm,\"foo\\\"");
    // invalid quoted text
    QTest::newRow("invalid-quoted") << QByteArray("Basic realm=\"\\\"foo\\\\\\\"") << QByteArray("Basic") << QByteArray();
    // ends in backslash without quoted value
    QTest::newRow("invalid-quote") << QByteArray("Basic realm=\"\\\"foo\\\\\\") << QByteArray("Basic") << QByteArray();
}

QByteArray joinQByteArray(const QList<QByteArray>& list)
{
    QByteArray data;
    const int count = list.count();

    for (int i = 0; i < count; ++i) {
        if (i > 0)
            data += ',';
        data += list.at(i);
    }

    return data;
}

void HTTPAuthenticationTest::testHeaderParsing()
{
    QFETCH(QByteArray, header);
    QFETCH(QByteArray, resultScheme);
    QFETCH(QByteArray, resultValues);

    QByteArray chosenHeader, chosenScheme;
    QList<QByteArray> parsingResult, expectedResult;
    parseAuthHeader(header, &chosenHeader, &chosenScheme, &parsingResult);
    QCOMPARE(chosenScheme, resultScheme);
    QCOMPARE(joinQByteArray(parsingResult), resultValues);
}

void HTTPAuthenticationTest::testAuthenticationSelection_data()
{
    QTest::addColumn<QByteArray>("input");
    QTest::addColumn<QByteArray>("expectedScheme");
    QTest::addColumn<QByteArray>("expectedOffer");

#if HAVE_LIBGSSAPI
    QTest::newRow("all-with-negotiate") << QByteArray("Negotiate , Digest , NTLM , Basic") << QByteArray("Negotiate") << QByteArray("Negotiate");
#endif
    QTest::newRow("all-without-negotiate") << QByteArray("Digest , NTLM , Basic , NewAuth") << QByteArray("Digest") << QByteArray("Digest");
    QTest::newRow("ntlm-basic-unknown") << QByteArray("NTLM , Basic , NewAuth") << QByteArray("NTLM") << QByteArray("NTLM");
    QTest::newRow("basic-unknown") << QByteArray("Basic , NewAuth") << QByteArray("Basic") << QByteArray("Basic");
    QTest::newRow("ntlm-basic+param-ntlm") << QByteArray("NTLM   , Basic realm=foo, bar = baz, NTLM") << QByteArray("NTLM") << QByteArray("NTLM");
    QTest::newRow("ntlm-with-type{2|3}") << QByteArray("NTLM VFlQRV8yX09SXzNfTUVTU0FHRQo=") << QByteArray("NTLM") << QByteArray("NTLM VFlQRV8yX09SXzNfTUVTU0FHRQo=");

    // Unknown schemes always return blank, i.e. auth request should be ignored
    QTest::newRow("unknown-param") << QByteArray("Newauth realm=\"newauth\"") << QByteArray() << QByteArray();
    QTest::newRow("unknown-unknown") << QByteArray("NewAuth , NewAuth2") << QByteArray() << QByteArray();
}

void HTTPAuthenticationTest::testAuthenticationSelection()
{
    QFETCH(QByteArray, input);
    QFETCH(QByteArray, expectedScheme);
    QFETCH(QByteArray, expectedOffer);

    QByteArray scheme, offer;
    parseAuthHeader(input, &offer, &scheme, 0);
    QCOMPARE(scheme, expectedScheme);
    QCOMPARE(offer, expectedOffer);
}

void HTTPAuthenticationTest::testAuthentication_data()
{
    QTest::addColumn<QByteArray>("input");
    QTest::addColumn<QByteArray>("expectedResponse");
    QTest::addColumn<QByteArray>("user");
    QTest::addColumn<QByteArray>("pass");
    QTest::addColumn<QByteArray>("url");
    QTest::addColumn<QByteArray>("cnonce");

    // Test cases from  RFC 2617...
    QTest::newRow("rfc-2617-basic-example")
        << QByteArray("Basic realm=\"WallyWorld\"")
        << QByteArray("Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==")
        << QByteArray("Aladdin")
        << QByteArray("open sesame")
        << QByteArray()
        << QByteArray();
    QTest::newRow("rfc-2617-digest-example")
        << QByteArray("Digest realm=\"testrealm@host.com\", qop=\"auth,auth-int\", nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\", opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"")
        << QByteArray("Digest username=\"Mufasa\", realm=\"testrealm@host.com\", nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\", uri=\"/dir/index.html\", algorithm=MD5, qop=auth, cnonce=\"0a4f113b\", nc=00000001, response=\"6629fae49393a05397450978507c4ef1\", opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"")
        << QByteArray("Mufasa")
        << QByteArray("Circle Of Life")
        << QByteArray("http://www.nowhere.org/dir/index.html")
        << QByteArray("0a4f113b");
    QTest::newRow("ntlm-negotiate-type1")
        << QByteArray("NTLM")
        << QByteArray("NTLM TlRMTVNTUAABAAAABQIAAAAAAAAAAAAAAAAAAAAAAAA=")
        << QByteArray()
        << QByteArray()
        << QByteArray()
        << QByteArray();
    QTest::newRow("ntlm-challenge-type2")
        << QByteArray("NTLM TlRMTVNTUAACAAAAAAAAACgAAAABggAAU3J2Tm9uY2UAAAAAAAAAAA==")
        << QByteArray("NTLM TlRMTVNTUAADAAAAGAAYAFgAAAAYABgAQAAAAAAAAAAAAAAAAAAAAHAAAAAWABYAcAAAAAAAAAAAAAAAAYIAAJSg10BK9h+dU9d6Ijn04m4iDZHzFECXU3sG2ZrxJPWBGnO3BnTKK13Ku1qYqpcE6VcATwBSAEsAUwBUAEEAVABJAE8ATgA=")
        << QByteArray("Ursa-Minor\\Zaphod")
        << QByteArray("Beeblebrox")
        << QByteArray()
        << QByteArray();
}

void HTTPAuthenticationTest::testAuthentication()
{
    QFETCH(QByteArray, input);
    QFETCH(QByteArray, expectedResponse);
    QFETCH(QByteArray, user);
    QFETCH(QByteArray, pass);
    QFETCH(QByteArray, url);
    QFETCH(QByteArray, cnonce);

    QByteArray bestOffer;
    parseAuthHeader(input, &bestOffer, 0, 0);
    KAbstractHttpAuthentication* authObj = KAbstractHttpAuthentication::newAuth(bestOffer);
    QVERIFY(authObj);
    if (!cnonce.isEmpty())
        authObj->setDigestNonceValue(cnonce);
    authObj->setChallenge(bestOffer, QUrl(url), "GET");
    authObj->generateResponse(QString(user), QString(pass));
    QCOMPARE(authObj->headerFragment().trimmed().constData(), expectedResponse.constData());
    delete authObj;
}
