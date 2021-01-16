/*
    SPDX-FileCopyrightText: 2003 Malte Starostik <malte@kde.org>
    SPDX-FileCopyrightText: 2011 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "proxyscout.h"

#include "config-kpac.h"

#include "discovery.h"
#include "script.h"

#include <QDebug>
#include <KLocalizedString>
#include <kprotocolmanager.h>
#include <KPluginFactory>

#ifdef HAVE_KF5NOTIFICATIONS
#include <KNotification>
#endif

#include <QNetworkConfigurationManager>

#include <QFileSystemWatcher>
#include <QDBusConnection>

#include <cstdlib>
#include <ctime>

namespace KPAC
{

K_PLUGIN_CLASS_WITH_JSON(ProxyScout, "proxyscout.json")

enum ProxyType {
    Unknown = -1,
    Proxy,
    Socks,
    Direct,
};

static ProxyType proxyTypeFor(const QString &mode)
{
    if (mode.compare(QLatin1String("PROXY"), Qt::CaseInsensitive) == 0) {
        return Proxy;
    }

    if (mode.compare(QLatin1String("DIRECT"), Qt::CaseInsensitive) == 0) {
        return Direct;
    }

    if (mode.compare(QLatin1String("SOCKS"), Qt::CaseInsensitive) == 0 ||
            mode.compare(QLatin1String("SOCKS5"), Qt::CaseInsensitive) == 0) {
        return Socks;
    }

    return Unknown;
}

ProxyScout::QueuedRequest::QueuedRequest(const QDBusMessage &reply, const QUrl &u, bool sendall)
    : transaction(reply), url(u), sendAll(sendall)
{
}

ProxyScout::ProxyScout(QObject *parent, const QList<QVariant> &)
    : KDEDModule(parent),
      m_componentName(QStringLiteral("proxyscout")),
      m_downloader(nullptr),
      m_script(nullptr),
      m_suspendTime(0),
      m_watcher(nullptr),
      m_networkConfig(new QNetworkConfigurationManager(this))
{
    connect(m_networkConfig, &QNetworkConfigurationManager::configurationChanged,
            this, &ProxyScout::disconnectNetwork);
}

ProxyScout::~ProxyScout()
{
    delete m_script;
}

QStringList ProxyScout::proxiesForUrl(const QString &checkUrl, const QDBusMessage &msg)
{
    QUrl url(checkUrl);

    if (m_suspendTime) {
        if (std::time(nullptr) - m_suspendTime < 300) {
            return QStringList(QStringLiteral("DIRECT"));
        }
        m_suspendTime = 0;
    }

    // Never use a proxy for the script itself
    if (m_downloader && url.matches(m_downloader->scriptUrl(), QUrl::StripTrailingSlash)) {
        return QStringList(QStringLiteral("DIRECT"));
    }

    if (m_script) {
        return handleRequest(url);
    }

    if (m_downloader || startDownload()) {
        msg.setDelayedReply(true);
        m_requestQueue.append(QueuedRequest(msg, url, true));
        return QStringList();   // return value will be ignored
    }

    return QStringList(QStringLiteral("DIRECT"));
}

QString ProxyScout::proxyForUrl(const QString &checkUrl, const QDBusMessage &msg)
{
    QUrl url(checkUrl);

    if (m_suspendTime) {
        if (std::time(nullptr) - m_suspendTime < 300) {
            return QStringLiteral("DIRECT");
        }
        m_suspendTime = 0;
    }

    // Never use a proxy for the script itself
    if (m_downloader && url.matches(m_downloader->scriptUrl(), QUrl::StripTrailingSlash)) {
        return QStringLiteral("DIRECT");
    }

    if (m_script) {
        return handleRequest(url).constFirst();
    }

    if (m_downloader || startDownload()) {
        msg.setDelayedReply(true);
        m_requestQueue.append(QueuedRequest(msg, url));
        return QString();   // return value will be ignored
    }

    return QStringLiteral("DIRECT");
}

void ProxyScout::blackListProxy(const QString &proxy)
{
    m_blackList[ proxy ] = std::time(nullptr);
}

void ProxyScout::reset()
{
    delete m_script;
    m_script = nullptr;
    delete m_downloader;
    m_downloader = nullptr;
    delete m_watcher;
    m_watcher = nullptr;
    m_blackList.clear();
    m_suspendTime = 0;
    KProtocolManager::reparseConfiguration();
}

bool ProxyScout::startDownload()
{
    switch (KProtocolManager::proxyType()) {
    case KProtocolManager::WPADProxy:
        if (m_downloader && !qobject_cast<Discovery *>(m_downloader)) {
            delete m_downloader;
            m_downloader = nullptr;
        }
        if (!m_downloader) {
            m_downloader = new Discovery(this);
            connect(m_downloader, QOverload<bool>::of(&Downloader::result), this, &ProxyScout::downloadResult);
        }
        break;
    case KProtocolManager::PACProxy: {
        if (m_downloader && !qobject_cast<Downloader *>(m_downloader)) {
            delete m_downloader;
            m_downloader = nullptr;
        }
        if (!m_downloader) {
            m_downloader = new Downloader(this);
            connect(m_downloader, QOverload<bool>::of(&Downloader::result), this, &ProxyScout::downloadResult);
        }

        const QUrl url(KProtocolManager::proxyConfigScript());
        if (url.isLocalFile()) {
            if (!m_watcher) {
                m_watcher = new QFileSystemWatcher(this);
                connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &ProxyScout::proxyScriptFileChanged);
            }
            proxyScriptFileChanged(url.path());
        } else {
            delete m_watcher;
            m_watcher = nullptr;
            m_downloader->download(url);
        }
        break;
    }
    default:
        return false;
    }

    return true;
}

void ProxyScout::disconnectNetwork(const QNetworkConfiguration &config)
{
    // NOTE: We only care of Defined state because we only want
    //to redo WPAD when a network interface is brought out of
    //hibernation or restarted for whatever reason.
    if (config.state() == QNetworkConfiguration::Defined) {
        reset();
    }
}

void ProxyScout::downloadResult(bool success)
{
    if (success) {
        try {
            if (!m_script) {
                m_script = new Script(m_downloader->script());
            }
        } catch (const Script::Error &e) {
            qWarning() << "Error:" << e.message();
#ifdef HAVE_KF5NOTIFICATIONS
            KNotification *notify = new KNotification(QStringLiteral("script-error"));
            notify->setText(i18n("The proxy configuration script is invalid:\n%1", e.message()));
            notify->setComponentName(m_componentName);
            notify->sendEvent();
#endif
            success = false;
        }
    } else {
#ifdef HAVE_KF5NOTIFICATIONS
        KNotification *notify = new KNotification(QStringLiteral("download-error"));
        notify->setText(m_downloader->error());
        notify->setComponentName(m_componentName);
        notify->sendEvent();
#endif
    }

    if (success) {
        for (const QueuedRequest &request : qAsConst(m_requestQueue)) {
            if (request.sendAll) {
                const QVariant result(handleRequest(request.url));
                QDBusConnection::sessionBus().send(request.transaction.createReply(result));
            } else {
                const QVariant result(handleRequest(request.url).constFirst());
                QDBusConnection::sessionBus().send(request.transaction.createReply(result));
            }
        }
    } else {
        for (const QueuedRequest &request : qAsConst(m_requestQueue)) {
            QDBusConnection::sessionBus().send(request.transaction.createReply(QLatin1String("DIRECT")));
        }
    }

    m_requestQueue.clear();

    // Suppress further attempts for 5 minutes
    if (!success) {
        m_suspendTime = std::time(nullptr);
    }
}

void ProxyScout::proxyScriptFileChanged(const QString &path)
{
    // Should never get called if we do not have a watcher...
    Q_ASSERT(m_watcher);

    // Remove the current file being watched...
    if (!m_watcher->files().isEmpty()) {
        m_watcher->removePaths(m_watcher->files());
    }

    // NOTE: QFileSystemWatcher only adds a path if it either exists or
    // is not already being monitored.
    m_watcher->addPath(path);

    // Reload...
    m_downloader->download(QUrl::fromLocalFile(path));
}

QStringList ProxyScout::handleRequest(const QUrl &url)
{
    try {
        QStringList proxyList;
        const QString result = m_script->evaluate(url).trimmed();
        const QStringList proxies = result.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        const int size = proxies.count();

        for (int i = 0; i < size; ++i) {
            QString mode, address;
            const QString proxy = proxies.at(i).trimmed();
            const int index = proxy.indexOf(QLatin1Char(' '));
            if (index == -1) { // Only "DIRECT" should match this!
                mode = proxy;
                address = proxy;
            } else {
                mode = proxy.left(index);
                address = proxy.mid(index + 1).trimmed();
            }

            const ProxyType type = proxyTypeFor(mode);
            if (type == Unknown) {
                continue;
            }

            if (type == Proxy || type == Socks) {
                const int index = address.indexOf(QLatin1Char(':'));
                if (index == -1 || !KProtocolInfo::isKnownProtocol(address.left(index))) {
                    const QString protocol((type == Proxy ? QStringLiteral("http://") : QStringLiteral("socks://")));
                    const QUrl url(protocol + address);
                    if (url.isValid()) {
                        address = url.toString();
                    } else {
                        continue;
                    }
                }
            }

            if (type == Direct || !m_blackList.contains(address)) {
                proxyList << address;
            } else if (std::time(nullptr) - m_blackList[address] > 1800) { // 30 minutes
                // black listing expired
                m_blackList.remove(address);
                proxyList << address;
            }
        }

        if (!proxyList.isEmpty()) {
            // qDebug() << proxyList;
            return proxyList;
        }
        // FIXME: blacklist
    } catch (const Script::Error &e) {
        qCritical() << e.message();
#ifdef HAVE_KF5NOTIFICATIONS
        KNotification *n = new KNotification(QStringLiteral("evaluation-error"));
        n->setText(i18n("The proxy configuration script returned an error:\n%1", e.message()));
        n->setComponentName(m_componentName);
        n->sendEvent();
#endif
    }

    return QStringList(QStringLiteral("DIRECT"));
}
}

#include "proxyscout.moc"
