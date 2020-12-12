/*
    This file is part of KDE

    SPDX-FileCopyrightText: 2004 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2008 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only
*/

#include <QDate>
#include <QString>

#include <QTest>
#include <qplatformdefs.h>
#include <QStandardPaths>

#include "../../src/ioslaves/http/kcookiejar/kcookiejar.cpp"

static KCookieJar *jar;
static QString *lastYear;
static QString *nextYear;
static KConfig *config = nullptr;
static int windowId = 1234; // random number to be used as windowId for test cookies

#define FAIL(x) QFAIL(qPrintable(x))

static void popArg(QString &command, QString &line)
{
    int i = line.indexOf(' ');
    if (i != -1) {
        command = line.left(i);
        line = line.mid(i + 1);
    } else {
        command = line;
        line.clear();
    }
}

static void clearConfig()
{
    delete config;
    QString file = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1Char('/') + "kcookiejar-testconfig";
    QFile::remove(file);
    config = new KConfig(file);
    KConfigGroup cg(config, "Cookie Policy");
    cg.writeEntry("RejectCrossDomainCookies", false);
    cg.writeEntry("AcceptSessionCookies", false);
    cg.writeEntry("CookieGlobalAdvice", "Ask");
    jar->loadConfig(config, false);
}

static void clearCookies(bool sessionOnly = false)
{
    if (sessionOnly) {
        jar->eatSessionCookies(windowId);
    } else {
        jar->eatAllCookies();
    }
}

static void saveCookies()
{
    QString file = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1Char('/') + "kcookiejar-testcookies";
    QFile::remove(file);
    jar->saveCookies(file);

    // Add an empty domain to the cookies file, just for testing robustness
    QFile f(file);
    f.open(QIODevice::Append);
    f.write("[]\n   \"\"   \"/\"    1584320400  0 h  4  x\n");
    f.close();

    delete jar;
    jar = new KCookieJar();
    clearConfig();
    jar->loadCookies(file);
}

static void endSession()
{
    jar->eatSessionCookies(windowId);
}

static void processCookie(QString &line)
{
    QString policy;
    popArg(policy, line);
    KCookieAdvice expectedAdvice = KCookieJar::strToAdvice(policy);
    if (expectedAdvice == KCookieDunno) {
        FAIL(QStringLiteral("Unknown accept policy '%1'").arg(policy));
    }

    QString urlStr;
    popArg(urlStr, line);
    QUrl url(urlStr);
    if (!url.isValid()) {
        FAIL(QStringLiteral("Invalid URL '%1'").arg(urlStr));
    }
    if (url.isEmpty()) {
        FAIL(QStringLiteral("Missing URL"));
    }

    line.replace(QLatin1String("%LASTYEAR%"), *lastYear);
    line.replace(QLatin1String("%NEXTYEAR%"), *nextYear);

    KHttpCookieList list = jar->makeCookies(urlStr, line.toUtf8(), windowId);

    if (list.isEmpty()) {
        FAIL(QStringLiteral("Failed to make cookies from: '%1'").arg(line));
    }

    for (KHttpCookieList::iterator cookieIterator = list.begin();
            cookieIterator != list.end(); ++cookieIterator) {
        KHttpCookie &cookie = *cookieIterator;
        const KCookieAdvice cookieAdvice = jar->cookieAdvice(cookie);
        if (cookieAdvice != expectedAdvice)
            FAIL(urlStr + QStringLiteral("\n'%2'\nGot advice '%3' expected '%4'")
                 .arg(line, KCookieJar::adviceToStr(cookieAdvice), KCookieJar::adviceToStr(expectedAdvice))
                );
        jar->addCookie(cookie);
    }
}

static void processCheck(QString &line)
{
    QString urlStr;
    popArg(urlStr, line);
    QUrl url(urlStr);
    if (!url.isValid()) {
        FAIL(QStringLiteral("Invalid URL '%1'").arg(urlStr));
    }
    if (url.isEmpty()) {
        FAIL(QStringLiteral("Missing URL"));
    }

    QString expectedCookies = line;

    QString cookies = jar->findCookies(urlStr, false, windowId, nullptr).trimmed();
    if (cookies != expectedCookies)
        FAIL(urlStr + QStringLiteral("\nGot '%1' expected '%2'")
             .arg(cookies, expectedCookies));
}

static void processClear(QString &line)
{
    if (line == QLatin1String("CONFIG")) {
        clearConfig();
    } else if (line == QLatin1String("COOKIES")) {
        clearCookies();
    } else if (line == QLatin1String("SESSIONCOOKIES")) {
        clearCookies(true);
    } else {
        FAIL(QStringLiteral("Unknown command 'CLEAR %1'").arg(line));
    }
}

static void processConfig(QString &line)
{
    QString key;
    popArg(key, line);

    if (key.isEmpty()) {
        FAIL(QStringLiteral("Missing Key"));
    }

    KConfigGroup cg(config, "Cookie Policy");
    cg.writeEntry(key, line);
    jar->loadConfig(config, false);
}

static void processLine(QString line)
{
    if (line.isEmpty()) {
        return;
    }

    if (line[0] == '#') {
        if (line[1] == '#') {
            qDebug().noquote() << line;
        }
        return;
    }

    QString command;
    popArg(command, line);
    if (command.isEmpty()) {
        return;
    }

    if (command == QLatin1String("COOKIE")) {
        processCookie(line);
    } else if (command == QLatin1String("CHECK")) {
        processCheck(line);
    } else if (command == QLatin1String("CLEAR")) {
        processClear(line);
    } else if (command == QLatin1String("CONFIG")) {
        processConfig(line);
    } else if (command == QLatin1String("SAVE")) {
        saveCookies();
    } else if (command == QLatin1String("ENDSESSION")) {
        endSession();
    } else {
        FAIL(QStringLiteral("Unknown command '%1'").arg(command));
    }
}

class KCookieJarTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        jar = new KCookieJar;
        QDateTime dt = QDateTime::currentDateTime();
        lastYear = new QString(dt.addYears(-1).toString(Qt::RFC2822Date));
        nextYear = new QString(dt.addYears(1).toString(Qt::RFC2822Date));
    }
    void testCookieFile_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::newRow("cookie.test") << QFINDTESTDATA("cookie.test");
        QTest::newRow("cookie_rfc.test") << QFINDTESTDATA("cookie_rfc.test");
        QTest::newRow("cookie_saving.test") << QFINDTESTDATA("cookie_saving.test");
        QTest::newRow("cookie_settings.test") << QFINDTESTDATA("cookie_settings.test");
        QTest::newRow("cookie_session.test") << QFINDTESTDATA("cookie_session.test");
    }
    void testCookieFile()
    {
        QFETCH(QString, fileName);
        clearConfig();

        QFile file(fileName);
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(fileName));
        while (!file.atEnd()) {
            const QByteArray buf = file.readLine().chopped(1);
            processLine(QString::fromUtf8(buf));
        }
    }

    void testParseUrl_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<bool>("expectedResult");
        QTest::addColumn<QString>("expectedFqdn");
        QTest::addColumn<QString>("expectedPath");
        QTest::newRow("empty") << "" << false << "" << "";
        QTest::newRow("url with no path") << "http://bugs.kde.org" << true << "bugs.kde.org" << "/";
        QTest::newRow("url with path") << "http://bugs.kde.org/foo" << true << "bugs.kde.org" << "/foo";
        QTest::newRow("just a host") << "bugs.kde.org" << false << "" << "";
    }
    void testParseUrl()
    {
        QFETCH(QString, url);
        QFETCH(bool, expectedResult);
        QFETCH(QString, expectedFqdn);
        QFETCH(QString, expectedPath);
        QString fqdn;
        QString path;
        bool result = KCookieJar::parseUrl(url, fqdn, path);
        QCOMPARE(result, expectedResult);
        QCOMPARE(fqdn, expectedFqdn);
        QCOMPARE(path, expectedPath);
    }

    void testExtractDomains_data()
    {
        QTest::addColumn<QString>("fqdn");
        QTest::addColumn<QStringList>("expectedDomains");
        QTest::newRow("empty") << "" << (QStringList() << QStringLiteral("localhost"));
        QTest::newRow("ipv4") << "1.2.3.4" << (QStringList() << QStringLiteral("1.2.3.4"));
        QTest::newRow("ipv6") << "[fe80::213:d3ff:fef4:8c92]" << (QStringList() << QStringLiteral("[fe80::213:d3ff:fef4:8c92]"));
        QTest::newRow("bugs.kde.org") << "bugs.kde.org" << (QStringList() << QStringLiteral("bugs.kde.org") << QStringLiteral(".bugs.kde.org") << QStringLiteral("kde.org") << QStringLiteral(".kde.org"));

    }
    void testExtractDomains()
    {
        QFETCH(QString, fqdn);
        QFETCH(QStringList, expectedDomains);
        KCookieJar jar;
        QStringList lst;
        jar.extractDomains(fqdn, lst);
        QCOMPARE(lst, expectedDomains);
    }
};

QTEST_MAIN(KCookieJarTest)

#include "kcookiejartest.moc"
