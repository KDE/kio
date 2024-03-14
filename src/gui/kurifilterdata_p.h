/*
    SPDX-FileCopyrightText: 2000-2001, 2003, 2010 Dawit Alemayehu <adawit at kde.org>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurifilter.h"
#include <QMap>

class KUriFilterDataPrivate
{
public:
    explicit KUriFilterDataPrivate(const QUrl &u, const QString &typedUrl)
        : checkForExecs(true)
        , wasModified(true)
        , uriType(KUriFilterData::Unknown)
        , searchFilterOptions(KUriFilterData::SearchFilterOptionNone)
        , url(u.adjusted(QUrl::NormalizePathSegments))
        , typedString(typedUrl)
    {
    }

    ~KUriFilterDataPrivate()
    {
    }

    static QString lookupIconNameFor(const QUrl &url, KUriFilterData::UriTypes type);

    void setData(const QUrl &u, const QString &typedUrl)
    {
        checkForExecs = true;
        wasModified = true;
        uriType = KUriFilterData::Unknown;
        searchFilterOptions = KUriFilterData::SearchFilterOptionNone;

        url = u.adjusted(QUrl::NormalizePathSegments);
        typedString = typedUrl;

        errMsg.clear();
        iconName.clear();
        absPath.clear();
        args.clear();
        searchTerm.clear();
        searchProvider.clear();
        searchTermSeparator = QChar();
        alternateDefaultSearchProvider.clear();
        alternateSearchProviders.clear();
        searchProviderMap.clear();
        defaultUrlScheme.clear();
    }

    KUriFilterDataPrivate(KUriFilterDataPrivate *data)
    {
        wasModified = data->wasModified;
        checkForExecs = data->checkForExecs;
        uriType = data->uriType;
        searchFilterOptions = data->searchFilterOptions;

        url = data->url;
        typedString = data->typedString;

        errMsg = data->errMsg;
        iconName = data->iconName;
        absPath = data->absPath;
        args = data->args;
        searchTerm = data->searchTerm;
        searchTermSeparator = data->searchTermSeparator;
        searchProvider = data->searchProvider;
        alternateDefaultSearchProvider = data->alternateDefaultSearchProvider;
        alternateSearchProviders = data->alternateSearchProviders;
        searchProviderMap = data->searchProviderMap;
        defaultUrlScheme = data->defaultUrlScheme;
    }

    bool checkForExecs;
    bool wasModified;
    KUriFilterData::UriTypes uriType;
    KUriFilterData::SearchFilterOptions searchFilterOptions;

    QUrl url;
    QString typedString;
    QString errMsg;
    QString iconName;
    QString absPath;
    QString args;
    QString searchTerm;
    QString searchProvider;
    QString alternateDefaultSearchProvider;
    QString defaultUrlScheme;
    QChar searchTermSeparator;

    QStringList alternateSearchProviders;
    QStringList searchProviderList;
    QMap<QString, KUriFilterSearchProvider *> searchProviderMap;
};
