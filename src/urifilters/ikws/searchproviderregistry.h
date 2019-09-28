/*
 * This file is part of the KDE project
 * Copyright 2017  David Faure <faure@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

private:
    void reload();
    QStringList directories() const;

    QList<SearchProvider *> m_searchProviders;
    QMap<QString, SearchProvider *> m_searchProvidersByKey;
    QMap<QString, SearchProvider *> m_searchProvidersByDesktopName;
};

#endif // SEARCHPROVIDERREGISTRY_H
