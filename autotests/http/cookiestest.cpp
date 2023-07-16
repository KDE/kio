/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class CookiesTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testReceiveCookies();
    void testReceiveCookies_data();
    void testSendCookies();
    void testSendCookies_data();
};

void CookiesTest::testReceiveCookies_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QString>("expectedCookieString");

    QTest::addRow("none") << "http://localhost:5000/cookies/none"
                          << ""
                          << "";

    QTest::addRow("one") << "http://localhost:5000/cookies/somecookie"
                         << ""
                         << "";
    QTest::addRow("two") << "http://localhost:5000/cookies/twocookies"
                         << ""
                         << "";

    QTest::addRow("none_disabled") << "http://localhost:5000/cookies/none"
                                   << "none"
                                   << "";

    QTest::addRow("one_disabled") << "http://localhost:5000/cookies/somecookie"
                                  << "none"
                                  << "";
    QTest::addRow("two_disabled") << "http://localhost:5000/cookies/twocookies"
                                  << "none"
                                  << "";

    QTest::addRow("none_manual") << "http://localhost:5000/cookies/none"
                                 << "manual"
                                 << "";

    QTest::addRow("one_manual") << "http://localhost:5000/cookies/somecookie"
                                << "manual"
                                << "Set-Cookie: userID=1234; Domain=localhost; Expires=Sat, 13 May 2045 18:52:00 GMT; HttpOnly; Path=/get/calendar\n";
    QTest::addRow("two_manual")
        << "http://localhost:5000/cookies/twocookies"
        << "manual"
        << "Set-Cookie: userID=1234; Domain=localhost; Expires=Sat, 13 May 2045 18:52:00 GMT; HttpOnly; Path=/get/calendar\nSet-Cookie: "
           "konqi=Yo; Domain=localhost; Expires=Sat, 13 May 2045 18:52:00 GMT; HttpOnly; Path=/get/text\n";
}

void CookiesTest::testReceiveCookies()
{
    QFETCH(QString, url);
    QFETCH(QString, mode);
    QFETCH(QString, expectedCookieString);

    auto *job = KIO::get(QUrl(url), KIO::NoReload, KIO::HideProgressInfo);
    job->addMetaData("cookies", mode);

    QSignalSpy spy(job, &KJob::finished);
    spy.wait();
    QVERIFY(spy.size());
    QCOMPARE(job->error(), KJob::NoError);

    auto cookiesFromString = [](const QString &input) -> std::optional<QList<QNetworkCookie>> {
        const QStringList splittedCookiesStrings = input.split('\n', Qt::SkipEmptyParts);

        QList<QNetworkCookie> result;

        if (input.isEmpty()) {
            return result;
        }

        for (const QString &cookieStringWithPrefix : splittedCookiesStrings) {
            if (!cookieStringWithPrefix.startsWith("Set-Cookie: ")) {
                return {};
            }

            const QString cookieString = cookieStringWithPrefix.mid(12).replace("-", " ");
            const auto parsedCookies = QNetworkCookie::parseCookies(cookieString.toUtf8());

            if (!parsedCookies.isEmpty()) {
                result << parsedCookies.first();
            }
        }

        return result;
    };

    const auto expectedCookies = cookiesFromString(expectedCookieString);

    const QString receivedCookieString = job->queryMetaData("setcookies");
    const auto receivedCookies = cookiesFromString(receivedCookieString);

    QCOMPARE(receivedCookies.value(), expectedCookies.value());
}

void CookiesTest::testSendCookies_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QString>("inputCookies");
    QTest::addColumn<QByteArray>("expectedData");

    QTest::addRow("none") << "http://localhost:5000/cookies/showsent"
                          << ""
                          << "" << QByteArray();

    QTest::addRow("one") << "http://localhost:5000/cookies/showsent"
                         << ""
                         << "Cookie: tasty_cookie=strawberry" << QByteArray();

    QTest::addRow("two") << "http://localhost:5000/cookies/showsent"
                         << ""
                         << "Cookie: tasty_cookie=strawberry;cake=cheesecake" << QByteArray();

    QTest::addRow("none_disabled") << "http://localhost:5000/cookies/showsent"
                                   << "none"
                                   << "" << QByteArray();

    QTest::addRow("one_disabled") << "http://localhost:5000/cookies/showsent"
                                  << "none"
                                  << "Cookie: tasty_cookie=strawberry" << QByteArray();

    QTest::addRow("two_disabled") << "http://localhost:5000/cookies/showsent"
                                  << "none"
                                  << "Cookie: tasty_cookie=strawberry;cake=cheesecake" << QByteArray();

    QTest::addRow("none_manual") << "http://localhost:5000/cookies/showsent"
                                 << "manual"
                                 << "" << QByteArray();

    QTest::addRow("one_manual") << "http://localhost:5000/cookies/showsent"
                                << "manual"
                                << "Cookie: tasty_cookie=strawberry" << QByteArray("tasty_cookie:strawberry\n");

    QTest::addRow("two_manual") << "http://localhost:5000/cookies/showsent"
                                << "manual"
                                << "Cookie: tasty_cookie=strawberry;cake=cheesecake" << QByteArray("tasty_cookie:strawberry\ncake:cheesecake\n");
}

void CookiesTest::testSendCookies()
{
    QFETCH(QString, url);
    QFETCH(QString, mode);
    QFETCH(QString, inputCookies);
    QFETCH(QByteArray, expectedData);

    auto *job = KIO::get(QUrl(url), KIO::NoReload, KIO::HideProgressInfo);
    job->addMetaData("cookies", mode);
    job->addMetaData("setcookies", inputCookies);

    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy spy(job, &KJob::finished);
    spy.wait();
    QVERIFY(spy.size());
    QCOMPARE(job->error(), KJob::NoError);

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, expectedData);
}

QTEST_GUILESS_MAIN(CookiesTest)

#include "cookiestest.moc"
