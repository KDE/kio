/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2005 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/
#ifndef KIO_METADATA_H
#define KIO_METADATA_H

#include "kiocore_export.h"
#include <QMap>
#include <QString>
#include <QVariant>

namespace KIO
{
/*!
 * MetaData is a simple map of key/value strings.
 * \internal
 */
class MetaData : public QMap<QString, QString>
{
public:
    /*!
     * Creates an empty meta data map.
     */
    MetaData()
        : QMap<QString, QString>()
    {
    }
    /*!
     * Copy constructor.
     */
    MetaData(const QMap<QString, QString> &metaData)
        : QMap<QString, QString>(metaData)
    {
    }

    /*!
     * Creates a meta data map from a QVaraint map.
     * \since 4.3.1
     */
    MetaData(const QMap<QString, QVariant> &);

    /*!
     * Adds the given meta data map to this map.
     *
     * \a metaData the map to add
     *
     * Returns this map
     */
    MetaData &operator+=(const QMap<QString, QString> &metaData)
    {
        QMap<QString, QString>::ConstIterator it;
        for (it = metaData.constBegin(); it != metaData.constEnd(); ++it) {
            insert(it.key(), it.value());
        }
        return *this;
    }

    /*!
     * Same as above except the value in the map is a QVariant.
     *
     * This convenience function allows you to easily assign the values
     * of a QVariant to this meta data class.
     *
     * \a metaData the map to add
     *
     * Returns this map
     *
     * \since 4.3.1
     */
    MetaData &operator+=(const QMap<QString, QVariant> &metaData);

    /*!
     * Sets the given meta data map to this map.
     *
     * \a metaData the map to add
     *
     * Returns this map
     *
     * \since 4.3.1
     */
    MetaData &operator=(const QMap<QString, QVariant> &metaData);

    /*!
     * Returns the contents of the map as a QVariant.
     *
     * Returns a QVariant representation of the meta data map,
     * using QMap<QString, QVariant> as stored type.
     *
     * \since 4.3.1
     */
    QVariant toVariant() const;
};

inline KIO::MetaData::MetaData(const QMap<QString, QVariant> &map)
{
    *this = map;
}

inline KIO::MetaData &KIO::MetaData::operator+=(const QMap<QString, QVariant> &metaData)
{
    for (const auto &[key, value] : metaData.asKeyValueRange()) {
        insert(key, value.toString());
    }

    return *this;
}

inline KIO::MetaData &KIO::MetaData::operator=(const QMap<QString, QVariant> &metaData)
{
    clear();
    return (*this += metaData);
}

inline QVariant KIO::MetaData::toVariant() const
{
    QMap<QString, QVariant> map;

    for (const auto &[key, value] : asKeyValueRange()) {
        map.insert(key, QVariant(value));
    }

    return QVariant(map);
}

} // namespace KIO

#endif /* KIO_METADATA_H */
