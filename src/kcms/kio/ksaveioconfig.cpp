/*
    SPDX-FileCopyrightText: 2001 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

// Own
#include "ksaveioconfig.h"

// Qt
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusInterface>

// KDE
#include <KConfig>
#include <KLocalizedString>
#include <KMessageBox>
#include <KConfigGroup>
#include <ioslave_defaults.h>
class KSaveIOConfigPrivate
{
public:
  KSaveIOConfigPrivate ();
  ~KSaveIOConfigPrivate ();

  KConfig* config = nullptr;
  KConfig* http_config = nullptr;
};

Q_GLOBAL_STATIC(KSaveIOConfigPrivate, d)

KSaveIOConfigPrivate::KSaveIOConfigPrivate ()
{
}

KSaveIOConfigPrivate::~KSaveIOConfigPrivate ()
{
  delete config;
  delete http_config;
}

static KConfig* config()
{
  if (!d->config)
     d->config = new KConfig(QStringLiteral("kioslaverc"), KConfig::NoGlobals);

  return d->config;
}

static KConfig* http_config()
{
  if (!d->http_config)
     d->http_config = new KConfig(QStringLiteral("kio_httprc"), KConfig::NoGlobals);

  return d->http_config;
}

int KSaveIOConfig::proxyDisplayUrlFlags()
{
    KConfigGroup cfg (config(), QString());
    return cfg.readEntry("ProxyUrlDisplayFlags", 0);
}

void KSaveIOConfig::setProxyDisplayUrlFlags (int flags)
{
    KConfigGroup cfg (config(), QString());
    cfg.writeEntry("ProxyUrlDisplayFlags", flags);
    cfg.sync();
}

void KSaveIOConfig::reparseConfiguration ()
{
  delete d->config;
  d->config = nullptr;
  delete d->http_config;
  d->http_config = nullptr;
}

void KSaveIOConfig::setReadTimeout( int _timeout )
{
  KConfigGroup cfg (config(), QString());
  cfg.writeEntry("ReadTimeout", qMax(MIN_TIMEOUT_VALUE,_timeout));
  cfg.sync();
}

void KSaveIOConfig::setConnectTimeout( int _timeout )
{
  KConfigGroup cfg (config(), QString());
  cfg.writeEntry("ConnectTimeout", qMax(MIN_TIMEOUT_VALUE,_timeout));
  cfg.sync();
}

void KSaveIOConfig::setProxyConnectTimeout( int _timeout )
{
  KConfigGroup cfg (config(), QString());
  cfg.writeEntry("ProxyConnectTimeout", qMax(MIN_TIMEOUT_VALUE,_timeout));
  cfg.sync();
}

void KSaveIOConfig::setResponseTimeout( int _timeout )
{
  KConfigGroup cfg (config(), QString());
  cfg.writeEntry("ResponseTimeout", qMax(MIN_TIMEOUT_VALUE,_timeout));
  cfg.sync();
}


void KSaveIOConfig::setMarkPartial( bool _mode )
{
  KConfigGroup cfg (config(), QString());
  cfg.writeEntry( "MarkPartial", _mode );
  cfg.sync();
}

void KSaveIOConfig::setMinimumKeepSize( int _size )
{
  KConfigGroup cfg (config(), QString());
  cfg.writeEntry( "MinimumKeepSize", _size );
  cfg.sync();
}

void KSaveIOConfig::setAutoResume( bool _mode )
{
  KConfigGroup cfg (config(), QString());
  cfg.writeEntry( "AutoResume", _mode );
  cfg.sync();
}

void KSaveIOConfig::setUseCache( bool _mode )
{
  KConfigGroup cfg (http_config(), QString());
  cfg.writeEntry( "UseCache", _mode );
  cfg.sync();
}

void KSaveIOConfig::setMaxCacheSize( int cache_size )
{
  KConfigGroup cfg (http_config(), QString());
  cfg.writeEntry( "MaxCacheSize", cache_size );
  cfg.sync();
}

void KSaveIOConfig::setCacheControl(KIO::CacheControl policy)
{
  KConfigGroup cfg (http_config(), QString());
  QString tmp = KIO::getCacheControlString(policy);
  cfg.writeEntry("cache", tmp);
  cfg.sync();
}

void KSaveIOConfig::setMaxCacheAge( int cache_age )
{
  KConfigGroup cfg (http_config(), QString());
  cfg.writeEntry( "MaxCacheAge", cache_age );
  cfg.sync();
}

void KSaveIOConfig::setUseReverseProxy( bool mode )
{
  KConfigGroup cfg (config(), "Proxy Settings");
  cfg.writeEntry("ReversedException", mode);
  cfg.sync();
}

void KSaveIOConfig::setProxyType(KProtocolManager::ProxyType type)
{
  KConfigGroup cfg (config(), "Proxy Settings");
  cfg.writeEntry("ProxyType", static_cast<int>(type));
  cfg.sync();
}

QString KSaveIOConfig::noProxyFor()
{
    KConfigGroup cfg(config(), "Proxy Settings");
    return cfg.readEntry("NoProxyFor");
}

void KSaveIOConfig::setNoProxyFor( const QString& _noproxy )
{
  KConfigGroup cfg (config(), "Proxy Settings");
  cfg.writeEntry("NoProxyFor", _noproxy);
  cfg.sync();
}

void KSaveIOConfig::setProxyFor( const QString& protocol,
                                 const QString& _proxy )
{
  KConfigGroup cfg (config(), "Proxy Settings");
  cfg.writeEntry(protocol.toLower() + QLatin1String("Proxy"), _proxy);
  cfg.sync();
}

void KSaveIOConfig::setProxyConfigScript( const QString& _url )
{
  KConfigGroup cfg (config(), "Proxy Settings");
  cfg.writeEntry("Proxy Config Script", _url);
  cfg.sync();
}

void KSaveIOConfig::updateRunningIOSlaves (QWidget *parent)
{
  // Inform all running io-slaves about the changes...
  // if we cannot update, ioslaves inform the end user...
  QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/KIO/Scheduler"), QStringLiteral("org.kde.KIO.Scheduler"), QStringLiteral("reparseSlaveConfiguration"));
  message << QString();
  if (!QDBusConnection::sessionBus().send(message))
  {
    KMessageBox::information (parent,
                              i18n("You have to restart the running applications "
                                   "for these changes to take effect."),
                              i18nc("@title:window", "Update Failed"));
  }
}

void KSaveIOConfig::updateProxyScout(QWidget * parent)
{
  // Inform the proxyscout kded module about changes if we cannot update,
  // ioslaves inform the end user...
  QDBusInterface kded(QStringLiteral("org.kde.kcookiejar5"), QStringLiteral("/modules/proxyscout"), QStringLiteral("org.kde.KPAC.ProxyScout"));
  QDBusReply<void> reply = kded.call(QStringLiteral("reset"));
  if (!reply.isValid())
  {
    KMessageBox::information (parent,
                              i18n("You have to restart KDE for these changes to take effect."),
                              i18nc("@title:window", "Update Failed"));
  }
}
