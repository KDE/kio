/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1999-2011 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kfileitem.h"

#include "kioglobal_p.h"
#include "kiocoredebug.h"
#include "../pathhelpers_p.h"

#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QDataStream>
#include <QMimeDatabase>
#include <QDebug>
#include <QLocale>

#include <KLocalizedString>
#include <KDesktopFile>
#include <kmountpoint.h>
#include <KConfigGroup>
#ifndef Q_OS_WIN
#include <knfsshare.h>
#include <ksambashare.h>
#endif
#include <KFileSystemType>
#include <KProtocolManager>

class KFileItemPrivate : public QSharedData
{
public:
    KFileItemPrivate(const KIO::UDSEntry &entry,
                     mode_t mode, mode_t permissions,
                     const QUrl &itemOrDirUrl,
                     bool urlIsDirectory,
                     bool delayedMimeTypes,
                     KFileItem::MimeTypeDetermination mimeTypeDetermination)
        : m_entry(entry),
          m_url(itemOrDirUrl),
          m_strName(),
          m_strText(),
          m_iconName(),
          m_strLowerCaseName(),
          m_mimeType(),
          m_fileMode(mode),
          m_permissions(permissions),
          m_bLink(false),
          m_bIsLocalUrl(itemOrDirUrl.isLocalFile()),
          m_bMimeTypeKnown(false),
          m_delayedMimeTypes(delayedMimeTypes),
          m_useIconNameCache(false),
          m_hidden(Auto),
          m_slow(SlowUnknown),
          m_bSkipMimeTypeFromContent(mimeTypeDetermination == KFileItem::SkipMimeTypeFromContent),
          m_bInitCalled(false)
    {
        if (entry.count() != 0) {
            readUDSEntry(urlIsDirectory);
        } else {
            Q_ASSERT(!urlIsDirectory);
            m_strName = itemOrDirUrl.fileName();
            m_strText = KIO::decodeFileName(m_strName);
        }
    }

    /**
     * Call init() if not yet done.
     */
    void ensureInitialized() const;

    /**
     * Computes the text and mode from the UDSEntry.
     */
    void init() const;

    QString localPath() const;
    KIO::filesize_t size() const;
    KIO::filesize_t recursiveSize() const;
    QDateTime time(KFileItem::FileTimes which) const;
    void setTime(KFileItem::FileTimes which, uint time_t_val) const;
    void setTime(KFileItem::FileTimes which, const QDateTime &val) const;
    bool cmp(const KFileItemPrivate &item) const;
    bool isSlow() const;

    /**
     * Extracts the data from the UDSEntry member and updates the KFileItem
     * accordingly.
     */
    void readUDSEntry(bool _urlIsDirectory);

    /**
     * Parses the given permission set and provides it for access()
     */
    QString parsePermissions(mode_t perm) const;

    /**
     * Mime type helper
     */
    void determineMimeTypeHelper(const QUrl &url) const;

    /**
     * The UDSEntry that contains the data for this fileitem, if it came from a directory listing.
     */
    mutable KIO::UDSEntry m_entry;
    /**
     * The url of the file
     */
    QUrl m_url;

    /**
     * The text for this item, i.e. the file name without path,
     */
    QString m_strName;

    /**
     * The text for this item, i.e. the file name without path, decoded
     * ('%%' becomes '%', '%2F' becomes '/')
     */
    QString m_strText;

    /**
     * The icon name for this item.
     */
    mutable QString m_iconName;

    /**
     * The filename in lower case (to speed up sorting)
     */
    mutable QString m_strLowerCaseName;

    /**
     * The MIME type of the file
     */
    mutable QMimeType m_mimeType;

    /**
     * The file mode
     */
    mutable mode_t m_fileMode;
    /**
     * The permissions
     */
    mutable mode_t m_permissions;

    /**
     * Whether the file is a link
     */
    mutable bool m_bLink: 1;
    /**
     * True if local file
     */
    bool m_bIsLocalUrl: 1;

    mutable bool m_bMimeTypeKnown: 1;
    mutable bool m_delayedMimeTypes: 1;

    /** True if m_iconName should be used as cache. */
    mutable bool m_useIconNameCache: 1;

    // Auto: check leading dot.
    enum { Auto, Hidden, Shown } m_hidden: 3;

    // Slow? (nfs/smb/ssh)
    mutable enum { SlowUnknown, Fast, Slow } m_slow: 3;

    /**
     * True if MIME type determination by content should be skipped
     */
    bool m_bSkipMimeTypeFromContent: 1;

    /**
     * True if init() was called on demand
     */
    mutable bool m_bInitCalled: 1;

    // For special case like link to dirs over FTP
    QString m_guessedMimeType;
    mutable QString m_access;

};

void KFileItemPrivate::ensureInitialized() const
{
    if (!m_bInitCalled) {
        init();
    }
}

void KFileItemPrivate::init() const
{
    m_access.clear();
    //  metaInfo = KFileMetaInfo();

    // stat() local files if needed
    if (m_fileMode == KFileItem::Unknown || m_permissions == KFileItem::Unknown || m_entry.count() == 0) {
        if (m_url.isLocalFile()) {
            /* directories may not have a slash at the end if
             * we want to stat() them; it requires that we
             * change into it .. which may not be allowed
             * stat("/is/unaccessible")  -> rwx------
             * stat("/is/unaccessible/") -> EPERM            H.Z.
             * This is the reason for the StripTrailingSlash
             */
            QT_STATBUF buf;
            const QString path = m_url.adjusted(QUrl::StripTrailingSlash).toLocalFile();
            const QByteArray pathBA = QFile::encodeName(path);
            if (QT_LSTAT(pathBA.constData(), &buf) == 0) {
                m_entry.reserve(9);
                m_entry.replace(KIO::UDSEntry::UDS_DEVICE_ID,           buf.st_dev);
                m_entry.replace(KIO::UDSEntry::UDS_INODE,               buf.st_ino);

                mode_t mode = buf.st_mode;
                if ((buf.st_mode & QT_STAT_MASK) == QT_STAT_LNK) {
                    m_bLink = true;
                    if (QT_STAT(pathBA.constData(), &buf) == 0) {
                        mode = buf.st_mode;
                    } else {// link pointing to nowhere (see FileProtocol::createUDSEntry() in ioslaves/file/file.cpp)
                        mode = (QT_STAT_MASK - 1) | S_IRWXU | S_IRWXG | S_IRWXO;
                    }
                }
                m_entry.replace(KIO::UDSEntry::UDS_SIZE,      buf.st_size);
                m_entry.replace(KIO::UDSEntry::UDS_FILE_TYPE, buf.st_mode & QT_STAT_MASK); // extract file type
                m_entry.replace(KIO::UDSEntry::UDS_ACCESS,    buf.st_mode & 07777); // extract permissions
                m_entry.replace(KIO::UDSEntry::UDS_MODIFICATION_TIME,   buf.st_mtime); // TODO: we could use msecs too...
                m_entry.replace(KIO::UDSEntry::UDS_ACCESS_TIME,         buf.st_atime);
#ifndef Q_OS_WIN
                m_entry.replace(KIO::UDSEntry::UDS_USER,                KUser(buf.st_uid).loginName());
                m_entry.replace(KIO::UDSEntry::UDS_GROUP,               KUserGroup(buf.st_gid).name());
#endif

                // TODO: these can be removed, we can use UDS_FILE_TYPE and UDS_ACCESS everywhere
                if (m_fileMode == KFileItem::Unknown) {
                    m_fileMode = mode & QT_STAT_MASK; // extract file type
                }
                if (m_permissions == KFileItem::Unknown) {
                    m_permissions = mode & 07777; // extract permissions
                }
            }
        }
    }

    m_bInitCalled = true;
}

void KFileItemPrivate::readUDSEntry(bool _urlIsDirectory)
{
    // extract fields from the KIO::UDS Entry

    m_fileMode = m_entry.numberValue(KIO::UDSEntry::UDS_FILE_TYPE, KFileItem::Unknown);
    m_permissions = m_entry.numberValue(KIO::UDSEntry::UDS_ACCESS, KFileItem::Unknown);
    m_strName = m_entry.stringValue(KIO::UDSEntry::UDS_NAME);

    const QString displayName = m_entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
    if (!displayName.isEmpty()) {
        m_strText = displayName;
    } else {
        m_strText = KIO::decodeFileName(m_strName);
    }

    const QString urlStr = m_entry.stringValue(KIO::UDSEntry::UDS_URL);
    const bool UDS_URL_seen = !urlStr.isEmpty();
    if (UDS_URL_seen) {
        m_url = QUrl(urlStr);
        if (m_url.isLocalFile()) {
            m_bIsLocalUrl = true;
        }
    }
    QMimeDatabase db;
    const QString mimeTypeStr = m_entry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE);
    m_bMimeTypeKnown = !mimeTypeStr.isEmpty();
    if (m_bMimeTypeKnown) {
        m_mimeType = db.mimeTypeForName(mimeTypeStr);
    }

    m_guessedMimeType = m_entry.stringValue(KIO::UDSEntry::UDS_GUESSED_MIME_TYPE);
    m_bLink = !m_entry.stringValue(KIO::UDSEntry::UDS_LINK_DEST).isEmpty();   // we don't store the link dest

    const int hiddenVal = m_entry.numberValue(KIO::UDSEntry::UDS_HIDDEN, -1);
    m_hidden = hiddenVal == 1 ? Hidden : (hiddenVal == 0 ? Shown : Auto);

    if (_urlIsDirectory && !UDS_URL_seen && !m_strName.isEmpty() && m_strName != QLatin1String(".")) {
        m_url.setPath(concatPaths(m_url.path(), m_strName));
    }

    m_iconName.clear();
}

inline //because it is used only in one place
KIO::filesize_t KFileItemPrivate::size() const
{
    ensureInitialized();

    // Extract it from the KIO::UDSEntry
    long long fieldVal = m_entry.numberValue(KIO::UDSEntry::UDS_SIZE, -1);
    if (fieldVal != -1) {
        return fieldVal;
    }

    // If not in the KIO::UDSEntry, or if UDSEntry empty, use stat() [if local URL]
    if (m_bIsLocalUrl) {
        return QFileInfo(m_url.toLocalFile()).size();
    }
    return 0;
}

KIO::filesize_t KFileItemPrivate::recursiveSize() const
{
    // Extract it from the KIO::UDSEntry
    long long fieldVal = m_entry.numberValue(KIO::UDSEntry::UDS_RECURSIVE_SIZE, -1);
    if (fieldVal != -1) {
        return static_cast<KIO::filesize_t>(fieldVal);
    }

    return 0;
}

static uint udsFieldForTime(KFileItem::FileTimes mappedWhich)
{
    switch (mappedWhich) {
    case KFileItem::ModificationTime:
        return KIO::UDSEntry::UDS_MODIFICATION_TIME;
    case KFileItem::AccessTime:
        return KIO::UDSEntry::UDS_ACCESS_TIME;
    case KFileItem::CreationTime:
        return KIO::UDSEntry::UDS_CREATION_TIME;
    }
    return 0;
}

void KFileItemPrivate::setTime(KFileItem::FileTimes mappedWhich, uint time_t_val) const
{
    m_entry.replace(udsFieldForTime(mappedWhich), time_t_val);
}

void KFileItemPrivate::setTime(KFileItem::FileTimes mappedWhich, const QDateTime &val) const
{
    const QDateTime dt = val.toLocalTime(); // #160979
    setTime(mappedWhich, dt.toSecsSinceEpoch());
}

QDateTime KFileItemPrivate::time(KFileItem::FileTimes mappedWhich) const
{
    ensureInitialized();

    // Extract it from the KIO::UDSEntry
    const uint uds = udsFieldForTime(mappedWhich);
    if (uds > 0) {
        const long long fieldVal = m_entry.numberValue(uds, -1);
        if (fieldVal != -1) {
            return QDateTime::fromMSecsSinceEpoch(1000 * fieldVal);
        }
    }

    return QDateTime();
}

inline //because it is used only in one place
bool KFileItemPrivate::cmp(const KFileItemPrivate &item) const
{
    if (item.m_bInitCalled) {
        ensureInitialized();
    }

    if (m_bInitCalled) {
        item.ensureInitialized();
    }

#if 0
    qDebug() << "Comparing" << m_url << "and" << item.m_url;
    qDebug() << " name" << (m_strName == item.m_strName);
    qDebug() << " local" << (m_bIsLocalUrl == item.m_bIsLocalUrl);
    qDebug() << " mode" << (m_fileMode == item.m_fileMode);
    qDebug() << " perm" << (m_permissions == item.m_permissions);
    qDebug() << " UDS_EXTENDED_ACL" << (m_entry.stringValue( KIO::UDSEntry::UDS_EXTENDED_ACL ) == item.m_entry.stringValue( KIO::UDSEntry::UDS_EXTENDED_ACL ));
    qDebug() << " UDS_ACL_STRING" << (m_entry.stringValue( KIO::UDSEntry::UDS_ACL_STRING ) == item.m_entry.stringValue( KIO::UDSEntry::UDS_ACL_STRING ));
    qDebug() << " UDS_DEFAULT_ACL_STRING" << (m_entry.stringValue( KIO::UDSEntry::UDS_DEFAULT_ACL_STRING ) == item.m_entry.stringValue( KIO::UDSEntry::UDS_DEFAULT_ACL_STRING ));
    qDebug() << " m_bLink" << (m_bLink == item.m_bLink);
    qDebug() << " m_hidden" << (m_hidden == item.m_hidden);
    qDebug() << " size" << (size() == item.size());
    qDebug() << " ModificationTime" << m_entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME) << item.m_entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME);
    qDebug() << " UDS_ICON_NAME" << (m_entry.stringValue( KIO::UDSEntry::UDS_ICON_NAME ) == item.m_entry.stringValue( KIO::UDSEntry::UDS_ICON_NAME ));
#endif
    return (m_strName == item.m_strName
            && m_bIsLocalUrl == item.m_bIsLocalUrl
            && m_fileMode == item.m_fileMode
            && m_permissions == item.m_permissions
            && m_entry.stringValue(KIO::UDSEntry::UDS_EXTENDED_ACL) == item.m_entry.stringValue(KIO::UDSEntry::UDS_EXTENDED_ACL)
            && m_entry.stringValue(KIO::UDSEntry::UDS_ACL_STRING) == item.m_entry.stringValue(KIO::UDSEntry::UDS_ACL_STRING)
            && m_entry.stringValue(KIO::UDSEntry::UDS_DEFAULT_ACL_STRING) == item.m_entry.stringValue(KIO::UDSEntry::UDS_DEFAULT_ACL_STRING)
            && m_bLink == item.m_bLink
            && m_hidden == item.m_hidden
            && size() == item.size()
            && m_entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME) == item.m_entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME)
            && m_entry.stringValue(KIO::UDSEntry::UDS_ICON_NAME) == item.m_entry.stringValue(KIO::UDSEntry::UDS_ICON_NAME)
            && m_entry.stringValue(KIO::UDSEntry::UDS_TARGET_URL) == item.m_entry.stringValue(KIO::UDSEntry::UDS_TARGET_URL)
            && m_entry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH) == item.m_entry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH)
           );

    // Don't compare the MIME types here. They might not be known, and we don't want to
    // do the slow operation of determining them here.
}

inline //because it is used only in one place
QString KFileItemPrivate::parsePermissions(mode_t perm) const
{
    ensureInitialized();

    static char buffer[ 12 ];

    char uxbit, gxbit, oxbit;

    if ((perm & (S_IXUSR | S_ISUID)) == (S_IXUSR | S_ISUID)) {
        uxbit = 's';
    } else if ((perm & (S_IXUSR | S_ISUID)) == S_ISUID) {
        uxbit = 'S';
    } else if ((perm & (S_IXUSR | S_ISUID)) == S_IXUSR) {
        uxbit = 'x';
    } else {
        uxbit = '-';
    }

    if ((perm & (S_IXGRP | S_ISGID)) == (S_IXGRP | S_ISGID)) {
        gxbit = 's';
    } else if ((perm & (S_IXGRP | S_ISGID)) == S_ISGID) {
        gxbit = 'S';
    } else if ((perm & (S_IXGRP | S_ISGID)) == S_IXGRP) {
        gxbit = 'x';
    } else {
        gxbit = '-';
    }

    if ((perm & (S_IXOTH | S_ISVTX)) == (S_IXOTH | S_ISVTX)) {
        oxbit = 't';
    } else if ((perm & (S_IXOTH | S_ISVTX)) == S_ISVTX) {
        oxbit = 'T';
    } else if ((perm & (S_IXOTH | S_ISVTX)) == S_IXOTH) {
        oxbit = 'x';
    } else {
        oxbit = '-';
    }

    // Include the type in the first char like ls does; people are more used to seeing it,
    // even though it's not really part of the permissions per se.
    if (m_bLink) {
        buffer[0] = 'l';
    } else if (m_fileMode != KFileItem::Unknown) {
        if ((m_fileMode & QT_STAT_MASK) == QT_STAT_DIR) {
            buffer[0] = 'd';
        }
#ifdef Q_OS_UNIX
        else if (S_ISSOCK(m_fileMode)) {
            buffer[0] = 's';
        } else if (S_ISCHR(m_fileMode)) {
            buffer[0] = 'c';
        } else if (S_ISBLK(m_fileMode)) {
            buffer[0] = 'b';
        } else if (S_ISFIFO(m_fileMode)) {
            buffer[0] = 'p';
        }
#endif // Q_OS_UNIX
        else {
            buffer[0] = '-';
        }
    } else {
        buffer[0] = '-';
    }

    buffer[1] = (((perm & S_IRUSR) == S_IRUSR) ? 'r' : '-');
    buffer[2] = (((perm & S_IWUSR) == S_IWUSR) ? 'w' : '-');
    buffer[3] = uxbit;
    buffer[4] = (((perm & S_IRGRP) == S_IRGRP) ? 'r' : '-');
    buffer[5] = (((perm & S_IWGRP) == S_IWGRP) ? 'w' : '-');
    buffer[6] = gxbit;
    buffer[7] = (((perm & S_IROTH) == S_IROTH) ? 'r' : '-');
    buffer[8] = (((perm & S_IWOTH) == S_IWOTH) ? 'w' : '-');
    buffer[9] = oxbit;
    // if (hasExtendedACL())
    if (m_entry.contains(KIO::UDSEntry::UDS_EXTENDED_ACL)) {
        buffer[10] = '+';
        buffer[11] = 0;
    } else {
        buffer[10] = 0;
    }

    return QString::fromLatin1(buffer);
}

void KFileItemPrivate::determineMimeTypeHelper(const QUrl &url) const
{
    QMimeDatabase db;
    if (m_bSkipMimeTypeFromContent) {
        const QString scheme = url.scheme();
        if (scheme.startsWith(QLatin1String("http")) || scheme == QLatin1String("mailto"))
            m_mimeType = db.mimeTypeForName(QLatin1String("application/octet-stream"));
        else
            m_mimeType = db.mimeTypeForFile(url.path(), QMimeDatabase::MatchMode::MatchExtension);
    } else {
        m_mimeType = db.mimeTypeForUrl(url);
    }
}

///////

KFileItem::KFileItem()
    : d(nullptr)
{
}

KFileItem::KFileItem(const KIO::UDSEntry &entry, const QUrl &itemOrDirUrl,
                     bool delayedMimeTypes, bool urlIsDirectory)
    : d(new KFileItemPrivate(entry, KFileItem::Unknown, KFileItem::Unknown, itemOrDirUrl, urlIsDirectory, delayedMimeTypes, KFileItem::NormalMimeTypeDetermination))
{
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 0)
KFileItem::KFileItem(mode_t mode, mode_t permissions, const QUrl &url, bool delayedMimeTypes)
    : d(new KFileItemPrivate(KIO::UDSEntry(), mode, permissions,
                             url, false, delayedMimeTypes, KFileItem::NormalMimeTypeDetermination))
{
}
#endif

KFileItem::KFileItem(const QUrl &url, const QString &mimeType, mode_t mode)
    : d(new KFileItemPrivate(KIO::UDSEntry(), mode, KFileItem::Unknown,
                             url, false, false, KFileItem::NormalMimeTypeDetermination))
{
    d->m_bMimeTypeKnown = !mimeType.isEmpty();
    if (d->m_bMimeTypeKnown) {
        QMimeDatabase db;
        d->m_mimeType = db.mimeTypeForName(mimeType);
    }
}

KFileItem::KFileItem(const QUrl &url, KFileItem::MimeTypeDetermination mimeTypeDetermination)
    : d(new KFileItemPrivate(KIO::UDSEntry(), KFileItem::Unknown, KFileItem::Unknown,
                             url, false, false, mimeTypeDetermination))
{
}


// Default implementations for:
// - Copy constructor
// - Move constructor
// - Copy assignment
// - Move assignment
// - Destructor
// The compiler will now generate the content of those.
KFileItem::KFileItem(const KFileItem&) = default;
KFileItem::~KFileItem() = default;
KFileItem::KFileItem(KFileItem&&) = default;
KFileItem& KFileItem::operator=(const KFileItem&) = default;
KFileItem& KFileItem::operator=(KFileItem&&) = default;

void KFileItem::refresh()
{
    if (!d) {
        qCWarning(KIO_CORE) << "null item";
        return;
    }

    d->m_fileMode = KFileItem::Unknown;
    d->m_permissions = KFileItem::Unknown;
    d->m_hidden = KFileItemPrivate::Auto;
    refreshMimeType();

    // Basically, we can't trust any information we got while listing.
    // Everything could have changed...
    // Clearing m_entry makes it possible to detect changes in the size of the file,
    // the time information, etc.
    d->m_entry.clear();
    d->init(); // re-populates d->m_entry
}

void KFileItem::refreshMimeType()
{
    if (!d) {
        return;
    }

    d->m_mimeType = QMimeType();
    d->m_bMimeTypeKnown = false;
    d->m_iconName.clear();
}

void KFileItem::setDelayedMimeTypes(bool b)
{
    if (!d) {
        return;
    }
    d->m_delayedMimeTypes = b;
}

void KFileItem::setUrl(const QUrl &url)
{
    if (!d) {
        qCWarning(KIO_CORE) << "null item";
        return;
    }

    d->m_url = url;
    setName(url.fileName());
}

void KFileItem::setLocalPath(const QString &path)
{
    if (!d) {
        qCWarning(KIO_CORE) << "null item";
        return;
    }

    d->m_entry.replace(KIO::UDSEntry::UDS_LOCAL_PATH, path);
}

void KFileItem::setName(const QString &name)
{
    if (!d) {
        qCWarning(KIO_CORE) << "null item";
        return;
    }

    d->ensureInitialized();

    d->m_strName = name;
    if (!d->m_strName.isEmpty()) {
        d->m_strText = KIO::decodeFileName(d->m_strName);
    }
    if (d->m_entry.contains(KIO::UDSEntry::UDS_NAME)) {
        d->m_entry.replace(KIO::UDSEntry::UDS_NAME, d->m_strName);    // #195385
    }

}

QString KFileItem::linkDest() const
{
    if (!d) {
        return QString();
    }

    d->ensureInitialized();

    // Extract it from the KIO::UDSEntry
    const QString linkStr = d->m_entry.stringValue(KIO::UDSEntry::UDS_LINK_DEST);
    if (!linkStr.isEmpty()) {
        return linkStr;
    }

    // If not in the KIO::UDSEntry, or if UDSEntry empty, use readlink() [if local URL]
    if (d->m_bIsLocalUrl) {
        return QFile::symLinkTarget(d->m_url.adjusted(QUrl::StripTrailingSlash).toLocalFile());
    }
    return QString();
}

QString KFileItemPrivate::localPath() const
{
    if (m_bIsLocalUrl) {
        return m_url.toLocalFile();
    }

    ensureInitialized();

    // Extract the local path from the KIO::UDSEntry
    return m_entry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH);
}

QString KFileItem::localPath() const
{
    if (!d) {
        return QString();
    }

    return d->localPath();
}

KIO::filesize_t KFileItem::size() const
{
    if (!d) {
        return 0;
    }

    return d->size();
}

KIO::filesize_t KFileItem::recursiveSize() const
{
    if (!d) {
        return 0;
    }

    return d->recursiveSize();
}

bool KFileItem::hasExtendedACL() const
{
    if (!d) {
        return false;
    }

    // Check if the field exists; its value doesn't matter
    return entry().contains(KIO::UDSEntry::UDS_EXTENDED_ACL);
}

KACL KFileItem::ACL() const
{
    if (!d) {
        return KACL();
    }

    if (hasExtendedACL()) {
        // Extract it from the KIO::UDSEntry
        const QString fieldVal = d->m_entry.stringValue(KIO::UDSEntry::UDS_ACL_STRING);
        if (!fieldVal.isEmpty()) {
            return KACL(fieldVal);
        }
    }

    // create one from the basic permissions
    return KACL(d->m_permissions);
}

KACL KFileItem::defaultACL() const
{
    if (!d) {
        return KACL();
    }

    // Extract it from the KIO::UDSEntry
    const QString fieldVal = entry().stringValue(KIO::UDSEntry::UDS_DEFAULT_ACL_STRING);
    if (!fieldVal.isEmpty()) {
        return KACL(fieldVal);
    } else {
        return KACL();
    }
}

QDateTime KFileItem::time(FileTimes which) const
{
    if (!d) {
        return QDateTime();
    }

    return d->time(which);
}

QString KFileItem::user() const
{
    if (!d) {
        return QString();
    }

    return entry().stringValue(KIO::UDSEntry::UDS_USER);
}

QString KFileItem::group() const
{
    if (!d) {
        return QString();
    }

    return entry().stringValue(KIO::UDSEntry::UDS_GROUP);
}

bool KFileItemPrivate::isSlow() const
{
    if (m_slow == SlowUnknown) {
        const QString path = localPath();
        if (!path.isEmpty()) {
            const KFileSystemType::Type fsType = KFileSystemType::fileSystemType(path);
            m_slow = (fsType == KFileSystemType::Nfs || fsType == KFileSystemType::Smb) ? Slow : Fast;
        } else {
            m_slow = Slow;
        }
    }
    return m_slow == Slow;
}

bool KFileItem::isSlow() const
{
    if (!d) {
        return false;
    }

    return d->isSlow();
}

QString KFileItem::mimetype() const
{
    if (!d) {
        return QString();
    }

    KFileItem *that = const_cast<KFileItem *>(this);
    return that->determineMimeType().name();
}

QMimeType KFileItem::determineMimeType() const
{
    if (!d) {
        return QMimeType();
    }

    if (!d->m_mimeType.isValid() || !d->m_bMimeTypeKnown) {
        QMimeDatabase db;
        if (isDir()) {
            d->m_mimeType = db.mimeTypeForName(QStringLiteral("inode/directory"));
        } else {
            bool isLocalUrl;
            const QUrl url = mostLocalUrl(&isLocalUrl);
            d->determineMimeTypeHelper(url);

            // was:  d->m_mimeType = KMimeType::findByUrl( url, d->m_fileMode, isLocalUrl );
            // => we are no longer using d->m_fileMode for remote URLs.
            Q_ASSERT(d->m_mimeType.isValid());
            //qDebug() << d << "finding final MIME type for" << url << ":" << d->m_mimeType.name();
        }
        d->m_bMimeTypeKnown = true;
    }

    if (d->m_delayedMimeTypes) { // if we delayed getting the iconName up till now, this is the right point in time to do so
        d->m_delayedMimeTypes = false;
        d->m_useIconNameCache = false;
        (void)iconName();
    }

    return d->m_mimeType;
}

bool KFileItem::isMimeTypeKnown() const
{
    if (!d) {
        return false;
    }

    // The MIME type isn't known if determineMimeType was never called (on-demand determination)
    // or if this fileitem has a guessed MIME type (e.g. ftp symlink) - in which case
    // it always remains "not fully determined"
    return d->m_bMimeTypeKnown && d->m_guessedMimeType.isEmpty();
}

static bool isDirectoryMounted(const QUrl &url)
{
    // Stating .directory files can cause long freezes when e.g. /home
    // uses autofs for every user's home directory, i.e. opening /home
    // in a file dialog will mount every single home directory.
    // These non-mounted directories can be identified by having 0 size.
    // There are also other directories with 0 size, such as /proc, that may
    // be mounted, but those are unlikely to contain .directory (and checking
    // this would require checking with KMountPoint).

    // TODO: maybe this could be checked with KFileSystemType instead?
    QFileInfo info(url.toLocalFile());
    if (info.isDir() && info.size() == 0) {
        return false;
    }
    return true;
}

bool KFileItem::isFinalIconKnown() const
{
    if (!d) {
        return false;
    }
    return d->m_bMimeTypeKnown && (!d->m_delayedMimeTypes);
}

// KDE5 TODO: merge with comment()? Need to see what lxr says about the usage of both.
QString KFileItem::mimeComment() const
{
    if (!d) {
        return QString();
    }

    const QString displayType = d->m_entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_TYPE);
    if (!displayType.isEmpty()) {
        return displayType;
    }

    bool isLocalUrl;
    QUrl url = mostLocalUrl(&isLocalUrl);

    QMimeType mime = currentMimeType();
    // This cannot move to kio_file (with UDS_DISPLAY_TYPE) because it needs
    // the MIME type to be determined, which is done here, and possibly delayed...
    if (isLocalUrl && !d->isSlow() && mime.inherits(QStringLiteral("application/x-desktop"))) {
        KDesktopFile cfg(url.toLocalFile());
        QString comment = cfg.desktopGroup().readEntry("Comment");
        if (!comment.isEmpty()) {
            return comment;
        }
    }

    // Support for .directory file in directories
    if (isLocalUrl && isDir() && !d->isSlow() && isDirectoryMounted(url)) {
        QUrl u(url);
        u.setPath(concatPaths(u.path(), QStringLiteral(".directory")));
        const KDesktopFile cfg(u.toLocalFile());
        const QString comment = cfg.readComment();
        if (!comment.isEmpty()) {
            return comment;
        }
    }

    const QString comment = mime.comment();
    //qDebug() << "finding comment for " << url.url() << " : " << d->m_mimeType->name();
    if (!comment.isEmpty()) {
        return comment;
    } else {
        return mime.name();
    }
}

static QString iconFromDirectoryFile(const QString &path)
{
    const QString filePath = path + QLatin1String("/.directory");
    if (!QFileInfo(filePath).isFile()) { // exists -and- is a file
        return QString();
    }

    KDesktopFile cfg(filePath);
    QString icon = cfg.readIcon();

    const KConfigGroup group = cfg.desktopGroup();
    const QString emptyIcon = group.readEntry("EmptyIcon");
    if (!emptyIcon.isEmpty()) {
        bool isDirEmpty = true;
        QDirIterator dirIt(path, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        while (dirIt.hasNext()) {
            dirIt.next();
            if (dirIt.fileName() != QLatin1String(".directory")) {
                isDirEmpty = false;
                break;
            }
        }
        if (isDirEmpty) {
            icon = emptyIcon;
        }
    }

    if (icon.startsWith(QLatin1String("./"))) {
        // path is relative with respect to the location
        // of the .directory file (#73463)
        return path + icon.midRef(1);
    }
    return icon;
}

static QString iconFromDesktopFile(const QString &path)
{
    KDesktopFile cfg(path);
    const QString icon = cfg.readIcon();
    if (cfg.hasLinkType()) {
        const KConfigGroup group = cfg.desktopGroup();
        const QString emptyIcon = group.readEntry("EmptyIcon");
        if (!emptyIcon.isEmpty()) {
            const QString u = cfg.readUrl();
            const QUrl url(u);
            if (url.scheme() == QLatin1String("trash")) {
                // We need to find if the trash is empty, preferably without using a KIO job.
                // So instead kio_trash leaves an entry in its config file for us.
                KConfig trashConfig(QStringLiteral("trashrc"), KConfig::SimpleConfig);
                if (trashConfig.group("Status").readEntry("Empty", true)) {
                    return emptyIcon;
                }
            }
        }
    }
    return icon;
}

QString KFileItem::iconName() const
{
    if (!d) {
        return QString();
    }

    if (d->m_useIconNameCache && !d->m_iconName.isEmpty()) {
        return d->m_iconName;
    }

    d->m_iconName = d->m_entry.stringValue(KIO::UDSEntry::UDS_ICON_NAME);
    if (!d->m_iconName.isEmpty()) {
        d->m_useIconNameCache = d->m_bMimeTypeKnown;
        return d->m_iconName;
    }

    bool isLocalUrl;
    QUrl url = mostLocalUrl(&isLocalUrl);

    QMimeDatabase db;
    QMimeType mime;
    // Use guessed MIME type for the icon
    if (!d->m_guessedMimeType.isEmpty()) {
        mime = db.mimeTypeForName(d->m_guessedMimeType);
    } else {
        mime = currentMimeType();
    }

    const bool delaySlowOperations = d->m_delayedMimeTypes;

    if (isLocalUrl && !delaySlowOperations && mime.inherits(QStringLiteral("application/x-desktop"))) {
        d->m_iconName = iconFromDesktopFile(url.toLocalFile());
        if (!d->m_iconName.isEmpty()) {
            d->m_useIconNameCache = d->m_bMimeTypeKnown;
            return d->m_iconName;
        }
    }

    if (isLocalUrl && !delaySlowOperations && isDir()) {
        if (isDirectoryMounted(url)) {
            d->m_iconName = iconFromDirectoryFile(url.toLocalFile());
            if (!d->m_iconName.isEmpty()) {
                d->m_useIconNameCache = d->m_bMimeTypeKnown;
                return d->m_iconName;
            }
        }

        d->m_iconName = KIOPrivate::iconForStandardPath(url.toLocalFile());
        if (!d->m_iconName.isEmpty()) {
            d->m_useIconNameCache = d->m_bMimeTypeKnown;
            return d->m_iconName;
        }
    }

    d->m_iconName = mime.iconName();
    d->m_useIconNameCache = d->m_bMimeTypeKnown;
    return d->m_iconName;
}

/**
 * Returns true if this is a desktop file.
 * MIME type determination is optional.
 */
static bool checkDesktopFile(const KFileItem &item, bool _determineMimeType)
{
    // only local files
    bool isLocalUrl;
    item.mostLocalUrl(&isLocalUrl);
    if (!isLocalUrl) {
        return false;
    }

    // only regular files
    if (!item.isRegularFile()) {
        return false;
    }

    // only if readable
    if (!item.isReadable()) {
        return false;
    }

    // return true if desktop file
    QMimeType mime = _determineMimeType ? item.determineMimeType() : item.currentMimeType();
    return mime.inherits(QStringLiteral("application/x-desktop"));
}

QStringList KFileItem::overlays() const
{
    if (!d) {
        return QStringList();
    }

    d->ensureInitialized();

    QStringList names = d->m_entry.stringValue(KIO::UDSEntry::UDS_ICON_OVERLAY_NAMES).split(QLatin1Char(','), Qt::SkipEmptyParts);

    if (d->m_bLink) {
        names.append(QStringLiteral("emblem-symbolic-link"));
    }

    if (!isReadable()) {
        names.append(QStringLiteral("emblem-locked"));
    }

    if (checkDesktopFile(*this, false)) {
        KDesktopFile cfg(localPath());
        const KConfigGroup group = cfg.desktopGroup();

        // Add a warning emblem if this is an executable desktop file
        // which is untrusted.
        if (group.hasKey("Exec") && !KDesktopFile::isAuthorizedDesktopFile(localPath())) {
            names.append(QStringLiteral("emblem-important"));
        }

        if (cfg.hasDeviceType()) {
            const QString dev = cfg.readDevice();
            if (!dev.isEmpty()) {
                KMountPoint::Ptr mountPoint = KMountPoint::currentMountPoints().findByDevice(dev);
                if (mountPoint) { // mounted?
                    names.append(QStringLiteral("emblem-mounted"));
                }
            }
        }
    }

    if (isHidden()) {
        names.append(QStringLiteral("hidden"));
    }
#ifndef Q_OS_WIN
    if (isDir()) {
        bool isLocalUrl;
        const QUrl url = mostLocalUrl(&isLocalUrl);
        if (isLocalUrl) {
            const QString path = url.toLocalFile();
            if (KSambaShare::instance()->isDirectoryShared(path)
                    || KNFSShare::instance()->isDirectoryShared(path)) {
                names.append(QStringLiteral("emblem-shared"));
            }
        }
    }
#endif  // Q_OS_WIN

    return names;
}

QString KFileItem::comment() const
{
    if (!d) {
        return QString();
    }

    return d->m_entry.stringValue(KIO::UDSEntry::UDS_COMMENT);
}

bool KFileItem::isReadable() const
{
    if (!d) {
        return false;
    }

    d->ensureInitialized();

    /*
      struct passwd * user = getpwuid( geteuid() );
      bool isMyFile = (QString::fromLocal8Bit(user->pw_name) == d->m_user);
      // This gets ugly for the group....
      // Maybe we want a static QString for the user and a static QStringList
      // for the groups... then we need to handle the deletion properly...
      */

    if (d->m_permissions != KFileItem::Unknown) {
        const mode_t readMask = S_IRUSR | S_IRGRP | S_IROTH;
        // No read permission at all
        if ((d->m_permissions & readMask) == 0) {
            return false;
        }

        // Read permissions for all: save a stat call
        if ((d->m_permissions & readMask) == readMask) {
            return true;
        }
    }

    // Or if we can't read it - not network transparent
    if (d->m_bIsLocalUrl && !QFileInfo(d->m_url.toLocalFile()).isReadable()) {
        return false;
    }

    return true;
}

bool KFileItem::isWritable() const
{
    if (!d) {
        return false;
    }

    d->ensureInitialized();

    /*
      struct passwd * user = getpwuid( geteuid() );
      bool isMyFile = (QString::fromLocal8Bit(user->pw_name) == d->m_user);
      // This gets ugly for the group....
      // Maybe we want a static QString for the user and a static QStringList
      // for the groups... then we need to handle the deletion properly...
      */

    if (d->m_permissions != KFileItem::Unknown) {
        // No write permission at all
        if (!(S_IWUSR & d->m_permissions) && !(S_IWGRP & d->m_permissions) && !(S_IWOTH & d->m_permissions)) {
            return false;
        }
    }

    // Or if we can't write it - not network transparent
    if (d->m_bIsLocalUrl) {
        return QFileInfo(d->m_url.toLocalFile()).isWritable();
    } else {
        return KProtocolManager::supportsWriting(d->m_url);
    }
}

bool KFileItem::isHidden() const
{
    if (!d) {
        return false;
    }

    // The kioslave can specify explicitly that a file is hidden or shown
    if (d->m_hidden != KFileItemPrivate::Auto) {
        return d->m_hidden == KFileItemPrivate::Hidden;
    }

    // Prefer the filename that is part of the URL, in case the display name is different.
    QString fileName = d->m_url.fileName();
    if (fileName.isEmpty()) { // e.g. "trash:/"
        fileName = d->m_strName;
    }
    return fileName.length() > 1 && fileName[0] == QLatin1Char('.');  // Just "." is current directory, not hidden.
}

void KFileItem::setHidden()
{
    if (d) {
        d->m_hidden = KFileItemPrivate::Hidden;
    }
}

bool KFileItem::isDir() const
{
    if (!d) {
        return false;
    }

    if (d->m_bSkipMimeTypeFromContent) {
        return false;
    }

    d->ensureInitialized();

    if (d->m_fileMode == KFileItem::Unknown) {
        // Probably the file was deleted already, and KDirLister hasn't told the world yet.
        //qDebug() << d << url() << "can't say -> false";
        return false; // can't say for sure, so no
    }
    return (d->m_fileMode & QT_STAT_MASK) == QT_STAT_DIR;
}

bool KFileItem::isFile() const
{
    if (!d) {
        return false;
    }

    return !isDir();
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(4, 0)
bool KFileItem::acceptsDrops() const
{
    // A directory ?
    if (isDir()) {
        return isWritable();
    }

    // But only local .desktop files and executables
    if (!d->m_bIsLocalUrl) {
        return false;
    }

    if (mimetype() == QLatin1String("application/x-desktop")) {
        return true;
    }

    // Executable, shell script ... ?
    if (QFileInfo(d->m_url.toLocalFile()).isExecutable()) {
        return true;
    }

    return false;
}
#endif

QString KFileItem::getStatusBarInfo() const
{
    if (!d) {
        return QString();
    }

    QString text = d->m_strText;
    const QString comment = mimeComment();

    if (d->m_bLink) {
        text += QLatin1Char(' ');
        if (comment.isEmpty()) {
            text += i18n("(Symbolic Link to %1)", linkDest());
        } else {
            text += i18n("(%1, Link to %2)", comment, linkDest());
        }
    } else if (targetUrl() != url()) {
        text += i18n(" (Points to %1)", targetUrl().toDisplayString());
    } else if ((d->m_fileMode & QT_STAT_MASK) == QT_STAT_REG) {
        text += QStringLiteral(" (%1, %2)").arg(comment, KIO::convertSize(size()));
    } else {
        text += QStringLiteral(" (%1)").arg(comment);
    }
    return text;
}

bool KFileItem::cmp(const KFileItem &item) const
{
    if (!d && !item.d) {
        return true;
    }

    if (!d || !item.d) {
        return false;
    }

    return d->cmp(*item.d);
}

bool KFileItem::operator==(const KFileItem &other) const
{
    if (!d && !other.d) {
        return true;
    }

    if (!d || !other.d) {
        return false;
    }

    return d->m_url == other.d->m_url;
}

bool KFileItem::operator!=(const KFileItem &other) const
{
    return !operator==(other);
}

bool KFileItem::operator<(const KFileItem &other) const
{
    if (!other.d) {
        return false;
    }
    if (!d) {
        return other.d->m_url.isValid();
    }
    return d->m_url < other.d->m_url;
}

bool KFileItem::operator<(const QUrl &other) const
{
    if (!d) {
        return other.isValid();
    }
    return d->m_url < other;
}

KFileItem::operator QVariant() const
{
    return QVariant::fromValue(*this);
}

QString KFileItem::permissionsString() const
{
    if (!d) {
        return QString();
    }

    d->ensureInitialized();

    if (d->m_access.isNull() && d->m_permissions != KFileItem::Unknown) {
        d->m_access = d->parsePermissions(d->m_permissions);
    }

    return d->m_access;
}

// check if we need to cache this
QString KFileItem::timeString(FileTimes which) const
{
    if (!d) {
        return QString();
    }

    return QLocale::system().toString(d->time(which), QLocale::LongFormat);
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(4, 0)
QString KFileItem::timeString(unsigned int which) const
{
    if (!d) {
        return QString();
    }

    switch (which) {
    case KIO::UDSEntry::UDS_ACCESS_TIME:
        return timeString(AccessTime);
    case KIO::UDSEntry::UDS_CREATION_TIME:
        return timeString(CreationTime);
    case KIO::UDSEntry::UDS_MODIFICATION_TIME:
    default:
        return timeString(ModificationTime);
    }
}
#endif

#if KIOCORE_BUILD_DEPRECATED_SINCE(4, 0)
void KFileItem::assign(const KFileItem &item)
{
    *this = item;
}
#endif

QUrl KFileItem::mostLocalUrl(bool *local) const
{
    if (!d) {
        return QUrl();
    }

    const QString local_path = localPath();
    if (!local_path.isEmpty()) {
        if (local) {
            *local = true;
        }
        return QUrl::fromLocalFile(local_path);
    } else {
        if (local) {
            *local = d->m_bIsLocalUrl;
        }
        return d->m_url;
    }
}

QDataStream &operator<< (QDataStream &s, const KFileItem &a)
{
    if (a.d) {
        // We don't need to save/restore anything that refresh() invalidates,
        // since that means we can re-determine those by ourselves.
        s << a.d->m_url;
        s << a.d->m_strName;
        s << a.d->m_strText;
    } else {
        s << QUrl();
        s << QString();
        s << QString();
    }

    return s;
}

QDataStream &operator>> (QDataStream &s, KFileItem &a)
{
    QUrl url;
    QString strName, strText;

    s >> url;
    s >> strName;
    s >> strText;

    if (!a.d) {
        qCWarning(KIO_CORE) << "null item";
        return s;
    }

    if (url.isEmpty()) {
        a.d = nullptr;
        return s;
    }

    a.d->m_url = url;
    a.d->m_strName = strName;
    a.d->m_strText = strText;
    a.d->m_bIsLocalUrl = a.d->m_url.isLocalFile();
    a.d->m_bMimeTypeKnown = false;
    a.refresh();

    return s;
}

QUrl KFileItem::url() const
{
    if (!d) {
        return QUrl();
    }

    return d->m_url;
}

mode_t KFileItem::permissions() const
{
    if (!d) {
        return 0;
    }

    d->ensureInitialized();

    return d->m_permissions;
}

mode_t KFileItem::mode() const
{
    if (!d) {
        return 0;
    }

    d->ensureInitialized();

    return d->m_fileMode;
}

bool KFileItem::isLink() const
{
    if (!d) {
        return false;
    }

    d->ensureInitialized();

    return d->m_bLink;
}

bool KFileItem::isLocalFile() const
{
    if (!d) {
        return false;
    }

    return d->m_bIsLocalUrl;
}

QString KFileItem::text() const
{
    if (!d) {
        return QString();
    }

    return d->m_strText;
}

QString KFileItem::name(bool lowerCase) const
{
    if (!d) {
        return QString();
    }

    if (!lowerCase) {
        return d->m_strName;
    } else if (d->m_strLowerCaseName.isNull()) {
        d->m_strLowerCaseName = d->m_strName.toLower();
    }
    return d->m_strLowerCaseName;
}

QUrl KFileItem::targetUrl() const
{
    if (!d) {
        return QUrl();
    }

    const QString targetUrlStr = d->m_entry.stringValue(KIO::UDSEntry::UDS_TARGET_URL);
    if (!targetUrlStr.isEmpty()) {
        return QUrl(targetUrlStr);
    } else {
        return url();
    }
}

/*
 * MIME type handling.
 *
 * Initial state: m_mimeType = QMimeType().
 * When currentMimeType() is called first: fast MIME type determination,
 *   might either find an accurate MIME type (-> Final state), otherwise we
 *   set m_mimeType but not m_bMimeTypeKnown (-> Intermediate state)
 * Intermediate state: determineMimeType() does the real determination -> Final state.
 *
 * If delayedMimeTypes isn't set, then we always go to the Final state directly.
 */

QMimeType KFileItem::currentMimeType() const
{
    if (!d) {
        return QMimeType();
    }

    if (!d->m_mimeType.isValid()) {
        // On-demand fast (but not always accurate) MIME type determination
        Q_ASSERT(!d->m_url.isEmpty());
        QMimeDatabase db;
        if (isDir()) {
            d->m_mimeType = db.mimeTypeForName(QStringLiteral("inode/directory"));
            return d->m_mimeType;
        }
        const QUrl url = mostLocalUrl();
        if (d->m_delayedMimeTypes) {
            const QList<QMimeType> mimeTypes = db.mimeTypesForFileName(url.path());
            if (mimeTypes.isEmpty()) {
                d->m_mimeType = db.mimeTypeForName(QStringLiteral("application/octet-stream"));
                d->m_bMimeTypeKnown = false;
            } else {
                d->m_mimeType = mimeTypes.first();
                // If there were conflicting globs. determineMimeType will be able to do better.
                d->m_bMimeTypeKnown = (mimeTypes.count() == 1);
            }
        } else {
            // ## d->m_fileMode isn't used anymore (for remote urls)
            d->determineMimeTypeHelper(url);
            d->m_bMimeTypeKnown = true;
        }
    }
    return d->m_mimeType;
}

KIO::UDSEntry KFileItem::entry() const
{
    if (!d) {
        return KIO::UDSEntry();
    }

    d->ensureInitialized();

    return d->m_entry;
}

bool KFileItem::isNull() const
{
    return d == nullptr;
}

KFileItemList::KFileItemList()
{
}

KFileItemList::KFileItemList(const QList<KFileItem> &items)
    : QList<KFileItem>(items)
{
}

KFileItemList::KFileItemList(std::initializer_list<KFileItem> items)
    : QList<KFileItem>(items)
{
}

KFileItem KFileItemList::findByName(const QString &fileName) const
{
    const_iterator it = begin();
    const const_iterator itend = end();
    for (; it != itend; ++it) {
        if ((*it).name() == fileName) {
            return *it;
        }
    }
    return KFileItem();
}

KFileItem KFileItemList::findByUrl(const QUrl &url) const
{
    const_iterator it = begin();
    const const_iterator itend = end();
    for (; it != itend; ++it) {
        if ((*it).url() == url) {
            return *it;
        }
    }
    return KFileItem();
}

QList<QUrl> KFileItemList::urlList() const
{
    QList<QUrl> lst;
    const_iterator it = begin();
    const const_iterator itend = end();
    for (; it != itend; ++it) {
        lst.append((*it).url());
    }
    return lst;
}

QList<QUrl> KFileItemList::targetUrlList() const
{
    QList<QUrl> lst;
    const_iterator it = begin();
    const const_iterator itend = end();
    for (; it != itend; ++it) {
        lst.append((*it).targetUrl());
    }
    return lst;
}

bool KFileItem::isDesktopFile() const
{
    return checkDesktopFile(*this, true);
}

bool KFileItem::isRegularFile() const
{
    if (!d) {
        return false;
    }

    d->ensureInitialized();

    return (d->m_fileMode & QT_STAT_MASK) == QT_STAT_REG;
}

QDebug operator<<(QDebug stream, const KFileItem &item)
{
    QDebugStateSaver saver(stream);
    stream.nospace();
    if (item.isNull()) {
        stream << "[null KFileItem]";
    } else {
        stream << "[KFileItem for " << item.url() << "]";
    }
    return stream;
}
