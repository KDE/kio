/*
    fixhostfilter.cpp

    This file is part of the KDE project
    Copyright (C) 2007 Lubos Lunak <llunak@suse.cz>
    Copyright (C) 2010 Dawit Alemayehu <adawit@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "fixhosturifilter.h"

#include <QtNetwork/QHostInfo>

#include <KPluginFactory>

#define QL1S(x)   QLatin1String(x)
#define QL1C(x)   QLatin1Char(x)

/**
 * IMPORTANT: If you change anything here, please run the regression test
 * ../tests/kurifiltertest
 */

FixHostUriFilter::FixHostUriFilter(QObject *parent, const QVariantList & /*args*/)
                 :KUriFilterPlugin("fixhosturifilter", parent)
{
}

static bool isHttpUrl(const QString& scheme)
{
    return (scheme.compare(QL1S("http"), Qt::CaseInsensitive) == 0 ||
            scheme.compare(QL1S("https"), Qt::CaseInsensitive) == 0 ||
            scheme.compare(QL1S("webdav"), Qt::CaseInsensitive) == 0 ||
            scheme.compare(QL1S("webdavs"), Qt::CaseInsensitive) == 0);
}

static bool hasCandidateHostName(const QString& host)
{
    return (host.contains(QL1C('.')) &&
            !host.startsWith(QL1S("www."), Qt::CaseInsensitive));
}

bool FixHostUriFilter::filterUri( KUriFilterData& data ) const
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

bool FixHostUriFilter::isResolvable(const QString& host) const
{
    // Unlike exists, this function returns true if the lookup timeout out.
    QHostInfo info = resolveName(host, 1500);
    return (info.error() == QHostInfo::NoError ||
            info.error() == QHostInfo::UnknownError);
}

bool FixHostUriFilter::exists(const QString& host) const
{
    QHostInfo info = resolveName(host, 1500);
    return (info.error() == QHostInfo::NoError);
}

K_PLUGIN_FACTORY(FixHostUriFilterFactory, registerPlugin<FixHostUriFilter>();)
K_EXPORT_PLUGIN(FixHostUriFilterFactory("kcmkurifilt"))

#include "fixhosturifilter.moc"
