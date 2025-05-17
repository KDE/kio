/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1999-2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KFILEITEM_H
#define KFILEITEM_H

#include "kiocore_export.h"
#include <QDateTime>
#include <QFile>
#include <QUrl>
#include <kacl.h>
#include <kio/global.h>
#include <kio/udsentry.h>

#include <QList>
#include <QMimeType>
#include <qplatformdefs.h>

class KFileItemPrivate;

/*!
 * \class KFileItem
 * \inmodule KIOCore
 *
 * \brief A KFileItem is a generic class to handle a file, local or remote.
 *
 * In particular, it makes it easier to handle the result of KIO::listDir
 * (UDSEntry isn't very friendly to use).
 * It includes many file attributes such as MIME type, icon, text, mode, link...
 *
 * KFileItem is implicitly shared, i.e. it can be used as a value and copied around at almost no cost.
 */
class KIOCORE_EXPORT KFileItem
{
    Q_GADGET

    /*!
     * \property KFileItem::url
     */
    Q_PROPERTY(QUrl url READ url WRITE setUrl)

    /*!
     * \property KFileItem::user
     */
    Q_PROPERTY(QString user READ user)

    /*!
     * \property KFileItem::group
     */
    Q_PROPERTY(QString group READ group)

    /*!
     * \property KFileItem::isLink
     */
    Q_PROPERTY(bool isLink READ isLink)

    /*!
     * \property KFileItem::isDir
     */
    Q_PROPERTY(bool isDir READ isDir)

    /*!
     * \property KFileItem::isFile
     */
    Q_PROPERTY(bool isFile READ isFile)

    /*!
     * \property KFileItem::isReadable
     */
    Q_PROPERTY(bool isReadable READ isReadable)

    /*!
     * \property KFileItem::isWritable
     */
    Q_PROPERTY(bool isWritable READ isWritable)

    /*!
     * \property KFileItem::isHidden
     */
    Q_PROPERTY(bool isHidden READ isHidden)

    /*!
     * \property KFileItem::isSlow
     */
    Q_PROPERTY(bool isSlow READ isSlow)

    /*!
     * \property KFileItem::isDesktopFile
     */
    Q_PROPERTY(bool isDesktopFile READ isDesktopFile)

    /*!
     * \property KFileItem::linkDest
     */
    Q_PROPERTY(QString linkDest READ linkDest)

    /*!
     * \property KFileItem::targetUrl
     */
    Q_PROPERTY(QUrl targetUrl READ targetUrl)

    /*!
     * \property KFileItem::localPath
     */
    Q_PROPERTY(QString localPath READ localPath WRITE setLocalPath)

    /*!
     * \property KFileItem::isLocalFile
     */
    Q_PROPERTY(bool isLocalFile READ isLocalFile)

    /*!
     * \property KFileItem::text
     */
    Q_PROPERTY(QString text READ text)

    /*!
     * \property KFileItem::name
     */
    Q_PROPERTY(QString name READ name WRITE setName)

    /*!
     * \property KFileItem::mimetype
     */
    Q_PROPERTY(QString mimetype READ mimetype)

    /*!
     * \property KFileItem::determineMimeType
     */
    Q_PROPERTY(QMimeType determineMimeType READ determineMimeType)

    /*!
     * \property KFileItem::currentMimeType
     */
    Q_PROPERTY(QMimeType currentMimeType READ currentMimeType)

    /*!
     * \property KFileItem::isFinalIconKnown
     */
    Q_PROPERTY(bool isFinalIconKnown READ isFinalIconKnown)

    /*!
     * \property KFileItem::isMimeTypeKnown
     */
    Q_PROPERTY(bool isMimeTypeKnown READ isMimeTypeKnown)

    /*!
     * \property KFileItem::mimeComment
     */
    Q_PROPERTY(QString mimeComment READ mimeComment)

    /*!
     * \property KFileItem::iconName
     */
    Q_PROPERTY(QString iconName READ iconName)

    /*!
     * \property KFileItem::overlays
     */
    Q_PROPERTY(QStringList overlays READ overlays)

    /*!
     * \property KFileItem::comment
     */
    Q_PROPERTY(QString comment READ comment)

    /*!
     * \property KFileItem::getStatusBarInfo
     */
    Q_PROPERTY(QString getStatusBarInfo READ getStatusBarInfo)

    /*!
     * \property KFileItem::isRegularFile
     */
    Q_PROPERTY(bool isRegularFile READ isRegularFile)

public:
    enum {
        Unknown = static_cast<mode_t>(-1)
    };

    /*!
     * The timestamps associated with a file.
     *
     * \value ModificationTime The time the file's contents were last modified
     * \value AccessTime The time the file was last accessed (last read or written to)
     * \value CreationTime The time the file was created
     */
    enum FileTimes {
        // warning: don't change without looking at the Private class
        ModificationTime = 0,
        AccessTime = 1,
        CreationTime = 2,
        // ChangeTime
    };
    Q_ENUM(FileTimes)

    /*!
     * \value NormalMimeTypeDetermination
     * \value SkipMimeTypeFromContent
     */
    enum MimeTypeDetermination {
        NormalMimeTypeDetermination = 0,
        SkipMimeTypeFromContent,
    };
    Q_ENUM(MimeTypeDetermination)

    /*!
     * Null KFileItem. Doesn't represent any file, only exists for convenience.
     */
    KFileItem();

    /*!
     * Creates an item representing a file, from a UDSEntry.
     * This is the preferred constructor when using KIO::listDir().
     *
     * \a entry the KIO entry used to get the file, contains info about it
     *
     * \a itemOrDirUrl the URL of the item or of the directory containing this item (see urlIsDirectory).
     *
     * \a delayedMimeTypes specifies if the MIME type of the given
     *       URL should be determined immediately or on demand.
     *       See the bool delayedMimeTypes in the KDirLister constructor.
     *
     * \a urlIsDirectory specifies if the url is just the directory of the
     *       fileitem and the filename from the UDSEntry should be used.
     *
     * When creating KFileItems out of the UDSEntry emitted by a KIO list job,
     * use KFileItem(entry, listjob->url(), delayedMimeTypes, true);
     */
    KFileItem(const KIO::UDSEntry &entry, const QUrl &itemOrDirUrl, bool delayedMimeTypes = false, bool urlIsDirectory = false);

    /*!
     * Creates an item representing a file, for which the MIME type is already known.
     *
     * \a url the file url
     *
     * \a mimeType the name of the file's MIME type
     *
     * \a mode the mode (S_IFDIR...)
     */
    explicit KFileItem(const QUrl &url, const QString &mimeType = QString(), mode_t mode = KFileItem::Unknown);

    /*!
     * Creates an item representing a file, with the option of skipping MIME type determination.
     *
     * \a url the file url
     *
     * \a mimeTypeDetermination the mode of determining the MIME type:
     * \list
     * \li NormalMimeTypeDetermination: By content if local file, i.e. access the file,
     *                                   open and read part of it;
     *                                   by QMimeDatabase::MatchMode::MatchExtension if not local.
     * \li SkipMimeTypeFromContent: Always by QMimeDatabase::MatchMode::MatchExtension,
     *                                   i.e. won't access the file by stat() or opening it;
     *                                   only suitable for files, directories won't be recognized.
     * \endlist
     * \since 5.57
     */
    KFileItem(const QUrl &url, KFileItem::MimeTypeDetermination mimeTypeDetermination);

    /*!
     * Copy constructor
     */
    KFileItem(const KFileItem &);

    ~KFileItem();

    KFileItem(KFileItem &&);

    KFileItem &operator=(const KFileItem &);

    KFileItem &operator=(KFileItem &&);

    /*!
     * Throw away and re-read (for local files) all information about the file.
     * This is called when the _file_ changes.
     */
    void refresh();

    /*!
     * Re-reads MIME type information.
     * This is called when the MIME type database changes.
     */
    void refreshMimeType();

    /*!
     * Sets MIME type determination to be immediate or on demand.
     * Call this after the constructor, and before using any MIME-type-related method.
     *  5.0
     */
    void setDelayedMimeTypes(bool b);

    /*!
     * Returns the url of the file.
     */
    QUrl url() const;

    /*!
     * Sets the item's URL. Do not call unless you know what you are doing!
     * (used for example when an item got renamed).
     *
     * \a url the item's URL
     */
    void setUrl(const QUrl &url);

    /*!
     * Sets the item's local path (UDS_LOCAL_PATH). Do not call unless you know what you are doing!
     * This won't change the item's name or URL.
     * (used for example when an item got renamed).
     *
     * \a path the item's local path
     * \since 5.20
     */
    void setLocalPath(const QString &path);

    /*!
     * Sets the item's name (i.e.\ the filename).
     * This is automatically done by setUrl, to set the name from the URL's fileName().
     * This method is provided for some special cases like relative paths as names (KFindPart)
     *
     * \a name the item's name
     */
    void setName(const QString &name);

    /*!
     * Returns the permissions of the file (stat.st_mode containing only permissions).
     */
    mode_t permissions() const;

    /*!
     * Returns the access permissions for the file as a string.
     */
    QString permissionsString() const;

    /*!
     * Tells if the file has extended access level information ( Posix ACL )
     */
    bool hasExtendedACL() const;

    /*!
     * Returns the access control list for the file.
     */
    KACL ACL() const;

    /*!
     * Returns the default access control list for the directory.
     */
    KACL defaultACL() const;

    /*!
     * Returns the file type (stat.st_mode containing only S_IFDIR, S_IFLNK, ...).
     */
    mode_t mode() const;

    /*!
     * Returns the file's owner's user id.
     * Available only on supported protocols.
     * \since 6.0
     */
    int userId() const;

    /*!
     * Returns the file's owner's group id.
     * Available only on supported protocols.
     * \since 6.0
     */
    int groupId() const;

    /*!
     * Returns the owner of the file.
     */
    QString user() const;

    /*!
     * Returns the group of the file.
     */
    QString group() const;

    /*!
     * Returns \c true if this item represents a link in the UNIX sense of
     * a link.
     */
    bool isLink() const;

    /*!
     * Returns \c true if this item represents a directory.
     */
    bool isDir() const;

    /*!
     * Returns \c true if this item represents a file (and not a directory)
     */
    bool isFile() const;

    /*!
     * Checks whether the file or directory is readable. In some cases
     * (remote files), we may return \c true even though it can't be read.
     *
     * Returns \c true if the file can be read - more precisely,
     *         false if we know for sure it can't
     */
    bool isReadable() const;

    /*!
     * Checks whether the file or directory is writable. In some cases
     * (remote files), we may return \c true even though it can't be written to.
     *
     * Returns \c true if the file or directory can be written to - more precisely,
     *         false if we know for sure it can't
     */
    bool isWritable() const;

    /*!
     * Checks whether the file is hidden.
     *
     * Returns \c true if the file is hidden.
     */
    bool isHidden() const;

    /*!
     * Returns \c true if the file is a remote URL, or a local file on a network mount.
     * It will return \c false only for really-local file systems.
     * \since 4.7.4
     */
    bool isSlow() const;

    /*!
     * Checks whether the file is a readable local .desktop file,
     * i.e. a file whose path can be given to KDesktopFile
     *
     * Returns \c true if the file is a desktop file.
     */
    bool isDesktopFile() const;

    /*!
     * Returns the link destination if isLink() == true, or QString() if the item is not a link
     */
    QString linkDest() const;

    /*!
     * Returns the target url of the file, which is the same as url()
     * in cases where the worker doesn't specify UDS_TARGET_URL
     */
    QUrl targetUrl() const;

    /*!
     * Returns the local path if isLocalFile() == true or the KIO item has
     * a UDS_LOCAL_PATH atom.
     *
     * Treat it as a readonly path to open/list contents, use original url to move/delete files.
     *
     * Returns the item local path, or QString() if not known
     */
    QString localPath() const;

    /*!
     * Returns the size of the file, if known.
     *
     * Returns 0 otherwise.
     */
    KIO::filesize_t size() const;

    /*!
     * For folders, returns its recursive size: the size of its files plus the recursiveSize of its folder
     *
     * Initially only implemented for trash:/
     *
     * \since 5.70
     */
    KIO::filesize_t recursiveSize() const;

    /*!
     * Requests the modification, access or creation time, depending on \a which.
     *
     * \a which the timestamp
     *
     * Returns the time asked for, QDateTime() if not available
     *
     * \sa timeString()
     */
    Q_INVOKABLE QDateTime time(KFileItem::FileTimes which) const;

    /*!
     * Requests the modification, access or creation time as a string, depending
     * on \a which.
     *
     * \a which the timestamp
     *
     * Returns a formatted string of the requested time.
     *
     * \sa time()
     */
    Q_INVOKABLE QString timeString(KFileItem::FileTimes which = ModificationTime) const;

    /*!
     * Returns \c true if the file is a local file.
     */
    bool isLocalFile() const;

    /*!
     * Returns the text of the file item.
     *
     * It's not exactly the filename since some decoding happens ('%2F'->'/').
     */
    QString text() const;

    /*!
     * Return the name of the file item (without a path).
     *
     * Similar to text(), but unencoded, i.e. the original name.
     *
     * \a lowerCase if true, the name will be returned in lower case,
     * which is useful to speed up sorting by name, case insensitively.
     *
     * Returns the file's name
     */
    QString name(bool lowerCase = false) const;

    /*!
     * Returns the MIME type of the file item.
     *
     * If \c delayedMimeTypes was used in the constructor, this will determine
     * the MIME type first. Equivalent to determineMimeType()->name()
     */
    QString mimetype() const;

    /*!
     * Returns the MIME type of the file item.
     *
     * If delayedMimeTypes was used in the constructor, this will determine
     * the MIME type first.
     */
    QMimeType determineMimeType() const;

    /*!
     * Returns the currently known MIME type of the file item.
     * This will not try to determine the MIME type if unknown.
     */
    QMimeType currentMimeType() const;

    /*!
     * Returns \c true if we have determined the final icon of this file already.
     * \since 4.10.2
     */
    bool isFinalIconKnown() const;

    /*!
     * Returns \c true if we have determined the MIME type of this file already,
     * i.e. if determineMimeType() will be fast. Otherwise it will have to
     * find what the MIME type is, which is a possibly slow operation; usually
     * this is delayed until necessary.
     */
    bool isMimeTypeKnown() const;

    /*!
     * Returns the user-readable string representing the type of this file,
     * like "OpenDocument Text File".
     */
    QString mimeComment() const;

    /*!
     * Returns the full path name to the icon that represents
     * this MIME type.
     */
    QString iconName() const;

    /*!
     * Returns the overlays (bitfield of KIconLoader::*Overlay flags) that are used
     * for this item's pixmap. Overlays are used to show for example, whether
     * a file can be modified.
     */
    QStringList overlays() const;

    /*!
     * A comment which can contain anything - even rich text. It will
     * simply be displayed to the user as is.
     */
    QString comment() const;

    /*!
     * Returns the string to be displayed in the statusbar,
     * e.g.\ when the mouse is over this item.
     */
    QString getStatusBarInfo() const;

    /*!
     * Returns the UDS entry. Used by the tree view to access all details
     * by position.
     */
    KIO::UDSEntry entry() const;

    /*!
     * Return \c true if this item is a regular file,
     * \c false otherwise (directory, link, character/block device, fifo, socket)
     */
    bool isRegularFile() const;

    /*!
     * Returns the file extension
     *
     * Similar to QFileInfo::suffix except it takes into account UDS_DISPLAY_NAME and saves a stat call
     * \since 6.0
     */
    QString suffix() const;

    /*!
     * Somewhat like a comparison operator, but more explicit,
     * and it can detect that two fileitems differ if any property of the file item
     * has changed (file size, modification date, etc.). Two items are equal if
     * all properties are equal. In contrast, operator== only compares URLs.
     *
     * \a item the item to compare
     *
     * Returns \c true if all values are equal
     */
    bool cmp(const KFileItem &item) const;

    /*!
     * Returns \c true if both items share the same URL.
     */
    bool operator==(const KFileItem &other) const;

    /*!
     * Returns \c true if both items do not share the same URL.
     */
    bool operator!=(const KFileItem &other) const;

    /*!
     * Returns \c true if this item's URL is lexically less than other's URL; otherwise returns \c false
     * \since 5.48
     */
    bool operator<(const KFileItem &other) const;

    /*!
     * Returns \c true if this item's URL is lexically less than url other; otherwise returns \c false
     * \since 5.48
     */
    bool operator<(const QUrl &other) const;

    /*!
     * Converts this KFileItem to a QVariant, this allows to use KFileItem
     * in QVariant() constructor
     */
    operator QVariant() const;

    /*!
     * Tries to return a local URL for this file item if possible.
     *
     * If \a local is not null, it will be set to \c true if the returned url is local,
     * \c false otherwise.
     *
     * Example:
     * \code
     * bool isLocal = false;
     * KFileItem item;
     * const QUrl url = item.mostLocalUrl(&isLocal);
     * if (isLocal) {
     *    // Use url
     * }
     * \endcode
     *
     */
    QUrl mostLocalUrl(bool *local = nullptr) const;

    // TODO qdoc?
    struct MostLocalUrlResult {
        QUrl url;
        bool local;
    };

    /*!
     * Returns a MostLocalUrlResult, with the local Url for this item if possible
     * (otherwise the item url), and a bool that is set to \c true if this Url
     * does represent a local file otherwise \c false.
     *
     * Basically this is an alternative to mostLocalUrl(bool*), that does not use an
     * output parameter.
     *
     * Example:
     * \code
     * KFileItem item;
     * const MostLocalUrlResult result = item.isMostLocalUrl();
     * if (result.local) { // A local file
     *    // Use result.url
     * }
     * \endcode
     * \since 5.84
     */
    MostLocalUrlResult isMostLocalUrl() const;

    /*!
     * Returns \c true if default-constructed
     */
    bool isNull() const;

    /*!
     * Returns whether the KFileItem exists on-disk
     *
     * Call only after initialization (i.e `KIO::stat` or `refresh()` for local files)
     * \since 6.0
     */
    bool exists() const;

    /*!
     * Returns \c true if the file has executable permission
     * \since 6.0
     */
    bool isExecutable() const;

private:
    QSharedDataPointer<KFileItemPrivate> d;

    /*!
     * Hides the file.
     * \internal
     */
    KIOCORE_NO_EXPORT void setHidden();

private:
    KIOCORE_EXPORT friend QDataStream &operator<<(QDataStream &s, const KFileItem &a);
    KIOCORE_EXPORT friend QDataStream &operator>>(QDataStream &s, KFileItem &a);

    friend class KFileItemTest;
    friend class KCoreDirListerCache;
};

Q_DECLARE_METATYPE(KFileItem)
Q_DECLARE_TYPEINFO(KFileItem, Q_RELOCATABLE_TYPE);

inline size_t qHash(const KFileItem &item, size_t seed = 0)
{
    return qHash(item.url(), seed);
}

/*!
 * \class KFileItemList
 * \inheaderfile KFileItem
 * \inmodule KIOCore
 *
 * List of KFileItem, which adds a few helper
 * methods to QList<KFileItem>.
 */
class KIOCORE_EXPORT KFileItemList : public QList<KFileItem>
{
public:
    /*!
     * Creates an empty list of file items.
     */
    KFileItemList();

    /*!
     * Creates a new KFileItemList from a QList of file \a items.
     */
    KFileItemList(const QList<KFileItem> &items);

    /*!
     * Creates a new KFileItemList from an initializer_list of file \a items.
     * \since 5.76
     */
    KFileItemList(std::initializer_list<KFileItem> items);

    /*!
     * Find a KFileItem by name and return it.
     *
     * Returns the item with the given name, or a null-item if none was found
     *         (see KFileItem::isNull())
     */
    KFileItem findByName(const QString &fileName) const;

    /*!
     * Find a KFileItem by URL and return it.
     *
     * Returns the item with the given URL, or a null-item if none was found
     *         (see KFileItem::isNull())
     */
    KFileItem findByUrl(const QUrl &url) const;

    /*!
     * Returns the list of URLs that those items represent
     */
    QList<QUrl> urlList() const;

    /*!
     * Returns the list of target URLs that those items represent
     * \since 4.2
     */
    QList<QUrl> targetUrlList() const;

    // TODO KDE-5 add d pointer here so that we can merge KFileItemListProperties into KFileItemList
};

KIOCORE_EXPORT QDataStream &operator<<(QDataStream &s, const KFileItem &a);
KIOCORE_EXPORT QDataStream &operator>>(QDataStream &s, KFileItem &a);

KIOCORE_EXPORT QDebug operator<<(QDebug stream, const KFileItem &item);

#endif
