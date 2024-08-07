/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2008 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2008 George Goldberg <grundleborg@googlemail.com>
    SPDX-FileCopyrightText: 2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KFILEITEMLISTPROPERTIES_H
#define KFILEITEMLISTPROPERTIES_H

#include "kiocore_export.h"

#include <QList>
#include <QSharedDataPointer>
#include <QUrl>

class KFileItemListPropertiesPrivate;
class KFileItemList;

/*!
 * \class KFileItemListProperties
 * \inmodule KIOCore
 *
 * \brief Provides information about the common properties of a group of
 *        KFileItem objects.
 *
 * Given a list of KFileItems, this class can determine (and cache) the common
 * MIME type for all items, whether all items are directories, whether all items
 * are readable, writable, etc.
 * As soon as one file item does not support a specific capability (read, write etc.),
 * it is marked as unsupported for all items.
 *
 * This class is implicitly shared, which means it can be used as a value and
 * copied around at almost no cost.
 */
class KIOCORE_EXPORT KFileItemListProperties
{
public:
    /*!
     * Default constructor. Use setItems to specify the items.
     */
    KFileItemListProperties();

    /*!
     * Constructor that takes a KFileItemList and sets the capabilities
     *        supported by all the FileItems as true.
     *
     * \a items The list of items that are to have their supported
     *              capabilities checked.
     */
    KFileItemListProperties(const KFileItemList &items);

    /*!
     * Copy constructor
     */
    KFileItemListProperties(const KFileItemListProperties &);

    virtual ~KFileItemListProperties();

    KFileItemListProperties &operator=(const KFileItemListProperties &other);

    /*!
     * Sets the items that are to have their supported capabilities checked.
     */
    void setItems(const KFileItemList &items);

    /*!
     * Check if reading capability is supported
     *
     * Returns \c true if all the FileItems can be read, otherwise false.
     */
    bool supportsReading() const;

    /*!
     * Check if deleting capability is supported
     *
     * Returns \c true if all the FileItems can be deleted, otherwise false.
     */
    bool supportsDeleting() const;

    /*!
     * Check if writing capability is supported
     * (file managers use this mostly for directories)
     *
     * Returns \c true if all the FileItems can be written to, otherwise false.
     */
    bool supportsWriting() const;

    /*!
     * Check if moving capability is supported
     *
     * Returns \c true if all the FileItems can be moved, otherwise false.
     */
    bool supportsMoving() const;

    /*!
     * Check if files are local
     *
     * Returns \c true if all the FileItems are local, otherwise there is one or more
     *         remote file, so false.
     */
    bool isLocal() const;

    /*!
     * List of fileitems passed to the constructor or to setItems().
     */
    KFileItemList items() const;

    /*!
     * List of urls, gathered from the fileitems
     */
    QList<QUrl> urlList() const;

    /*!
     * Returns \c true if all items are directories
     */
    bool isDirectory() const;

    /*!
     * Returns whether all items are files, as reported by KFileItem::isFile().
     * \since 5.47
     */
    bool isFile() const;

    /*!
     * Returns the MIME type of all items, if they all have the same, otherwise an empty string
     */
    QString mimeType() const;

    /*!
     * Returns the MIME type group (e.g. "text") of all items, if they all have the same, otherwise an empty string
     */
    QString mimeGroup() const;

private:
    QSharedDataPointer<KFileItemListPropertiesPrivate> d;
};

#endif /* KFILEITEMLISTPROPERTIES_H */
