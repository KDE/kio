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

KIOCORE_EXPORT QDebug operator<<(QDebug stream, const KIO::UDSEntry &entry);

namespace KIO
{
class UDSEntryPrivate;

// TODO qdoc
/*!
 * Returns true if the entry contains the same data as the other
 * \since 5.63
 */
KIOCORE_EXPORT bool operator==(const UDSEntry &entry, const UDSEntry &other);

/*!
 * Returns true if the entry does not contain the same data as the other
 * \since 5.63
 */
KIOCORE_EXPORT bool operator!=(const UDSEntry &entry, const UDSEntry &other);

/*!
 * \class KIO::UDSEntry
 * \inheaderfile KIO/UDSEntry
 * \inmodule KIOCore
 * \brief Universal Directory Service.
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
    /*!
     *
     */
    UDSEntry();

    /*!
     * Create a UDSEntry by QT_STATBUF
     *
     * \a buff QT_STATBUF object
     *
     * \a name filename
     *
     * \since 5.0
     */
    UDSEntry(const QT_STATBUF &buff, const QString &name = QString());

    /*!
     * Copy constructor
     */
    UDSEntry(const UDSEntry &);

    ~UDSEntry();

    UDSEntry(UDSEntry &&);

    UDSEntry &operator=(const UDSEntry &);

    UDSEntry &operator=(UDSEntry &&);

    /*!
     * Returns value of a textual field
     */
    QString stringValue(uint field) const;

    /*!
     * Returns value of a numeric field
     */
    long long numberValue(uint field, long long defaultValue = 0) const;

    // Convenience methods.
    // Let's not add one method per field, only methods that have some more logic
    // than just calling stringValue(field) or numberValue(field).

    /*!
     * Returns \c true if this entry is a directory (or a link to a directory)
     */
    bool isDir() const;

    /*!
     * Returns \c true if this entry is a link
     */
    bool isLink() const;

    /*!
     * Calling this function before inserting items into an empty UDSEntry may save time and memory.
     *
     * \a size number of items for which memory will be pre-allocated
     */
    void reserve(int size);

    /*!
     * insert field with string value, it will assert if the field is already inserted. In that case, use replace() instead.
     *
     * \a field numeric field id
     *
     * \a value to set
     *
     * \since 5.48
     */
    void fastInsert(uint field, const QString &value);

    /*!
     * insert field with numeric value, it will assert if the field is already inserted. In that case, use replace() instead.
     *
     * \a field numeric field id
     *
     * \a l value to set
     *
     * \since 5.48
     */
    void fastInsert(uint field, long long l);

    /*!
     * Returns the number of fields
     */
    int count() const;

    /*!
     * check existence of a field
     *
     * \a field numeric field id
     */
    bool contains(uint field) const;

    /*!
     * A vector of fields being present for the current entry.
     *
     * Returns all fields for the current entry.
     * \since 5.8
     */
    QList<uint> fields() const;

    /*!
     * remove all fields
     */
    void clear();

    /*!
     * Bit field used to specify the item type of a StandardFieldTypes.
     *
     * \value UDS_STRING Indicates that the field is a QString
     * \value UDS_NUMBER Indicates that the field is a number (long long)
     * \value UDS_TIME Indicates that the field represents a time, which is modelled by a long long
     */
    enum ItemTypes {
        // Those are a bit field
        UDS_STRING = 0x01000000,
        UDS_NUMBER = 0x02000000,
        UDS_TIME = 0x04000000 | UDS_NUMBER,
    };

    /*!
     * Constants used to specify the type of a UDSEntry’s field.
     *
     * \value UDS_SIZE Size of the file
     * \omitvalue UDS_SIZE_LARGE
     * \value UDS_USER User Name of the file owner. Not present on local fs, use UDS_LOCAL_USER_ID
     * \value UDS_ICON_NAME Name of the icon, that should be used for displaying. It overrides all other detection mechanisms
     * \value UDS_GROUP Group Name of the file owner. Not present on local fs, use UDS_LOCAL_GROUP_ID
     * \value UDS_NAME Filename - as displayed in directory listings etc.
     * "." has the usual special meaning of "current directory"
     * UDS_NAME must always be set and never be empty, neither contain '/'.
     * Note that KIO will append the UDS_NAME to the url of their
     * parent directory, so all KIO workers must use that naming scheme
     * ("url_of_parent/filename" will be the full url of that file).
     * To customize the appearance of files without changing the url
     * of the items, use UDS_DISPLAY_NAME.
     * \value UDS_LOCAL_PATH A local file path if the KIO worker display files sitting on the local filesystem (but in another hierarchy, e.g.\ settings:/ or
     * remote:/)
     * \value UDS_HIDDEN Treat the file as a hidden file (if set to 1) or as a normal file (if set to 0). This field overrides the default behavior (the check
     * for a leading dot in the filename).
     * \value UDS_ACCESS Access permissions (part of the mode returned by stat)
     * \value UDS_MODIFICATION_TIME The last time the file was modified. Required time format: seconds since UNIX epoch.
     * \value UDS_ACCESS_TIME The last time the file was opened. Required time format: seconds since UNIX epoch.
     * \value UDS_CREATION_TIME The time the file was created. Required time format: seconds since UNIX epoch.
     * \value UDS_FILE_TYPE File type, part of the mode returned by stat (for a link, this returns the file type of the pointed item) check UDS_LINK_DEST to
     * know if this is a link
     * \value UDS_LINK_DEST Name of the file where the link points to. Allows to check for a symlink (don't use S_ISLNK !)
     * \value UDS_URL An alternative URL (If different from the caption). Can be used to mix different hierarchies. Use UDS_DISPLAY_NAME if you simply want to
     * customize the user-visible filenames, or use UDS_TARGET_URL if you want "links" to unrelated urls.
     * \value UDS_MIME_TYPE A MIME type; the KIO worker should set it if it's known.
     * \value UDS_GUESSED_MIME_TYPE A MIME type to be used for displaying only. But when 'running' the file, the MIME type is re-determined. This is for special
     * cases like symlinks in FTP; you probably don't want to use this one
     * \value UDS_XML_PROPERTIES XML properties, e.g.\ for WebDAV
     * \value UDS_EXTENDED_ACL Indicates that the entry has extended ACL entries
     * \value UDS_ACL_STRING The access control list serialized into a single string
     * \value UDS_DEFAULT_ACL_STRING The default access control list serialized into a single string. Only available for directories
     * \value[since 4.1] UDS_DISPLAY_NAME If set, contains the label to display instead of the 'real name' in UDS_NAME
     * \value[since 4.1] UDS_TARGET_URL This file is a shortcut or mount, pointing to an URL in a different hierarchy
     * \value[since 4.4] UDS_DISPLAY_TYPE User-readable type of file (if not specified, the MIME type's description is used)
     * \value[since 4.5] UDS_ICON_OVERLAY_NAMES A comma-separated list of supplementary icon overlays which will be added to the list of overlays created by
     * KFileItem.
     * \value[since 4.6] UDS_COMMENT A comment which will be displayed as is to the user. The string value may contain plain text or Qt-style rich-text
     * extensions.
     * \value[since 4.7.3] UDS_DEVICE_ID Device number for this file, used to detect hardlinks
     * \value[since 4.7.3] UDS_INODE Inode number for this file, used to detect hardlinks
     * \value[since 5.70] UDS_RECURSIVE_SIZE For folders, the recursize size of its content
     * \value[since 6.0] UDS_LOCAL_USER_ID User ID of the file owner
     * \value[since 6.0] UDS_LOCAL_GROUP_ID Group ID of the file owner
     * \value UDS_EXTRA Extra data (used only if you specified Columns/ColumnsTypes). NB: you cannot repeat this entry; use UDS_EXTRA + i until UDS_EXTRA_END
     * \value UDS_EXTRA_END
     */
    enum StandardFieldTypes {
        // The highest bit is reserved to store the used FieldTypes
        UDS_SIZE = 1 | UDS_NUMBER,
        UDS_SIZE_LARGE = 2 | UDS_NUMBER,
        UDS_USER = 3 | UDS_STRING,
        UDS_ICON_NAME = 4 | UDS_STRING,
        UDS_GROUP = 5 | UDS_STRING,
        UDS_NAME = 6 | UDS_STRING,
        UDS_LOCAL_PATH = 7 | UDS_STRING,
        UDS_HIDDEN = 8 | UDS_NUMBER,
        UDS_ACCESS = 9 | UDS_NUMBER,
        UDS_MODIFICATION_TIME = 10 | UDS_TIME,
        UDS_ACCESS_TIME = 11 | UDS_TIME,
        UDS_CREATION_TIME = 12 | UDS_TIME,
        UDS_FILE_TYPE = 13 | UDS_NUMBER,
        UDS_LINK_DEST = 14 | UDS_STRING,
        UDS_URL = 15 | UDS_STRING,
        UDS_MIME_TYPE = 16 | UDS_STRING,
        UDS_GUESSED_MIME_TYPE = 17 | UDS_STRING,
        UDS_XML_PROPERTIES = 18 | UDS_STRING,
        UDS_EXTENDED_ACL = 19 | UDS_NUMBER,
        UDS_ACL_STRING = 20 | UDS_STRING,
        UDS_DEFAULT_ACL_STRING = 21 | UDS_STRING,
        UDS_DISPLAY_NAME = 22 | UDS_STRING,
        UDS_TARGET_URL = 23 | UDS_STRING,
        UDS_DISPLAY_TYPE = 24 | UDS_STRING,
        UDS_ICON_OVERLAY_NAMES = 25 | UDS_STRING,
        UDS_COMMENT = 26 | UDS_STRING,
        UDS_DEVICE_ID = 27 | UDS_NUMBER,
        UDS_INODE = 28 | UDS_NUMBER,
        UDS_RECURSIVE_SIZE = 29 | UDS_NUMBER,
        UDS_LOCAL_USER_ID = 30 | UDS_NUMBER,
        UDS_LOCAL_GROUP_ID = 31 | UDS_NUMBER,
        UDS_SUBVOL_ID = 32 | UDS_NUMBER,
        UDS_EXTRA = 100 | UDS_STRING,
        UDS_EXTRA_END = 140 | UDS_STRING,
    };

private:
    QSharedDataPointer<UDSEntryPrivate> d;
    friend KIOCORE_EXPORT QDataStream & ::operator<<(QDataStream & s, const KIO::UDSEntry & a);
    friend KIOCORE_EXPORT QDataStream & ::operator>>(QDataStream & s, KIO::UDSEntry & a);
    friend KIOCORE_EXPORT QDebug(::operator<<)(QDebug stream, const KIO::UDSEntry &entry);

public:
    /*!
     * Replace or insert field with string value
     *
     * \a field numeric field id
     *
     * \a value to set
     *
     * \since 5.47
     */
    void replace(uint field, const QString &value);

    /*!
     * Replace or insert field with numeric value
     *
     * \a field numeric field id
     *
     * \a l value to set
     *
     * \since 5.47
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
/*!
 * \typedef KIO::UDSEntryList
 *
 * \relates KIO::UDSEntry
 *
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
