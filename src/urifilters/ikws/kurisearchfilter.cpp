/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1999 Simon Hausmann <hausmann@kde.org>
    SPDX-FileCopyrightText: 2000 Yves Arrouye <yves@realnames.com>
    SPDX-FileCopyrightText: 2002, 2003 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kurisearchfilter.h"
#include "ikwsopts.h"
#include "kuriikwsfiltereng.h"
#include "searchprovider.h"

#include <KLocalizedString>
#include <KPluginFactory>

#include <QDBusConnection>
#include <QLoggingCategory>

/**
 * IMPORTANT: If you change anything here, make sure you run the kurifiltertest
 * regression test (this should be included as part of "make test").
 */

K_PLUGIN_FACTORY_WITH_JSON(KUriSearchFilterFactory, "kurisearchfilter.json", registerPlugin<KUriSearchFilter>(); registerPlugin<FilterOptions>();)

namespace
{
Q_LOGGING_CATEGORY(category, "kf.kio.urifilters.ikws", QtWarningMsg)
}

KUriSearchFilter::KUriSearchFilter(QObject *parent, const QVariantList &)
    : KUriFilterPlugin(QStringLiteral("kurisearchfilter"), parent)
{
    QDBusConnection::sessionBus()
        .connect(QString(), QStringLiteral("/"), QStringLiteral("org.kde.KUriFilterPlugin"), QStringLiteral("configure"), this, SLOT(configure()));
}

KUriSearchFilter::~KUriSearchFilter()
{
}

void KUriSearchFilter::configure()
{
    qCDebug(category) << "Config reload requested...";
    KURISearchFilterEngine::self()->loadConfig();
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
    KURISearchFilterEngine *filter = KURISearchFilterEngine::self();
    SearchProvider *provider(filter->webShortcutQuery(data.typedString(), searchTerm));
    if (!provider) {
        return false;
    }

    const QUrl result = filter->formatResult(provider->query(), provider->charset(), QString(), searchTerm, true);
    setFilteredUri(data, result);
    setUriType(data, KUriFilterData::NetProtocol);
    setSearchProvider(data, provider->name(), searchTerm, QLatin1Char(filter->keywordDelimiter()));
    return true;
}

// TODO KF6: unused, remove
KCModule *KUriSearchFilter::configModule(QWidget *, const char *) const
{
    return nullptr;
}

// TODO KF6: unused, remove
QString KUriSearchFilter::configName() const
{
    return i18n("Search F&ilters");
}

#include "kurisearchfilter.moc"
