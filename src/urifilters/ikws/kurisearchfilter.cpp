/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1999 Simon Hausmann <hausmann@kde.org>
    SPDX-FileCopyrightText: 2000 Yves Arrouye <yves@realnames.com>
    SPDX-FileCopyrightText: 2002, 2003 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kurisearchfilter.h"
#include "kurifilterdata_p.h"
#include "kuriikwsfiltereng_p.h"
#include "searchprovider.h"

#include <KPluginFactory>

#ifdef WITH_QTDBUS
#include <QDBusConnection>
#endif

#include <QLoggingCategory>

K_PLUGIN_CLASS_WITH_JSON(KUriSearchFilter, "kurisearchfilter.json")

namespace
{
Q_LOGGING_CATEGORY(category, "kf.kio.urifilters.ikws", QtWarningMsg)
}

KUriSearchFilter::~KUriSearchFilter()
{
}

bool KUriSearchFilter::filterUri(KUriFilterData &data) const
{
    qCDebug(category) << data.typedString() << ":" << data.uri() << ", type =" << data.uriType();

    // some URLs like gg:www.kde.org are not accepted by QUrl, but we still want them
    // This means we also have to allow KUriFilterData::Error
    if (data.uriType() != KUriFilterData::Unknown && data.uriType() != KUriFilterData::Error) {
        return false;
    }

    QString searchTerm;
    auto filter = KIO::KURISearchFilterEngine::self();
    SearchProvider *provider(filter->webShortcutQuery(data.typedString(), searchTerm));
    if (!provider) {
        return false;
    }

    const QUrl result = filter->formatResult(provider->query(), provider->charset(), QString(), searchTerm, true);
    setFilteredUri(data, result);
    setUriType(data, KUriFilterData::NetProtocol);
    setSearchProvider(data, provider, searchTerm, QLatin1Char(filter->keywordDelimiter()));
    return true;
}

#include "kurisearchfilter.moc"
#include "moc_kurisearchfilter.cpp"
