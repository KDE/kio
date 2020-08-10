/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2017 David Faure <faure@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef SEARCHPROVIDERREGISTRY_H
#define SEARCHPROVIDERREGISTRY_H

#include <QList>
#include <QMap>

class SearchProvider;

/**
 * Memory cache for search provider desktop files
 */
class SearchProviderRegistry
{
public:
    /**
     * Default constructor
     */
    SearchProviderRegistry();

    /**
     * Destructor
     */
    ~SearchProviderRegistry();

    SearchProviderRegistry(const SearchProviderRegistry &) = delete;
    SearchProviderRegistry &operator=(const SearchProviderRegistry &) = delete;

    QList<SearchProvider *> findAll();

    SearchProvider *findByKey(const QString &key) const;

    SearchProvider *findByDesktopName(const QString &desktopName) const;

    void reload();

private:
    QStringList directories() const;

    QList<SearchProvider *> m_searchProviders;
    QMap<QString, SearchProvider *> m_searchProvidersByKey;
    QMap<QString, SearchProvider *> m_searchProvidersByDesktopName;
};

#endif // SEARCHPROVIDERREGISTRY_H
