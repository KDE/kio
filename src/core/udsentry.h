/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2005 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2007 Norbert Frese <nf2@scheinwelt.at>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef UDSENTRY_H
#define UDSENTRY_H

#include <QList>
#include <QMetaType>
#include <QSharedData>
#include <QString>
#include <QtGlobal>
#include <qplatformdefs.h>

#include "kiocore_export.h"

namespace KIO
{
class UDSEntry;
}

KIOCORE_EXPORT QDataStream &operator<<(QDataStream &s, const KIO::UDSEntry &a);
KIOCORE_EXPORT QDataStream &operator>>(QDataStream &s, KIO::UDSEntry &a);

/**
 * Support for qDebug() << aUDSEntry
 * \since 5.22
 */
KIOCORE_EXPORT QDebug operator<<(QDebug stream, const KIO::UDSEntry &entry);

namespace KIO
{
class UDSEntryPrivate;

/**
 * Returns true if the entry contains the same data as the other
 * @since 5.63
 */
KIOCORE_EXPORT bool operator==(const UDSEntry &entry, const UDSEntry &other);

/**
 * Returns true if the entry does not contain the same data as the other
 * @since 5.63
 */
KIOCORE_EXPORT bool operator!=(const UDSEntry &entry, const UDSEntry &other);

/**
 * @class KIO::UDSEntry udsentry.h <KIO/UDSEntry>
 *
 * Universal Directory Service
 *
 * UDS entry is the data structure representing all the fields about a given URL
 * (file or directory).
 *
 * The KIO::listDir() and KIO:stat() operations use this data structure.
 *
 * KIO defines a number of standard fields, see the UDS_XXX enums (see StandardFieldTypes).
 * at the moment UDSEntry only provides fields with numeric indexes,
 * but there might be named fields with string indexes in the future.
 *
 * For instance, to retrieve the name of the entry, use:
 * \code
 * QString displayName = entry.stringValue( KIO::UDSEntry::UDS_NAME );
 * \endcode
 *
 * To know the modification time of the file/url:
 * \code
 *  QDateTime mtime = QDateTime::fromSecsSinceEpoch(entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, 0));
 *  if (mtime.isValid())
 *      ...
 * \endcode
 */
class KIOCORE_EXPORT UDSEntry
{
public:
    UDSEntry();

    /**
     * Create a UDSEntry by QT_STATBUF
     * @param buff QT_STATBUF object
     * @param name filename
     * @since 5.0
     */
    UDSEntry(const QT_STATBUF &buff, const QString &name = QString());

    /**
     * Copy constructor
     */
    UDSEntry(const UDSEntry &);

    /**
     * Destructor
     */
    ~UDSEntry();

    /**
     * Move constructor
     * @since 5.44
     */
    UDSEntry(UDSEntry &&);

    /**
     * Copy assignment
     */
    UDSEntry &operator=(const UDSEntry &);

    /**
     * Move assignment
     * @since 5.44
     */
    UDSEntry &operator=(UDSEntry &&);

    /**
     * @return value of a textual field
     */
    QString stringValue(uint field) const;

    /**
     * @return value of a numeric field
     */
    long long numberValue(uint field, long long defaultValue = 0) const;

    // Convenience methods.
    // Let's not add one method per field, only methods that have some more logic
    // than just calling stringValue(field) or numberValue(field).

    /// @return true if this entry is a directory (or a link to a directory)
    bool isDir() const;
    /// @return true if this entry is a link
    bool isLink() const;

    /**
     * Calling this function before inserting items into an empty UDSEntry may save time and memory.
     * @param size number of items for which memory will be pre-allocated
     */
    void reserve(int size);

    /**
     * insert field with string value, it will assert if the field is already inserted. In that case, use replace() instead.
     * @param field numeric field id
     * @param value to set
     * @since 5.48
     */
    void fastInsert(uint field, const QString &value);

    /**
     * insert field with numeric value, it will assert if the field is already inserted. In that case, use replace() instead.
     * @param field numeric field id
     * @param l value to set
     * @since 5.48
     */
    void fastInsert(uint field, long long l);

    /**
     * count fields
     * @return the number of fields
     */
    int count() const;

    /**
     * check existence of a field
     * @param field numeric field id
     */
    bool contains(uint field) const;

    /**
     * A vector of fields being present for the current entry.
     * @return all fields for the current entry.
     * @since 5.8
     */
    QList<uint> fields() const;

    /**
     * remove all fields
     */
    void clear();

    /**
     * Bit field used to specify the item type of a StandardFieldTypes.
     */
    enum ItemTypes {
        // Those are a bit field
        /// Indicates that the field is a QString
        UDS_STRING = 0x01000000,
        /// Indicates that the field is a number (long long)
        UDS_NUMBER = 0x02000000,
        /// Indicates that the field represents a time, which is modelled by a long long
        UDS_TIME = 0x04000000 | UDS_NUMBER,
    };

    /**
     * Constants used to specify the type of a UDSEntry’s field.
     */
    enum StandardFieldTypes {

        // The highest bit is reserved to store the used FieldTypes

        /// Size of the file
        UDS_SIZE = 1 | UDS_NUMBER,
        /// @internal
        UDS_SIZE_LARGE = 2 | UDS_NUMBER,
        /// User Name of the file owner
        /// Not present on local fs, use UDS_LOCAL_USER_ID
        UDS_USER = 3 | UDS_STRING,
        /// Name of the icon, that should be used for displaying.
        /// It overrides all other detection mechanisms
        UDS_ICON_NAME = 4 | UDS_STRING,
        /// Group Name of the file owner
        /// Not present on local fs, use UDS_LOCAL_GROUP_ID
        UDS_GROUP = 5 | UDS_STRING,
        /// Filename - as displayed in directory listings etc.
        /// "." has the usual special meaning of "current directory"
        /// UDS_NAME must always be set and never be empty, neither contain '/'.
        ///
        /// Note that KIO will append the UDS_NAME to the url of their
        /// parent directory, so all KIO workers must use that naming scheme
        /// ("url_of_parent/filename" will be the full url of that file).
        /// To customize the appearance of files without changing the url
        /// of the items, use UDS_DISPLAY_NAME.
        UDS_NAME = 6 | UDS_STRING,
        /// A local file path if the KIO worker display files sitting
        /// on the local filesystem (but in another hierarchy, e.g.\ settings:/ or remote:/)
        UDS_LOCAL_PATH = 7 | UDS_STRING,
        /// Treat the file as a hidden file (if set to 1) or as a normal file (if set to 0).
        /// This field overrides the default behavior (the check for a leading dot in the filename).
        UDS_HIDDEN = 8 | UDS_NUMBER,
        /// Access permissions (part of the mode returned by stat)
        UDS_ACCESS = 9 | UDS_NUMBER,
        /// The last time the file was modified. Required time format: seconds since UNIX epoch.
        UDS_MODIFICATION_TIME = 10 | UDS_TIME,
        /// The last time the file was opened. Required time format: seconds since UNIX epoch.
        UDS_ACCESS_TIME = 11 | UDS_TIME,
        /// The time the file was created. Required time format: seconds since UNIX epoch.
        UDS_CREATION_TIME = 12 | UDS_TIME,
        /// File type, part of the mode returned by stat
        /// (for a link, this returns the file type of the pointed item)
        /// check UDS_LINK_DEST to know if this is a link
        UDS_FILE_TYPE = 13 | UDS_NUMBER,
        /// Name of the file where the link points to
        /// Allows to check for a symlink (don't use S_ISLNK !)
        UDS_LINK_DEST = 14 | UDS_STRING,
        /// An alternative URL (If different from the caption).
        /// Can be used to mix different hierarchies.
        ///
        /// Use UDS_DISPLAY_NAME if you simply want to customize the user-visible filenames, or use
        /// UDS_TARGET_URL if you want "links" to unrelated urls.
        UDS_URL = 15 | UDS_STRING,
        /// A MIME type; the KIO worker should set it if it's known.
        UDS_MIME_TYPE = 16 | UDS_STRING,
        /// A MIME type to be used for displaying only.
        /// But when 'running' the file, the MIME type is re-determined
        /// This is for special cases like symlinks in FTP; you probably don't want to use this one.
        UDS_GUESSED_MIME_TYPE = 17 | UDS_STRING,
        /// XML properties, e.g.\ for WebDAV
        UDS_XML_PROPERTIES = 18 | UDS_STRING,

        /// Indicates that the entry has extended ACL entries
        UDS_EXTENDED_ACL = 19 | UDS_NUMBER,
        /// The access control list serialized into a single string.
        UDS_ACL_STRING = 20 | UDS_STRING,
        /// The default access control list serialized into a single string.
        /// Only available for directories.
        UDS_DEFAULT_ACL_STRING = 21 | UDS_STRING,

        /// If set, contains the label to display instead of
        /// the 'real name' in UDS_NAME
        /// @since 4.1
        UDS_DISPLAY_NAME = 22 | UDS_STRING,
        /// This file is a shortcut or mount, pointing to an
        /// URL in a different hierarchy
        /// @since 4.1
        UDS_TARGET_URL = 23 | UDS_STRING,

        /// User-readable type of file (if not specified,
        /// the MIME type's description is used)
        /// @since 4.4
        UDS_DISPLAY_TYPE = 24 | UDS_STRING,

        /// A comma-separated list of supplementary icon overlays
        /// which will be added to the list of overlays created
        /// by KFileItem.
        ///
        /// @since 4.5
        UDS_ICON_OVERLAY_NAMES = 25 | UDS_STRING,

        /// A comment which will be displayed as is to the user. The string
        /// value may contain plain text or Qt-style rich-text extensions.
        ///
        /// @since 4.6
        UDS_COMMENT = 26 | UDS_STRING,

        /// Device number for this file, used to detect hardlinks
        /// @since 4.7.3
        UDS_DEVICE_ID = 27 | UDS_NUMBER,
        /// Inode number for this file, used to detect hardlinks
        /// @since 4.7.3
        UDS_INODE = 28 | UDS_NUMBER,

        /// For folders, the recursize size of its content
        /// @since 5.70
        UDS_RECURSIVE_SIZE = 29 | UDS_NUMBER,

        /// User ID of the file owner
        /// @since 6.0
        UDS_LOCAL_USER_ID = 30 | UDS_NUMBER,
        /// Group ID of the file owner
        /// @since 6.0
        UDS_LOCAL_GROUP_ID = 31 | UDS_NUMBER,

        /// Extra data (used only if you specified Columns/ColumnsTypes)
        /// NB: you cannot repeat this entry; use UDS_EXTRA + i
        /// until UDS_EXTRA_END.
        UDS_EXTRA = 100 | UDS_STRING,
        /// Extra data (used only if you specified Columns/ColumnsTypes)
        /// NB: you cannot repeat this entry; use UDS_EXTRA + i
        /// until UDS_EXTRA_END.
        UDS_EXTRA_END = 140 | UDS_STRING,
    };

private:
    QSharedDataPointer<UDSEntryPrivate> d;
    friend KIOCORE_EXPORT QDataStream & ::operator<<(QDataStream &s, const KIO::UDSEntry &a);
    friend KIOCORE_EXPORT QDataStream & ::operator>>(QDataStream &s, KIO::UDSEntry &a);
    friend KIOCORE_EXPORT QDebug(::operator<<)(QDebug stream, const KIO::UDSEntry &entry);

public:
    /**
     * Replace or insert field with string value
     * @param field numeric field id
     * @param value to set
     * @since 5.47
     */
    void replace(uint field, const QString &value);

    /**
     * Replace or insert field with numeric value
     * @param field numeric field id
     * @param l value to set
     * @since 5.47
     */
    void replace(uint field, long long l);
};

// allows operator ^ and | between UDSEntry::StandardFieldTypes and UDSEntry::ItemTypes
inline constexpr UDSEntry::StandardFieldTypes operator|(UDSEntry::StandardFieldTypes fieldType, UDSEntry::ItemTypes type)
{
    return static_cast<UDSEntry::StandardFieldTypes>((char)fieldType | (char)type);
}
inline constexpr UDSEntry::StandardFieldTypes operator^(UDSEntry::StandardFieldTypes fieldType, UDSEntry::ItemTypes type)
{
    return static_cast<UDSEntry::StandardFieldTypes>((char)fieldType ^ (char)type);
}
}

Q_DECLARE_TYPEINFO(KIO::UDSEntry, Q_RELOCATABLE_TYPE);

namespace KIO
{
/**
 * A directory listing is a list of UDSEntry instances.
 *
 * To list the name and size of all the files in a directory listing you would do:
 * \code
 *   KIO::UDSEntryList::ConstIterator it = entries.begin();
 *   const KIO::UDSEntryList::ConstIterator end = entries.end();
 *   for (; it != end; ++it) {
 *     const KIO::UDSEntry& entry = *it;
 *     QString name = entry.stringValue( KIO::UDSEntry::UDS_NAME );
 *     bool isDir = entry.isDir();
 *     KIO::filesize_t size = entry.numberValue( KIO::UDSEntry::UDS_SIZE, -1 );
 *     ...
 *   }
 * \endcode
 */
typedef QList<UDSEntry> UDSEntryList;
} // end namespace

Q_DECLARE_METATYPE(KIO::UDSEntry)

#endif /*UDSENTRY_H*/
