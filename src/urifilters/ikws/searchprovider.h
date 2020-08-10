/*
    SPDX-FileCopyrightText: 2000 Malte Starostik <malte@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SEARCHPROVIDER_H
#define SEARCHPROVIDER_H

#include <KUriFilter>

class SearchProvider : public KUriFilterSearchProvider
{
public:
    SearchProvider() : m_dirty(false), m_isHidden(false)
    {
    }

    explicit SearchProvider(const QString &servicePath);
    ~SearchProvider();

    const QString &charset() const
    {
        return m_charset;
    }

    const QString &query() const
    {
        return m_query;
    }

    bool isDirty() const
    {
        return m_dirty;
    }

    bool isHidden() const {
        return m_isHidden;
    }

    void setName(const QString &);
    void setQuery(const QString &);
    void setKeys(const QStringList &);
    void setCharset(const QString &);
    void setDirty(bool dirty);

    QString iconName() const override;

private:
    QString m_query;
    QString m_charset;
    QString m_iconName;
    bool m_dirty;
    bool m_isHidden;
};

#endif
