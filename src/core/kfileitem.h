/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1999-2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KFILEITEM_H
#define KFILEITEM_H

#include "kiocore_export.h"
#include <kio/global.h>
#include <kacl.h>
#include <kio/udsentry.h>
#include <QDateTime>
#include <QUrl>
#include <QFile>

#include <QMimeType>
#include <QList>
#include <qplatformdefs.h>

class KFileItemPrivate;

/**
 * @class KFileItem kfileitem.h <KFileItem>
 *
 * A KFileItem is a generic class to handle a file, local or remote.
 * In particular, it makes it easier to handle the result of KIO::listDir
 * (UDSEntry isn't very friendly to use).
 * It includes many file attributes such as MIME type, icon, text, mode, link...
 *
 * KFileItem is implicitly shared, i.e. it can be used as a value and copied around at almost no cost.
 */
class KIOCORE_EXPORT KFileItem
{
public:
    enum { Unknown = static_cast<mode_t>(-1) };

    /**
     * The timestamps associated with a file.
     * - ModificationTime: the time the file's contents were last modified
     * - AccessTime: the time the file was last accessed (last read or written to)
     * - CreationTime: the time the file was created
     */
    enum FileTimes {
        // warning: don't change without looking at the Private class
        ModificationTime = 0,
        AccessTime = 1,
        CreationTime = 2,
                       //ChangeTime
    };

    enum MimeTypeDetermination {
        NormalMimeTypeDetermination = 0,
        SkipMimeTypeFromContent,
    };

    /**
     * Null KFileItem. Doesn't represent any file, only exists for convenience.
     */
    KFileItem();

    /**
     * Creates an item representing a file, from a UDSEntry.
     * This is the preferred constructor when using KIO::listDir().
     *
     * @param entry the KIO entry used to get the file, contains info about it
     * @param itemOrDirUrl the URL of the item or of the directory containing this item (see urlIsDirectory).
     * @param delayedMimeTypes specifies if the MIME type of the given
     *       URL should be determined immediately or on demand.
     *       See the bool delayedMimeTypes in the KDirLister constructor.
     * @param urlIsDirectory specifies if the url is just the directory of the
     *       fileitem and the filename from the UDSEntry should be used.
     *
     * When creating KFileItems out of the UDSEntry emitted by a KIO list job,
     * use KFileItem(entry, listjob->url(), delayedMimeTypes, true);
     */
    KFileItem(const KIO::UDSEntry &entry, const QUrl &itemOrDirUrl,
              bool delayedMimeTypes = false,
              bool urlIsDirectory = false);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    /**
     * Creates an item representing a file, from all the necessary info for it.
     * @param mode the file mode (according to stat() (e.g. S_IFDIR...)
     * Set to KFileItem::Unknown if unknown. For local files, KFileItem will use stat().
     * @param permissions the access permissions
     * If you set both the mode and the permissions, you save a ::stat() for
     * local files.
     * Set to KFileItem::Unknown if you don't know the mode or the permission.
     * @param url the file url
     *
     * @param delayedMimeTypes specify if the MIME type of the given URL
     *       should be determined immediately or on demand
     * @deprecated since 5.0. Most callers gave Unknown for mode and permissions,
     * so just port to KFileItem(url) and setDelayedMimeTypes(true) if necessary.
     */
    KIOCORE_DEPRECATED_VERSION(5, 0, "See API docs")
    KFileItem(mode_t mode, mode_t permissions, const QUrl &url,
              bool delayedMimeTypes = false);
#endif

    /**
     * Creates an item representing a file, for which the MIME type is already known.
     * @param url the file url
     * @param mimeType the name of the file's MIME type
     * @param mode the mode (S_IFDIR...)
     */
    KFileItem(const QUrl &url, const QString &mimeType = QString(), mode_t mode = KFileItem::Unknown); // KF6 TODO: explicit!

    /**
     * Creates an item representing a file, with the option of skipping MIME type determination.
     * @param url the file url
     * @param mimeTypeDetermination the mode of determining the MIME type:
     *       NormalMimeTypeDetermination by content if local file, i.e. access the file,
     *                                   open and read part of it;
     *                                   by QMimeDatabase::MatchMode::MatchExtension if not local.
     *       SkipMimeTypeFromContent     always by QMimeDatabase::MatchMode::MatchExtension,
     *                                   i.e. won't access the file by stat() or opening it;
     *                                   only suitable for files, directories won't be recognized.
     * @since 5.57
     */
    KFileItem(const QUrl &url, KFileItem::MimeTypeDetermination mimeTypeDetermination);

    /**
     * Copy constructor
     */
    KFileItem(const KFileItem&);

    /**
     * Destructor
     */
    ~KFileItem();

    /**
     * Move constructor
     * @since 5.43
     */
    KFileItem(KFileItem&&);

    /**
     * Copy assignment
     */
    KFileItem& operator=(const KFileItem&);

    /**
     * Move assignment
     * @since 5.43
     */
    KFileItem& operator=(KFileItem&&);

    /**
     * Throw away and re-read (for local files) all information about the file.
     * This is called when the _file_ changes.
     */
    void refresh();

    /**
     * Re-reads MIME type information.
     * This is called when the MIME type database changes.
     */
    void refreshMimeType();

    /**
     * Sets MIME type determination to be immediate or on demand.
     * Call this after the constructor, and before using any MIME-type-related method.
     * @since 5.0
     */
    void setDelayedMimeTypes(bool b);

    /**
     * Returns the url of the file.
     * @return the url of the file
     */
    QUrl url() const;

    /**
     * Sets the item's URL. Do not call unless you know what you are doing!
     * (used for example when an item got renamed).
     * @param url the item's URL
     */
    void setUrl(const QUrl &url);

    /**
     * Sets the item's local path (UDS_LOCAL_PATH). Do not call unless you know what you are doing!
     * This won't change the item's name or URL.
     * (used for example when an item got renamed).
     * @param path the item's local path
     * @since 5.20
     */
    void setLocalPath(const QString &path);

    /**
     * Sets the item's name (i.e. the filename).
     * This is automatically done by setUrl, to set the name from the URL's fileName().
     * This method is provided for some special cases like relative paths as names (KFindPart)
     * @param name the item's name
     */
    void setName(const QString &name);

    /**
     * Returns the permissions of the file (stat.st_mode containing only permissions).
     * @return the permissions of the file
     */
    mode_t permissions() const;

    /**
     * Returns the access permissions for the file as a string.
     * @return the access permission as string
     */
    QString permissionsString() const;

    /**
     * Tells if the file has extended access level information ( Posix ACL )
     * @return true if the file has extend ACL information or false if it hasn't
     */
    bool hasExtendedACL() const;

    /**
     * Returns the access control list for the file.
     * @return the access control list as a KACL
     */
    KACL ACL() const;

    /**
     * Returns the default access control list for the directory.
     * @return the default access control list as a KACL
     */
    KACL defaultACL() const;

    /**
     * Returns the file type (stat.st_mode containing only S_IFDIR, S_IFLNK, ...).
     * @return the file type
     */
    mode_t mode() const;

    /**
     * Returns the owner of the file.
     * @return the file's owner
     */
    QString user() const;

    /**
     * Returns the group of the file.
     * @return the file's group
     */
    QString group() const;

    /**
     * Returns true if this item represents a link in the UNIX sense of
     * a link.
     * @return true if the file is a link
     */
    bool isLink() const;

    /**
     * Returns true if this item represents a directory.
     * @return true if the item is a directory
     */
    bool isDir() const;

    /**
     * Returns true if this item represents a file (and not a directory)
     * @return true if the item is a file
     */
    bool isFile() const;

    /**
     * Checks whether the file or directory is readable. In some cases
     * (remote files), we may return true even though it can't be read.
     * @return true if the file can be read - more precisely,
     *         false if we know for sure it can't
     */
    bool isReadable() const;

    /**
     * Checks whether the file or directory is writable. In some cases
     * (remote files), we may return true even though it can't be written to.
     * @return true if the file or directory can be written to - more precisely,
     *         false if we know for sure it can't
     */
    bool isWritable() const;

    /**
     * Checks whether the file is hidden.
     * @return true if the file is hidden.
     */
    bool isHidden() const;

    /**
     * @return true if the file is a remote URL, or a local file on a network mount.
     * It will return false only for really-local file systems.
     * @since 4.7.4
     */
    bool isSlow() const;

    /**
     * Checks whether the file is a readable local .desktop file,
     * i.e. a file whose path can be given to KDesktopFile
     * @return true if the file is a desktop file.
     * @since 4.1
     */
    bool isDesktopFile() const;

    /**
     * Returns the link destination if isLink() == true.
     * @return the link destination. QString() if the item is not a link
     */
    QString linkDest() const;

    /**
     * Returns the target url of the file, which is the same as url()
     * in cases where the slave doesn't specify UDS_TARGET_URL
     * @return the target url.
     * @since 4.1
     */
    QUrl targetUrl() const;

    /**
     * Returns the local path if isLocalFile() == true or the KIO item has
     * a UDS_LOCAL_PATH atom.
     * @return the item local path, or QString() if not known
     */
    QString localPath() const;

    /**
     * Returns the size of the file, if known.
     * @return the file size, or 0 if not known
     */
    KIO::filesize_t size() const;

    /**
     * @brief For folders, its recursive size:
     * the size of its files plus the recursiveSize of its folder
     *
     * Initially only implemented for trash:/
     *
     * @since 5.70
     * @return The recursive size
     */
    KIO::filesize_t recursiveSize() const;

    /**
     * Requests the modification, access or creation time, depending on @p which.
     * @param which the timestamp
     * @return the time asked for, QDateTime() if not available
     * @see timeString()
     */
    QDateTime time(FileTimes which) const;

    /**
     * Requests the modification, access or creation time as a string, depending
     * on @p which.
     * @param which the timestamp
     * @returns a formatted string of the requested time.
     * @see time
     */
    QString timeString(FileTimes which = ModificationTime) const;

#if KIOCORE_ENABLE_DEPRECATED_SINCE(4, 0)
    KIOCORE_DEPRECATED_VERSION(4, 0, "Use KFileItem::timeString(FileTimes)")
    QString timeString(unsigned int which) const;
#endif

    /**
     * Returns true if the file is a local file.
     * @return true if the file is local, false otherwise
     */
    bool isLocalFile() const;

    /**
     * Returns the text of the file item.
     * It's not exactly the filename since some decoding happens ('%2F'->'/').
     * @return the text of the file item
     */
    QString text() const;

    /**
     * Return the name of the file item (without a path).
     * Similar to text(), but unencoded, i.e. the original name.
     * @param lowerCase if true, the name will be returned in lower case,
     * which is useful to speed up sorting by name, case insensitively.
     * @return the file's name
     */
    QString name(bool lowerCase = false) const;

    /**
     * Returns the MIME type of the file item.
     * If @p delayedMimeTypes was used in the constructor, this will determine
     * the MIME type first. Equivalent to determineMimeType()->name()
     * @return the MIME type of the file
     */
    QString mimetype() const;

    /**
     * Returns the MIME type of the file item.
     * If delayedMimeTypes was used in the constructor, this will determine
     * the MIME type first.
     * @return the MIME type
     */
    QMimeType determineMimeType() const;

    /**
     * Returns the currently known MIME type of the file item.
     * This will not try to determine the MIME type if unknown.
     * @return the known MIME type
     */
    QMimeType currentMimeType() const;

    /**
     * @return true if we have determined the final icon of this file already.
     * @since 4.10.2
     */
    bool isFinalIconKnown() const;

    /**
     * @return true if we have determined the MIME type of this file already,
     * i.e. if determineMimeType() will be fast. Otherwise it will have to
     * find what the MIME type is, which is a possibly slow operation; usually
     * this is delayed until necessary.
     */
    bool isMimeTypeKnown() const;

    /**
     * Returns the user-readable string representing the type of this file,
     * like "OpenDocument Text File".
     * @return the type of this KFileItem
     */
    QString mimeComment() const;

    /**
     * Returns the full path name to the icon that represents
     * this MIME type.
     * @return iconName the name of the file's icon
     */
    QString iconName() const;

    /**
     * Returns the overlays (bitfield of KIconLoader::*Overlay flags) that are used
     * for this item's pixmap. Overlays are used to show for example, whether
     * a file can be modified.
     * @return the overlays of the pixmap
     */
    QStringList overlays() const;

    /**
     * A comment which can contain anything - even rich text. It will
     * simply be displayed to the user as is.
     *
     * @since 4.6
     */
    QString comment() const;

    /**
     * Returns the string to be displayed in the statusbar,
     * e.g. when the mouse is over this item
     * @return the status bar information
     */
    QString getStatusBarInfo() const;

#if KIOCORE_ENABLE_DEPRECATED_SINCE(4, 0)
    /**
     * Returns true if files can be dropped over this item.
     * Contrary to popular belief, not only dirs will return true :)
     * Executables, .desktop files, will do so as well.
     * @return true if you can drop files over the item
     *
     * @deprecated Since 4.0. This logic is application-dependent, the behavior described above
     * mostly makes sense for file managers only.
     * KDirModel has setDropsAllowed for similar (but configurable) logic.
     */
    KIOCORE_DEPRECATED_VERSION(4, 0, "See API docs")
    bool acceptsDrops() const;
#endif

    /**
     * Returns the UDS entry. Used by the tree view to access all details
     * by position.
     * @return the UDS entry
     */
    KIO::UDSEntry entry() const;

    /**
     * Return true if this item is a regular file,
     * false otherwise (directory, link, character/block device, fifo, socket)
     * @since 4.3
     */
    bool isRegularFile() const;

    /**
     * Somewhat like a comparison operator, but more explicit,
     * and it can detect that two fileitems differ if any property of the file item
     * has changed (file size, modification date, etc.). Two items are equal if
     * all properties are equal. In contrast, operator== only compares URLs.
     * @param item the item to compare
     * @return true if all values are equal
     */
    bool cmp(const KFileItem &item) const;

    /**
     * Returns true if both items share the same URL.
     */
    bool operator==(const KFileItem &other) const;

    /**
     * Returns true if both items do not share the same URL.
     */
    bool operator!=(const KFileItem &other) const;

    /**
     * Returns true if this item's URL is lexically less than other's URL; otherwise returns false
     * @since 5.48
     */
    bool operator<(const KFileItem &other) const;

    /**
     * Returns true if this item's URL is lexically less than url other; otherwise returns false
     * @since 5.48
     */
    bool operator<(const QUrl &other) const;

    /**
     * Converts this KFileItem to a QVariant, this allows to use KFileItem
     * in QVariant() constructor
     */
    operator QVariant() const;

#if KIOCORE_ENABLE_DEPRECATED_SINCE(4, 0)
    /**
     * @deprecated Since 4.0, simply use '='
     */
    KIOCORE_DEPRECATED_VERSION(4, 0, "Use KFileItem::operator=(const KFileItem&)")
    void assign(const KFileItem &item);
#endif

    /**
     * Tries to give a local URL for this file item if possible.
     * The given boolean indicates if the returned url is local or not.
     * \since 4.6
     */
    QUrl mostLocalUrl(bool *local = nullptr) const;

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    /**
     * @deprecated since 5.0 add '&' in front of your boolean argument
     */
    KIOCORE_DEPRECATED_VERSION(5, 0, "Use KFileItem::mostLocalUrl(bool *)")
    QUrl mostLocalUrl(bool &local) const { return mostLocalUrl(&local); }
#endif

    /**
     * Return true if default-constructed
     */
    bool isNull() const;

private:
    QSharedDataPointer<KFileItemPrivate> d;

    /**
     * Hides the file.
     */
    void setHidden();

private:
    KIOCORE_EXPORT friend QDataStream &operator<< (QDataStream &s, const KFileItem &a);
    KIOCORE_EXPORT friend QDataStream &operator>> (QDataStream &s, KFileItem &a);

    friend class KFileItemTest;
    friend class KCoreDirListerCache;
};

Q_DECLARE_METATYPE(KFileItem)
Q_DECLARE_TYPEINFO(KFileItem, Q_MOVABLE_TYPE);

inline uint qHash(const KFileItem &item)
{
    return qHash(item.url());
}

/**
 * @class KFileItemList kfileitem.h <KFileItem>
 *
 * List of KFileItems, which adds a few helper
 * methods to QList<KFileItem>.
 */
class KIOCORE_EXPORT KFileItemList : public QList<KFileItem>
{
public:
    /// Creates an empty list of file items.
    KFileItemList();

    /// Creates a new KFileItemList from a QList of file @p items.
    KFileItemList(const QList<KFileItem> &items);

    /// Creates a new KFileItemList from an initializer_list of file @p items.
    /// @since 5.76
    KFileItemList(std::initializer_list<KFileItem> items);

    /**
     * Find a KFileItem by name and return it.
     * @return the item with the given name, or a null-item if none was found
     *         (see KFileItem::isNull())
     */
    KFileItem findByName(const QString &fileName) const;

    /**
     * Find a KFileItem by URL and return it.
     * @return the item with the given URL, or a null-item if none was found
     *         (see KFileItem::isNull())
     */
    KFileItem findByUrl(const QUrl &url) const;

    /// @return the list of URLs that those items represent
    QList<QUrl> urlList() const;

    /// @return the list of target URLs that those items represent
    /// @since 4.2
    QList<QUrl> targetUrlList() const;

    // TODO KDE-5 add d pointer here so that we can merge KFileItemListProperties into KFileItemList
};

KIOCORE_EXPORT QDataStream &operator<< (QDataStream &s, const KFileItem &a);
KIOCORE_EXPORT QDataStream &operator>> (QDataStream &s, KFileItem &a);

/**
 * Support for qDebug() << aFileItem
 * \since 4.4
 */
KIOCORE_EXPORT QDebug operator<<(QDebug stream, const KFileItem &item);

#endif
