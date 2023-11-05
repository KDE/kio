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

    /**
     * Types of proxy configuration
     * @li NoProxy     - No proxy is used
     * @li ManualProxy - Proxies are manually configured
     * @li PACProxy    - A Proxy configuration URL has been given
     * @li WPADProxy   - A proxy should be automatically discovered
     * @li EnvVarProxy - Use the proxy values set through environment variables.
     */
    enum ProxyType {
        NoProxy,
        ManualProxy,
        PACProxy,
        WPADProxy,
        EnvVarProxy,
    };

    KProtocolManagerPrivate();
    ~KProtocolManagerPrivate();
    bool shouldIgnoreProxyFor(const QUrl &url);
    void sync();
    ProxyType proxyType();
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

    /**
     * Returns all the possible proxy server addresses for @p url.
     *
     * If the selected proxy type is @ref PACProxy or @ref WPADProxy, then a
     * helper kded module, proxyscout, is used to determine the proxy information.
     * Otherwise, @ref proxyFor is used to find the proxy to use for the given url.
     *
     * If this function returns empty list, then the request is to a proxy server
     * must be denied. For a direct connection, this function will return a single
     * entry of "DIRECT".
     *
     *
     * @param url the URL whose proxy info is needed
     * @returns the proxy server address if one is available, otherwise an empty list .
     */
    static QStringList proxiesForUrl(const QUrl &url);

    /**
     * Returns the default user-agent value used for web browsing, for example
     * "Mozilla/5.0 (compatible; Konqueror/4.0; Linux; X11; i686; en_US) KHTML/4.0.1 (like Gecko)"
     *
     * @param keys can be any of the following:
     * @li 'o'    Show OS
     * @li 'v'    Show OS Version
     * @li 'p'    Show platform (only for X11)
     * @li 'm'    Show machine architecture
     * @li 'l'    Show language
     * @return the default user-agent value with the given @p keys
     */
    static QString defaultUserAgent(const QString &keys);

    /**
     * Returns system name, version and machine type, for example "Windows", "5.1", "i686".
     * This information can be used for constructing custom user-agent strings.
     *
     * @param systemName system name
     * @param systemVersion system version
     * @param machine machine type

     * @return true if system name, version and machine type has been provided
     */
    static bool getSystemNameVersionAndMachine(QString &systemName, QString &systemVersion, QString &machine);
};

#endif
