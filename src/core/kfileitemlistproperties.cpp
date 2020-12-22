/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2008 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2008 George Goldberg <grundleborg@googlemail.com>
    SPDX-FileCopyrightText: 2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
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
          m_isFile(false),
          m_supportsReading(false),
          m_supportsDeleting(false),
          m_supportsWriting(false),
          m_supportsMoving(false),
          m_isLocal(true)
    { }
    void setItems(const KFileItemList &items);

    void determineMimeTypeAndGroup() const;

    KFileItemList m_items;
    mutable QString m_mimeType;
    mutable QString m_mimeGroup;
    bool m_isDirectory : 1;
    bool m_isFile : 1;
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

KFileItemListProperties::KFileItemListProperties(const KFileItemList &items)
    : d(new KFileItemListPropertiesPrivate)
{
    setItems(items);
}

void KFileItemListProperties::setItems(const KFileItemList &items)
{
    d->setItems(items);
}

void KFileItemListPropertiesPrivate::setItems(const KFileItemList &items)
{
    const bool initialValue = !items.isEmpty();
    m_items = items;
    m_supportsReading = initialValue;
    m_supportsDeleting = initialValue;
    m_supportsWriting = initialValue;
    m_supportsMoving = initialValue;
    m_isDirectory = initialValue;
    m_isFile = initialValue;
    m_isLocal = true;
    m_mimeType.clear();
    m_mimeGroup.clear();

    QFileInfo parentDirInfo;
    for (const KFileItem &item : items) {
        bool isLocal = false;
        const QUrl url = item.mostLocalUrl(&isLocal);
        m_isLocal = m_isLocal && isLocal;
        m_supportsReading  = m_supportsReading  && KProtocolManager::supportsReading(url);
        m_supportsDeleting = m_supportsDeleting && KProtocolManager::supportsDeleting(url);
        m_supportsWriting  = m_supportsWriting  && KProtocolManager::supportsWriting(url) && item.isWritable();
        m_supportsMoving   = m_supportsMoving   && KProtocolManager::supportsMoving(url);

        // For local files we can do better: check if we have write permission in parent directory
        // TODO: if we knew about the parent KFileItem, we could even do that for remote protocols too
#ifndef Q_OS_WIN
        if (m_isLocal && (m_supportsDeleting || m_supportsMoving)) {
            const QString directory = url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile();
            if (parentDirInfo.filePath() != directory) {
                parentDirInfo.setFile(directory);
            }
            if (!parentDirInfo.isWritable()) {
                m_supportsDeleting = false;
                m_supportsMoving = false;
            }
        }
#else
        if (m_isLocal && m_supportsDeleting) {
            if (!QFileInfo(url.toLocalFile()).isWritable())
                m_supportsDeleting = false;
        }
#endif
        if (m_isDirectory && !item.isDir()) {
            m_isDirectory = false;
        }

        if (m_isFile && !item.isFile()) {
            m_isFile = false;
        }
    }
}

KFileItemListProperties::KFileItemListProperties(const KFileItemListProperties &other)
    : d(other.d)
{ }

KFileItemListProperties &KFileItemListProperties::operator=(const KFileItemListProperties &other)
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
    return d->m_items.targetUrlList();
}

bool KFileItemListProperties::isDirectory() const
{
    return d->m_isDirectory;
}

bool KFileItemListProperties::isFile() const
{
    return d->m_isFile;
}

QString KFileItemListProperties::mimeType() const
{
    if (d->m_mimeType.isEmpty()) {
        d->determineMimeTypeAndGroup();
    }
    return d->m_mimeType;
}

QString KFileItemListProperties::mimeGroup() const
{
    if (d->m_mimeType.isEmpty()) {
        d->determineMimeTypeAndGroup();
    }
    return d->m_mimeGroup;
}

void KFileItemListPropertiesPrivate::determineMimeTypeAndGroup() const
{
    if (!m_items.isEmpty()) {
        m_mimeType = m_items.first().mimetype();
        m_mimeGroup = m_mimeType.left(m_mimeType.indexOf(QLatin1Char('/')));
    }
    for (const KFileItem &item : qAsConst(m_items)) {
        const QString itemMimeType = item.mimetype();
        // Determine if common MIME type among all items
        if (m_mimeType != itemMimeType) {
            m_mimeType.clear();
            if (m_mimeGroup != itemMimeType.leftRef(itemMimeType.indexOf(QLatin1Char('/')))) {
                m_mimeGroup.clear(); // MIME type groups are different as well!
            }
        }
    }
}
