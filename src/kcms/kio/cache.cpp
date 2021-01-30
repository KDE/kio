/*
    cache.cpp - Proxy configuration dialog
    SPDX-FileCopyrightText: 2001, 2002, 2003 Dawit Alemayehu <adawit@kde.org>
    SPDX-License-Identifier: LGPL-2.0-only
*/

// Own
#include "cache.h"

// Qt
#include <QCheckBox>
#include <QPushButton>
#include <QStandardPaths>

// KDE
#include <KProcess>
#include <KPluginFactory>
#include <http_slave_defaults.h>
#include <config-kiocore.h>
#include <KLocalizedString>

// Local
#include "ksaveioconfig.h"

K_PLUGIN_FACTORY_DECLARATION(KioConfigFactory)

CacheConfigModule::CacheConfigModule(QWidget *parent, const QVariantList &)
                  :KCModule(parent)
{
  ui.setupUi(this);

  connect(ui.clearCacheButton, &QAbstractButton::clicked, this, &CacheConfigModule::clearCache);
}

CacheConfigModule::~CacheConfigModule()
{
}

void CacheConfigModule::load()
{
  ui.cbUseCache->setChecked(KProtocolManager::useCache());
  ui.sbMaxCacheSize->setValue( KProtocolManager::maxCacheSize() );

  KIO::CacheControl cc = KProtocolManager::cacheControl();

  if (cc==KIO::CC_Verify)
      ui.rbVerifyCache->setChecked( true );
  else if (cc==KIO::CC_Refresh)
      ui.rbVerifyCache->setChecked( true );
  else if (cc==KIO::CC_CacheOnly)
      ui.rbOfflineMode->setChecked( true );
  else if (cc==KIO::CC_Cache)
      ui.rbCacheIfPossible->setChecked( true );

  // Config changed notifications...
  connect(ui.cbUseCache, &QAbstractButton::toggled,
          this, &CacheConfigModule::configChanged);
  connect(ui.rbVerifyCache, &QAbstractButton::toggled,
          this, &CacheConfigModule::configChanged);
  connect(ui.rbOfflineMode, &QAbstractButton::toggled,
          this, &CacheConfigModule::configChanged);
  connect(ui.rbCacheIfPossible, &QAbstractButton::toggled,
          this, &CacheConfigModule::configChanged);
  connect(ui.sbMaxCacheSize, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &CacheConfigModule::configChanged);
  Q_EMIT changed( false );
}

void CacheConfigModule::save()
{
  KSaveIOConfig::setUseCache( ui.cbUseCache->isChecked() );
  KSaveIOConfig::setMaxCacheSize( ui.sbMaxCacheSize->value() );

  if ( !ui.cbUseCache->isChecked() )
      KSaveIOConfig::setCacheControl(KIO::CC_Refresh);
  else if ( ui.rbVerifyCache->isChecked() )
      KSaveIOConfig::setCacheControl(KIO::CC_Refresh);
  else if ( ui.rbOfflineMode->isChecked() )
      KSaveIOConfig::setCacheControl(KIO::CC_CacheOnly);
  else if ( ui.rbCacheIfPossible->isChecked() )
      KSaveIOConfig::setCacheControl(KIO::CC_Cache);

  KProtocolManager::reparseConfiguration();

  // Update running io-slaves...
  KSaveIOConfig::updateRunningIOSlaves (this);

  Q_EMIT changed( false );
}

void CacheConfigModule::defaults()
{
  ui.cbUseCache->setChecked( true );
  ui.rbVerifyCache->setChecked( true );
  ui.sbMaxCacheSize->setValue( DEFAULT_MAX_CACHE_SIZE );
}

QString CacheConfigModule::quickHelp() const
{
  return i18n( "<h1>Cache</h1><p>This module lets you configure your cache settings.</p>"
                "<p>This specific cache is an area on the disk where recently "
                "read web pages are stored. If you want to retrieve a web "
                "page again that you have recently read, it will not be "
                "downloaded from the Internet, but rather retrieved from the "
                "cache, which is a lot faster.</p>" );
}

void CacheConfigModule::configChanged()
{
  Q_EMIT changed( true );
}

void CacheConfigModule::clearCache()
{
    const QString exe = QFile::decodeName(KDE_INSTALL_FULL_LIBEXECDIR_KF5 "/kio_http_cache_cleaner");

    if (QFile::exists(exe)) {
        QProcess::startDetached(exe, QStringList(QStringLiteral("--clear-all")));
    }
}


