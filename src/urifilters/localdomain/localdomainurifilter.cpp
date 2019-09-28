/*
    localdomainfilter.cpp

    This file is part of the KDE project
    Copyright (C) 2002 Lubos Lunak <llunak@suse.cz>
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

#include "localdomainurifilter.h"

#include <KProtocolInfo>
#include <KPluginFactory>

#include <QHostInfo>
#include <QLoggingCategory>

#define QL1C(x)   QLatin1Char(x)
#define QL1S(x)   QLatin1String(x)

#define HOSTPORT_PATTERN "[a-zA-Z0-9][a-zA-Z0-9+-]*(?:\\:[0-9]{1,5})?(?:/[\\w:@&=+$,-.!~*'()]*)*"

namespace {
QLoggingCategory category("org.kde.kurifilter-localdomain", QtWarningMsg);
}

/**
 * IMPORTANT: If you change anything here, make sure you run the kurifiltertest
 * regression test (this should be included as part of "make test").
 */
LocalDomainUriFilter::LocalDomainUriFilter(QObject *parent, const QVariantList & /*args*/)
    : KUriFilterPlugin(QStringLiteral("localdomainurifilter"), parent)
    , m_hostPortPattern(QL1S(HOSTPORT_PATTERN))
{
}

bool LocalDomainUriFilter::filterUri(KUriFilterData &data) const
{
    const QUrl url = data.uri();
    const QString protocol = url.scheme();

    // When checking for local domain just validate it is indeed a local domain,
    // but do not modify the hostname! See bug#
    if ((protocol.isEmpty() || !KProtocolInfo::isKnownProtocol(protocol))
        && m_hostPortPattern.exactMatch(data.typedString())) {
        QString host(data.typedString().left(data.typedString().indexOf(QL1C('/'))));
        const int pos = host.indexOf(QL1C(':'));
        if (pos > -1) {
            host.truncate(pos); // Remove port number
        }
        if (exists(host)) {
            qCDebug(category) << "QHostInfo found a host called" << host;
            QString scheme(data.defaultUrlScheme());
            if (scheme.isEmpty()) {
                scheme = QStringLiteral("http://");
            }
            setFilteredUri(data, QUrl(scheme + data.typedString()));
            setUriType(data, KUriFilterData::NetProtocol);
            return true;
        }
    }

    return false;
}

bool LocalDomainUriFilter::exists(const QString &host) const
{
    qCDebug(category) << "Checking if a host called" << host << "exists";
    QHostInfo hostInfo = resolveName(host, 1500);
    return hostInfo.error() == QHostInfo::NoError;
}

K_PLUGIN_CLASS_WITH_JSON(LocalDomainUriFilter, "localdomainurifilter.json")

#include "localdomainurifilter.moc"
