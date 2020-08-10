/*
    localdomainfilter.cpp

    This file is part of the KDE project
    SPDX-FileCopyrightText: 2002 Lubos Lunak <llunak@suse.cz>
    SPDX-FileCopyrightText: 2010 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "localdomainurifilter.h"

#include <KProtocolInfo>
#include <KPluginFactory>

#include <QHostInfo>
#include <QLoggingCategory>

#define QL1C(x)   QLatin1Char(x)
#define QL1S(x)   QLatin1String(x)

namespace {
QLoggingCategory category("kf.kio.urifilters.localdomain", QtWarningMsg);
}

/**
 * IMPORTANT: If you change anything here, make sure you run the kurifiltertest
 * regression test (this should be included as part of "make test").
 */
LocalDomainUriFilter::LocalDomainUriFilter(QObject *parent, const QVariantList & /*args*/)
    : KUriFilterPlugin(QStringLiteral("localdomainurifilter"), parent)
{
    m_hostPortPattern = QRegularExpression(QRegularExpression::anchoredPattern(
         QStringLiteral("[a-zA-Z0-9][a-zA-Z0-9+-]*(?:\\:[0-9]{1,5})?(?:/[\\w:@&=+$,-.!~*'()]*)*")));
}

bool LocalDomainUriFilter::filterUri(KUriFilterData &data) const
{
    const QUrl url = data.uri();
    const QString protocol = url.scheme();

    // When checking for local domain just validate it is indeed a local domain,
    // but do not modify the hostname! See bug#
    if ((protocol.isEmpty() || !KProtocolInfo::isKnownProtocol(protocol))
        && m_hostPortPattern.match(data.typedString()).hasMatch()) {
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
