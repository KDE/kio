/* This file is part of KDE
    Copyright (c) 2006-2016 David Faure <faure@kde.org>

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

#include <qtest.h>
#include <QStandardPaths>
#include <KIO/Job>
#include <KConfigGroup>
#include <KIO/FavIconRequestJob>
#include <QDateTime>
#include <QDir>
#include <QSignalSpy>
#include <QLoggingCategory>

#include <QFutureSynchronizer>
#include <QtConcurrentRun>
#include <QThreadPool>

static const char s_hostUrl[] = "http://www.google.com/index.html";
static const char s_pageUrl[] = "http://www.google.com/somepage.html";
static const char s_iconUrl[] = "http://www.google.com/favicon.ico";
static const char s_altIconUrl[] = "http://www.ibm.com/favicon.ico";
static const char s_thirdIconUrl[] = "http://www.google.fr/favicon.ico";
static const char s_iconUrlForThreadTest[] = "http://www.google.de/favicon.ico";

static enum NetworkAccess { Unknown, Yes, No } s_networkAccess = Unknown;
static bool checkNetworkAccess()
{
    if (s_networkAccess == Unknown) {
        QElapsedTimer tm;
        tm.start();
        KIO::Job *job = KIO::get(QUrl(s_iconUrl), KIO::NoReload, KIO::HideProgressInfo);
        if (job->exec()) {
            s_networkAccess = Yes;
            qDebug() << "Network access OK. Download time" << tm.elapsed() << "ms";
        } else {
            qWarning() << job->errorString();
            s_networkAccess = No;
        }
    }
    return s_networkAccess == Yes;
}

class FavIconTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void favIconForUrlShouldBeEmptyInitially();
    void hostJobShouldDownloadIconThenUseCache();
    void iconUrlJobShouldDownloadIconThenUseCache();
    void reloadShouldReload();
    void failedDownloadShouldBeRemembered();
    void simultaneousRequestsShouldWork();
    void concurrentRequestsShouldWork();

private:
};

void FavIconTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    // To avoid a runtime dependency on klauncher
    qputenv("KDE_FORK_SLAVES", "yes");
    // To let ctest exist, we shouldn't start kio_http_cache_cleaner
    qputenv("KIO_DISABLE_CACHE_CLEANER", "yes");

    if (!checkNetworkAccess()) {
        QSKIP("no network access", SkipAll);
    }

    // Ensure we start with no cache on disk
    const QString favIconCacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/favicons");
    QDir(favIconCacheDir).removeRecursively();
    QVERIFY(!QFileInfo::exists(favIconCacheDir));

    // Enable debug output
    QLoggingCategory::setFilterRules(QStringLiteral("kde.kio.favicons.debug=true"));
}

void FavIconTest::favIconForUrlShouldBeEmptyInitially()
{
    QCOMPARE(KIO::favIconForUrl(QUrl(s_hostUrl)), QString());
}

// Waits for start() and checks whether a transfer job was created.
static bool willDownload(KIO::FavIconRequestJob *job)
{
    qApp->sendPostedEvents(job, QEvent::MetaCall); // start() is delayed
    return job->findChild<KIO::TransferJob *>();
}

void FavIconTest::hostJobShouldDownloadIconThenUseCache()
{
    const QUrl url(s_hostUrl);

    KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url);
    QVERIFY(willDownload(job));
    QVERIFY(job->exec());
    const QString iconFile = job->iconFile();
    QVERIFY(iconFile.endsWith(QLatin1String("favicons/www.google.com.png")));
    QVERIFY2(QFile::exists(iconFile), qPrintable(iconFile));
    QVERIFY(!QIcon(iconFile).isNull()); // pass full path to QIcon
    // This requires https://codereview.qt-project.org/148444
    //QVERIFY(!QIcon::fromTheme(iconFile).isNull()); // old code ported from kdelibs4 might do that, should work too

    // Lookup should give the same result
    QCOMPARE(KIO::favIconForUrl(url), iconFile);

    // Second job should use the cache
    KIO::FavIconRequestJob *secondJob = new KIO::FavIconRequestJob(url);
    QVERIFY(!willDownload(secondJob));
    QVERIFY(secondJob->exec());
    QCOMPARE(secondJob->iconFile(), iconFile);

    // The code from the class docu
    QString goticonFile;
    {
        KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url);
        connect(job, &KIO::FavIconRequestJob::result, this, [job, &goticonFile](KJob *){
            if (!job->error()) {
                goticonFile = job->iconFile();
            }
        });
        QVERIFY(job->exec());
    }
    QCOMPARE(goticonFile, iconFile);
}

void FavIconTest::iconUrlJobShouldDownloadIconThenUseCache()
{
    const QUrl url(s_pageUrl);

    // Set icon URL to "ibm"
    KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url);
    job->setIconUrl(QUrl(s_altIconUrl));
    QVERIFY(willDownload(job));
    QVERIFY(job->exec());
    const QString iconFile = job->iconFile();
    QVERIFY(iconFile.endsWith(QLatin1String("favicons/www.ibm.com.png")));
    QVERIFY2(QFile::exists(iconFile), qPrintable(iconFile));
    QVERIFY(!QPixmap(iconFile).isNull()); // pass full path to QPixmap (to test this too)

    // Lookup should give the same result
    QCOMPARE(KIO::favIconForUrl(url), iconFile);

    // Second job should use the cache. It doesn't even need the icon url again.
    KIO::FavIconRequestJob *secondJob = new KIO::FavIconRequestJob(url);
    QVERIFY(!willDownload(secondJob));
    QVERIFY(secondJob->exec());
    QCOMPARE(secondJob->iconFile(), iconFile);

    // Set icon URL to "www.google.fr/favicon.ico"
    KIO::FavIconRequestJob *thirdJob = new KIO::FavIconRequestJob(url);
    thirdJob->setIconUrl(QUrl(s_thirdIconUrl));
    QVERIFY(willDownload(thirdJob));
    QVERIFY(thirdJob->exec());
    const QString newiconFile = thirdJob->iconFile();
    QVERIFY(newiconFile.endsWith(QLatin1String("favicons/www.google.fr.png")));

    // Lookup should give the same result
    QCOMPARE(KIO::favIconForUrl(url), newiconFile);
}

void FavIconTest::reloadShouldReload()
{
    const QUrl url(s_hostUrl);

    // First job, to make sure it's in the cache (if the other tests didn't run first)
    KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url);
    QVERIFY(job->exec());
    const QString iconFile = job->iconFile();

    // Second job should use the cache
    KIO::FavIconRequestJob *secondJob = new KIO::FavIconRequestJob(url);
    QVERIFY(!willDownload(secondJob));
    QVERIFY(secondJob->exec());
    QCOMPARE(secondJob->iconFile(), iconFile);

    // job with Reload should not use the cache
    KIO::FavIconRequestJob *jobWithReload = new KIO::FavIconRequestJob(url, KIO::Reload);
    QVERIFY(willDownload(jobWithReload));
    QVERIFY(jobWithReload->exec());
    QCOMPARE(jobWithReload->iconFile(), iconFile);
}

void FavIconTest::failedDownloadShouldBeRemembered()
{
    const QUrl url(s_pageUrl);

    // Set icon URL to a non-existing favicon
    KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url);
    job->setIconUrl(QUrl("http://www.kde.org/favicon.ico"));
    QVERIFY(willDownload(job));
    QVERIFY(!job->exec());
    QVERIFY(job->iconFile().isEmpty());

    // Second job should use the cache and not do anything
    KIO::FavIconRequestJob *secondJob = new KIO::FavIconRequestJob(url);
    QVERIFY(!willDownload(secondJob));
    QVERIFY(!secondJob->exec());
    QVERIFY(secondJob->iconFile().isEmpty());
}

void FavIconTest::simultaneousRequestsShouldWork()
{
    const QUrl url(s_hostUrl);

    // First job, to find out the iconFile and delete it
    QString iconFile;
    {
        KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url);
        QVERIFY(job->exec());
        iconFile = job->iconFile();
        QFile::remove(iconFile);
    }

    // This is a case we could maybe optimize: not downloading twice in parallel
    KIO::FavIconRequestJob *job1 = new KIO::FavIconRequestJob(url);
    KIO::FavIconRequestJob *job2 = new KIO::FavIconRequestJob(url);
    QVERIFY(willDownload(job1));
    QVERIFY(willDownload(job2));

    QVERIFY(job1->exec());
    QCOMPARE(job1->iconFile(), iconFile);

    QVERIFY(job2->exec());
    QCOMPARE(job2->iconFile(), iconFile);
}

static QString getAltIconUrl()
{
    const QUrl url(s_pageUrl);
    // Set icon URL to one that we haven't downloaded yet
    KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url);
    job->setIconUrl(QUrl(s_iconUrlForThreadTest));
    job->exec();
    return job->iconFile();
}

static bool allFinished(const QList<QFuture<QString> > &futures)
{
    return std::find_if(futures.constBegin(), futures.constEnd(),
                        [](const QFuture<QString> &future) { return !future.isFinished(); })
            == futures.constEnd();
}

void FavIconTest::concurrentRequestsShouldWork()
{
    const int numThreads = 3;
    QThreadPool::globalInstance()->setMaxThreadCount(numThreads);
    QFutureSynchronizer<QString> sync;
    for (int i = 0; i < numThreads; ++i) {
        sync.addFuture(QtConcurrent::run(getAltIconUrl));
    }
    //sync.waitForFinished();
    // same as sync.waitForFinished() but with a timeout
    QTRY_VERIFY(allFinished(sync.futures()));

    const QString firstResult = sync.futures().at(0).result();
    for (int i = 1; i < numThreads; ++i) {
        QCOMPARE(sync.futures().at(i).result(), firstResult);
    }
    QVERIFY(!QPixmap(firstResult).isNull());
}

QTEST_MAIN(FavIconTest)

#include "favicontest.moc"
