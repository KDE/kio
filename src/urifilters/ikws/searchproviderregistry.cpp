/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2017 David Faure <faure@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "searchproviderregistry.h"
#include "searchprovider.h"

#include <QStandardPaths>
#include <QDir>

SearchProviderRegistry::SearchProviderRegistry()
{
    reload();
}

SearchProviderRegistry::~SearchProviderRegistry()
{
    qDeleteAll(m_searchProviders);
}

QStringList SearchProviderRegistry::directories() const
{
    const QString testDir = QFile::decodeName(qgetenv("KIO_SEARCHPROVIDERS_DIR")); // for unittests
    if (!testDir.isEmpty()) {
        return { testDir };
    }
    return QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("kservices5/searchproviders/"), QStandardPaths::LocateDirectory);
}

void SearchProviderRegistry::reload()
{
    m_searchProvidersByKey.clear();
    m_searchProvidersByDesktopName.clear();
    qDeleteAll(m_searchProviders);
    m_searchProviders.clear();

    const QStringList servicesDirs = directories();
    for (const QString &dirPath : servicesDirs) {
        QDir dir(dirPath);
        const auto files = dir.entryList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QString &file : files) {
            if (!m_searchProvidersByDesktopName.contains(file)) {
                const QString filePath = dir.path() + QLatin1Char('/') + file;
                auto *provider = new SearchProvider(filePath);
                m_searchProvidersByDesktopName.insert(file, provider);
                m_searchProviders.append(provider);
                const auto keys = provider->keys();
                for (const QString &key : keys) {
                    m_searchProvidersByKey.insert(key, provider);
                }
            }
        }
    }
}

QList<SearchProvider *> SearchProviderRegistry::findAll()
{
    return m_searchProviders;
}

SearchProvider *SearchProviderRegistry::findByKey(const QString &key) const
{
    return m_searchProvidersByKey.value(key);
}

SearchProvider *SearchProviderRegistry::findByDesktopName(const QString &name) const
{
    return m_searchProvidersByDesktopName.value(name + QLatin1String(".desktop"));
}
