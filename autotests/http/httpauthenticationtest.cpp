/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2011 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "httpauthenticationtest.h"

#include <QTest>

#include <QList>
#include <QByteArray>
#include <QtEndian>

#include <KConfigCore/KConfig>

#define ENABLE_HTTP_AUTH_NONCE_SETTER
#include "httpauthentication.cpp"

// QT5 TODO QTEST_GUILESS_MAIN(HTTPAuthenticationTest)
QTEST_MAIN(HTTPAuthenticationTest)

static void parseAuthHeader(const QByteArray &header,
                            QByteArray *bestOffer,
                            QByteArray *scheme,
                            QList<QByteArray> *result)
{
    const QList<QByteArray> authHeaders = KAbstractHttpAuthentication::splitOffers(QList<QByteArray>{header});
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

static QByteArray hmacMD5(const QByteArray &data, const QByteArray &key)
{
    QByteArray ipad(64, 0x36);
    QByteArray opad(64, 0x5c);

    Q_ASSERT(key.size() <= 64);

    for (int i = qMin(key.size(), 64) - 1; i >= 0; i--) {
        ipad.data()[i] ^= key[i];
        opad.data()[i] ^= key[i];
    }

    QByteArray content(ipad + data);

    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(content);
    content = opad + md5.result();

    md5.reset();
    md5.addData(content);

    return md5.result();
}

static QByteArray QString2UnicodeLE(const QString &target)
{
    QByteArray unicode(target.length() * 2, 0);

    for (int i = 0; i < target.length(); i++) {
        ((quint16 *) unicode.data()) [ i ] = qToLittleEndian(target[i].unicode());
    }

    return unicode;
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

QByteArray joinQByteArray(const QList<QByteArray> &list)
{
    QByteArray data;
    const int count = list.count();

    for (int i = 0; i < count; ++i) {
        if (i > 0) {
            data += ',';
        }
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
    QList<QByteArray> parsingResult;
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
    parseAuthHeader(input, &offer, &scheme, nullptr);
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
            << QByteArray("NTLM TlRMTVNTUAACAAAAFAAUACgAAAABggAAU3J2Tm9uY2UAAAAAAAAAAFUAcgBzAGEALQBNAGEAagBvAHIA")
            << QByteArray("NTLM TlRMTVNTUAADAAAAGAAYAFgAAAAYABgAQAAAABQAFABwAAAADAAMAIQAAAAWABYAkAAAAAAAAAAAAAAAAYIAAODgDeMQShvyBT8Hx92oLTxImumJ4bAA062Hym3v40aFucQ8R3qMQtYAZn1okufol1UAcgBzAGEALQBNAGkAbgBvAHIAWgBhAHAAaABvAGQAVwBPAFIASwBTAFQAQQBUAEkATwBOAA==")
            << QByteArray("Ursa-Minor\\Zaphod")
            << QByteArray("Beeblebrox")
            << QByteArray()
            << QByteArray();
    QTest::newRow("ntlm-challenge-type2-no-domain")
            << QByteArray("NTLM TlRMTVNTUAACAAAAFAAUACgAAAABggAAU3J2Tm9uY2UAAAAAAAAAAFUAcgBzAGEALQBNAGEAagBvAHIA")
            << QByteArray("NTLM TlRMTVNTUAADAAAAGAAYAFgAAAAYABgAQAAAABQAFABwAAAADAAMAIQAAAAWABYAkAAAAAAAAAAAAAAAAYIAAODgDeMQShvyBT8Hx92oLTxImumJ4bAA062Hym3v40aFucQ8R3qMQtYAZn1okufol1UAcgBzAGEALQBNAGEAagBvAHIAWgBhAHAAaABvAGQAVwBPAFIASwBTAFQAQQBUAEkATwBOAA==")
            << QByteArray("Zaphod")
            << QByteArray("Beeblebrox")
            << QByteArray()
            << QByteArray();
    QTest::newRow("ntlm-challenge-type2-empty-domain")
            << QByteArray("NTLM TlRMTVNTUAACAAAAFAAUACgAAAABggAAU3J2Tm9uY2UAAAAAAAAAAFUAcgBzAGEALQBNAGEAagBvAHIA")
            << QByteArray("NTLM TlRMTVNTUAADAAAAGAAYAFgAAAAYABgAQAAAAAAAAAAAAAAADAAMAHAAAAAWABYAfAAAAAAAAAAAAAAAAYIAAODgDeMQShvyBT8Hx92oLTxImumJ4bAA062Hym3v40aFucQ8R3qMQtYAZn1okufol1oAYQBwAGgAbwBkAFcATwBSAEsAUwBUAEEAVABJAE8ATgA=")
            << QByteArray("\\Zaphod")
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
    parseAuthHeader(input, &bestOffer, nullptr, nullptr);
    KAbstractHttpAuthentication *authObj = KAbstractHttpAuthentication::newAuth(bestOffer);
    QVERIFY(authObj);
    if (!cnonce.isEmpty()) {
        authObj->setDigestNonceValue(cnonce);
    }
    authObj->setChallenge(bestOffer, QUrl(url), "GET");
    authObj->generateResponse(QString(user), QString(pass));
    QCOMPARE(authObj->headerFragment().trimmed().constData(), expectedResponse.constData());
    delete authObj;
}

void HTTPAuthenticationTest::testAuthenticationNTLMv2()
{
    QByteArray input("NTLM TlRMTVNTUAACAAAABgAGADgAAAAFAokCT0wyUnb4OSQAAAAAAAAAAMYAxgA+AAAABgGxHQAAAA9UAFMAVAACAAYAVABTAFQAAQASAEQAVgBHAFIASwBWAFEAUABEAAQAKgB0AHMAdAAuAGQAagBrAGgAcQBjAGkAaABtAGMAbwBmAGoALgBvAHIAZwADAD4ARABWAEcAUgBLAFYAUQBQAEQALgB0AHMAdAAuAGQAagBrAGgAcQBjAGkAaABtAGMAbwBmAGoALgBvAHIAZwAFACIAZABqAGsAaABxAGMAaQBoAG0AYwBvAGYAagAuAG8AcgBnAAcACABvb9jXZl7RAQAAAAA=");
    QByteArray expectedResponse("TlRMTVNTUAADAAAAGAAYADYBAAD2APYAQAAAAAYABgBOAQAABgAGAFQBAAAWABYAWgEAAAAAAAAAAAAABQKJArXyhsxZPveKcfcV21viIsUBAQAAAAAAAAC8GQxfX9EBTHOi1kJbHbQAAAAAAgAGAFQAUwBUAAEAEgBEAFYARwBSAEsAVgBRAFAARAAEACoAdABzAHQALgBkAGoAawBoAHEAYwBpAGgAbQBjAG8AZgBqAC4AbwByAGcAAwA+AEQAVgBHAFIASwBWAFEAUABEAC4AdABzAHQALgBkAGoAawBoAHEAYwBpAGgAbQBjAG8AZgBqAC4AbwByAGcABQAiAGQAagBrAGgAcQBjAGkAaABtAGMAbwBmAGoALgBvAHIAZwAHAAgAb2/Y12Ze0QEAAAAAAAAAAOInN0N/15GHBtz3WXvvV159KG/2MbYk0FQAUwBUAGIAbwBiAFcATwBSAEsAUwBUAEEAVABJAE8ATgA=");
    QString user("TST\\bob");
    QString pass("cacamas");
    QString target("TST");

    QByteArray bestOffer;
    parseAuthHeader(input, &bestOffer, nullptr, nullptr);
    KConfig conf;
    KConfigGroup confGroup = conf.group("test");
    confGroup.writeEntry("EnableNTLMv2Auth", true);
    KAbstractHttpAuthentication *authObj = KAbstractHttpAuthentication::newAuth(bestOffer, &confGroup);
    QVERIFY(authObj);

    authObj->setChallenge(bestOffer, QUrl(), "GET");
    authObj->generateResponse(QString(user), QString(pass));

    QByteArray resp(QByteArray::fromBase64(authObj->headerFragment().trimmed().mid(5)));
    QByteArray expResp(QByteArray::fromBase64(expectedResponse));

    /* Prepare responses stripped from any data that is variable. */
    QByteArray strippedResp(resp);
    memset(strippedResp.data() + 0x40, 0, 0x10); // NTLMv2 MAC
    memset(strippedResp.data() + 0x58, 0, 0x10); // timestamp + client nonce
    memset(strippedResp.data() + 0x136, 0, 0x18); // LMv2 MAC
    QByteArray strippedExpResp(expResp);
    memset(strippedExpResp.data() + 0x40, 0, 0x10); // NTLMv2 MAC
    memset(strippedExpResp.data() + 0x58, 0, 0x10); // timestamp + client nonce
    memset(strippedExpResp.data() + 0x136, 0, 0x18); // LMv2 MAC

    /* Compare the stripped responses. */
    QCOMPARE(strippedResp.toBase64(), strippedExpResp.toBase64());

    /* Verify the NTLMv2 response MAC. */
    QByteArray challenge(QByteArray::fromBase64(input.mid(5)));
    QByteArray serverNonce(challenge.mid(0x18, 8));

    QByteArray uniPass(QString2UnicodeLE(pass));
    QByteArray ntlmHash(QCryptographicHash::hash(uniPass, QCryptographicHash::Md4));
    int i = user.indexOf('\\');
    QString username;
    if (i >= 0) {
        username = user.mid(i + 1);
    }
    else {
        username = user;
    }

    QByteArray userTarget(QString2UnicodeLE(username.toUpper() + target));
    QByteArray ntlm2Hash(hmacMD5(userTarget, ntlmHash));
    QByteArray hashData(serverNonce + resp.mid(0x50, 230));
    QByteArray mac(hmacMD5(hashData, ntlm2Hash));

    QCOMPARE(mac.toHex(), resp.mid(0x40, 16).toHex());

    /* Verify the LMv2 response MAC. */
    QByteArray lmHashData(serverNonce + resp.mid(0x146, 8));
    QByteArray lmHash(hmacMD5(lmHashData, ntlm2Hash));

    QCOMPARE(lmHash.toHex(), resp.mid(0x136, 16).toHex());

    delete authObj;
}
