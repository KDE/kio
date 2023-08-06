/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2000 Waldo Bastain <bastain@kde.org>
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2008 Jarosław Staniek <staniek@kde.org>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KPROTOCOLMANAGER_P_H
#define KPROTOCOLMANAGER_P_H

#include <kiocore_export.h>

#include <QCache>
#include <QHostAddress>
#include <QMutex>
#include <QString>
#include <QUrl>

#include <KSharedConfig>

#include "kprotocolmanager.h"

class KProxyData : public QObject
{
    Q_OBJECT
public:
    KProxyData(const QString &workerProtocol, const QStringList &proxyAddresses)
        : protocol(workerProtocol)
        , proxyList(proxyAddresses)
    {
    }

    void removeAddress(const QString &address)
    {
        proxyList.removeAll(address);
    }

    QString protocol;
    QStringList proxyList;
};

class KIOCORE_EXPORT KProtocolManagerPrivate
{
public:
    using SubnetPair = QPair<QHostAddress, int>;

    KProtocolManagerPrivate();
    ~KProtocolManagerPrivate();
    bool shouldIgnoreProxyFor(const QUrl &url);
    void sync();
    KProtocolManager::ProxyType proxyType();
    bool useReverseProxy();
    QString readNoProxyFor();
    QString proxyFor(const QString &protocol);
    QStringList getSystemProxyFor(const QUrl &url);

    QMutex mutex; // protects all member vars
    KSharedConfig::Ptr configPtr;
    KSharedConfig::Ptr http_config;
    QString modifiers;
    QString useragent;
    QString noProxyFor;
    QList<SubnetPair> noProxySubnets;
    QCache<QString, KProxyData> cachedProxyData;

    QMap<QString /*mimetype*/, QString /*protocol*/> protocolForArchiveMimetypes;

    /**
     * Return the protocol to use in order to handle the given @p url
     * It's usually the same, except that FTP, when handled by a proxy,
     * needs an HTTP KIO worker.
     *
     * When a proxy is to be used, proxy contains the URL for the proxy.
     * @param url the url to check
     * @param proxy the URL of the proxy to use
     * @return the worker protocol (e.g. 'http'), can be null if unknown
     *
     */
    static QString workerProtocol(const QUrl &url, QStringList &proxy);
};

#endif
