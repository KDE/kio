/*
   Copyright (c) 2003 Malte Starostik <malte@kde.org>
   Copyright (c) 2011 Dawit Alemayehu <adawit@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "proxyscout.h"

#include "config-kpac.h"

#include "discovery.h"
#include "script.h"

#include <QDebug>
#include <klocalizedstring.h>
#include <knotification.h>
#include <kprotocolmanager.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>

#if ! KPAC_NO_SOLID
#include <solid/networking.h>
#endif

#include <QtCore/QFileSystemWatcher>
#include <QDBusConnection>

#include <cstdlib>
#include <ctime>

K_PLUGIN_FACTORY(ProxyScoutFactory,
                 registerPlugin<KPAC::ProxyScout>();
    )

namespace KPAC
{
    enum ProxyType {
        Unknown = -1,
        Proxy,
        Socks,
        Direct
    };

    static ProxyType proxyTypeFor(const QString& mode)
    {
        if (mode.compare(QLatin1String("PROXY"), Qt::CaseInsensitive) == 0)
            return Proxy;

        if (mode.compare(QLatin1String("DIRECT"), Qt::CaseInsensitive) == 0)
            return Direct;

        if (mode.compare(QLatin1String("SOCKS"), Qt::CaseInsensitive) == 0 ||
            mode.compare(QLatin1String("SOCKS5"), Qt::CaseInsensitive) == 0)
            return Socks;

        return Unknown;
    }

    ProxyScout::QueuedRequest::QueuedRequest( const QDBusMessage &reply, const QUrl & u, bool sendall )
        : transaction( reply ), url( u ), sendAll(sendall)
    {
    }

    ProxyScout::ProxyScout(QObject* parent, const QList<QVariant>&)
        : KDEDModule(parent),
          m_componentName("proxyscout"),
          m_downloader( 0 ),
          m_script( 0 ),
          m_suspendTime( 0 ),
          m_watcher( 0 )
    {
#if ! KPAC_NO_SOLID
        connect (Solid::Networking::notifier(), SIGNAL(shouldDisconnect()), SLOT(disconnectNetwork()));
#endif
    }

    ProxyScout::~ProxyScout()
    {
        delete m_script;
    }

    QStringList ProxyScout::proxiesForUrl( const QString& checkUrl, const QDBusMessage &msg )
    {
        QUrl url(checkUrl);

        if (m_suspendTime) {
            if ( std::time( 0 ) - m_suspendTime < 300 ) {
                return QStringList (QLatin1String("DIRECT"));
            }
            m_suspendTime = 0;
        }

        // Never use a proxy for the script itself
        if (m_downloader && url.matches(m_downloader->scriptUrl(), QUrl::StripTrailingSlash)) {
            return QStringList (QLatin1String("DIRECT"));
        }

        if (m_script) {
            return handleRequest(url);
        }

        if (m_downloader || startDownload()) {
            msg.setDelayedReply(true);
            m_requestQueue.append( QueuedRequest( msg, url, true ) );
            return QStringList();   // return value will be ignored
        }

        return QStringList(QLatin1String("DIRECT"));
    }

    QString ProxyScout::proxyForUrl( const QString& checkUrl, const QDBusMessage &msg )
    {
        QUrl url(checkUrl);

        if (m_suspendTime) {
            if ( std::time( 0 ) - m_suspendTime < 300 ) {
                return QLatin1String("DIRECT");
            }
            m_suspendTime = 0;
        }

        // Never use a proxy for the script itself
        if (m_downloader && url.matches(m_downloader->scriptUrl(), QUrl::StripTrailingSlash)) {
            return QLatin1String("DIRECT");
        }

        if (m_script) {
            return handleRequest(url).first();
        }

        if (m_downloader || startDownload()) {
            msg.setDelayedReply(true);
            m_requestQueue.append( QueuedRequest( msg, url ) );
            return QString();   // return value will be ignored
        }

        return QLatin1String("DIRECT");
    }

    void ProxyScout::blackListProxy( const QString& proxy )
    {
        m_blackList[ proxy ] = std::time( 0 );
    }

    void ProxyScout::reset()
    {
        delete m_script;
        m_script = 0;
        delete m_downloader;
        m_downloader = 0;
        delete m_watcher;
        m_watcher = 0;
        m_blackList.clear();
        m_suspendTime = 0;
        KProtocolManager::reparseConfiguration();
    }

    bool ProxyScout::startDownload()
    {
        switch ( KProtocolManager::proxyType() )
        {
            case KProtocolManager::WPADProxy:
                if (m_downloader && !qobject_cast<Discovery*>(m_downloader)) {
                    delete m_downloader;
                    m_downloader = 0;
                }
                if (!m_downloader) {
                    m_downloader = new Discovery(this);
                    connect(m_downloader, SIGNAL(result(bool)), this, SLOT(downloadResult(bool)));
                }
                break;
            case KProtocolManager::PACProxy: {
                if (m_downloader && !qobject_cast<Downloader*>(m_downloader)) {
                    delete m_downloader;
                    m_downloader = 0;
                }
                if (!m_downloader) {
                    m_downloader = new Downloader(this);
                    connect(m_downloader, SIGNAL(result(bool)), this, SLOT(downloadResult(bool)));
                }

                const QUrl url (KProtocolManager::proxyConfigScript());
                if (url.isLocalFile()) {
                    if (!m_watcher) {
                        m_watcher = new QFileSystemWatcher(this);
                        connect (m_watcher, SIGNAL(fileChanged(QString)), SLOT(proxyScriptFileChanged(QString)));
                    }
                    proxyScriptFileChanged(url.path());
                } else {
                    delete m_watcher;
                    m_watcher = 0;
                    m_downloader->download( url );
                }
                break;
            }
            default:
                return false;
        }

        return true;
    }

    void ProxyScout::disconnectNetwork()
    {
        // NOTE: We only connect to Solid's network notifier's shouldDisconnect
        // signal because we only want to redo WPAD when a network interface is 
        // brought out of hibernation or restarted for whatever reason.
        reset();
    }

    void ProxyScout::downloadResult( bool success )
    {
        if ( success ) {
            try
            {
                if (!m_script) {
                    m_script = new Script(m_downloader->script());
                }
            }
            catch ( const Script::Error& e )
            {
                qWarning() << "Error:" << e.message();
                KNotification *notify= new KNotification ( "script-error" );
                notify->setText( i18n("The proxy configuration script is invalid:\n%1" , e.message() ) );
                notify->setComponentName(m_componentName);
                notify->sendEvent();
                success = false;
            }
        } else {
            KNotification *notify = new KNotification ("download-error");
            notify->setText( m_downloader->error() );
            notify->setComponentName(m_componentName);
            notify->sendEvent();
        }

        if (success) {
            for (RequestQueue::Iterator it = m_requestQueue.begin(), itEnd = m_requestQueue.end(); it != itEnd; ++it) {
                if ((*it).sendAll) {
                    const QVariant result (handleRequest((*it).url));
                    QDBusConnection::sessionBus().send((*it).transaction.createReply(result));
                } else {
                    const QVariant result (handleRequest((*it).url).first());
                    QDBusConnection::sessionBus().send((*it).transaction.createReply(result));
                }
            }
        } else {
            for (RequestQueue::Iterator it = m_requestQueue.begin(), itEnd = m_requestQueue.end(); it != itEnd; ++it) {
                QDBusConnection::sessionBus().send((*it).transaction.createReply(QString::fromLatin1("DIRECT")));
            }
        }

        m_requestQueue.clear();

        // Suppress further attempts for 5 minutes
        if ( !success ) {
            m_suspendTime = std::time( 0 );
        }
    }

    void ProxyScout::proxyScriptFileChanged(const QString& path)
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

    QStringList ProxyScout::handleRequest( const QUrl & url )
    {
        try
        {
            QStringList proxyList;
            const QString result = m_script->evaluate(url).trimmed();
            const QStringList proxies = result.split(QLatin1Char(';'), QString::SkipEmptyParts);
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
                        const QString protocol ((type == Proxy ? QLatin1String("http://") : QLatin1String("socks://")));
                        const QUrl url (protocol + address);
                        if (url.isValid()) {
                            address = url.toString();
                        } else {
                            continue;
                        }
                    }
                }

                if (type == Direct || !m_blackList.contains(address)) {
                    proxyList << address;
                } else if (std::time(0) - m_blackList[address] > 1800) { // 30 minutes
                    // black listing expired
                    m_blackList.remove( address );
                    proxyList << address;
                }
            }

            if (!proxyList.isEmpty()) {
                // qDebug() << proxyList;
                return proxyList;
            }
            // FIXME: blacklist
        }
        catch ( const Script::Error& e )
        {
            qCritical() << e.message();
            KNotification *n=new KNotification( "evaluation-error" );
            n->setText( i18n( "The proxy configuration script returned an error:\n%1" , e.message() ) );
            n->setComponentName(m_componentName);
            n->sendEvent();
        }

        return QStringList (QLatin1String("DIRECT"));
    }
}

#include "proxyscout.moc"
// vim: ts=4 sw=4 et
