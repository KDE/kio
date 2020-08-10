/*
    fixhostfilter.cpp

    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Lubos Lunak <llunak@suse.cz>
    SPDX-FileCopyrightText: 2010 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fixhosturifilter.h"

#include <QHostInfo>

#include <KPluginFactory>

#define QL1S(x)   QLatin1String(x)
#define QL1C(x)   QLatin1Char(x)

/**
 * IMPORTANT: If you change anything here, make sure you run the kurifiltertest
 * regression test (this should be included as part of "make test").
 */

FixHostUriFilter::FixHostUriFilter(QObject *parent, const QVariantList & /*args*/)
    : KUriFilterPlugin(QStringLiteral("fixhosturifilter"), parent)
{
}

static bool isHttpUrl(const QString &scheme)
{
    return scheme.compare(QL1S("http"), Qt::CaseInsensitive) == 0
           || scheme.compare(QL1S("https"), Qt::CaseInsensitive) == 0
           || scheme.compare(QL1S("webdav"), Qt::CaseInsensitive) == 0
           || scheme.compare(QL1S("webdavs"), Qt::CaseInsensitive) == 0;
}

static bool hasCandidateHostName(const QString &host)
{
    return host.contains(QL1C('.'))
           && !host.startsWith(QL1S("www."), Qt::CaseInsensitive);
}

bool FixHostUriFilter::filterUri(KUriFilterData &data) const
{
    QUrl url = data.uri();

    const QString protocol = url.scheme();
    const bool isHttp = isHttpUrl(protocol);

    if (isHttp || protocol == data.defaultUrlScheme()) {
        const QString host = url.host();
        if (hasCandidateHostName(host) && !isResolvable(host)) {
            if (isHttp) {
                url.setHost((QL1S("www.") + host));
                if (exists(url.host())) {
                    setFilteredUri(data, url);
                    setUriType(data, KUriFilterData::NetProtocol);
                    return true;
                }
            }
        }
    }

    return false;
}

bool FixHostUriFilter::isResolvable(const QString &host) const
{
    // Unlike exists, this function returns true if the lookup timeout out.
    QHostInfo info = resolveName(host, 1500);
    return info.error() == QHostInfo::NoError
           || info.error() == QHostInfo::UnknownError;
}

bool FixHostUriFilter::exists(const QString &host) const
{
    QHostInfo info = resolveName(host, 1500);
    return info.error() == QHostInfo::NoError;
}

K_PLUGIN_CLASS_WITH_JSON(FixHostUriFilter, "fixhosturifilter.json")

#include "fixhosturifilter.moc"
