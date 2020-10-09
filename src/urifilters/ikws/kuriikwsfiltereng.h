/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2002, 2003 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 1999 Simon Hausmann <hausmann@kde.org>
    SPDX-FileCopyrightText: 1999 Yves Arrouye <yves@realnames.com>

    Advanced web shortcuts
    SPDX-FileCopyrightText: 2001 Andreas Hochsteger <e9625392@student.tuwien.ac.at>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KURIIKWSFILTERENG_H
#define KURIIKWSFILTERENG_H

#include <QMap>
#include <QStringList>
#include <QUrl>
#include "searchproviderregistry.h"

#define DEFAULT_PREFERRED_SEARCH_PROVIDERS \
    QStringList{QStringLiteral("google"), QStringLiteral("youtube"), QStringLiteral("yahoo"), QStringLiteral("wikipedia"), QStringLiteral("wikit")}

class SearchProvider;

class KURISearchFilterEngine
{
public:
    typedef QMap <QString, QString> SubstMap;

    KURISearchFilterEngine();
    ~KURISearchFilterEngine();

    QByteArray name() const;
    char keywordDelimiter() const;
    QString defaultSearchEngine() const;
    QStringList favoriteEngineList() const;
    SearchProvider *webShortcutQuery(const QString &typedString, QString &searchTerm) const;
    SearchProvider *autoWebSearchQuery(const QString &typedString, const QString &defaultShortcut = QString()) const;
    QUrl formatResult(const QString &url, const QString &cset1, const QString &cset2, const QString &query, bool isMalformed) const;

    SearchProviderRegistry *registry();

    static KURISearchFilterEngine *self();
    void loadConfig();

protected:
    QUrl formatResult(const QString &url, const QString &cset1, const QString &cset2, const QString &query, bool isMalformed, SubstMap &map) const;

private:
    KURISearchFilterEngine(const KURISearchFilterEngine &) = delete;
    KURISearchFilterEngine &operator=(const KURISearchFilterEngine &) = delete;

    QStringList modifySubstitutionMap(SubstMap &map, const QString &query) const;
    QString substituteQuery(const QString &url, SubstMap &map, const QString &userquery, QTextCodec *codec) const;

    SearchProviderRegistry m_registry;
    QString m_defaultWebShortcut;
    QStringList m_preferredWebShortcuts;
    bool m_bWebShortcutsEnabled;
    bool m_bUseOnlyPreferredWebShortcuts;
    char m_cKeywordDelimiter;
};

#endif // KURIIKWSFILTERENG_H
