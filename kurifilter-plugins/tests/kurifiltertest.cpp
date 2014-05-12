/*
 *  Copyright (C) 2002, 2003 David Faure   <faure@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "kurifiltertest.h"


#include <KUriFilter>
#include <KSharedConfig>
#include <KConfigGroup>

#include <QtTestWidgets>
#include <QDir>
#include <QRegExp>
#include <QHostInfo>

#include <iostream>

QTEST_MAIN( KUriFilterTest)

static const char * const s_uritypes[] = { "NetProtocol", "LOCAL_FILE", "LOCAL_DIR", "EXECUTABLE", "HELP", "SHELL", "BLOCKED", "ERROR", "UNKNOWN" };
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

static void addRow(const char *input, const char *expectedResult = 0, int expectedUriType = -1, const QStringList& list = QStringList(), const char *absPath = 0, bool checkForExecutables = true )
{
    QTest::newRow(input) << input << expectedResult << expectedUriType << list << absPath << checkForExecutables;
}

static void runFilterTest(const QString &a, const QString &expectedResult = 0, int expectedUriType = -1, const QStringList& list = QStringList(), const QString &absPath = 0, bool checkForExecutables = true)
{
    KUriFilterData * filterData = new KUriFilterData;
    filterData->setData( a );
    filterData->setCheckForExecutables( checkForExecutables );

    if (!absPath.isEmpty()) {
        filterData->setAbsolutePath(absPath);
        qDebug() << "Filtering: " << a << " with absPath=" << absPath;
    }
    else {
        qDebug() << "Filtering: " << a;
    }

    if (KUriFilter::self()->filterUri(*filterData, list))
    {
        if ( expectedUriType == NO_FILTERING ) {
            qCritical() << a << "Did not expect filtering. Got" << filterData->uri();
            QVERIFY( expectedUriType != NO_FILTERING ); // fail the test
        }

        // Copied from minicli...
        QString cmd;
        QUrl uri = filterData->uri();

        if ( uri.isLocalFile() && !uri.hasFragment() && !uri.hasQuery() &&
             (filterData->uriType() != KUriFilterData::NetProtocol))
            cmd = uri.toLocalFile();
        else
            cmd = uri.url(QUrl::FullyEncoded);

        switch( filterData->uriType() )
        {
            case KUriFilterData::LocalFile:
            case KUriFilterData::LocalDir:
                qDebug() << "*** Result: Local Resource =>  '"
                          << filterData->uri().toLocalFile() << "'" << endl;
                break;
            case KUriFilterData::Help:
                qDebug() << "*** Result: Local Resource =>  '"
                          << filterData->uri().url() << "'" << endl;
                break;
            case KUriFilterData::NetProtocol:
                qDebug() << "*** Result: Network Resource => '"
                          << filterData->uri().url() << "'" << endl;
                break;
            case KUriFilterData::Shell:
            case KUriFilterData::Executable:
                if( filterData->hasArgsAndOptions() )
                    cmd += filterData->argsAndOptions();
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
            cmd = cmd.replace( QRegExp( "www\\.google\\.[^/]*/" ), "www.google.com/" );
            if (cmd != expectedResult) {
                qWarning() << a;
                QCOMPARE( cmd, expectedResult );
            }
        }

        if ( expectedUriType != -1 && expectedUriType != filterData->uriType() )
        {
            qWarning() << a << "Got URI type" << s_uritypes[filterData->uriType()]
                      << "expected" << s_uritypes[expectedUriType];
            QCOMPARE( s_uritypes[filterData->uriType()],
                      s_uritypes[expectedUriType] );
        }
    }
    else
    {
        if ( expectedUriType == NO_FILTERING )
            qDebug() << "*** No filtering required.";
        else
        {
            qDebug() << "*** Could not be filtered.";
            if( expectedUriType != filterData->uriType() )
            {
                QCOMPARE( s_uritypes[filterData->uriType()],
                          s_uritypes[expectedUriType] );
            }
        }
    }

    delete filterData;
    qDebug() << "-----";
}

static void runFilterTest() {
    QFETCH(QString, input);
    QFETCH(QString, expectedResult);
    QFETCH(int, expectedUriType);
    QFETCH(QStringList, list);
    QFETCH(QString, absPath);
    QFETCH(bool, checkForExecutables);
    runFilterTest(input, expectedResult, expectedUriType, list, absPath, checkForExecutables);
}

static void testLocalFile( const QString& filename )
{
    QFile tmpFile( filename ); // Yeah, I know, security risk blah blah. This is a test prog!

    if ( tmpFile.open( QIODevice::ReadWrite ) )
    {
        QByteArray fname = QFile::encodeName( tmpFile.fileName() );
        runFilterTest(fname, fname, KUriFilterData::LocalFile);
        tmpFile.close();
        tmpFile.remove();
    }
    else
        qDebug() << "Couldn't create " << tmpFile.fileName() << ", skipping test";
}

static char s_delimiter = ':'; // the alternative is ' '

KUriFilterTest::KUriFilterTest()
{
    QStandardPaths::setTestModeEnabled(true);
    minicliFilters << "kshorturifilter" << "kurisearchfilter" << "localdomainurifilter";
    qtdir = qgetenv("QTDIR");
    home = qgetenv("HOME");
    qputenv("DATAHOME", QFile::encodeName(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)));
    datahome = qgetenv("DATAHOME");
    qDebug() << "libpaths" << QCoreApplication::libraryPaths();
}

void KUriFilterTest::init()
{
    qDebug() ;
    setenv( "KDE_FORK_SLAVES", "yes", true ); // simpler, for the final cleanup

    // Allow testing of the search engine using both delimiters...
    const char* envDelimiter = ::getenv( "KURIFILTERTEST_DELIMITER" );
    if ( envDelimiter )
        s_delimiter = envDelimiter[0];

    // Many tests check the "default search engine" feature.
    // There is no default search engine by default (since it was annoying when making typos),
    // so the user has to set it up, which we do here.
    {
      KConfigGroup cfg( KSharedConfig::openConfig( "kuriikwsfilterrc", KConfig::SimpleConfig ), "General" );
      cfg.writeEntry( "DefaultWebShortcut", "google" );
      cfg.writeEntry( "Verbose", true );
      cfg.writeEntry( "KeywordDelimiter", QString(s_delimiter) );
      cfg.sync();
    }

    // Enable verbosity for debugging
    {
      KSharedConfig::openConfig("kshorturifilterrc", KConfig::SimpleConfig )->group(QString()).writeEntry( "Verbose", true );
    }

    QDir().mkpath(datahome + "/urifilter");
}

void KUriFilterTest::noFiltering_data()
{
    setupColumns();
    // URI that should require no filtering
    addRow( "http://www.kde.org", "http://www.kde.org", KUriFilterData::NetProtocol );
    addRow( "http://www.kde.org/developer//index.html", "http://www.kde.org/developer//index.html", KUriFilterData::NetProtocol );
    addRow( "file:///", "/", KUriFilterData::LocalDir );
    addRow( "file:///etc", "/etc", KUriFilterData::LocalDir );
    addRow( "file:///etc/passwd", "/etc/passwd", KUriFilterData::LocalFile );
}

void KUriFilterTest::noFiltering()
{
    runFilterTest();
}


void KUriFilterTest::localFiles_data()
{
    setupColumns();
    addRow( "/", "/", KUriFilterData::LocalDir );
    addRow( "/", "/", KUriFilterData::LocalDir, QStringList( "kshorturifilter" ) );
    if (QFile::exists(QDir::homePath() + QLatin1String("/.bashrc")))
        addRow( "~/.bashrc", QDir::homePath().toLocal8Bit()+"/.bashrc", KUriFilterData::LocalFile, QStringList( "kshorturifilter" ) );
    addRow( "~", QDir::homePath().toLocal8Bit(), KUriFilterData::LocalDir, QStringList( "kshorturifilter" ), "/tmp" );
    addRow( "~bin", 0, KUriFilterData::LocalDir, QStringList( "kshorturifilter" ) );
    addRow( "~does_not_exist", 0, KUriFilterData::Error, QStringList( "kshorturifilter" ) );

    // Absolute Path tests for kshorturifilter
    const QStringList kshorturifilter( QString("kshorturifilter") );
    addRow( "./", datahome, KUriFilterData::LocalDir, kshorturifilter, datahome+"/" ); // cleanPath removes the trailing slash
    const QString parentDir = QDir().cleanPath(datahome + "/..");
    addRow( "../", QFile::encodeName(parentDir), KUriFilterData::LocalDir, kshorturifilter, datahome );
    addRow( "share", datahome, KUriFilterData::LocalDir, kshorturifilter, QFile::encodeName(parentDir) );
    // Invalid URLs
    addRow( "http://a[b]", "http://a[b]", KUriFilterData::Unknown, kshorturifilter, "/" );
}

void KUriFilterTest::localFiles()
{
    runFilterTest();
}

void KUriFilterTest::refOrQuery_data()
{
    setupColumns();
    // URL with reference
    addRow( "http://www.kde.org/index.html#q8", "http://www.kde.org/index.html#q8", KUriFilterData::NetProtocol );
    // local file with reference
    addRow( "file:/etc/passwd#q8", "file:///etc/passwd#q8", KUriFilterData::LocalFile );
    addRow( "file:///etc/passwd#q8", "file:///etc/passwd#q8", KUriFilterData::LocalFile );
    addRow( "/etc/passwd#q8", "file:///etc/passwd#q8", KUriFilterData::LocalFile );
    // local file with query (can be used by javascript)
    addRow( "file:/etc/passwd?foo=bar", "file:///etc/passwd?foo=bar", KUriFilterData::LocalFile );
    testLocalFile( "/tmp/kurifiltertest?foo" ); // local file with ? in the name (#58990)
    testLocalFile( "/tmp/kurlfiltertest#foo" ); // local file with '#' in the name
    testLocalFile( "/tmp/kurlfiltertest#foo?bar" ); // local file with both
    testLocalFile( "/tmp/kurlfiltertest?foo#bar" ); // local file with both, the other way round
}

void KUriFilterTest::refOrQuery()
{
    runFilterTest();
}

void KUriFilterTest::shortUris_data()
{
    setupColumns();
    // hostnames are lowercased by QUrl
    addRow( "http://www.myDomain.commyPort/ViewObjectRes//Default:name=hello",
            "http://www.mydomain.commyport/ViewObjectRes//Default:name=hello", KUriFilterData::NetProtocol);
    addRow( "ftp://ftp.kde.org", "ftp://ftp.kde.org", KUriFilterData::NetProtocol );
    addRow( "ftp://username@ftp.kde.org:500", "ftp://username@ftp.kde.org:500", KUriFilterData::NetProtocol );

    // ShortURI/LocalDomain filter tests. NOTE: any of these tests can fail
    // if you have specified your own patterns in kshorturifilterrc. For
    // examples, see $KDEDIR/share/config/kshorturifilterrc .
    addRow( "linuxtoday.com", "http://linuxtoday.com", KUriFilterData::NetProtocol );
    addRow( "LINUXTODAY.COM", "http://linuxtoday.com", KUriFilterData::NetProtocol );
    addRow( "kde.org", "http://kde.org", KUriFilterData::NetProtocol );
    addRow( "ftp.kde.org", "ftp://ftp.kde.org", KUriFilterData::NetProtocol );
    addRow( "ftp.kde.org:21", "ftp://ftp.kde.org:21", KUriFilterData::NetProtocol );
    addRow( "cr.yp.to", "http://cr.yp.to", KUriFilterData::NetProtocol );
    addRow( "www.kde.org:21", "http://www.kde.org:21", KUriFilterData::NetProtocol );
    addRow( "foobar.local:8000", "http://foobar.local:8000", KUriFilterData::NetProtocol );
    addRow( "foo@bar.com", "mailto:foo@bar.com", KUriFilterData::NetProtocol );
    addRow( "firstname.lastname@x.foo.bar", "mailto:firstname.lastname@x.foo.bar", KUriFilterData::NetProtocol );
    addRow( "www.123.foo", "http://www.123.foo", KUriFilterData::NetProtocol );
    addRow( "user@www.123.foo:3128", "http://user@www.123.foo:3128", KUriFilterData::NetProtocol );
    addRow( "ftp://user@user@www.123.foo:3128", "ftp://user%40user@www.123.foo:3128", KUriFilterData::NetProtocol );
    addRow( "user@user@www.123.foo:3128", "http://user%40user@www.123.foo:3128", KUriFilterData::NetProtocol );

    // IPv4 address formats...
    addRow( "user@192.168.1.0:3128", "http://user@192.168.1.0:3128", KUriFilterData::NetProtocol );
    addRow( "127.0.0.1", "http://127.0.0.1", KUriFilterData::NetProtocol );
    addRow( "127.0.0.1:3128", "http://127.0.0.1:3128", KUriFilterData::NetProtocol );
    addRow( "127.1", "http://127.0.0.1", KUriFilterData::NetProtocol ); // Qt5: QUrl resolves to 127.0.0.1
    addRow( "127.0.1", "http://127.0.0.1", KUriFilterData::NetProtocol ); // Qt5: QUrl resolves to 127.0.0.1

    // IPv6 address formats (taken from RFC 2732)...
    addRow("[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:80/index.html", "http://[fedc:ba98:7654:3210:fedc:ba98:7654:3210]:80/index.html", KUriFilterData::NetProtocol );
    addRow("[1080:0:0:0:8:800:200C:417A]/index.html", "http://[1080::8:800:200c:417a]/index.html", KUriFilterData::NetProtocol ); // Qt5 QUrl change
    addRow("[3ffe:2a00:100:7031::1]", "http://[3ffe:2a00:100:7031::1]", KUriFilterData::NetProtocol );
    addRow("[1080::8:800:200C:417A]/foo", "http://[1080::8:800:200c:417a]/foo", KUriFilterData::NetProtocol );
    addRow("[::192.9.5.5]/ipng", "http://[::192.9.5.5]/ipng", KUriFilterData::NetProtocol );
    addRow("[::FFFF:129.144.52.38]:80/index.html", "http://[::ffff:129.144.52.38]:80/index.html", KUriFilterData::NetProtocol );
    addRow("[2010:836B:4179::836B:4179]", "http://[2010:836b:4179::836b:4179]", KUriFilterData::NetProtocol );

    // Local domain filter - If you uncomment these test, make sure you
    // you adjust it based on the localhost entry in your /etc/hosts file.
    // addRow( "localhost:3128", "http://localhost.localdomain:3128", KUriFilterData::NetProtocol );
    // addRow( "localhost", "http://localhost.localdomain", KUriFilterData::NetProtocol );
    // addRow( "localhost/~blah", "http://localhost.localdomain/~blah", KUriFilterData::NetProtocol );

    addRow( "user@host.domain", "mailto:user@host.domain", KUriFilterData::NetProtocol ); // new in KDE-3.2

    // Windows style SMB (UNC) URL. Should be converted into the valid smb format...
    addRow( "\\\\mainserver\\share\\file", "smb://mainserver/share/file" , KUriFilterData::NetProtocol );

    // KDE3: was not be filtered at all. All valid protocols of this form were be ignored.
    // KDE4: parsed as "network protocol", seems fine to me (DF)
    addRow( "ftp:" , "ftp:", KUriFilterData::NetProtocol );
    addRow( "http:" , "http:", KUriFilterData::NetProtocol );

    // The default search engine is set to 'Google'
    //this may fail if your DNS knows domains KDE or FTP
    addRow( "gg:", "", KUriFilterData::NetProtocol ); // see bug 56218
    addRow( "KDE", "https://www.google.com/search?q=KDE&ie=UTF-8", KUriFilterData::NetProtocol );
    addRow( "HTTP", "https://www.google.com/search?q=HTTP&ie=UTF-8", KUriFilterData::NetProtocol );
}

void KUriFilterTest::shortUris()
{
    runFilterTest();
}

void KUriFilterTest::executables_data()
{
    setupColumns();
    // Executable tests - No IKWS in minicli
    addRow( "cp", "cp", KUriFilterData::Executable, minicliFilters );
    addRow( "ktraderclient5", "ktraderclient5", KUriFilterData::Executable, minicliFilters );
    addRow( "KDE", "KDE", NO_FILTERING, minicliFilters );
    addRow( "I/dont/exist", "I/dont/exist", NO_FILTERING, minicliFilters );      //krazy:exclude=spelling
    addRow( "/I/dont/exist", 0, KUriFilterData::Error, minicliFilters );         //krazy:exclude=spelling
    addRow( "/I/dont/exist#a", 0, KUriFilterData::Error, minicliFilters );       //krazy:exclude=spelling
    addRow( "ktraderclient5 --help", "ktraderclient5 --help", KUriFilterData::Executable, minicliFilters ); // the args are in argsAndOptions()
    addRow( "/usr/bin/gs", "/usr/bin/gs", KUriFilterData::Executable, minicliFilters );
    addRow( "/usr/bin/gs -q -option arg1", "/usr/bin/gs -q -option arg1", KUriFilterData::Executable, minicliFilters ); // the args are in argsAndOptions()

    // Typing 'cp' or any other valid unix command in konq's location bar should result in
    // a search using the default search engine
    // 'ls' is a bit of a special case though, due to the toplevel domain called 'ls'
    addRow( "cp", "https://www.google.com/search?q=cp&ie=UTF-8", KUriFilterData::NetProtocol,
            QStringList(), 0, false /* don't check for executables, see konq_misc.cc */ );
}

void KUriFilterTest::executables()
{
    runFilterTest();
}

void KUriFilterTest::environmentVariables_data()
{
    setupColumns();
    // ENVIRONMENT variable
    setenv( "SOMEVAR", "/somevar", 0 );
    setenv( "ETC", "/etc", 0 );

    addRow( "$SOMEVAR/kdelibs/kio", 0, KUriFilterData::Error ); // note: this dir doesn't exist...
    addRow( "$ETC/passwd", "/etc/passwd", KUriFilterData::LocalFile );
    QString qtdocPath = qtdir+"/doc/html/functions.html";
    if (QFile::exists(qtdocPath)) {
        QString expectedUrl = QUrl::fromLocalFile(qtdocPath).toString()+"#s";
        addRow( "$QTDIR/doc/html/functions.html#s", expectedUrl.toUtf8(), KUriFilterData::LocalFile );
    }
    addRow( "http://www.kde.org/$USER", "http://www.kde.org/$USER", KUriFilterData::NetProtocol ); // no expansion

    addRow( "$DATAHOME", datahome, KUriFilterData::LocalDir );
    QDir().mkpath(datahome + "/urifilter/a+plus");
    addRow( "$DATAHOME/urifilter/a+plus", datahome+"/urifilter/a+plus", KUriFilterData::LocalDir );

    // BR 27788
    QDir().mkpath(datahome + "/Dir With Space");
    addRow( "$DATAHOME/Dir With Space", datahome+"/Dir With Space", KUriFilterData::LocalDir );

    // support for name filters (BR 93825)
    addRow( "$DATAHOME/*.txt", datahome+"/*.txt", KUriFilterData::LocalDir );
    addRow( "$DATAHOME/[a-b]*.txt", datahome+"/[a-b]*.txt", KUriFilterData::LocalDir );
    addRow( "$DATAHOME/a?c.txt", datahome+"/a?c.txt", KUriFilterData::LocalDir );
    addRow( "$DATAHOME/?c.txt", datahome+"/?c.txt", KUriFilterData::LocalDir );
    // but let's check that a directory with * in the name still works
    QDir().mkpath(datahome + "/share/Dir*With*Stars");
    addRow( "$DATAHOME/Dir*With*Stars", datahome+"/Dir*With*Stars", KUriFilterData::LocalDir );
    QDir().mkpath(datahome + "/Dir?QuestionMark");
    addRow( "$DATAHOME/Dir?QuestionMark", datahome+"/Dir?QuestionMark", KUriFilterData::LocalDir );
    QDir().mkpath(datahome + "/Dir[Bracket");
    addRow( "$DATAHOME/Dir[Bracket", datahome+"/Dir[Bracket", KUriFilterData::LocalDir );

    addRow( "$HOME/$KDEDIR/kdebase/kcontrol/ebrowsing", 0, KUriFilterData::Error );
    addRow( "$1/$2/$3", "https://www.google.com/search?q=%241%2F%242%2F%243&ie=UTF-8", KUriFilterData::NetProtocol );  // can be used as bogus or valid test. Currently triggers default search, i.e. google
    addRow( "$$$$", "https://www.google.com/search?q=%24%24%24%24&ie=UTF-8", KUriFilterData::NetProtocol ); // worst case scenarios.

    if (!qtdir.isEmpty()) {
        addRow( "$QTDIR", qtdir, KUriFilterData::LocalDir, QStringList( "kshorturifilter" ) ); //use specific filter.
    }
    addRow( "$HOME", home, KUriFilterData::LocalDir, QStringList( "kshorturifilter" ) ); //use specific filter.
}

void KUriFilterTest::environmentVariables()
{
    runFilterTest();
}

void KUriFilterTest::internetKeywords_data()
{
    setupColumns();
    QString sc;
    addRow( sc.sprintf("gg%cfoo bar",s_delimiter).toUtf8(), "https://www.google.com/search?q=foo+bar&ie=UTF-8", KUriFilterData::NetProtocol );
    addRow( sc.sprintf("bug%c55798", s_delimiter).toUtf8(), "https://bugs.kde.org/show_bug.cgi?id=55798", KUriFilterData::NetProtocol );

    addRow( sc.sprintf("gg%cC++", s_delimiter).toUtf8(), "https://www.google.com/search?q=C%2B%2B&ie=UTF-8", KUriFilterData::NetProtocol );
    addRow( sc.sprintf("gg%cC#", s_delimiter).toUtf8(), "https://www.google.com/search?q=C%23&ie=UTF-8", KUriFilterData::NetProtocol );
    addRow( sc.sprintf("ya%cfoo bar was here", s_delimiter).toUtf8(), 0, -1 ); // this triggers default search, i.e. google
    addRow( sc.sprintf("gg%cwww.kde.org", s_delimiter).toUtf8(), "https://www.google.com/search?q=www.kde.org&ie=UTF-8", KUriFilterData::NetProtocol );
    addRow( QString::fromUtf8("gg%1é").arg(s_delimiter).toUtf8() /*eaccent in utf8*/, "https://www.google.com/search?q=%C3%A9&ie=UTF-8", KUriFilterData::NetProtocol );
    addRow( QString::fromUtf8("gg%1прйвет").arg(s_delimiter).toUtf8() /* greetings in russian utf-8*/, "https://www.google.com/search?q=%D0%BF%D1%80%D0%B9%D0%B2%D0%B5%D1%82&ie=UTF-8", KUriFilterData::NetProtocol );
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
        runFilterTest(host, expected, KUriFilterData::NetProtocol, QStringList() << "localdomainurifilter", 0, false);
    }
}

#include "kurifiltertest.moc"
