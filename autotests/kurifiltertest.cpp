/*
    SPDX-FileCopyrightText: 2002, 2003 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kurifiltertest.h"

#include <KUriFilter>
#include <KSharedConfig>
#include <KConfigGroup>
#include <QLoggingCategory>

#include <QtTestWidgets>
#include <QDir>
#include <QRegularExpression>
#include <QHostInfo>

#include <iostream>

QTEST_MAIN(KUriFilterTest)

static const char *const s_uritypes[] = { "NetProtocol", "LOCAL_FILE", "LOCAL_DIR", "EXECUTABLE", "HELP", "SHELL", "BLOCKED", "ERROR", "UNKNOWN" };
#define NO_FILTERING -2

static void setupColumns()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expectedResult");
    QTest::addColumn<int>("expectedUriType");
    QTest::addColumn<QStringList>("list");
    QTest::addColumn<QString>("absPath");
    QTest::addColumn<bool>("checkForExecutables");
}

static void addRow(const char *input, const QString &expectedResult = QString(), int expectedUriType = -1, const QStringList &list = QStringList(), const QString &absPath = QString(), bool checkForExecutables = true)
{
    QTest::newRow(input) << input << expectedResult << expectedUriType << list << absPath << checkForExecutables;
}

static void runFilterTest(const QString &a, const QString &expectedResult = nullptr, int expectedUriType = -1, const QStringList &list = QStringList(), const QString &absPath = nullptr, bool checkForExecutables = true)
{
    KUriFilterData *filterData = new KUriFilterData;
    filterData->setData(a);
    filterData->setCheckForExecutables(checkForExecutables);

    if (!absPath.isEmpty()) {
        filterData->setAbsolutePath(absPath);
        qDebug() << "Filtering: " << a << " with absPath=" << absPath;
    } else {
        qDebug() << "Filtering: " << a;
    }

    if (KUriFilter::self()->filterUri(*filterData, list)) {
        if (expectedUriType == NO_FILTERING) {
            qCritical() << a << "Did not expect filtering. Got" << filterData->uri();
            QVERIFY(expectedUriType != NO_FILTERING);   // fail the test
        }

        // Copied from minicli...
        QString cmd;
        QUrl uri = filterData->uri();

        if (uri.isLocalFile() && !uri.hasFragment() && !uri.hasQuery() &&
                (filterData->uriType() != KUriFilterData::NetProtocol)) {
            cmd = uri.toLocalFile();
        } else {
            cmd = uri.url(QUrl::FullyEncoded);
        }

        switch (filterData->uriType()) {
        case KUriFilterData::LocalFile:
        case KUriFilterData::LocalDir:
            qDebug() << "*** Result: Local Resource =>  '"
                     << filterData->uri().toLocalFile() << "'";
            break;
        case KUriFilterData::Help:
            qDebug() << "*** Result: Local Resource =>  '"
                     << filterData->uri().url() << "'";
            break;
        case KUriFilterData::NetProtocol:
            qDebug() << "*** Result: Network Resource => '"
                     << filterData->uri().url() << "'";
            break;
        case KUriFilterData::Shell:
        case KUriFilterData::Executable:
            if (filterData->hasArgsAndOptions()) {
                cmd += filterData->argsAndOptions();
            }
            qDebug() << "*** Result: Executable/Shell => '" << cmd << "'";
            break;
        case KUriFilterData::Error:
            qDebug() << "*** Result: Encountered error => '" << cmd << "'";
            qDebug() << "Reason:" << filterData->errorMsg();
            break;
        default:
            qDebug() << "*** Result: Unknown or invalid resource.";
        }

        if (!expectedResult.isEmpty()) {
            // Hack for other locales than english, normalize google hosts to google.com
            cmd.replace(QRegularExpression(QStringLiteral("www\\.google\\.[^/]*/")), QStringLiteral("www.google.com/"));
            if (cmd != expectedResult) {
                qWarning() << a;
                QCOMPARE(cmd, expectedResult);
            }
        }

        if (expectedUriType != -1 && expectedUriType != filterData->uriType()) {
            qWarning() << a << "Got URI type" << s_uritypes[filterData->uriType()]
                       << "expected" << s_uritypes[expectedUriType];
            QCOMPARE(s_uritypes[filterData->uriType()],
                     s_uritypes[expectedUriType]);
        }
    } else {
        if (expectedUriType == NO_FILTERING) {
            qDebug() << "*** No filtering required.";
        } else {
            qDebug() << "*** Could not be filtered.";
            if (expectedUriType != filterData->uriType()) {
                QCOMPARE(s_uritypes[filterData->uriType()],
                         s_uritypes[expectedUriType]);
            }
        }
    }

    delete filterData;
    qDebug() << "-----";
}

static void runFilterTest()
{
    QFETCH(QString, input);
    QFETCH(QString, expectedResult);
    QFETCH(int, expectedUriType);
    QFETCH(QStringList, list);
    QFETCH(QString, absPath);
    QFETCH(bool, checkForExecutables);
    runFilterTest(input, expectedResult, expectedUriType, list, absPath, checkForExecutables);
}

static void testLocalFile(const QString &filename)
{
    QFile tmpFile(filename);   // Yeah, I know, security risk blah blah. This is a test prog!

    if (tmpFile.open(QIODevice::ReadWrite)) {
        QByteArray fname = QFile::encodeName(tmpFile.fileName());
        runFilterTest(fname, fname, KUriFilterData::LocalFile);
        tmpFile.close();
        tmpFile.remove();
    } else {
        qDebug() << "Couldn't create " << tmpFile.fileName() << ", skipping test";
    }
}

static const char s_delimiter = WEBSHORTCUT_SEPARATOR;

void KUriFilterTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    minicliFilters << QStringLiteral("kshorturifilter") << QStringLiteral("kurisearchfilter") << QStringLiteral("localdomainurifilter");
    qtdir = qgetenv("QTDIR");
    home = qgetenv("HOME");
    qputenv("DATAHOME", QFile::encodeName(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)));
    datahome = qgetenv("DATAHOME");
    qDebug() << "libpaths" << QCoreApplication::libraryPaths();

    qputenv("KDE_FORK_SLAVES", "yes");   // simpler, for the final cleanup
    QLoggingCategory::setFilterRules(QStringLiteral("kf.kio.urifilters.*=true"));

    QString searchProvidersDir = QFINDTESTDATA("../src/urifilters/ikws/searchproviders/google.desktop").section('/', 0, -2);
    QVERIFY(!searchProvidersDir.isEmpty());
    qputenv("KIO_SEARCHPROVIDERS_DIR", QFile::encodeName(searchProvidersDir));

    // Many tests check the "default search engine" feature.
    // There is no default search engine by default (since it was annoying when making typos),
    // so the user has to set it up, which we do here.
    {
        KConfigGroup cfg(KSharedConfig::openConfig(QStringLiteral("kuriikwsfilterrc"), KConfig::SimpleConfig), "General");
        cfg.writeEntry("DefaultWebShortcut", "google");
        cfg.writeEntry("KeywordDelimiter", QString(s_delimiter));
        cfg.sync();
    }

    // Copy kshorturifilterrc from the src dir so we don't depend on make install / env vars.
    {
        const QString rcFile = QFINDTESTDATA("../src/urifilters/shorturi/kshorturifilterrc");
        QVERIFY(!rcFile.isEmpty());
        const QString localFile = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/kshorturifilterrc";
        QFile::remove(localFile);
        QVERIFY(QFile(rcFile).copy(localFile));
    }

    QDir().mkpath(datahome + QStringLiteral("/urifilter"));
}

void KUriFilterTest::pluginNames()
{
    const QStringList plugins = KUriFilter::self()->pluginNames();
    qDebug() << plugins;
    const QByteArray debugString = plugins.join(',').toLatin1();
    // To make it possible to have external plugins (if there's any...)
    // we don't just have an expected result list, we just probe it for specific entries.
    QVERIFY2(plugins.contains("kshorturifilter"), debugString.constData());
    QVERIFY2(plugins.contains("kurisearchfilter"), debugString.constData());
    QVERIFY2(plugins.contains("localdomainurifilter"), debugString.constData());
    QVERIFY2(plugins.contains("fixhosturifilter"), debugString.constData());
    QVERIFY2(plugins.contains("kuriikwsfilter"), debugString.constData());
    // No duplicates
    QCOMPARE(plugins.count("kshorturifilter"), 1);
}

void KUriFilterTest::noFiltering_data()
{
    setupColumns();
    // URI that should require no filtering
    addRow("http://www.kde.org", QStringLiteral("http://www.kde.org"), KUriFilterData::NetProtocol);
    // qtbase commit eaf4438b3511c preserves the double slashes
    addRow("http://www.kde.org/developer//index.html", QStringLiteral("http://www.kde.org/developer//index.html"), KUriFilterData::NetProtocol);
    addRow("file:///", QStringLiteral("/"), KUriFilterData::LocalDir);
    addRow("file:///etc", QStringLiteral("/etc"), KUriFilterData::LocalDir);
    addRow("file:///etc/passwd", QStringLiteral("/etc/passwd"), KUriFilterData::LocalFile);
}

void KUriFilterTest::noFiltering()
{
    runFilterTest();
}

void KUriFilterTest::localFiles_data()
{
    setupColumns();
    addRow("/", QStringLiteral("/"), KUriFilterData::LocalDir);
    addRow("/", QStringLiteral("/"), KUriFilterData::LocalDir, QStringList(QStringLiteral("kshorturifilter")));
    addRow("//", QStringLiteral("/"), KUriFilterData::LocalDir);
    addRow("///", QStringLiteral("/"), KUriFilterData::LocalDir);
    addRow("////", QStringLiteral("/"), KUriFilterData::LocalDir);
    addRow("///tmp", QStringLiteral("/tmp"), KUriFilterData::LocalDir);
    addRow("///tmp/", QStringLiteral("/tmp/"), KUriFilterData::LocalDir);
    addRow("///tmp//", QStringLiteral("/tmp/"), KUriFilterData::LocalDir);
    addRow("///tmp///", QStringLiteral("/tmp/"), KUriFilterData::LocalDir);

    if (QFile::exists(QDir::homePath() + QLatin1String("/.bashrc"))) {
        addRow("~/.bashrc", QDir::homePath() + QStringLiteral("/.bashrc"), KUriFilterData::LocalFile, QStringList(QStringLiteral("kshorturifilter")));
    }
    addRow("~", QDir::homePath(), KUriFilterData::LocalDir, QStringList(QStringLiteral("kshorturifilter")), QStringLiteral("/tmp"));
    addRow("~bin", nullptr, KUriFilterData::LocalDir, QStringList(QStringLiteral("kshorturifilter")));
    addRow("~does_not_exist", nullptr, KUriFilterData::Error, QStringList(QStringLiteral("kshorturifilter")));
    addRow("~/does_not_exist", QDir::homePath() + "/does_not_exist", KUriFilterData::LocalFile, QStringList(QStringLiteral("kshorturifilter")));

    // Absolute Path tests for kshorturifilter
    const QStringList kshorturifilter(QStringLiteral("kshorturifilter"));
    addRow("./", datahome, KUriFilterData::LocalDir, kshorturifilter, datahome + QStringLiteral("/")); // cleanPath removes the trailing slash
    const QString parentDir = QDir().cleanPath(datahome + QStringLiteral("/.."));
    addRow("../", QFile::encodeName(parentDir), KUriFilterData::LocalDir, kshorturifilter, datahome);
    addRow("share", datahome, KUriFilterData::LocalDir, kshorturifilter, QFile::encodeName(parentDir));
    // Invalid URLs
    addRow("http://a[b]", QStringLiteral("http://a[b]"), KUriFilterData::Unknown, kshorturifilter, QStringLiteral("/"));
}

void KUriFilterTest::localFiles()
{
    runFilterTest();
}

void KUriFilterTest::refOrQuery_data()
{
    setupColumns();
    // URL with reference
    addRow("http://www.kde.org/index.html#q8", QStringLiteral("http://www.kde.org/index.html#q8"), KUriFilterData::NetProtocol);
    // local file with reference
    addRow("file:/etc/passwd#q8", QStringLiteral("file:///etc/passwd#q8"), KUriFilterData::LocalFile);
    addRow("file:///etc/passwd#q8", QStringLiteral("file:///etc/passwd#q8"), KUriFilterData::LocalFile);
    addRow("/etc/passwd#q8", QStringLiteral("file:///etc/passwd#q8"), KUriFilterData::LocalFile);
    // local file with query (can be used by javascript)
    addRow("file:/etc/passwd?foo=bar", QStringLiteral("file:///etc/passwd?foo=bar"), KUriFilterData::LocalFile);
    testLocalFile(QStringLiteral("/tmp/kurifiltertest?foo"));   // local file with ? in the name (#58990)
    testLocalFile(QStringLiteral("/tmp/kurlfiltertest#foo"));   // local file with '#' in the name
    testLocalFile(QStringLiteral("/tmp/kurlfiltertest#foo?bar"));   // local file with both
    testLocalFile(QStringLiteral("/tmp/kurlfiltertest?foo#bar"));   // local file with both, the other way round
}

void KUriFilterTest::refOrQuery()
{
    runFilterTest();
}

void KUriFilterTest::shortUris_data()
{
    setupColumns();
    // hostnames are lowercased by QUrl
    // qtbase commit eaf4438b3511c preserves the double slashes
    addRow("http://www.myDomain.commyPort/ViewObjectRes//Default:name=hello",
           QStringLiteral("http://www.mydomain.commyport/ViewObjectRes//Default:name=hello"), KUriFilterData::NetProtocol);
    addRow("http://www.myDomain.commyPort/ViewObjectRes/Default:name=hello?a=a///////",
           QStringLiteral("http://www.mydomain.commyport/ViewObjectRes/Default:name=hello?a=a///////"), KUriFilterData::NetProtocol);
    addRow("ftp://ftp.kde.org", QStringLiteral("ftp://ftp.kde.org"), KUriFilterData::NetProtocol);
    addRow("ftp://username@ftp.kde.org:500", QStringLiteral("ftp://username@ftp.kde.org:500"), KUriFilterData::NetProtocol);

    // ShortURI/LocalDomain filter tests.
    addRow("linuxtoday.com", QStringLiteral("http://linuxtoday.com"), KUriFilterData::NetProtocol);
    addRow("LINUXTODAY.COM", QStringLiteral("http://linuxtoday.com"), KUriFilterData::NetProtocol);
    addRow("kde.org", QStringLiteral("http://kde.org"), KUriFilterData::NetProtocol);
    addRow("ftp.kde.org", QStringLiteral("ftp://ftp.kde.org"), KUriFilterData::NetProtocol);
    addRow("ftp.kde.org:21", QStringLiteral("ftp://ftp.kde.org:21"), KUriFilterData::NetProtocol);
    addRow("cr.yp.to", QStringLiteral("http://cr.yp.to"), KUriFilterData::NetProtocol);
    addRow("www.kde.org:21", QStringLiteral("http://www.kde.org:21"), KUriFilterData::NetProtocol);
    // This one passes but the DNS lookup takes 5 seconds to fail
    //addRow("foobar.local:8000", QStringLiteral("http://foobar.local:8000"), KUriFilterData::NetProtocol);
    addRow("foo@bar.com", QStringLiteral("mailto:foo@bar.com"), KUriFilterData::NetProtocol);
    addRow("firstname.lastname@x.foo.bar", QStringLiteral("mailto:firstname.lastname@x.foo.bar"), KUriFilterData::NetProtocol);
    addRow("mailto:foo@bar.com", QStringLiteral("mailto:foo@bar.com"), KUriFilterData::NetProtocol);
    addRow("www.123.foo", QStringLiteral("http://www.123.foo"), KUriFilterData::NetProtocol);
    addRow("user@www.123.foo:3128", QStringLiteral("http://user@www.123.foo:3128"), KUriFilterData::NetProtocol);
    addRow("ftp://user@user@www.123.foo:3128", QStringLiteral("ftp://user%40user@www.123.foo:3128"), KUriFilterData::NetProtocol);
    addRow("user@user@www.123.foo:3128", QStringLiteral("http://user%40user@www.123.foo:3128"), KUriFilterData::NetProtocol);

    // IPv4 address formats...
    addRow("user@192.168.1.0:3128", QStringLiteral("http://user@192.168.1.0:3128"), KUriFilterData::NetProtocol);
    addRow("127.0.0.1", QStringLiteral("http://127.0.0.1"), KUriFilterData::NetProtocol);
    addRow("127.0.0.1:3128", QStringLiteral("http://127.0.0.1:3128"), KUriFilterData::NetProtocol);
    addRow("127.1", QStringLiteral("http://127.0.0.1"), KUriFilterData::NetProtocol);   // Qt5: QUrl resolves to 127.0.0.1
    addRow("127.0.1", QStringLiteral("http://127.0.0.1"), KUriFilterData::NetProtocol);   // Qt5: QUrl resolves to 127.0.0.1

    // IPv6 address formats (taken from RFC 2732)...
    addRow("[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:80/index.html", QStringLiteral("http://[fedc:ba98:7654:3210:fedc:ba98:7654:3210]:80/index.html"), KUriFilterData::NetProtocol);
    addRow("[1080:0:0:0:8:800:200C:417A]/index.html", QStringLiteral("http://[1080::8:800:200c:417a]/index.html"), KUriFilterData::NetProtocol);  // Qt5 QUrl change
    addRow("[3ffe:2a00:100:7031::1]", QStringLiteral("http://[3ffe:2a00:100:7031::1]"), KUriFilterData::NetProtocol);
    addRow("[1080::8:800:200C:417A]/foo", QStringLiteral("http://[1080::8:800:200c:417a]/foo"), KUriFilterData::NetProtocol);
    addRow("[::192.9.5.5]/ipng", QStringLiteral("http://[::192.9.5.5]/ipng"), KUriFilterData::NetProtocol);
    addRow("[::FFFF:129.144.52.38]:80/index.html", QStringLiteral("http://[::ffff:129.144.52.38]:80/index.html"), KUriFilterData::NetProtocol);
    addRow("[2010:836B:4179::836B:4179]", QStringLiteral("http://[2010:836b:4179::836b:4179]"), KUriFilterData::NetProtocol);

    // Local domain filter - If you uncomment these test, make sure you
    // you adjust it based on the localhost entry in your /etc/hosts file.
    // addRow( "localhost:3128", "http://localhost.localdomain:3128", KUriFilterData::NetProtocol );
    // addRow( "localhost", "http://localhost.localdomain", KUriFilterData::NetProtocol );
    // addRow( "localhost/~blah", "http://localhost.localdomain/~blah", KUriFilterData::NetProtocol );

    addRow("user@host.domain", QStringLiteral("mailto:user@host.domain"), KUriFilterData::NetProtocol);   // new in KDE-3.2

    // Windows style SMB (UNC) URL. Should be converted into the valid smb format...
    addRow("\\\\mainserver\\share\\file", QStringLiteral("smb://mainserver/share/file"), KUriFilterData::NetProtocol);

    // KDE3: was not be filtered at all. All valid protocols of this form were be ignored.
    // KDE4: parsed as "network protocol", seems fine to me (DF)
    addRow("ftp:", QStringLiteral("ftp:"), KUriFilterData::NetProtocol);
    addRow("http:", QStringLiteral("http:"), KUriFilterData::NetProtocol);

    // The default search engine is set to 'Google'
    addRow("gg:", QLatin1String(""), KUriFilterData::NetProtocol);   // see bug 56218
    // disable localdomain in case the local DNS or /etc/hosts knows domains KDE or HTTP (e.g. due to search domain)
    addRow("KDE", QStringLiteral("https://www.google.com/search?q=KDE&ie=UTF-8"), KUriFilterData::NetProtocol, { QStringLiteral("kshorturifilter"), QStringLiteral("kuriikwsfilter") });
    addRow("HTTP", QStringLiteral("https://www.google.com/search?q=HTTP&ie=UTF-8"), KUriFilterData::NetProtocol, { QStringLiteral("kshorturifilter"), QStringLiteral("kuriikwsfilter") });
}

void KUriFilterTest::shortUris()
{
    runFilterTest();
}

void KUriFilterTest::executables_data()
{
    setupColumns();
    // Executable tests - No IKWS in minicli
    addRow("cp", QStringLiteral("cp"), KUriFilterData::Executable, minicliFilters);
    addRow("kbuildsycoca5", QStringLiteral("kbuildsycoca5"), KUriFilterData::Executable, minicliFilters);
    addRow("KDE", QStringLiteral("KDE"), NO_FILTERING, minicliFilters);
    addRow("does/not/exist", QStringLiteral("does/not/exist"), NO_FILTERING, minicliFilters);
    addRow("/does/not/exist", QStringLiteral("/does/not/exist"), KUriFilterData::LocalFile, minicliFilters);
    addRow("/does/not/exist#a", QStringLiteral("/does/not/exist#a"), KUriFilterData::LocalFile, minicliFilters);
    addRow("kbuildsycoca5 --help", QStringLiteral("kbuildsycoca5 --help"), KUriFilterData::Executable, minicliFilters);   // the args are in argsAndOptions()
    addRow("/bin/sh", QStringLiteral("/bin/sh"), KUriFilterData::Executable, minicliFilters);
    addRow("/bin/sh -q -option arg1", QStringLiteral("/bin/sh -q -option arg1"), KUriFilterData::Executable, minicliFilters);   // the args are in argsAndOptions()

    // Typing 'cp' or any other valid unix command in konq's location bar should result in
    // a search using the default search engine
    // 'ls' is a bit of a special case though, due to the toplevel domain called 'ls'
    addRow("cp", QStringLiteral("https://www.google.com/search?q=cp&ie=UTF-8"), KUriFilterData::NetProtocol,
           QStringList(), nullptr, false /* don't check for executables, see konq_misc.cc */);
}

void KUriFilterTest::executables()
{
    runFilterTest();
}

void KUriFilterTest::environmentVariables_data()
{
    setupColumns();
    // ENVIRONMENT variable
    qputenv("SOMEVAR", "/somevar");
    qputenv("ETC", "/etc");

    addRow("$SOMEVAR/kdelibs/kio", "/somevar/kdelibs/kio", KUriFilterData::LocalFile);   // note: this dir doesn't exist...
    addRow("$ETC/passwd", QStringLiteral("/etc/passwd"), KUriFilterData::LocalFile);
    QString qtdocPath = qtdir + QStringLiteral("/doc/html/functions.html");
    if (QFile::exists(qtdocPath)) {
        QString expectedUrl = QUrl::fromLocalFile(qtdocPath).toString() + "#s";
        addRow("$QTDIR/doc/html/functions.html#s", expectedUrl.toUtf8(), KUriFilterData::LocalFile);
    }
    addRow("http://www.kde.org/$USER", QStringLiteral("http://www.kde.org/$USER"), KUriFilterData::NetProtocol);   // no expansion

    addRow("$DATAHOME", datahome, KUriFilterData::LocalDir);
    QDir().mkpath(datahome + QStringLiteral("/urifilter/a+plus"));
    addRow("$DATAHOME/urifilter/a+plus", datahome + QStringLiteral("/urifilter/a+plus"), KUriFilterData::LocalDir);

    // BR 27788
    QDir().mkpath(datahome + QStringLiteral("/Dir With Space"));
    addRow("$DATAHOME/Dir With Space", datahome + QStringLiteral("/Dir With Space"), KUriFilterData::LocalDir);

    // support for name filters (BR 93825)
    addRow("$DATAHOME/*.txt", datahome + QStringLiteral("/*.txt"), KUriFilterData::LocalDir);
    addRow("$DATAHOME/[a-b]*.txt", datahome + QStringLiteral("/[a-b]*.txt"), KUriFilterData::LocalDir);
    addRow("$DATAHOME/a?c.txt", datahome + QStringLiteral("/a?c.txt"), KUriFilterData::LocalDir);
    addRow("$DATAHOME/?c.txt", datahome + QStringLiteral("/?c.txt"), KUriFilterData::LocalDir);
    // but let's check that a directory with * in the name still works
    QDir().mkpath(datahome + QStringLiteral("/share/Dir*With*Stars"));
    addRow("$DATAHOME/Dir*With*Stars", datahome + QStringLiteral("/Dir*With*Stars"), KUriFilterData::LocalDir);
    QDir().mkpath(datahome + QStringLiteral("/Dir?QuestionMark"));
    addRow("$DATAHOME/Dir?QuestionMark", datahome + QStringLiteral("/Dir?QuestionMark"), KUriFilterData::LocalDir);
    QDir().mkpath(datahome + QStringLiteral("/Dir[Bracket"));
    addRow("$DATAHOME/Dir[Bracket", datahome + QStringLiteral("/Dir[Bracket"), KUriFilterData::LocalDir);

    addRow("$HOME/$KDEDIR/kdebase/kcontrol/ebrowsing", "", KUriFilterData::LocalFile);
    addRow("$1/$2/$3", QStringLiteral("https://www.google.com/search?q=%241%2F%242%2F%243&ie=UTF-8"), KUriFilterData::NetProtocol);    // can be used as bogus or valid test. Currently triggers default search, i.e. google
    addRow("$$$$", QStringLiteral("https://www.google.com/search?q=%24%24%24%24&ie=UTF-8"), KUriFilterData::NetProtocol);   // worst case scenarios.

    if (!qtdir.isEmpty()) {
        addRow("$QTDIR", qtdir, KUriFilterData::LocalDir, QStringList(QStringLiteral("kshorturifilter")));     //use specific filter.
    }
    addRow("$HOME", home, KUriFilterData::LocalDir, QStringList(QStringLiteral("kshorturifilter")));     //use specific filter.
}

void KUriFilterTest::environmentVariables()
{
    runFilterTest();
}

void KUriFilterTest::internetKeywords_data()
{
    setupColumns();
    addRow(QString::asprintf("gg%cfoo bar", s_delimiter).toUtf8(), QStringLiteral("https://www.google.com/search?q=foo%20bar&ie=UTF-8"), KUriFilterData::NetProtocol);
    addRow("!gg foo bar", QStringLiteral("https://www.google.com/search?q=foo%20bar&ie=UTF-8"), KUriFilterData::NetProtocol);
    addRow("foo !gg bar", QStringLiteral("https://www.google.com/search?q=foo%20bar&ie=UTF-8"), KUriFilterData::NetProtocol);
    addRow("foo bar!gg", QStringLiteral("https://www.google.com/search?q=foo%20bar&ie=UTF-8"), KUriFilterData::NetProtocol);
    addRow(QString::asprintf("bug%c55798", s_delimiter).toUtf8(), QStringLiteral("https://bugs.kde.org/buglist.cgi?quicksearch=55798"), KUriFilterData::NetProtocol);

    addRow(QString::asprintf("gg%cC++", s_delimiter).toUtf8(), QStringLiteral("https://www.google.com/search?q=C%2B%2B&ie=UTF-8"), KUriFilterData::NetProtocol);
    addRow(QString::asprintf("gg%cC#", s_delimiter).toUtf8(), QStringLiteral("https://www.google.com/search?q=C%23&ie=UTF-8"), KUriFilterData::NetProtocol);
    addRow(QString::asprintf("ya%cfoo bar was here", s_delimiter).toUtf8(), nullptr, -1);   // this triggers default search, i.e. google
    addRow(QString::asprintf("gg%cwww.kde.org", s_delimiter).toUtf8(), QStringLiteral("https://www.google.com/search?q=www.kde.org&ie=UTF-8"), KUriFilterData::NetProtocol);
    addRow(QStringLiteral("gg%1é").arg(s_delimiter).toUtf8() /*eaccent in utf8*/, QStringLiteral("https://www.google.com/search?q=%C3%A9&ie=UTF-8"), KUriFilterData::NetProtocol);
    addRow(QStringLiteral("gg%1прйвет").arg(s_delimiter).toUtf8() /* greetings in russian utf-8*/, QStringLiteral("https://www.google.com/search?q=%D0%BF%D1%80%D0%B9%D0%B2%D0%B5%D1%82&ie=UTF-8"), KUriFilterData::NetProtocol);
}

void KUriFilterTest::internetKeywords()
{
    runFilterTest();
}

void KUriFilterTest::localdomain()
{
    const QString host = QHostInfo::localHostName();
    if (host.isEmpty()) {
        const QString expected = QLatin1String("http://") + host;
        runFilterTest(host, expected, KUriFilterData::NetProtocol, QStringList{QStringLiteral("localdomainurifilter")}, nullptr, false);
    }
}

void KUriFilterTest::relativeGoUp()
{
    // When the text is "../"
    KUriFilterData filteredData(QStringLiteral("../"));
    filteredData.setCheckForExecutables(false);
    // Using kshorturifilter
    const auto filtersList = QStringList{ QStringLiteral("kshorturifilter") };
    // Then the text isn't filtered and returned as-is
    QVERIFY(!KUriFilter::self()->filterUri(filteredData, filtersList));
}
