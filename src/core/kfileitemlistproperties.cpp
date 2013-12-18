/* This file is part of the KDE project
   Copyright (C) 2008 by Peter Penz <peter.penz@gmx.at>
   Copyright (C) 2008 by George Goldberg <grundleborg@googlemail.com>
   Copyright     2009 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2 of the License or
   ( at your option ) version 3 or, at the discretion of KDE e.V.
   ( which shall act as a proxy as in section 14 of the GPLv3 ), any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kfileitemlistproperties.h"

#include <kfileitem.h>
#include <kprotocolmanager.h>

#include <QFileInfo>

class KFileItemListPropertiesPrivate : public QSharedData
{
public:
    KFileItemListPropertiesPrivate()
        : m_isDirectory(false),
          m_supportsReading(false),
          m_supportsDeleting(false),
          m_supportsWriting(false),
          m_supportsMoving(false),
          m_isLocal(true)
    { }
    void setItems(const KFileItemList& items);

    void determineMimeTypeAndGroup() const;

    KFileItemList m_items;
    QList<QUrl> m_urlList;
    mutable QString m_mimeType;
    mutable QString m_mimeGroup;
    bool m_isDirectory : 1;
    bool m_supportsReading : 1;
    bool m_supportsDeleting : 1;
    bool m_supportsWriting : 1;
    bool m_supportsMoving : 1;
    bool m_isLocal : 1;
};


KFileItemListProperties::KFileItemListProperties()
    : d(new KFileItemListPropertiesPrivate)
{
}

KFileItemListProperties::KFileItemListProperties(const KFileItemList& items)
    : d(new KFileItemListPropertiesPrivate)
{
    setItems(items);
}

void KFileItemListProperties::setItems(const KFileItemList& items)
{
    d->setItems(items);
}

void KFileItemListPropertiesPrivate::setItems(const KFileItemList& items)
{
    const bool initialValue = !items.isEmpty();
    m_items = items;
    m_urlList = items.targetUrlList();
    m_supportsReading = initialValue;
    m_supportsDeleting = initialValue;
    m_supportsWriting = initialValue;
    m_supportsMoving = initialValue;
    m_isDirectory = initialValue;
    m_isLocal = true;
    m_mimeType.clear();
    m_mimeGroup.clear();

    QFileInfo parentDirInfo;
    foreach (const KFileItem &item, items) {
        const QUrl url = item.url();
        m_isLocal = m_isLocal && url.isLocalFile();
        m_supportsReading  = m_supportsReading  && KProtocolManager::supportsReading(url);
        m_supportsDeleting = m_supportsDeleting && KProtocolManager::supportsDeleting(url);
        m_supportsWriting  = m_supportsWriting  && KProtocolManager::supportsWriting(url) && item.isWritable();
        m_supportsMoving   = m_supportsMoving   && KProtocolManager::supportsMoving(url);

        // For local files we can do better: check if we have write permission in parent directory
        // TODO: if we knew about the parent KFileItem, we could even do that for remote protocols too
        if (m_isLocal && (m_supportsDeleting || m_supportsMoving)) {
            const QString directory = url.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash).path();
            if (parentDirInfo.filePath() != directory) {
                parentDirInfo.setFile(directory);
            }
            if (!parentDirInfo.isWritable()) {
                m_supportsDeleting = false;
                m_supportsMoving = false;
            }
        }
        if (m_isDirectory && !item.isDir()) {
            m_isDirectory = false;
        }
    }
}

KFileItemListProperties::KFileItemListProperties(const KFileItemListProperties& other)
    : d(other.d)
{ }


KFileItemListProperties& KFileItemListProperties::operator=(const KFileItemListProperties& other)
{
    d = other.d;
    return *this;
}

KFileItemListProperties::~KFileItemListProperties()
{
}

bool KFileItemListProperties::supportsReading() const
{
    return d->m_supportsReading;
}

bool KFileItemListProperties::supportsDeleting() const
{
    return d->m_supportsDeleting;
}

bool KFileItemListProperties::supportsWriting() const
{
    return d->m_supportsWriting;
}

bool KFileItemListProperties::supportsMoving() const
{
    return d->m_supportsMoving && d->m_supportsDeleting;
}

bool KFileItemListProperties::isLocal() const
{
    return d->m_isLocal;
}

KFileItemList KFileItemListProperties::items() const
{
    return d->m_items;
}

QList<QUrl> KFileItemListProperties::urlList() const
{
    return d->m_urlList;
}

bool KFileItemListProperties::isDirectory() const
{
    return d->m_isDirectory;
}

QString KFileItemListProperties::mimeType() const
{
    if (d->m_mimeType.isEmpty())
        d->determineMimeTypeAndGroup();
    return d->m_mimeType;
}

QString KFileItemListProperties::mimeGroup() const
{
    if (d->m_mimeType.isEmpty())
        d->determineMimeTypeAndGroup();
    return d->m_mimeGroup;
}

void KFileItemListPropertiesPrivate::determineMimeTypeAndGroup() const
{
    if (!m_items.isEmpty()) {
        m_mimeType = m_items.first().mimetype();
        m_mimeGroup = m_mimeType.left(m_mimeType.indexOf('/'));
    }
    foreach (const KFileItem &item, m_items) {
        const QString itemMimeType = item.mimetype();
        // Determine if common mimetype among all items
        if (m_mimeType != itemMimeType) {
            m_mimeType.clear();
            if (m_mimeGroup != itemMimeType.left(itemMimeType.indexOf('/'))) {
                m_mimeGroup.clear(); // mimetype groups are different as well!
            }
        }
    }
}
