/*
    This file is part of KDE
    SPDX-FileCopyrightText: 2006-2016 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>
#include <QStandardPaths>
#include <KIO/Job>
#include <KIO/FavIconRequestJob>
#include <QDir>
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
    void tooBigFaviconShouldAbort();
    void simultaneousRequestsShouldWork();
    void concurrentRequestsShouldWork();

private:
};

void FavIconTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    // To avoid a runtime dependency on klauncher
    qputenv("KDE_FORK_SLAVES", "yes");
    // To let ctest exit, we shouldn't start kio_http_cache_cleaner
    qputenv("KIO_DISABLE_CACHE_CLEANER", "yes");
    // To get KJob::errorString() in English
    qputenv("LC_ALL", "en_US.UTF-8");

    if (!checkNetworkAccess()) {
        QSKIP("no network access", SkipAll);
    }

    // Ensure we start with no cache on disk
    const QString favIconCacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/favicons");
    QDir(favIconCacheDir).removeRecursively();
    QVERIFY(!QFileInfo::exists(favIconCacheDir));

    // Enable debug output
    QLoggingCategory::setFilterRules(QStringLiteral("kf.kio.gui.favicons.debug=true"));
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
    job->setIconUrl(QUrl("https://kde.org/doesnotexist/favicon.ico"));
    QVERIFY(willDownload(job));
    QVERIFY(!job->exec());
    QVERIFY(job->iconFile().isEmpty());
    qDebug() << job->errorString();
    QCOMPARE(job->error(), int(KIO::ERR_DOES_NOT_EXIST));
    QCOMPARE(job->errorString(), QStringLiteral("The file or folder https://kde.org/doesnotexist/favicon.ico does not exist."));

    // Second job should use the cache and not do anything
    KIO::FavIconRequestJob *secondJob = new KIO::FavIconRequestJob(url);
    QVERIFY(!willDownload(secondJob));
    QVERIFY(!secondJob->exec());
    QVERIFY(secondJob->iconFile().isEmpty());
    QCOMPARE(job->error(), int(KIO::ERR_DOES_NOT_EXIST));
    QCOMPARE(job->errorString(), QStringLiteral("The file or folder https://kde.org/doesnotexist/favicon.ico does not exist."));
}

void FavIconTest::tooBigFaviconShouldAbort()
{
    const QUrl url(s_pageUrl);

    // Set icon URL to a >65KB file
    KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url);
    job->setIconUrl(QUrl("http://download.kde.org/Attic/4.13.2/src/kcalc-4.13.2.tar.xz"));
    QVERIFY(willDownload(job));
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), int(KIO::ERR_SLAVE_DEFINED));
    QCOMPARE(job->errorString(), QStringLiteral("Icon file too big, download aborted"));
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
    job1->setAutoDelete(false);
    KIO::FavIconRequestJob *job2 = new KIO::FavIconRequestJob(url);
    job2->setAutoDelete(false);
    QVERIFY(willDownload(job1));
    QVERIFY(willDownload(job2));

    QVERIFY(job1->exec());
    QCOMPARE(job1->iconFile(), iconFile);

    QVERIFY(job2->exec());
    QCOMPARE(job2->iconFile(), iconFile);

    delete job1;
    delete job2;
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

void FavIconTest::concurrentRequestsShouldWork()
{
    const int numThreads = 3;
    QThreadPool tp;
    tp.setMaxThreadCount(numThreads);
    QVector<QFuture<QString>> futures(numThreads);
    for (int i = 0; i < numThreads; ++i) {
        futures[i] = QtConcurrent::run(&tp, getAltIconUrl);
    }
    QVERIFY(tp.waitForDone(60000));

    const QString firstResult = futures.at(0).result();
    for (int i = 1; i < numThreads; ++i) {
        QCOMPARE(futures.at(i).result(), firstResult);
    }
    QVERIFY(!QPixmap(firstResult).isNull());
}

QTEST_MAIN(FavIconTest)

#include "favicontest.moc"
