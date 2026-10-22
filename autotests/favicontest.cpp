/*
    This file is part of KDE
    SPDX-FileCopyrightText: 2006-2016 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/FavIconRequestJob>
#include <KIO/TransferJob>
#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QThread>

#include <QFutureSynchronizer>
#include <QThreadPool>
#include <QtConcurrentRun>

// An HTTP server on a port of 127.0.0.1 that answers GET /favicon.ico with a PNG image, GET /big.ico
// with 100 KiB of data, and every other path with 404.
class IconServer : public QTcpServer
{
public:
    explicit IconServer(const QByteArray &icon)
        : m_icon(icon)
    {
    }

    QUrl url(const QString &path) const
    {
        QUrl url(QStringLiteral("http://127.0.0.1"));
        url.setPort(serverPort());
        url.setPath(path);
        return url;
    }

protected:
    void incomingConnection(qintptr socketDescriptor) override
    {
        auto *socket = new QTcpSocket(this);
        socket->setSocketDescriptor(socketDescriptor);
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
            // The request is complete once the empty line after the headers arrived.
            const QByteArray request = socket->peek(socket->bytesAvailable());
            if (!request.contains("\r\n\r\n")) {
                return;
            }
            socket->readAll();
            const QByteArray path = request.split(' ').value(1);
            if (path == "/favicon.ico") {
                reply(socket, "200 OK", "image/png", m_icon);
            } else if (path == "/big.ico") {
                reply(socket, "200 OK", "image/png", QByteArray(100 * 1024, 'x'));
            } else {
                reply(socket, "404 Not Found", "text/html", QByteArray("<p>Not found</p>"));
            }
        });
    }

private:
    static void reply(QTcpSocket *socket, const QByteArray &status, const QByteArray &contentType, const QByteArray &body)
    {
        socket->write("HTTP/1.1 " + status + "\r\nContent-Type: " + contentType + "\r\nContent-Length: " + QByteArray::number(body.size())
                      + "\r\nConnection: close\r\n\r\n" + body);
        socket->disconnectFromHost();
    }

    const QByteArray m_icon;
};

class FavIconTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void favIconForUrlShouldBeEmptyInitially();
    void hostJobShouldDownloadIconThenUseCache();
    void iconUrlJobShouldDownloadIconThenUseCache();
    void reloadShouldReload();
    void failedDownloadShouldBeRemembered();
    void tooBigFaviconShouldAbort();
    void simultaneousRequestsShouldWork();
    void concurrentRequestsShouldWork();

private:
    // The favicon cache names an icon after the host and the port of its url, so each server acts as
    // a different web site.
    enum Site {
        Host,
        Alt,
        Third,
        ThreadTest,
        SiteCount,
    };

    QUrl hostUrl() const
    {
        return m_servers[Host]->url(QStringLiteral("/index.html"));
    }
    QUrl pageUrl() const
    {
        return m_servers[Host]->url(QStringLiteral("/somepage.html"));
    }
    QUrl iconUrl(Site site) const
    {
        return m_servers[site]->url(QStringLiteral("/favicon.ico"));
    }
    QString iconFileName(Site site) const
    {
        return QStringLiteral("favicons/127.0.0.1_%1.png").arg(m_servers[site]->serverPort());
    }

    // The servers answer from their own thread, so a test can wait on a job in the main thread or
    // in several other threads.
    QThread m_serverThread;
    QObject *m_serverContext = nullptr;
    IconServer *m_servers[SiteCount] = {};
};

void FavIconTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    // To let ctest exit, we shouldn't start kio_http_cache_cleaner
    qputenv("KIO_DISABLE_CACHE_CLEANER", "yes");
    // To get KJob::errorString() in English
    qputenv("LC_ALL", "en_US.UTF-8");
    qputenv("LANGUAGE", "en");

    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QBuffer iconBuffer;
    iconBuffer.open(QIODevice::WriteOnly);
    QVERIFY(image.save(&iconBuffer, "PNG"));
    const QByteArray icon = iconBuffer.data();

    m_serverContext = new QObject;
    m_serverContext->moveToThread(&m_serverThread);
    m_serverThread.start();
    bool listening = true;
    QMetaObject::invokeMethod(
        m_serverContext,
        [this, &icon, &listening] {
            for (auto &server : m_servers) {
                server = new IconServer(icon);
                listening = listening && server->listen(QHostAddress::LocalHost);
            }
        },
        Qt::BlockingQueuedConnection);
    QVERIFY(listening);

    // Ensure we start with no cache on disk
    const QString favIconCacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/favicons");
    QDir(favIconCacheDir).removeRecursively();
    QVERIFY(!QFileInfo::exists(favIconCacheDir));

    // Enable debug output
    QLoggingCategory::setFilterRules(QStringLiteral("kf.kio.gui.favicons.debug=true"));
}

void FavIconTest::cleanupTestCase()
{
    QMetaObject::invokeMethod(
        m_serverContext,
        [this] {
            for (auto &server : m_servers) {
                delete server;
                server = nullptr;
            }
            delete m_serverContext;
        },
        Qt::BlockingQueuedConnection);
    m_serverThread.quit();
    m_serverThread.wait();
}

void FavIconTest::favIconForUrlShouldBeEmptyInitially()
{
    QCOMPARE(KIO::favIconForUrl(hostUrl()), QString());
}

// Waits for start() and checks whether a transfer job was created.
static bool willDownload(KIO::FavIconRequestJob *job)
{
    qApp->sendPostedEvents(job, QEvent::MetaCall); // start() is delayed
    return job->findChild<KIO::TransferJob *>();
}

void FavIconTest::hostJobShouldDownloadIconThenUseCache()
{
    const QUrl url = hostUrl();

    KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url);
    QVERIFY(willDownload(job));
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    const QString iconFile = job->iconFile();
    QVERIFY2(iconFile.endsWith(iconFileName(Host)), qPrintable(iconFile));
    QVERIFY2(QFile::exists(iconFile), qPrintable(iconFile));
    QVERIFY(!QIcon(iconFile).isNull()); // pass full path to QIcon
    // This requires https://codereview.qt-project.org/148444
    // QVERIFY(!QIcon::fromTheme(iconFile).isNull()); // old code ported from kdelibs4 might do that, should work too

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
        KIO::FavIconRequestJob *favJob = new KIO::FavIconRequestJob(url);
        connect(favJob, &KIO::FavIconRequestJob::result, this, [favJob, &goticonFile](KJob *) {
            if (!favJob->error()) {
                goticonFile = favJob->iconFile();
            }
        });
        QVERIFY(favJob->exec());
    }
    QCOMPARE(goticonFile, iconFile);
}

void FavIconTest::iconUrlJobShouldDownloadIconThenUseCache()
{
    const QUrl url = pageUrl();

    // Set icon URL to the one of another site
    KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url);
    job->setIconUrl(iconUrl(Alt));
    QVERIFY(willDownload(job));
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    const QString iconFile = job->iconFile();
    QVERIFY2(iconFile.endsWith(iconFileName(Alt)), qPrintable(iconFile));
    QVERIFY2(QFile::exists(iconFile), qPrintable(iconFile));
    QVERIFY(!QPixmap(iconFile).isNull()); // pass full path to QPixmap (to test this too)

    // Lookup should give the same result
    QCOMPARE(KIO::favIconForUrl(url), iconFile);

    // Second job should use the cache. It doesn't even need the icon url again.
    KIO::FavIconRequestJob *secondJob = new KIO::FavIconRequestJob(url);
    QVERIFY(!willDownload(secondJob));
    QVERIFY(secondJob->exec());
    QCOMPARE(secondJob->iconFile(), iconFile);

    // Set icon URL to the one of a third site
    KIO::FavIconRequestJob *thirdJob = new KIO::FavIconRequestJob(url);
    thirdJob->setIconUrl(iconUrl(Third));
    QVERIFY(willDownload(thirdJob));
    QVERIFY2(thirdJob->exec(), qPrintable(thirdJob->errorString()));
    const QString newiconFile = thirdJob->iconFile();
    QVERIFY2(newiconFile.endsWith(iconFileName(Third)), qPrintable(newiconFile));

    // Lookup should give the same result
    QCOMPARE(KIO::favIconForUrl(url), newiconFile);
}

void FavIconTest::reloadShouldReload()
{
    const QUrl url = hostUrl();

    // First job, to make sure it's in the cache (if the other tests didn't run first)
    KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    const QString iconFile = job->iconFile();

    // Second job should use the cache
    KIO::FavIconRequestJob *secondJob = new KIO::FavIconRequestJob(url);
    QVERIFY(!willDownload(secondJob));
    QVERIFY(secondJob->exec());
    QCOMPARE(secondJob->iconFile(), iconFile);

    // job with Reload should not use the cache
    KIO::FavIconRequestJob *jobWithReload = new KIO::FavIconRequestJob(url, KIO::Reload);
    QVERIFY(willDownload(jobWithReload));
    QVERIFY2(jobWithReload->exec(), qPrintable(jobWithReload->errorString()));
    QCOMPARE(jobWithReload->iconFile(), iconFile);
}

void FavIconTest::failedDownloadShouldBeRemembered()
{
    const QUrl url = pageUrl();
    const QUrl missingIconUrl = m_servers[Host]->url(QStringLiteral("/doesnotexist/favicon.ico"));
    const QString expectedError = QStringLiteral("The file or folder %1 does not exist.").arg(missingIconUrl.toDisplayString());

    // Set icon URL to a non-existing favicon
    KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url);
    job->setIconUrl(missingIconUrl);
    QVERIFY(willDownload(job));
    QVERIFY(!job->exec());
    QVERIFY(job->iconFile().isEmpty());
    qDebug() << job->errorString();
    QCOMPARE(job->error(), int(KIO::ERR_DOES_NOT_EXIST));
    QCOMPARE(job->errorString(), expectedError);

    // Second job should use the cache and not do anything
    KIO::FavIconRequestJob *secondJob = new KIO::FavIconRequestJob(url);
    QVERIFY(!willDownload(secondJob));
    QVERIFY(!secondJob->exec());
    QVERIFY(secondJob->iconFile().isEmpty());
    QCOMPARE(job->error(), int(KIO::ERR_DOES_NOT_EXIST));
    QCOMPARE(job->errorString(), expectedError);
}

void FavIconTest::tooBigFaviconShouldAbort()
{
    const QUrl url = pageUrl();

    // Set icon URL to a >65KB file
    KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url);
    job->setIconUrl(m_servers[Host]->url(QStringLiteral("/big.ico")));
    QVERIFY(willDownload(job));
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), int(KIO::ERR_WORKER_DEFINED));
    QCOMPARE(job->errorString(), QStringLiteral("Icon file too big, download aborted"));
}

void FavIconTest::simultaneousRequestsShouldWork()
{
    const QUrl url = hostUrl();

    // First job, to find out the iconFile and delete it
    QString iconFile;
    {
        KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(url);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
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

static QString getAltIconUrl(const QUrl &pageUrl, const QUrl &iconUrl)
{
    // Set icon URL to one that we haven't downloaded yet
    KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(pageUrl);
    job->setIconUrl(iconUrl);
    job->exec();
    return job->iconFile();
}

void FavIconTest::concurrentRequestsShouldWork()
{
#ifdef Q_OS_WIN
    QSKIP("Skipping test on Windows, QSaveFile fails to handle concurrent accesses");
#endif

    const int numThreads = 3;
    QThreadPool tp;
    tp.setMaxThreadCount(numThreads);
    QList<QFuture<QString>> futures(numThreads);
    for (int i = 0; i < numThreads; ++i) {
        futures[i] = QtConcurrent::run(&tp, getAltIconUrl, pageUrl(), iconUrl(ThreadTest));
    }
    QVERIFY(tp.waitForDone(60000));

    const QString firstResult = futures.at(0).result();
    QVERIFY2(firstResult.endsWith(iconFileName(ThreadTest)), qPrintable(firstResult));
    for (int i = 1; i < numThreads; ++i) {
        QCOMPARE(futures.at(i).result(), firstResult);
    }
    QVERIFY(!QPixmap(firstResult).isNull());
}

QTEST_MAIN(FavIconTest)

#include "favicontest.moc"
