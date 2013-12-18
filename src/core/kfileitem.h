/* This file is part of the KDE project
   Copyright (C) 1999-2006 David Faure <faure@kde.org>

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

#ifndef KFILEITEM_H
#define KFILEITEM_H

#include <sys/stat.h>

#include <kio/kiocore_export.h>
#include <kio/global.h>
#include <kacl.h>
#include <kio/udsentry.h>
#include <QDateTime>
#include <QUrl>
#include <QFile>

#include <qmimetype.h>
#include <QtCore/QList>

class KFileItemPrivate;

/**
 * A KFileItem is a generic class to handle a file, local or remote.
 * In particular, it makes it easier to handle the result of KIO::listDir
 * (UDSEntry isn't very friendly to use).
 * It includes many file attributes such as mimetype, icon, text, mode, link...
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
        CreationTime = 2
        //ChangeTime
    };

    /**
     * Null KFileItem. Doesn't represent any file, only exists for convenience.
     *
     * NOTE KDE 4.0 when porting from KFileItem* to KFileItem&:
     * '(KFileItem*)a==0'  becomes '(KFileItem)a.isNull()'
     */
    KFileItem();

    /**
     * Creates an item representing a file, from a UDSEntry.
     * This is the preferred constructor when using KIO::listDir().
     *
     * @param entry the KIO entry used to get the file, contains info about it
     * @param itemOrDirUrl the URL of the item or of the directory containing this item (see urlIsDirectory).
     * @param delayedMimeTypes specifies if the mimetype of the given
     *       URL should be determined immediately or on demand.
     *       See the bool delayedMimeTypes in the KDirLister constructor.
     * @param urlIsDirectory specifies if the url is just the directory of the
     *       fileitem and the filename from the UDSEntry should be used.
     *
     * When creating KFileItems out of the UDSEntry emitted by a KIO list job,
     * use KFileItem(entry, listjob->url(), delayedMimeTypes, true);
     */
    KFileItem( const KIO::UDSEntry& entry, const QUrl& itemOrDirUrl,
               bool delayedMimeTypes = false,
               bool urlIsDirectory = false );

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
     * @param delayedMimeTypes specify if the mimetype of the given URL
     *       should be determined immediately or on demand
     * @deprecated since 5.0. Most callers gave Unknown for mode and permissions,
     * so just port to KFileItem(url) and setDelayedMimeTypes(true) if necessary.
     */
#ifndef KDE_NO_DEPRECATED
    KIOCORE_DEPRECATED KFileItem( mode_t mode, mode_t permissions, const QUrl& url,
               bool delayedMimeTypes = false );
#endif

    /**
     * Creates an item representing a file, for which the mimetype is already known.
     * @param url the file url
     * @param mimeType the name of the file's mimetype
     * @param mode the mode (S_IFDIR...)
     */
    KFileItem(const QUrl &url, const QString &mimeType = QString(), mode_t mode = KFileItem::Unknown);

    /**
     * Copy constructor
     */
    KFileItem(const KFileItem& other);
    /**
     * Assignment operator
     */
    KFileItem& operator=(const KFileItem& other);

    /**
     * Destructs the KFileItem. Extra data set via setExtraData()
     * is not deleted.
     */
    ~KFileItem();

    /**
     * Throw away and re-read (for local files) all information about the file.
     * This is called when the _file_ changes.
     */
    void refresh();

    /**
     * Re-reads mimetype information.
     * This is called when the mimetype database changes.
     */
    void refreshMimeType();

    /**
     * Sets mimetype determination to be immediate or on demand.
     * Call this after the constructor, and before using any mimetype-related method.
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
    void setUrl( const QUrl &url );

    /**
     * Sets the item's name (i.e. the filename).
     * This is automatically done by setUrl, to set the name from the URL's fileName().
     * This method is provided for some special cases like relative paths as names (KFindPart)
     * @param name the item's name
     */
    void setName( const QString &name );

    /**
     * Returns the permissions of the file (stat.st_mode containing only permissions).
     * @return the permissions of the file
     */
    mode_t permissions() const;

    /**
     * Returns the access permissions for the file as a string.
     * @return the access persmission as string
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
     * Returns true if this item represents a file (and not a a directory)
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
     * Requests the modification, access or creation time, depending on @p which.
     * @param which the timestamp
     * @return the time asked for, QDateTime() if not available
     * @see timeString()
     */
    QDateTime time( FileTimes which ) const;

    /**
     * Requests the modification, access or creation time as a string, depending
     * on @p which.
     * @param which the timestamp
     * @returns a formatted string of the requested time.
     * @see time
     */
    QString timeString( FileTimes which = ModificationTime ) const;
#ifndef KDE_NO_DEPRECATED
    KIOCORE_DEPRECATED QString timeString( unsigned int which) const;
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
    QString name( bool lowerCase = false ) const;

    /**
     * Returns the mimetype of the file item.
     * If @p delayedMimeTypes was used in the constructor, this will determine
     * the mimetype first. Equivalent to determineMimeType()->name()
     * @return the mime type of the file
     */
    QString mimetype() const;

    /**
     * Returns the mimetype of the file item.
     * If delayedMimeTypes was used in the constructor, this will determine
     * the mimetype first.
     * @return the mime type
     */
    QMimeType determineMimeType() const;

    /**
     * Returns the currently known mimetype of the file item.
     * This will not try to determine the mimetype if unknown.
     * @return the known mime type
     */
    QMimeType currentMimeType() const; // KDE4: was mimeTypePtr()

    /**
     * @return true if we have determined the final icon of this file already.
     * @since 4.10.2
     */
    bool isFinalIconKnown() const;

    /**
     * @return true if we have determined the mimetype of this file already,
     * i.e. if determineMimeType() will be fast. Otherwise it will have to
     * find what the mimetype is, which is a possibly slow operation; usually
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
     * this mime type.
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

    /**
     * Returns true if files can be dropped over this item.
     * Contrary to popular belief, not only dirs will return true :)
     * Executables, .desktop files, will do so as well.
     * @return true if you can drop files over the item
     *
     * @deprecated This logic is application-dependent, the behavior described above
     * mostly makes sense for file managers only.
     * KDirModel has setDropsAllowed for similar (but configurable) logic.
     */
#ifndef KDE_NO_DEPRECATED
    KIOCORE_DEPRECATED bool acceptsDrops() const;
#endif

    /**
     * Returns the UDS entry. Used by the tree view to access all details
     * by position.
     * @return the UDS entry
     */
    KIO::UDSEntry entry() const;

    /**
     * Used when updating a directory. marked == seen when refreshing.
     * @return true if the file item is marked
     */
    bool isMarked() const;
    /**
     * Marks the item.
     * @see isMarked()
     */
    void mark();
    /**
     * Unmarks the item.
     * @see isMarked()
     */
    void unmark();

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
    bool cmp( const KFileItem & item ) const;

    /**
     * Returns true if both items share the same URL.
     */
    bool operator==(const KFileItem& other) const;

    /**
     * Returns true if both items do not share the same URL.
     */
    bool operator!=(const KFileItem& other) const;


    /**
     * Converts this KFileItem to a QVariant, this allows to use KFileItem
     * in QVariant() constructor
     */
    operator QVariant() const;

    /**
     * This allows to associate some "extra" data to a KFileItem. As one
     * KFileItem can be used by several objects (often views) which all need
     * to add some data, you have to use a key to reference your extra data
     * within the KFileItem.
     *
     * That way a KFileItem can hold and provide access to all those views
     * separately.
     *
     * I.e. a KFileIconView that associates a KFileIconViewItem (an item suitable
     * for use with QIconView) does
     *
     * \code
     * kfileItem->setExtraData( this, iconViewItem );
     * \endcode
     *
     * and can later access the iconViewItem by doing
     *
     * \code
     * KFileIconViewItem *iconViewItem = static_cast<KFileIconViewItem*>( kfileItem->extraData( this ));
     * \endcode
     *
     * This is usually more efficient then having every view associate data to
     * items by using a separate QDict or QMap.
     *
     * Note: you have to remove and destroy the data you associated yourself
     * when you don't need it anymore!
     *
     * @param key the key of the extra data
     * @param value the value of the extra data
     * @see extraData
     * @see removeExtraData
     *
     * @deprecated use model/view (KDirModel) and you won't need this anymore
     */
#ifndef KDE_NO_DEPRECATED
    KIOCORE_DEPRECATED void setExtraData( const void *key, void *value );
#endif

    /**
     * Retrieves the extra data with the given @p key.
     * @param key the key of the extra data
     * @return the extra data associated to an item with @p key via
     * setExtraData. 0L if nothing was associated with @p key.
     * @see extraData
     *
     * @deprecated use model/view (KDirModel) and you won't need this anymore
     */
#ifndef KDE_NO_DEPRECATED
    KIOCORE_DEPRECATED const void * extraData( const void *key ) const;
#endif

    /**
     * Removes the extra data associated with an item via @p key.
     * @param key the key of the extra data to remove
     *
     * @deprecated use model/view (KDirModel) and you won't need this anymore
     */
#ifndef KDE_NO_DEPRECATED
    KIOCORE_DEPRECATED void removeExtraData( const void *key );
#endif

    /**
     * @deprecated simply use '='
     */
#ifndef KDE_NO_DEPRECATED
    KIOCORE_DEPRECATED void assign( const KFileItem & item );
#endif

    /**
     * Tries to give a local URL for this file item if possible.
     * The given boolean indicates if the returned url is local or not.
     */
    QUrl mostLocalUrl(bool &local) const; // KDE5 TODO: bool* local = 0

    /**
     * Tries to give a local URL for this file item if possible.
     *
     * \since 4.6
     */
    QUrl mostLocalUrl() const; // KDE5: merge with above version

    /**
     * Return true if default-constructed
     */
    bool isNull() const;

private:
    QSharedDataPointer<KFileItemPrivate> d;

private:
    KIOCORE_EXPORT friend QDataStream & operator<< ( QDataStream & s, const KFileItem & a );
    KIOCORE_EXPORT friend QDataStream & operator>> ( QDataStream & s, KFileItem & a );

    friend class KFileItemTest;
};

Q_DECLARE_METATYPE(KFileItem)

inline uint qHash(const KFileItem& item) { return qHash(item.url()); }

/**
 * List of KFileItems, which adds a few helper
 * methods to QList<KFileItem>.
 */
class KIOCORE_EXPORT KFileItemList : public QList<KFileItem>
{
public:
  /// Creates an empty list of file items.
  KFileItemList();

  /// Creates a new KFileItemList from a QList of file @p items.
  KFileItemList( const QList<KFileItem> &items );

  /**
   * Find a KFileItem by name and return it.
   * @return the item with the given name, or a null-item if none was found
   *         (see KFileItem::isNull())
   */
  KFileItem findByName( const QString& fileName ) const;

  /**
   * Find a KFileItem by URL and return it.
   * @return the item with the given URL, or a null-item if none was found
   *         (see KFileItem::isNull())
   */
  KFileItem findByUrl( const QUrl& url ) const;

  /// @return the list of URLs that those items represent
  QList<QUrl> urlList() const;

  /// @return the list of target URLs that those items represent
  /// @since 4.2
  QList<QUrl> targetUrlList() const;

  // TODO KDE-5 add d pointer here so that we can merge KFileItemListProperties into KFileItemList
};

KIOCORE_EXPORT QDataStream & operator<< ( QDataStream & s, const KFileItem & a );
KIOCORE_EXPORT QDataStream & operator>> ( QDataStream & s, KFileItem & a );

/**
 * Support for qDebug() << aFileItem
 * \since 4.4
 */
KIOCORE_EXPORT QDebug operator<<(QDebug stream, const KFileItem& item);

#endif
