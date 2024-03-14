/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Yves Arrouye <yves@realnames.com>
    SPDX-FileCopyrightText: 2000, 2010 Dawit Alemayehu <adawit at kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurifilter.h"
#include "kurifilterdata_p.h"

#include <KService>
#include <kio/global.h>

#include <KPluginFactory>
#include <KPluginMetaData>

#include <QHashIterator>
#include <QHostAddress>
#include <QHostInfo>
#include <QIcon>

#include "kurifilterplugin_p.h"

QString KUriFilterDataPrivate::lookupIconNameFor(const QUrl &url, KUriFilterData::UriTypes type)
{
    QString iconName;

    switch (type) {
    case KUriFilterData::NetProtocol:
        iconName = KIO::iconNameForUrl(url);
        break;
    case KUriFilterData::Executable: {
        QString exeName = url.path();
        exeName.remove(0, exeName.lastIndexOf(QLatin1Char('/')) + 1); // strip path if given
        KService::Ptr service = KService::serviceByDesktopName(exeName);
        if (service && service->icon() != QLatin1String("unknown")) {
            iconName = service->icon();
        }
        // Try to find an icon with the same name as the binary (useful for non-kde apps)
        else if (!QIcon::fromTheme(exeName).isNull()) {
            iconName = exeName;
        } else {
            // not found, use default
            iconName = QStringLiteral("system-run");
        }
        break;
    }
    case KUriFilterData::Help: {
        iconName = QStringLiteral("khelpcenter");
        break;
    }
    case KUriFilterData::Shell: {
        iconName = QStringLiteral("konsole");
        break;
    }
    case KUriFilterData::Error:
    case KUriFilterData::Blocked: {
        iconName = QStringLiteral("error");
        break;
    }
    default:
        break;
    }

    return iconName;
}

class Q_DECL_HIDDEN KUriFilterSearchProvider::KUriFilterSearchProviderPrivate
{
public:
    KUriFilterSearchProviderPrivate()
    {
    }
    KUriFilterSearchProviderPrivate(const KUriFilterSearchProviderPrivate &other)
        : desktopEntryName(other.desktopEntryName)
        , iconName(other.iconName)
        , name(other.name)
        , keys(other.keys)
    {
    }

    QString desktopEntryName;
    QString iconName;
    QString name;
    QStringList keys;
};

KUriFilterSearchProvider::KUriFilterSearchProvider()
    : d(new KUriFilterSearchProvider::KUriFilterSearchProviderPrivate)
{
}

KUriFilterSearchProvider::KUriFilterSearchProvider(const KUriFilterSearchProvider &other)
    : d(new KUriFilterSearchProvider::KUriFilterSearchProviderPrivate(*(other.d)))
{
}

KUriFilterSearchProvider::~KUriFilterSearchProvider() = default;

QString KUriFilterSearchProvider::desktopEntryName() const
{
    return d->desktopEntryName;
}

QString KUriFilterSearchProvider::iconName() const
{
    return d->iconName;
}

QString KUriFilterSearchProvider::name() const
{
    return d->name;
}

QStringList KUriFilterSearchProvider::keys() const
{
    return d->keys;
}

QString KUriFilterSearchProvider::defaultKey() const
{
    if (d->keys.isEmpty()) {
        return QString();
    }

    return d->keys.first();
}

KUriFilterSearchProvider &KUriFilterSearchProvider::operator=(const KUriFilterSearchProvider &other)
{
    d->desktopEntryName = other.d->desktopEntryName;
    d->iconName = other.d->iconName;
    d->keys = other.d->keys;
    d->name = other.d->name;
    return *this;
}

void KUriFilterSearchProvider::setDesktopEntryName(const QString &desktopEntryName)
{
    d->desktopEntryName = desktopEntryName;
}

void KUriFilterSearchProvider::setIconName(const QString &iconName)
{
    d->iconName = iconName;
}

void KUriFilterSearchProvider::setName(const QString &name)
{
    d->name = name;
}

void KUriFilterSearchProvider::setKeys(const QStringList &keys)
{
    d->keys = keys;
}

KUriFilterData::KUriFilterData()
    : d(new KUriFilterDataPrivate(QUrl(), QString()))
{
}

KUriFilterData::KUriFilterData(const QUrl &url)
    : d(new KUriFilterDataPrivate(url, url.toString()))
{
}

KUriFilterData::KUriFilterData(const QString &url)
    : d(new KUriFilterDataPrivate(QUrl::fromUserInput(url), url))
{
}

KUriFilterData::KUriFilterData(const KUriFilterData &other)
    : d(new KUriFilterDataPrivate(*other.d))
{
}

KUriFilterData::~KUriFilterData() = default;

QUrl KUriFilterData::uri() const
{
    return d->url;
}

QString KUriFilterData::errorMsg() const
{
    return d->errMsg;
}

KUriFilterData::UriTypes KUriFilterData::uriType() const
{
    return d->uriType;
}

QString KUriFilterData::absolutePath() const
{
    return d->absPath;
}

bool KUriFilterData::hasAbsolutePath() const
{
    return !d->absPath.isEmpty();
}

QString KUriFilterData::argsAndOptions() const
{
    return d->args;
}

bool KUriFilterData::hasArgsAndOptions() const
{
    return !d->args.isEmpty();
}

bool KUriFilterData::checkForExecutables() const
{
    return d->checkForExecs;
}

QString KUriFilterData::typedString() const
{
    return d->typedString;
}

QString KUriFilterData::searchTerm() const
{
    return d->searchTerm;
}

QChar KUriFilterData::searchTermSeparator() const
{
    return d->searchTermSeparator;
}

QString KUriFilterData::searchProvider() const
{
    return d->searchProvider;
}

QStringList KUriFilterData::preferredSearchProviders() const
{
    return d->searchProviderList;
}

KUriFilterSearchProvider KUriFilterData::queryForSearchProvider(const QString &provider) const
{
    const KUriFilterSearchProvider *searchProvider = d->searchProviderMap.value(provider);

    if (searchProvider) {
        return *(searchProvider);
    }

    return KUriFilterSearchProvider();
}

QString KUriFilterData::queryForPreferredSearchProvider(const QString &provider) const
{
    const KUriFilterSearchProvider *searchProvider = d->searchProviderMap.value(provider);
    if (searchProvider) {
        return (searchProvider->defaultKey() % searchTermSeparator() % searchTerm());
    }
    return QString();
}

QStringList KUriFilterData::allQueriesForSearchProvider(const QString &provider) const
{
    const KUriFilterSearchProvider *searchProvider = d->searchProviderMap.value(provider);
    if (searchProvider) {
        return searchProvider->keys();
    }
    return QStringList();
}

QString KUriFilterData::iconNameForPreferredSearchProvider(const QString &provider) const
{
    const KUriFilterSearchProvider *searchProvider = d->searchProviderMap.value(provider);
    if (searchProvider) {
        return searchProvider->iconName();
    }
    return QString();
}

QStringList KUriFilterData::alternateSearchProviders() const
{
    return d->alternateSearchProviders;
}

QString KUriFilterData::alternateDefaultSearchProvider() const
{
    return d->alternateDefaultSearchProvider;
}

QString KUriFilterData::defaultUrlScheme() const
{
    return d->defaultUrlScheme;
}

KUriFilterData::SearchFilterOptions KUriFilterData::searchFilteringOptions() const
{
    return d->searchFilterOptions;
}

QString KUriFilterData::iconName()
{
    auto foundProvider = d->searchProviderMap.constFind(searchProvider());
    if (foundProvider != d->searchProviderMap.cend() && !foundProvider.value()->iconName().isEmpty()) {
        return foundProvider.value()->iconName();
    }

    if (d->wasModified) {
        d->iconName = KUriFilterDataPrivate::lookupIconNameFor(d->url, d->uriType);
        d->wasModified = false;
    }

    return d->iconName;
}

void KUriFilterData::setData(const QUrl &url)
{
    d->setData(url, url.toString());
}

void KUriFilterData::setData(const QString &url)
{
    d->setData(QUrl(url), url);
}

bool KUriFilterData::setAbsolutePath(const QString &absPath)
{
    // Since a malformed URL could possibly be a relative
    // URL we tag it as a possible local resource...
    if ((d->url.scheme().isEmpty() || d->url.isLocalFile())) {
        d->absPath = absPath;
        return true;
    }
    return false;
}

void KUriFilterData::setCheckForExecutables(bool check)
{
    d->checkForExecs = check;
}

void KUriFilterData::setAlternateSearchProviders(const QStringList &providers)
{
    d->alternateSearchProviders = providers;
}

void KUriFilterData::setAlternateDefaultSearchProvider(const QString &provider)
{
    d->alternateDefaultSearchProvider = provider;
}

void KUriFilterData::setDefaultUrlScheme(const QString &scheme)
{
    d->defaultUrlScheme = scheme;
}

void KUriFilterData::setSearchFilteringOptions(SearchFilterOptions options)
{
    d->searchFilterOptions = options;
}

KUriFilterData &KUriFilterData::operator=(const QUrl &url)
{
    d->setData(url, url.toString());
    return *this;
}

KUriFilterData &KUriFilterData::operator=(const QString &url)
{
    d->setData(QUrl(url), url);
    return *this;
}

/*******************************  KUriFilter ******************************/

class KUriFilterPrivate
{
public:
    KUriFilterPrivate()
    {
    }
    ~KUriFilterPrivate()
    {
        qDeleteAll(pluginList);
        pluginList.clear();
    }
    QList<KUriFilterPlugin *> pluginList;
};

class KUriFilterSingleton
{
public:
    KUriFilter instance;
};

Q_GLOBAL_STATIC(KUriFilterSingleton, m_self)

KUriFilter *KUriFilter::self()
{
    return &m_self()->instance;
}

KUriFilter::KUriFilter()
    : d(new KUriFilterPrivate())
{
    QList<KPluginMetaData> plugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/urifilters"));
    const QString prefKey = QStringLiteral("X-KDE-InitialPreference");
    // Sort the plugins by order of priority
    std::sort(plugins.begin(), plugins.end(), [prefKey](const KPluginMetaData &a, const KPluginMetaData &b) {
        return a.value(prefKey, 0) > b.value(prefKey, 0);
    });

    for (const KPluginMetaData &pluginMetaData : std::as_const(plugins)) {
        if (auto plugin = KPluginFactory::instantiatePlugin<KUriFilterPlugin>(pluginMetaData).plugin) {
            d->pluginList << plugin;
        }
    }
}

KUriFilter::~KUriFilter() = default;

bool KUriFilter::filterUri(KUriFilterData &data, const QStringList &filters)
{
    bool filtered = false;

    for (KUriFilterPlugin *plugin : std::as_const(d->pluginList)) {
        // If no specific filters were requested, iterate through all the plugins.
        // Otherwise, only use available filters.
        if (filters.isEmpty() || filters.contains(plugin->objectName())) {
            if (plugin->filterUri(data)) {
                filtered = true;
            }
        }
    }

    return filtered;
}

bool KUriFilter::filterUri(QUrl &uri, const QStringList &filters)
{
    KUriFilterData data(uri);
    bool filtered = filterUri(data, filters);
    if (filtered) {
        uri = data.uri();
    }
    return filtered;
}

bool KUriFilter::filterUri(QString &uri, const QStringList &filters)
{
    KUriFilterData data(uri);
    bool filtered = filterUri(data, filters);
    if (filtered) {
        uri = data.uri().toString();
    }
    return filtered;
}

QUrl KUriFilter::filteredUri(const QUrl &uri, const QStringList &filters)
{
    KUriFilterData data(uri);
    filterUri(data, filters);
    return data.uri();
}

QString KUriFilter::filteredUri(const QString &uri, const QStringList &filters)
{
    KUriFilterData data(uri);
    filterUri(data, filters);
    return data.uri().toString();
}

bool KUriFilter::filterSearchUri(KUriFilterData &data, SearchFilterTypes types)
{
    QStringList filters;

    if (types & WebShortcutFilter) {
        filters << QStringLiteral("kurisearchfilter");
    }

    if (types & NormalTextFilter) {
        filters << QStringLiteral("kuriikwsfilter");
    }

    return filterUri(data, filters);
}

QStringList KUriFilter::pluginNames() const
{
    QStringList res;
    res.reserve(d->pluginList.size());
    std::transform(d->pluginList.constBegin(), d->pluginList.constEnd(), std::back_inserter(res), [](const KUriFilterPlugin *plugin) {
        return plugin->objectName();
    });
    return res;
}
