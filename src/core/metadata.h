/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2005 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/
#ifndef KIO_METADATA_H
#define KIO_METADATA_H

#include <QMap>
#include <QString>
#include <QVariant>
#include "kiocore_export.h"

namespace KIO
{

/**
 * @class KIO::MetaData metadata.h <KIO/MetaData>
 *
 * MetaData is a simple map of key/value strings.
 */
class MetaData : public QMap<QString, QString>
{
public:
    /**
     * Creates an empty meta data map.
     */
    MetaData() : QMap<QString, QString>() { }
    /**
     * Copy constructor.
     */
    MetaData(const QMap<QString, QString> &metaData) :
        QMap<QString, QString>(metaData) { }

    /**
     * Creates a meta data map from a QVaraint map.
     * @since 4.3.1
     */
    MetaData(const QMap<QString, QVariant> &);

    /**
     * Adds the given meta data map to this map.
     * @param metaData the map to add
     * @return this map
     */
    MetaData &operator += (const QMap<QString, QString> &metaData)
    {
        QMap<QString, QString>::ConstIterator it;
        for (it = metaData.constBegin(); it !=  metaData.constEnd(); ++it) {
            insert(it.key(), it.value());
        }
        return *this;
    }

    /**
     * Same as above except the value in the map is a QVariant.
     *
     * This convenience function allows you to easily assign the values
     * of a QVariant to this meta data class.
     *
     * @param metaData the map to add
     * @return this map
     * @since 4.3.1
     */
    MetaData &operator += (const QMap<QString, QVariant> &metaData);

    /**
     * Sets the given meta data map to this map.
     * @param metaData the map to add
     * @return this map
     * @since 4.3.1
     */
    MetaData &operator = (const QMap<QString, QVariant> &metaData);

    /**
     * Returns the contents of the map as a QVariant.
     *
     * @return a QVariant representation of the meta data map.
     * @since 4.3.1
     */
    QVariant toVariant() const;
};

inline KIO::MetaData::MetaData(const QMap<QString, QVariant> &map)
{
    *this = map;
}

inline KIO::MetaData &KIO::MetaData::operator += (const QMap<QString, QVariant> &metaData)
{
    QMapIterator<QString, QVariant> it(metaData);

    while (it.hasNext()) {
        it.next();
        insert(it.key(), it.value().toString());
    }

    return *this;
}

inline KIO::MetaData &KIO::MetaData::operator = (const QMap<QString, QVariant> &metaData)
{
    clear();
    return (*this += metaData);
}

inline QVariant KIO::MetaData::toVariant() const
{
    QMap<QString, QVariant> map;
    QMapIterator <QString, QString> it(*this);

    while (it.hasNext()) {
        it.next();
        map.insert(it.key(), it.value());
    }

    return QVariant(map);
}

} // namespace KIO

#endif /* KIO_METADATA_H */
