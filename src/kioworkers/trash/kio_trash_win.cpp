/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2009 Christian Ehrlicher <ch.ehrlicher@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kio_trash_win.h"
#include "kio/job.h"
#include "kioglobal_p.h"
#include "kiotrashdebug.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSettings>
#include <QTimeZone>

#include <KConfigGroup>
#include <KLocalizedString>

#include <optional>

#include <sddl.h>

// Pseudo plugin class to embed meta data
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.trash" FILE "trash.json")
};

extern "C" {
int Q_DECL_EXPORT kdemain(int argc, char **argv)
{
    // necessary to use other KIO workers
    QCoreApplication app(argc, argv);

    TrashProtocol worker(argv[1], argv[2], argv[3]);
    worker.dispatchLoop();

    return 0;
}
}

static const qint64 KDE_SECONDS_SINCE_1601 = 11644473600LL;
namespace
{
// One item in the recycle bin as stored on disk: a $R payload (the deleted file or
// directory) and its $I metadata file, which records the original path, original size,
// and deletion time.
struct RecycledItem {
    QString rPath;
    QString iPath;
    QString originalPath;
    qint64 size = 0;
    QDateTime deletedAt;
    bool isDir = false;
};

// Read a recycle-bin $I metadata file. Layout: an 8-byte format version, an 8-byte
// original size, an 8-byte deletion time (Windows FILETIME, 100-nanosecond ticks since
// 1601), then the original path. Format 1 (Vista) stores a fixed 260 UTF-16 characters;
// format 2 (Windows 10 and later) a 4-byte character count followed by that many UTF-16
// characters.
bool readInfoFile(const QString &iPath, QString &originalPath, qint64 &size, QDateTime &deletedAt)
{
    QFile file(iPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray data = file.readAll();
    if (data.size() < 24) {
        return false;
    }
    const auto readU64 = [](const char *p) {
        quint64 v = 0;
        for (int b = 0; b < 8; ++b) {
            v |= static_cast<quint64>(static_cast<unsigned char>(p[b])) << (8 * b);
        }
        return v;
    };
    const quint64 version = readU64(data.constData());
    size = static_cast<qint64>(readU64(data.constData() + 8));
    const quint64 fileTime = readU64(data.constData() + 16);
    deletedAt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(fileTime / 10000000ULL) - KDE_SECONDS_SINCE_1601, QTimeZone::UTC);

    const char *p = data.constData() + 24;
    int remaining = static_cast<int>(data.size()) - 24;
    int chars = remaining / 2;
    if (version >= 2) {
        if (remaining < 4) {
            return false;
        }
        const quint32 count = static_cast<quint32>(static_cast<unsigned char>(p[0])) | (static_cast<quint32>(static_cast<unsigned char>(p[1])) << 8)
            | (static_cast<quint32>(static_cast<unsigned char>(p[2])) << 16) | (static_cast<quint32>(static_cast<unsigned char>(p[3])) << 24);
        p += 4;
        remaining -= 4;
        chars = qMin<int>(remaining / 2, static_cast<int>(count));
    }
    originalPath = QString::fromUtf16(reinterpret_cast<const char16_t *>(p), chars);
    const int nul = originalPath.indexOf(QChar(QChar::Null));
    if (nul >= 0) {
        originalPath.truncate(nul);
    }
    return !originalPath.isEmpty();
}

// The $R payload that pairs with a $I file shares its random suffix: $IABCDEF.txt pairs
// with $RABCDEF.txt.
QString payloadPathFor(const QString &iPath)
{
    const QFileInfo info(iPath);
    return info.absolutePath() + QLatin1String("/$R") + info.fileName().mid(2);
}

// The string form of the current user's SID, such as S-1-5-21-..., or an empty string if the
// process token cannot be read.
QString currentUserSid()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return QString();
    }
    QString sid;
    DWORD length = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &length);
    if (length > 0) {
        QByteArray buffer(static_cast<qsizetype>(length), Qt::Uninitialized);
        if (GetTokenInformation(token, TokenUser, buffer.data(), length, &length)) {
            LPWSTR text = nullptr;
            if (ConvertSidToStringSidW(reinterpret_cast<const TOKEN_USER *>(buffer.constData())->User.Sid, &text)) {
                sid = QString::fromWCharArray(text);
                LocalFree(text);
            }
        }
    }
    CloseHandle(token);
    return sid;
}

// The current user's recycle-bin folders, <drive>$Recycle.Bin\<user SID>, on the given drive
// roots. The folders of the other users are left out, even when the process can read them.
QStringList userRecycleBinDirs(const QStringList &drives)
{
    static const QString sid = currentUserSid();
    QStringList dirs;
    if (sid.isEmpty()) {
        return dirs;
    }
    for (const QString &drive : drives) {
        const QString dir = drive + QLatin1String("$Recycle.Bin/") + sid;
        if (QFileInfo(dir).isDir()) {
            dirs << dir;
        }
    }
    return dirs;
}

// The recycle-bin item described by the given $I metadata file. Empty when the file or its $R
// payload does not exist.
std::optional<RecycledItem> readRecycledItem(const QString &iPath)
{
    const QFileInfo infoFile(iPath);
    if (!infoFile.isFile()) {
        return std::nullopt;
    }
    RecycledItem item;
    item.iPath = infoFile.absoluteFilePath();
    item.rPath = payloadPathFor(item.iPath);
    const QFileInfo payload(item.rPath);
    if (!payload.exists()) {
        return std::nullopt; // a $I metadata file with no $R payload
    }
    item.isDir = payload.isDir();
    if (!readInfoFile(item.iPath, item.originalPath, item.size, item.deletedAt)) {
        // Unreadable metadata: fall back to what the payload itself reports.
        item.originalPath = payload.fileName();
        item.size = payload.size();
        item.deletedAt = payload.lastModified();
    }
    return item;
}

// All recycle-bin items found in the given recycle-bin folders. Reads the filesystem directly
// so it never enumerates the aggregate shell folder, which can stall on a drive that is
// not a ready local disk.
QList<RecycledItem> recycledItems(const QStringList &binDirs)
{
    QList<RecycledItem> items;
    for (const QString &binDir : binDirs) {
        QDir dir(binDir);
        const QFileInfoList infoFiles = dir.entryInfoList({QStringLiteral("$I*")}, QDir::Files | QDir::Hidden | QDir::System);
        for (const QFileInfo &infoFile : infoFiles) {
            if (std::optional<RecycledItem> item = readRecycledItem(infoFile.absoluteFilePath())) {
                items.append(std::move(*item));
            }
        }
    }
    return items;
}

// The item whose $R payload has the given file name. The $R name is the item's
// identifier in its trash: URL. Only the $I file with the same suffix is read.
std::optional<RecycledItem> findRecycledItem(const QStringList &binDirs, const QString &payloadName)
{
    if (!payloadName.startsWith(QLatin1String("$R")) || payloadName.contains(QLatin1Char('/')) || payloadName.contains(QLatin1Char('\\'))) {
        return std::nullopt;
    }
    for (const QString &binDir : binDirs) {
        if (std::optional<RecycledItem> item = readRecycledItem(binDir + QLatin1String("/$I") + payloadName.mid(2))) {
            return item;
        }
    }
    return std::nullopt;
}

// Describe one recycle-bin item as a directory entry. UDS_NAME is the $R file name, the
// identifier used in the item's trash: URL; the display name is the original file name.
void fillEntry(const RecycledItem &item, KIO::UDSEntry &entry)
{
    const QString payloadName = QFileInfo(item.rPath).fileName();
    const QString displayName = QFileInfo(item.originalPath).fileName();
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, payloadName);
    entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, displayName.isEmpty() ? payloadName : displayName);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, item.size);
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, item.isDir ? S_IFDIR : S_IFREG);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0700);
    if (item.deletedAt.isValid()) {
        entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, item.deletedAt.toSecsSinceEpoch());
    }
    if (!item.originalPath.isEmpty()) {
        // The original location and deletion date, like the Unix worker exposes.
        entry.fastInsert(KIO::UDSEntry::UDS_EXTRA, item.originalPath);
        if (item.deletedAt.isValid()) {
            entry.fastInsert(KIO::UDSEntry::UDS_EXTRA + 1, item.deletedAt.toString(Qt::ISODate));
        }
    }
    if (item.isDir) {
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
    }
}

// Describe a file or folder inside a trashed folder. UDS_NAME is its file name on disk. It is
// read-only, like everything inside the Unix trash.
void fillInnerEntry(const QFileInfo &info, KIO::UDSEntry &entry)
{
    const bool isDir = info.isDir();
    entry.insert({{KIO::UDSEntry::UDS_SIZE, isDir ? 0 : info.size()},
                  {KIO::UDSEntry::UDS_FILE_TYPE, isDir ? S_IFDIR : S_IFREG},
                  {KIO::UDSEntry::UDS_ACCESS, isDir ? 0500 : 0400},
                  {KIO::UDSEntry::UDS_MODIFICATION_TIME, info.lastModified().toSecsSinceEpoch()}});
    if (isDir) {
        entry.insert({{KIO::UDSEntry::UDS_NAME, info.fileName()}, {KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory")}});
    } else {
        entry.insert({{KIO::UDSEntry::UDS_NAME, info.fileName()}});
    }
}

// The target of a trash: URL. The URL path is /<$R name> for a recycle-bin item, and
// /<$R name>/<relative path> for a file or folder inside a trashed folder. relativePath is
// empty for the item itself, and localPath is the path of the target on disk.
struct TrashLocation {
    RecycledItem item;
    QString relativePath;
    QString localPath;
};

// The target of the URL among the items of the given recycle-bin folders. Empty when no item
// has that $R name, or when the path holds a "." or ".." segment.
std::optional<TrashLocation> locate(const QStringList &binDirs, const QUrl &url)
{
    const QStringList segments = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.isEmpty()) {
        return std::nullopt;
    }
    for (const QString &segment : segments) {
        if (segment == QLatin1String(".") || segment == QLatin1String("..")) {
            return std::nullopt;
        }
    }
    const std::optional<RecycledItem> item = findRecycledItem(binDirs, segments.first());
    if (!item) {
        return std::nullopt;
    }
    TrashLocation location;
    location.item = *item;
    location.relativePath = segments.mid(1).join(QLatin1Char('/'));
    location.localPath = location.relativePath.isEmpty() ? item->rPath : item->rPath + QLatin1Char('/') + location.relativePath;
    return location;
}

// True when the size of the file, or of all the files in the folder, is above limit bytes.
bool isLargerThan(const QString &path, qint64 limit)
{
    const QFileInfo info(path);
    if (!info.isDir()) {
        return info.size() > limit;
    }
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        total += it.nextFileInfo().size();
        if (total > limit) {
            return true;
        }
    }
    return false;
}

// True when the policy "Do not move deleted files to the Recycle Bin" is set.
bool recycleBinDisabledByPolicy()
{
    for (const QString &root : {QStringLiteral("HKEY_CURRENT_USER"), QStringLiteral("HKEY_LOCAL_MACHINE")}) {
        const QSettings policy(root + QLatin1String("\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer"), QSettings::NativeFormat);
        if (policy.value(QStringLiteral("NoRecycleFiles"), 0).toInt() != 0) {
            return true;
        }
    }
    return false;
}

// The GUID of the volume mounted at the given root, such as C:\, with its braces. The shell
// keeps the recycle-bin settings of each volume under this name. Empty if it is unknown.
QString volumeGuid(const QString &volumeRoot)
{
    wchar_t name[64] = {0};
    if (!GetVolumeNameForVolumeMountPointW(reinterpret_cast<LPCWSTR>(volumeRoot.utf16()), name, 64)) {
        return QString();
    }
    // The name has the form \\?\Volume{GUID}\.
    const QString volumeName = QString::fromWCharArray(name);
    const qsizetype open = volumeName.indexOf(QLatin1Char('{'));
    const qsizetype close = volumeName.indexOf(QLatin1Char('}'));
    if (open < 0 || close < open) {
        return QString();
    }
    return volumeName.mid(open, close - open + 1);
}
} // namespace

TrashProtocol::TrashProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app)
    : WorkerBase(protocol, pool, app)
    , m_config(QString::fromLatin1("trashrc"), KConfig::SimpleConfig)
{
    updateRecycleBin();
}

TrashProtocol::~TrashProtocol() = default;

KIO::WorkerResult TrashProtocol::restore(const QUrl &trashURL, const QUrl & /*destURL*/)
{
    // An item is restored to the original location recorded in its $I metadata, so the
    // requested destination is ignored. Move the $R payload back to that location and
    // drop the $I metadata.
    const std::optional<TrashLocation> location = locate(userRecycleBinDirs(localRecycleBinDrives()), trashURL);
    if (!location) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, trashURL.toDisplayString());
    }
    // Only a whole item is restored, as in the Unix trash.
    if (!location->relativePath.isEmpty()) {
        return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED, trashURL.toDisplayString());
    }
    const RecycledItem *item = &location->item;
    if (item->originalPath.isEmpty()) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("The trash item %1 cannot be restored.", trashURL.toDisplayString()));
    }

    const QString target = QDir::toNativeSeparators(item->originalPath);
    // Recreate the original parent folder if it is gone, so the move has somewhere to land.
    const QString parent = QFileInfo(target).absolutePath();
    if (!parent.isEmpty()) {
        QDir().mkpath(parent);
    }

    // MoveFileExW moves a file or a whole directory in a single call when source and
    // destination are on the same volume, which a recycle-bin item and its origin always
    // are (the bin lives on the item's own drive).
    const QString source = QDir::toNativeSeparators(item->rPath);
    if (!MoveFileExW(reinterpret_cast<LPCWSTR>(source.utf16()), reinterpret_cast<LPCWSTR>(target.utf16()), 0)) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("The trash item %1 cannot be restored.", trashURL.toDisplayString()));
    }
    QFile::remove(item->iPath);
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::rename(const QUrl &oldURL, const QUrl &newURL, KIO::JobFlags flags)
{
    qCDebug(KIO_TRASH) << "TrashProtocol::rename(): old=" << oldURL << " new=" << newURL << " overwrite=" << (flags & KIO::Overwrite);

    if (oldURL.scheme() == QLatin1String("trash") && newURL.scheme() == QLatin1String("trash")) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_RENAME, oldURL.toDisplayString());
    }

    // Moving an item out of the trash to a local path restores it.
    if (oldURL.scheme() == QLatin1String("trash") && newURL.isLocalFile()) {
        return restore(oldURL, newURL);
    }

    // Moving a local file into the trash sends it to the recycle bin.
    if (oldURL.isLocalFile() && newURL.scheme() == QLatin1String("trash")) {
        return trashLocalFile(oldURL);
    }

    return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, i18n("Internal error in rename, should never happen"));
}

KIO::WorkerResult TrashProtocol::copy(const QUrl &src, const QUrl &dest, int /*permissions*/, KIO::JobFlags /*flags*/)
{
    qCDebug(KIO_TRASH) << "TrashProtocol::copy(): " << src << " " << dest;

    if (src.scheme() == QLatin1String("trash") && dest.scheme() == QLatin1String("trash")) {
        return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, i18n("This file is already in the trash bin."));
    }

    // Copying out of the trash is not supported; restoring an item is a move, see rename().
    if (src.scheme() == QLatin1String("trash") && dest.isLocalFile()) {
        return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, i18n("not supported"));
    }

    // Copying a local file into the trash sends it to the recycle bin.
    if (src.isLocalFile() && dest.scheme() == QLatin1String("trash")) {
        return trashLocalFile(src);
    }

    return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, i18n("Internal error in copy, should never happen"));
}

KIO::WorkerResult TrashProtocol::checkRecyclable(const QUrl &url, const QString &winPath) const
{
    // The mount point of the volume that holds the file, such as C:\.
    wchar_t volume[MAX_PATH + 1] = {0};
    if (!GetVolumePathNameW(reinterpret_cast<LPCWSTR>(winPath.utf16()), volume, MAX_PATH + 1)) {
        return KIO::WorkerResult::fail(KIO::ERR_TRASH_NOT_AVAILABLE, url.toDisplayString());
    }
    const QString volumeRoot = QString::fromWCharArray(volume);

    // The trash lists the recycle bins of the fixed local drives only. A network share, a
    // removable drive or a volume mounted in a folder has no recycle bin listed here.
    if (GetDriveTypeW(volume) != DRIVE_FIXED || !localRecycleBinDrives().contains(volumeRoot, Qt::CaseInsensitive)) {
        return KIO::WorkerResult::fail(KIO::ERR_TRASH_NOT_AVAILABLE, url.toDisplayString());
    }
    if (recycleBinDisabledByPolicy()) {
        return KIO::WorkerResult::fail(KIO::ERR_TRASH_NOT_AVAILABLE, url.toDisplayString());
    }

    // The settings of the recycle bin of this volume. NukeOnDelete is set by "Don't move files
    // to the Recycle Bin", and MaxCapacity is the size of the bin in megabytes.
    const QString guid = volumeGuid(volumeRoot);
    if (guid.isEmpty()) {
        return KIO::WorkerResult::pass();
    }
    const QSettings settings(QLatin1String("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\BitBucket\\Volume\\") + guid,
                             QSettings::NativeFormat);
    if (settings.value(QStringLiteral("NukeOnDelete"), 0).toInt() != 0) {
        return KIO::WorkerResult::fail(KIO::ERR_TRASH_NOT_AVAILABLE, url.toDisplayString());
    }
    bool hasCapacity = false;
    const qint64 capacityMegabytes = settings.value(QStringLiteral("MaxCapacity")).toLongLong(&hasCapacity);
    if (hasCapacity && isLargerThan(winPath, capacityMegabytes * 1024 * 1024)) {
        return KIO::WorkerResult::fail(KIO::ERR_TRASH_FILE_TOO_LARGE, url.toDisplayString());
    }
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::trashLocalFile(const QUrl &url)
{
    // QUrl::toLocalFile() gives the drive-letter path the shell accepts (QUrl::path()
    // would prefix a slash).
    const QString winPath = QDir::toNativeSeparators(url.toLocalFile());
    // With FOF_ALLOWUNDO, the shell deletes for good, without asking, a file that its recycle
    // bin cannot keep. Such a file gets an error instead, so the user can choose to delete it.
    if (const KIO::WorkerResult check = checkRecyclable(url, winPath); !check.success()) {
        return check;
    }
    // SHFileOperationW's pFrom must be double-null terminated.
    QByteArray buffer((winPath.length() + 2) * 2, 0);
    memcpy(buffer.data(), winPath.utf16(), winPath.length() * 2);

    SHFILEOPSTRUCTW op;
    memset(&op, 0, sizeof(op));
    op.wFunc = FO_DELETE;
    op.pFrom = (LPCWSTR)buffer.constData();
    // FOF_ALLOWUNDO recycles the file instead of deleting it for good, with no
    // confirmation, error or progress dialogs. SHFileOperationW is synchronous and
    // apartment independent, unlike IFileOperation.
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT | FOF_ALLOWUNDO;
    return translateError(SHFileOperationW(&op));
}

KIO::WorkerResult TrashProtocol::stat(const QUrl &url)
{
    KIO::UDSEntry entry;
    if (url.path() == QLatin1String("/")) {
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0700);
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QString::fromLatin1("inode/directory"));

        // The trash root is virtual. Its modification time is the most recent change to the
        // user's recycle-bin folder on any local drive. The shell adds and removes the $I and
        // $R files in that folder, so its timestamp changes with every item, while the one of
        // $Recycle.Bin does not.
        qint64 latest = 0;
        const QStringList binDirs = userRecycleBinDirs(localRecycleBinDrives());
        for (const QString &binDir : binDirs) {
            latest = qMax(latest, QFileInfo(binDir).lastModified().toSecsSinceEpoch());
        }
        entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, latest);
    } else {
        // A trashed item, addressed by its $R file name, or a file or folder inside it.
        const std::optional<TrashLocation> location = locate(userRecycleBinDirs(localRecycleBinDrives()), url);
        if (!location) {
            return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
        }
        if (location->relativePath.isEmpty()) {
            fillEntry(location->item, entry);
        } else {
            const QFileInfo info(location->localPath);
            if (!info.exists()) {
                return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
            }
            fillInnerEntry(info, entry);
        }
    }
    statEntry(entry);
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::del(const QUrl &url, bool /*isfile*/)
{
    // Permanently remove the item, addressed by its $R file name: drop both the $R
    // payload and its $I metadata.
    const std::optional<TrashLocation> location = locate(userRecycleBinDirs(localRecycleBinDrives()), url);
    if (!location) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
    // Only a whole item is deleted, as in the Unix trash.
    if (!location->relativePath.isEmpty()) {
        return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED, url.toDisplayString());
    }
    const RecycledItem *item = &location->item;

    const bool removed = item->isDir ? QDir(item->rPath).removeRecursively() : QFile::remove(item->rPath);
    QFile::remove(item->iPath);
    if (!removed) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_DELETE, url.toDisplayString());
    }
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::listDir(const QUrl &url)
{
    qCDebug(KIO_TRASH) << "TrashProtocol::listDir(): " << url;

    if (!url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts).isEmpty()) {
        return listTrashedDirectory(url);
    }

    // A directory listing is expected to describe the directory itself with a "." entry.
    // The trash root is virtual and has no shell item, so describe it directly.
    KIO::UDSEntry self;
    self.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    self.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    self.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0700);
    self.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
    listEntry(self);

    // Read each local drive's recycle bin from the filesystem rather than enumerating the
    // aggregate shell folder, whose enumeration can stall on a drive that is not a ready
    // local disk.
    const QList<RecycledItem> items = recycledItems(userRecycleBinDirs(localRecycleBinDrives()));
    for (const RecycledItem &item : items) {
        KIO::UDSEntry entry;
        fillEntry(item, entry);
        listEntry(entry);
    }
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::listTrashedDirectory(const QUrl &url)
{
    const std::optional<TrashLocation> location = locate(userRecycleBinDirs(localRecycleBinDrives()), url);
    if (!location) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
    const QFileInfo info(location->localPath);
    if (!info.exists()) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
    if (!info.isDir()) {
        return KIO::WorkerResult::fail(KIO::ERR_IS_FILE, url.toDisplayString());
    }

    KIO::UDSEntry self;
    self.insert({{KIO::UDSEntry::UDS_NAME, QStringLiteral(".")}, {KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory")}});
    self.insert({{KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR}, {KIO::UDSEntry::UDS_ACCESS, 0500}});
    listEntry(self);

    const QFileInfoList children = QDir(location->localPath).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &child : children) {
        KIO::UDSEntry entry;
        fillInnerEntry(child, entry);
        listEntry(entry);
    }
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::special(const QByteArray &data)
{
    QDataStream stream(data);
    int cmd;
    stream >> cmd;

    switch (cmd) {
    case 1: {
        // Empty the bin by dropping the $R payload and $I metadata of every item on each
        // local drive. A global SHEmptyRecycleBin would walk every volume and can stall on
        // a drive that is not a ready local disk.
        const QList<RecycledItem> items = recycledItems(userRecycleBinDirs(localRecycleBinDrives()));
        for (const RecycledItem &item : items) {
            if (item.isDir) {
                QDir(item.rPath).removeRecursively();
            } else {
                QFile::remove(item.rPath);
            }
            QFile::remove(item.iPath);
        }
        return KIO::WorkerResult::pass();
    }
    case 2:
        // convert old trash folder (non-windows only)
        return KIO::WorkerResult::pass();
    case 3: {
        QUrl url;
        stream >> url;
        return restore(url, QUrl());
    }
    default:
        qCWarning(KIO_TRASH) << "Unknown command in special(): " << cmd;
        return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, QString::number(cmd));
        break;
    }

    return KIO::WorkerResult::pass();
}

QStringList TrashProtocol::localRecycleBinDrives() const
{
    QStringList result;
    wchar_t drives[512] = {0};
    GetLogicalDriveStringsW(511, drives);
    for (const wchar_t *d = drives; *d; d += wcslen(d) + 1) {
        // A network drive or a drive of unknown type can stall the shell recycle-bin
        // calls, so leave both out.
        const UINT type = GetDriveTypeW(d);
        if (type == DRIVE_REMOTE || type == DRIVE_UNKNOWN) {
            continue;
        }
        // Keep only drives backed by a real local disk. QueryDosDevice is a non-blocking
        // object-manager lookup that resolves the drive letter (given without a trailing
        // slash) to its device. A letter resolving to anything other than
        // \Device\HarddiskVolume..., or that fails to resolve, is one of the volumes that
        // wedges the recycle-bin calls.
        const wchar_t devName[3] = {d[0], d[1], L'\0'};
        wchar_t target[1024] = {0};
        if (!QueryDosDeviceW(devName, target, 1024) || wcsncmp(target, L"\\Device\\HarddiskVolume", 22) != 0) {
            continue;
        }
        result << QString::fromWCharArray(d);
    }
    return result;
}

void TrashProtocol::updateRecycleBin()
{
    // The bin is empty when no local drive holds a recycled item. Reading the items from
    // the filesystem keeps the worker off the shell recycle-bin APIs entirely.
    const bool bEmpty = recycledItems(userRecycleBinDirs(localRecycleBinDrives())).isEmpty();
    KConfigGroup group = m_config.group(QStringLiteral("Status"));
    group.writeEntry("Empty", bEmpty);
    m_config.sync();
}

KIO::WorkerResult TrashProtocol::put(const QUrl &url, int /*permissions*/, KIO::JobFlags)
{
    qCDebug(KIO_TRASH) << "put: " << url;
    // create deleted file. We need to get the mtime and original location from metadata...
    // Maybe we can find the info file for url.fileName(), in case ::rename() was called first, and failed...
    return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED, url.toDisplayString());
}

KIO::WorkerResult TrashProtocol::get(const QUrl &url)
{
    // A trash item is virtual, so resolve it to its $R payload on disk and stream that
    // file's contents back.
    const std::optional<TrashLocation> location = locate(userRecycleBinDirs(localRecycleBinDrives()), url);
    if (!location) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }

    const QFileInfo info(location->localPath);
    if (!info.exists()) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
    if (info.isDir()) {
        return KIO::WorkerResult::fail(KIO::ERR_IS_DIRECTORY, url.toDisplayString());
    }

    QFile file(location->localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_READING, url.toDisplayString());
    }

    // Detect the type from the original name, which carries the real extension. A file inside a
    // trashed folder keeps its own name.
    const RecycledItem &item = location->item;
    const QString typePath = !location->relativePath.isEmpty() ? location->localPath : (item.originalPath.isEmpty() ? item.rPath : item.originalPath);
    mimeType(QMimeDatabase().mimeTypeForFile(typePath).name());
    totalSize(file.size());

    KIO::filesize_t processed = 0;
    QByteArray buffer;
    buffer.resize(16 * 1024);
    while (true) {
        const qint64 bytesRead = file.read(buffer.data(), buffer.size());
        if (bytesRead < 0) {
            return KIO::WorkerResult::fail(KIO::ERR_CANNOT_READ, url.toDisplayString());
        }
        if (bytesRead == 0) {
            break;
        }
        data(QByteArray::fromRawData(buffer.constData(), bytesRead));
        processed += bytesRead;
        processedSize(processed);
    }
    // An empty data block marks the end of the transfer.
    data(QByteArray());
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::translateError(HRESULT hRes)
{
    // Both the shell file operations (SHFileOperation, SHEmptyRecycleBin) and the COM
    // calls used here return 0 on success, so any non-zero value is a failure. A plain
    // comparison is needed because SHFileOperation reports its errors as small positive
    // codes that the FAILED() macro does not recognise.
    if (hRes == 0) {
        return KIO::WorkerResult::pass();
    }
    return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED,
                                   i18n("Trash operation failed with error code 0x%1.", QString::number(static_cast<quint32>(hRes), 16)));
}

#include "kio_trash_win.moc"

#include "moc_kio_trash_win.cpp"
