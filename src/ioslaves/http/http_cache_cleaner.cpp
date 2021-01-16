/*
    This file is part of KDE
    SPDX-FileCopyrightText: 1999-2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2009 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: MIT
*/

// KDE HTTP Cache cleanup tool

#include <cstring>
#include <stdlib.h>

#include <QDateTime>
#include <QDir>
#include <QString>
#include <QElapsedTimer>
#include <QDBusConnection>
#include <QLocalServer>
#include <QLocalSocket>

#include <QDebug>
#include <KLocalizedString>
#include <kprotocolmanager.h>

#include <qplatformdefs.h>
#include <QStandardPaths>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QCryptographicHash>
#include <QDBusError>
#include <QDataStream>

QDateTime g_currentDate;
int g_maxCacheAge;
qint64 g_maxCacheSize;

static const char appFullName[] = "org.kio5.kio_http_cache_cleaner";
static const char appName[] = "kio_http_cache_cleaner";

// !START OF SYNC!
// Keep the following in sync with the cache code in http.cpp

static const int s_hashedUrlBits = 160;   // this number should always be divisible by eight
static const int s_hashedUrlNibbles = s_hashedUrlBits / 4;
static const int s_hashedUrlBytes = s_hashedUrlBits / 8;

static const char version[] = "A\n";

// never instantiated, on-disk / wire format only
struct SerializedCacheFileInfo {
// from http.cpp
    quint8 version[2];
    quint8 compression; // for now fixed to 0
    quint8 reserved;    // for now; also alignment
    static const int useCountOffset = 4;
    qint32 useCount;
    qint64 servedDate;
    qint64 lastModifiedDate;
    qint64 expireDate;
    qint32 bytesCached;
    static const int size = 36;

    QString url;
    QString etag;
    QString mimeType;
    QStringList responseHeaders; // including status response like "HTTP 200 OK"
};

struct MiniCacheFileInfo {
// data from cache entry file, or from scoreboard file
    qint32 useCount;
// from filesystem
    QDateTime lastUsedDate;
    qint64 sizeOnDisk;
    // we want to delete the least "useful" files and we'll have to sort a list for that...
    bool operator<(const MiniCacheFileInfo &other) const;
    void debugPrint() const
    {
        // qDebug() << "useCount:" << useCount
        //             << "\nlastUsedDate:" << lastUsedDate.toString(Qt::ISODate)
        //             << "\nsizeOnDisk:" << sizeOnDisk << '\n';
    }
};

struct CacheFileInfo : MiniCacheFileInfo {
    quint8 version[2];
    quint8 compression; // for now fixed to 0
    quint8 reserved;    // for now; also alignment

    QDateTime servedDate;
    QDateTime lastModifiedDate;
    QDateTime expireDate;
    qint32 bytesCached;

    QString baseName;
    QString url;
    QString etag;
    QString mimeType;
    QStringList responseHeaders; // including status response like "HTTP 200 OK"

    void prettyPrint() const
    {
        QTextStream out(stdout, QIODevice::WriteOnly);
        out << "File " << baseName << " version " << version[0] << version[1];
        out << "\n cached bytes     " << bytesCached << " useCount " << useCount;
        out << "\n servedDate       " << servedDate.toString(Qt::ISODate);
        out << "\n lastModifiedDate " << lastModifiedDate.toString(Qt::ISODate);
        out << "\n expireDate       " << expireDate.toString(Qt::ISODate);
        out << "\n entity tag       " << etag;
        out << "\n encoded URL      " << url;
        out << "\n mimetype         " << mimeType;
        out << "\nResponse headers follow...\n";
        for (const QString &h : qAsConst(responseHeaders)) {
            out << h << '\n';
        }
    }
};

bool MiniCacheFileInfo::operator<(const MiniCacheFileInfo &other) const
{
    const int thisUseful = useCount / qMax(lastUsedDate.secsTo(g_currentDate), qint64(1));
    const int otherUseful = other.useCount / qMax(other.lastUsedDate.secsTo(g_currentDate), qint64(1));
    return thisUseful < otherUseful;
}

bool CacheFileInfoPtrLessThan(const CacheFileInfo *cf1, const CacheFileInfo *cf2)
{
    return *cf1 < *cf2;
}

enum OperationMode {
    CleanCache = 0,
    DeleteCache,
    FileInfo,
};

static bool readBinaryHeader(const QByteArray &d, CacheFileInfo *fi)
{
    if (d.size() < SerializedCacheFileInfo::size) {
        // qDebug() << "readBinaryHeader(): file too small?";
        return false;
    }
    QDataStream stream(d);
    stream.setVersion(QDataStream::Qt_4_5);

    stream >> fi->version[0];
    stream >> fi->version[1];
    if (fi->version[0] != version[0] || fi->version[1] != version[1]) {
        // qDebug() << "readBinaryHeader(): wrong magic bytes";
        return false;
    }
    stream >> fi->compression;
    stream >> fi->reserved;

    stream >> fi->useCount;

    SerializedCacheFileInfo serialized;

    stream >> serialized.servedDate; fi->servedDate.setSecsSinceEpoch(serialized.servedDate);
    stream >> serialized.lastModifiedDate; fi->lastModifiedDate.setSecsSinceEpoch(serialized.lastModifiedDate);
    stream >> serialized.expireDate; fi->expireDate.setSecsSinceEpoch(serialized.expireDate);

    stream >> fi->bytesCached;
    return true;
}

static QString filenameFromUrl(const QByteArray &url)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(url);
    return QString::fromLatin1(hash.result().toHex());
}

static QString cacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1String("/kio_http");
}

static QString filePath(const QString &baseName)
{
    QString cacheDirName = cacheDir();
    if (!cacheDirName.endsWith(QLatin1Char('/'))) {
        cacheDirName.append(QLatin1Char('/'));
    }
    return cacheDirName + baseName;
}

static bool readLineChecked(QIODevice *dev, QByteArray *line)
{
    *line = dev->readLine(8192);
    // if nothing read or the line didn't fit into 8192 bytes(!)
    if (line->isEmpty() || !line->endsWith('\n')) {
        return false;
    }
    // we don't actually want the newline!
    line->chop(1);
    return true;
}

static bool readTextHeader(QFile *file, CacheFileInfo *fi, OperationMode mode)
{
    bool ok = true;
    QByteArray readBuf;

    ok = ok && readLineChecked(file, &readBuf);
    fi->url = QString::fromLatin1(readBuf);
    if (filenameFromUrl(readBuf) != QFileInfo(*file).baseName()) {
        // qDebug() << "You have witnessed a very improbable hash collision!";
        return false;
    }

    // only read the necessary info for cache cleaning. Saves time and (more importantly) memory.
    if (mode != FileInfo) {
        return true;
    }

    ok = ok && readLineChecked(file, &readBuf);
    fi->etag = QString::fromLatin1(readBuf);

    ok = ok && readLineChecked(file, &readBuf);
    fi->mimeType = QString::fromLatin1(readBuf);

    // read as long as no error and no empty line found
    while (true) {
        ok = ok && readLineChecked(file, &readBuf);
        if (ok && !readBuf.isEmpty()) {
            fi->responseHeaders.append(QString::fromLatin1(readBuf));
        } else {
            break;
        }
    }
    return ok; // it may still be false ;)
}

// TODO common include file with http.cpp?
enum CacheCleanerCommand {
    InvalidCommand = 0,
    CreateFileNotificationCommand,
    UpdateFileCommand,
};

static bool readCacheFile(const QString &baseName, CacheFileInfo *fi, OperationMode mode)
{
    QFile file(filePath(baseName));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    fi->baseName = baseName;

    QByteArray header = file.read(SerializedCacheFileInfo::size);
    // do *not* modify/delete the file if we're in file info mode.
    if (!(readBinaryHeader(header, fi) && readTextHeader(&file, fi, mode)) && mode != FileInfo) {
        // qDebug() << "read(Text|Binary)Header() returned false, deleting file" << baseName;
        file.remove();
        return false;
    }
    // get meta-information from the filesystem
    QFileInfo fileInfo(file);
    fi->lastUsedDate = fileInfo.lastModified();
    fi->sizeOnDisk = fileInfo.size();
    return true;
}

class Scoreboard;

class CacheIndex
{
public:
    explicit CacheIndex(const QString &baseName)
    {
        QByteArray ba = baseName.toLatin1();
        const int sz = ba.size();
        const char *input = ba.constData();
        Q_ASSERT(sz == s_hashedUrlNibbles);

        int translated = 0;
        for (int i = 0; i < sz; i++) {
            int c = input[i];

            if (c >= '0' && c <= '9') {
                translated |= c - '0';
            } else if (c >= 'a' && c <= 'f') {
                translated |= c - 'a' + 10;
            } else {
                Q_ASSERT(false);
            }

            if (i & 1) {
                // odd index
                m_index[i >> 1] = translated;
                translated = 0;
            } else  {
                translated = translated << 4;
            }
        }

        computeHash();
    }

    bool operator==(const CacheIndex &other) const
    {
        const bool isEqual = memcmp(m_index, other.m_index, s_hashedUrlBytes) == 0;
        if (isEqual) {
            Q_ASSERT(m_hash == other.m_hash);
        }
        return isEqual;
    }

private:
    explicit CacheIndex(const QByteArray &index)
    {
        Q_ASSERT(index.length() >= s_hashedUrlBytes);
        memcpy(m_index, index.constData(), s_hashedUrlBytes);
        computeHash();
    }

    void computeHash()
    {
        uint hash = 0;
        const int ints = s_hashedUrlBytes / sizeof(uint);
        for (int i = 0; i < ints; i++) {
            hash ^= reinterpret_cast<uint *>(&m_index[0])[i];
        }
        if (const int bytesLeft = s_hashedUrlBytes % sizeof(uint)) {
            // dead code until a new url hash algorithm or architecture with sizeof(uint) != 4 appears.
            // we have the luxury of ignoring endianness because the hash is never written to disk.
            // just merge the bits into the hash in some way.
            const int offset = ints * sizeof(uint);
            for (int i = 0; i < bytesLeft; i++) {
                hash ^= static_cast<uint>(m_index[offset + i]) << (i * 8);
            }
        }
        m_hash = hash;
    }

    friend uint qHash(const CacheIndex &);
    friend class Scoreboard;

    quint8 m_index[s_hashedUrlBytes]; // packed binary version of the hexadecimal name
    uint m_hash;
};

uint qHash(const CacheIndex &ci)
{
    return ci.m_hash;
}

static CacheCleanerCommand readCommand(const QByteArray &cmd, CacheFileInfo *fi)
{
    readBinaryHeader(cmd, fi);
    QDataStream stream(cmd);
    stream.skipRawData(SerializedCacheFileInfo::size);

    quint32 ret;
    stream >> ret;

    QByteArray baseName;
    baseName.resize(s_hashedUrlNibbles);
    stream.readRawData(baseName.data(), s_hashedUrlNibbles);
    Q_ASSERT(stream.atEnd());
    fi->baseName = QString::fromLatin1(baseName);

    Q_ASSERT(ret == CreateFileNotificationCommand || ret == UpdateFileCommand);
    return static_cast<CacheCleanerCommand>(ret);
}

// never instantiated, on-disk format only
struct ScoreboardEntry {
// from scoreboard file
    quint8 index[s_hashedUrlBytes];
    static const int indexSize = s_hashedUrlBytes;
    qint32 useCount;
// from scoreboard file, but compared with filesystem to see if scoreboard has current data
    qint64 lastUsedDate;
    qint32 sizeOnDisk;
    static const int size = 36;
    // we want to delete the least "useful" files and we'll have to sort a list for that...
    bool operator<(const MiniCacheFileInfo &other) const;
};

class Scoreboard
{
public:
    Scoreboard()
    {
        // read in the scoreboard...
        QFile sboard(filePath(QStringLiteral("scoreboard")));
        if (sboard.open(QIODevice::ReadOnly)) {
            while (true) {
                QByteArray baIndex = sboard.read(ScoreboardEntry::indexSize);
                QByteArray baRest = sboard.read(ScoreboardEntry::size - ScoreboardEntry::indexSize);
                if (baIndex.size() + baRest.size() != ScoreboardEntry::size) {
                    break;
                }

                const QString entryBasename = QString::fromLatin1(baIndex.toHex());
                MiniCacheFileInfo mcfi;
                if (readAndValidateMcfi(baRest, entryBasename, &mcfi)) {
                    m_scoreboard.insert(CacheIndex(baIndex), mcfi);
                }
            }
        }
    }

    void writeOut()
    {
        // write out the scoreboard
        QFile sboard(filePath(QStringLiteral("scoreboard")));
        if (!sboard.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return;
        }
        QDataStream stream(&sboard);

        QHash<CacheIndex, MiniCacheFileInfo>::ConstIterator it = m_scoreboard.constBegin();
        for (; it != m_scoreboard.constEnd(); ++it) {
            const char *indexData = reinterpret_cast<const char *>(it.key().m_index);
            stream.writeRawData(indexData, s_hashedUrlBytes);

            stream << it.value().useCount;
            stream << it.value().lastUsedDate.toSecsSinceEpoch();
            stream << qint32(it.value().sizeOnDisk);
        }
    }

    bool fillInfo(const QString &baseName, MiniCacheFileInfo *mcfi)
    {
        QHash<CacheIndex, MiniCacheFileInfo>::ConstIterator it =
            m_scoreboard.constFind(CacheIndex(baseName));
        if (it == m_scoreboard.constEnd()) {
            return false;
        }
        *mcfi = it.value();
        return true;
    }

    qint64 runCommand(const QByteArray &cmd)
    {
        // execute the command; return number of bytes if a new file was created, zero otherwise.
        Q_ASSERT(cmd.size() == 80);
        CacheFileInfo fi;
        const CacheCleanerCommand ccc = readCommand(cmd, &fi);
        QString fileName = filePath(fi.baseName);

        switch (ccc) {
        case CreateFileNotificationCommand:
            // qDebug() << "CreateNotificationCommand for" << fi.baseName;
            if (!readBinaryHeader(cmd, &fi)) {
                return 0;
            }
            break;

        case UpdateFileCommand: {
            // qDebug() << "UpdateFileCommand for" << fi.baseName;
            QFile file(fileName);
            file.open(QIODevice::ReadWrite);

            CacheFileInfo fiFromDisk;
            QByteArray header = file.read(SerializedCacheFileInfo::size);
            if (!readBinaryHeader(header, &fiFromDisk) || fiFromDisk.bytesCached != fi.bytesCached) {
                return 0;
            }

            // adjust the use count, to make sure that we actually count up. (slaves read the file
            // asynchronously...)
            const quint32 newUseCount = fiFromDisk.useCount + 1;
            QByteArray newHeader = cmd.mid(0, SerializedCacheFileInfo::size);
            {
                QDataStream stream(&newHeader, QIODevice::ReadWrite);
                stream.skipRawData(SerializedCacheFileInfo::useCountOffset);
                stream << newUseCount;
            }

            file.seek(0);
            file.write(newHeader);
            file.close();

            if (!readBinaryHeader(newHeader, &fi)) {
                return 0;
            }
            break;
        }

        default:
            // qDebug() << "received invalid command";
            return 0;
        }

        QFileInfo fileInfo(fileName);
        fi.lastUsedDate = fileInfo.lastModified();
        fi.sizeOnDisk = fileInfo.size();
        fi.debugPrint();
        // a CacheFileInfo is-a MiniCacheFileInfo which enables the following assignment...
        add(fi);
        // finally, return cache dir growth (only relevant if a file was actually created!)
        return ccc == CreateFileNotificationCommand ? fi.sizeOnDisk : 0;
    }

    void add(const CacheFileInfo &fi)
    {
        m_scoreboard[CacheIndex(fi.baseName)] = fi;
    }

    void remove(const QString &basename)
    {
        m_scoreboard.remove(CacheIndex(basename));
    }

    // keep memory usage reasonably low - otherwise entries of nonexistent files don't hurt.
    void maybeRemoveStaleEntries(const QList<CacheFileInfo *> &fiList)
    {
        // don't bother when there are a few bogus entries
        if (m_scoreboard.count() < fiList.count() + 100) {
            return;
        }
        // qDebug() << "we have too many fake/stale entries, cleaning up...";
        QSet<CacheIndex> realFiles;
        for (CacheFileInfo *fi : fiList) {
            realFiles.insert(CacheIndex(fi->baseName));
        }
        QHash<CacheIndex, MiniCacheFileInfo>::Iterator it = m_scoreboard.begin();
        while (it != m_scoreboard.end()) {
            if (realFiles.contains(it.key())) {
                ++it;
            } else {
                it = m_scoreboard.erase(it);
            }
        }
    }

private:
    bool readAndValidateMcfi(const QByteArray &rawData, const QString &basename, MiniCacheFileInfo *mcfi)
    {
        QDataStream stream(rawData);
        stream >> mcfi->useCount;
        // check those against filesystem
        qint64 lastUsedDate;
        stream >> lastUsedDate; mcfi->lastUsedDate.setSecsSinceEpoch(lastUsedDate);

        qint32 sizeOnDisk;
        stream >> sizeOnDisk; mcfi->sizeOnDisk = sizeOnDisk;
        //qDebug() << basename << "sizeOnDisk" << mcfi->sizeOnDisk;

        QFileInfo fileInfo(filePath(basename));
        if (!fileInfo.exists()) {
            return false;
        }
        bool ok = true;
        ok = ok && fileInfo.lastModified() == mcfi->lastUsedDate;
        ok = ok && fileInfo.size() == mcfi->sizeOnDisk;
        if (!ok) {
            // size or last-modified date not consistent with entry file; reload useCount
            // note that avoiding to open the file is the whole purpose of the scoreboard - we only
            // open the file if we really have to.
            QFile entryFile(fileInfo.absoluteFilePath());
            if (!entryFile.open(QIODevice::ReadOnly)) {
                return false;
            }
            if (entryFile.size() < SerializedCacheFileInfo::size) {
                return false;
            }
            QDataStream stream(&entryFile);
            stream.skipRawData(SerializedCacheFileInfo::useCountOffset);

            stream >> mcfi->useCount;
            mcfi->lastUsedDate = fileInfo.lastModified();
            mcfi->sizeOnDisk = fileInfo.size();
            ok = true;
        }
        return ok;
    }

    QHash<CacheIndex, MiniCacheFileInfo> m_scoreboard;
};

// Keep the above in sync with the cache code in http.cpp
// !END OF SYNC!

// remove files and directories used by earlier versions of the HTTP cache.
static void removeOldFiles()
{
    const char *oldDirs = "0abcdefghijklmnopqrstuvwxyz";
    const int n = strlen(oldDirs);
    const QString cacheRootDir = filePath(QString());
    for (int i = 0; i < n; ++i) {
        const QString dirName = QString::fromLatin1(&oldDirs[i], 1);
        QDir(cacheRootDir + dirName).removeRecursively();
    }
    QFile::remove(cacheRootDir + QLatin1String("cleaned"));
}

class CacheCleaner
{
public:
    CacheCleaner(const QDir &cacheDir)
        : m_totalSizeOnDisk(0)
    {
        // qDebug();
        m_fileNameList = cacheDir.entryList(QDir::Files);
    }

    // Delete some of the files that need to be deleted. Return true when done, false otherwise.
    // This makes interleaved cleaning / serving ioslaves possible.
    bool processSlice(Scoreboard *scoreboard = nullptr)
    {
        QElapsedTimer t;
        t.start();
        // phase one: gather information about cache files
        if (!m_fileNameList.isEmpty()) {
            while (t.elapsed() < 100 && !m_fileNameList.isEmpty()) {
                QString baseName = m_fileNameList.takeFirst();
                // check if the filename is of the $s_hashedUrlNibbles letters, 0...f type
                if (baseName.length() < s_hashedUrlNibbles) {
                    continue;
                }
                bool nameOk = true;
                for (int i = 0; i < s_hashedUrlNibbles && nameOk; i++) {
                    QChar c = baseName[i];
                    nameOk = (c >= QLatin1Char('0') && c <= QLatin1Char('9')) || (c >= QLatin1Char('a') && c <= QLatin1Char('f'));
                }
                if (!nameOk) {
                    continue;
                }
                if (baseName.length() > s_hashedUrlNibbles) {
                    if (QFileInfo(filePath(baseName)).lastModified().secsTo(g_currentDate) > 15 * 60) {
                        // it looks like a temporary file that hasn't been touched in > 15 minutes...
                        QFile::remove(filePath(baseName));
                    }
                    // the temporary file might still be written to, leave it alone
                    continue;
                }

                CacheFileInfo *fi = new CacheFileInfo();
                fi->baseName = baseName;

                bool gotInfo = false;
                if (scoreboard) {
                    gotInfo = scoreboard->fillInfo(baseName, fi);
                }
                if (!gotInfo) {
                    gotInfo = readCacheFile(baseName, fi, CleanCache);
                    if (gotInfo && scoreboard) {
                        scoreboard->add(*fi);
                    }
                }
                if (gotInfo) {
                    m_fiList.append(fi);
                    m_totalSizeOnDisk += fi->sizeOnDisk;
                } else {
                    delete fi;
                }
            }
            // qDebug() << "total size of cache files is" << m_totalSizeOnDisk;

            if (m_fileNameList.isEmpty()) {
                // final step of phase one
                std::sort(m_fiList.begin(), m_fiList.end(), CacheFileInfoPtrLessThan);
            }
            return false;
        }

        // phase two: delete files until cache is under maximum allowed size

        // TODO: delete files larger than allowed for a single file
        while (t.elapsed() < 100) {
            if (m_totalSizeOnDisk <= g_maxCacheSize || m_fiList.isEmpty()) {
                // qDebug() << "total size of cache files after cleaning is" << m_totalSizeOnDisk;
                if (scoreboard) {
                    scoreboard->maybeRemoveStaleEntries(m_fiList);
                    scoreboard->writeOut();
                }
                qDeleteAll(m_fiList);
                m_fiList.clear();
                return true;
            }
            CacheFileInfo *fi = m_fiList.takeFirst();
            QString filename = filePath(fi->baseName);
            if (QFile::remove(filename)) {
                m_totalSizeOnDisk -= fi->sizeOnDisk;
                if (scoreboard) {
                    scoreboard->remove(fi->baseName);
                }
            }
            delete fi;
        }
        return false;
    }

private:
    QStringList m_fileNameList;
    QList<CacheFileInfo *> m_fiList;
    qint64 m_totalSizeOnDisk;
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationVersion(QStringLiteral("5.0"));

    KLocalizedString::setApplicationDomain("kio5");

    QCommandLineParser parser;
    parser.addVersionOption();
    parser.setApplicationDescription(QCoreApplication::translate("main", "KDE HTTP cache maintenance tool"));
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("clear-all")},
                                        QCoreApplication::translate("main", "Empty the cache")));
    parser.addOption(QCommandLineOption(
        QStringList{QStringLiteral("file-info")},
        QCoreApplication::translate("main", "Display information about cache file"),
        QStringLiteral("filename")));
    parser.process(app);

    OperationMode mode = CleanCache;
    if (parser.isSet(QStringLiteral("clear-all"))) {
        mode = DeleteCache;
    } else if (parser.isSet(QStringLiteral("file-info"))) {
        mode = FileInfo;
    }

    // file info mode: no scanning of directories, just output info and exit.
    if (mode == FileInfo) {
        CacheFileInfo fi;
        if (!readCacheFile(parser.value(QStringLiteral("file-info")), &fi, mode)) {
            return 1;
        }
        fi.prettyPrint();
        return 0;
    }

    // make sure we're the only running instance of the cleaner service
    if (mode == CleanCache) {
        if (!QDBusConnection::sessionBus().isConnected()) {
            QDBusError error(QDBusConnection::sessionBus().lastError());
            fprintf(stderr, "%s: Could not connect to D-Bus! (%s: %s)\n", appName,
                    qPrintable(error.name()), qPrintable(error.message()));
            return 1;
        }

        if (!QDBusConnection::sessionBus().registerService(QString::fromLatin1(appFullName))) {
            fprintf(stderr, "%s: Already running!\n", appName);
            return 0;
        }
    }

    g_currentDate = QDateTime::currentDateTime();
    g_maxCacheAge = KProtocolManager::maxCacheAge();
    g_maxCacheSize = mode == DeleteCache ? -1 : KProtocolManager::maxCacheSize() * 1024;

    QString cacheDirName = cacheDir();
    QDir().mkpath(cacheDirName);
    QDir cacheDir(cacheDirName);
    if (!cacheDir.exists()) {
        fprintf(stderr, "%s: '%s' does not exist.\n", appName, qPrintable(cacheDirName));
        return 0;
    }

    removeOldFiles();

    if (mode == DeleteCache) {
        QElapsedTimer t;
        t.start();
        cacheDir.refresh();
        //qDebug() << "time to refresh the cacheDir QDir:" << t.elapsed();
        CacheCleaner cleaner(cacheDir);
        while (!cleaner.processSlice()) { }
        QFile::remove(filePath(QStringLiteral("scoreboard")));
        return 0;
    }

    QLocalServer lServer;
    const QString socketFileName = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + QLatin1String("/kio_http_cache_cleaner");
    // we need to create the file by opening the socket, otherwise it won't work
    QFile::remove(socketFileName);
    if (!lServer.listen(socketFileName)) {
        qWarning() << "Error listening on" << socketFileName;
    }
    QList<QLocalSocket *> sockets;
    qint64 newBytesCounter = LLONG_MAX;  // force cleaner run on startup

    Scoreboard scoreboard;
    CacheCleaner *cleaner = nullptr;
    while (QDBusConnection::sessionBus().isConnected()) {
        g_currentDate = QDateTime::currentDateTime();

        if (!lServer.isListening()) {
            return 1;
        }
        lServer.waitForNewConnection(100);

        while (QLocalSocket *sock = lServer.nextPendingConnection()) {
            sock->waitForConnected();
            sockets.append(sock);
        }

        for (int i = 0; i < sockets.size(); i++) {
            QLocalSocket *sock = sockets[i];
            if (sock->state() != QLocalSocket::ConnectedState) {
                if (sock->state() != QLocalSocket::UnconnectedState) {
                    sock->waitForDisconnected();
                }
                delete sock;
                sockets.removeAll(sock);
                i--;
                continue;
            }
            sock->waitForReadyRead(0);
            while (true) {
                QByteArray recv = sock->read(80);
                if (recv.isEmpty()) {
                    break;
                }
                Q_ASSERT(recv.size() == 80);
                newBytesCounter += scoreboard.runCommand(recv);
            }
        }

        // interleave cleaning with serving ioslaves to reduce "garbage collection pauses"
        if (cleaner) {
            if (cleaner->processSlice(&scoreboard)) {
                // that was the last slice, done
                delete cleaner;
                cleaner = nullptr;
            }
        } else if (newBytesCounter > (g_maxCacheSize / 8)) {
            cacheDir.refresh();
            cleaner = new CacheCleaner(cacheDir);
            newBytesCounter = 0;
        }
    }
    return 0;
}
