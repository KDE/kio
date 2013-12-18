// -*- c++ -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (c) 2001 Waldo Bastian <bastian@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#include "slaveconfig.h"

#include <QtCore/QHash>

#include <kconfig.h>
#include <ksharedconfig.h>
#include <kprotocolinfo.h>
#include <kprotocolmanager.h>

using namespace KIO;

namespace KIO {

class SlaveConfigProtocol
{
public:
  SlaveConfigProtocol() {}
  ~SlaveConfigProtocol()
  {
     delete configFile;
  }

public:
  MetaData global;
  QHash<QString, MetaData> host;
  KConfig *configFile;
};

static void readConfig(KConfig *config, const QString & group, MetaData *metaData)
{
   *metaData += config->entryMap(group);
}

class SlaveConfigPrivate
{
  public:
     void readGlobalConfig();
     SlaveConfigProtocol *readProtocolConfig(const QString &_protocol);
     SlaveConfigProtocol *findProtocolConfig(const QString &_protocol);
     void readConfigProtocolHost(const QString &_protocol, SlaveConfigProtocol *scp, const QString &host);
  public:
     MetaData global;
     QHash<QString, SlaveConfigProtocol*> protocol;
};

void SlaveConfigPrivate::readGlobalConfig()
{
   global.clear();
   // Read stuff...
   KSharedConfig::Ptr config = KProtocolManager::config();
   readConfig(KSharedConfig::openConfig().data(), "Socks", &global); // Socks settings.
   if ( config )
       readConfig(config.data(), "<default>", &global);
}

SlaveConfigProtocol* SlaveConfigPrivate::readProtocolConfig(const QString &_protocol)
{
   SlaveConfigProtocol *scp = protocol.value(_protocol,0);
   if (!scp)
   {
      QString filename = KProtocolInfo::config(_protocol);
      scp = new SlaveConfigProtocol;
      scp->configFile = new KConfig(filename, KConfig::NoGlobals);
      protocol.insert(_protocol, scp);
   }
   // Read global stuff...
   readConfig(scp->configFile, "<default>", &(scp->global));
   return scp;
}

SlaveConfigProtocol* SlaveConfigPrivate::findProtocolConfig(const QString &_protocol)
{
   SlaveConfigProtocol *scp = protocol.value(_protocol,0);
   if (!scp)
      scp = readProtocolConfig(_protocol);
   return scp;
}

void SlaveConfigPrivate::readConfigProtocolHost(const QString &, SlaveConfigProtocol *scp, const QString &host)
{
   MetaData metaData;
   scp->host.insert(host, metaData);

   // Read stuff
   // Break host into domains
   QString domain = host;

   if (!domain.contains('.'))
   {
      // Host without domain.
      if (scp->configFile->hasGroup("<local>")) {
         readConfig(scp->configFile, "<local>", &metaData);
         scp->host.insert(host, metaData);
      }
   }

   int pos = 0;
   do
   {
      pos = host.lastIndexOf('.', pos-1);

      if (pos < 0)
        domain = host;
      else
        domain = host.mid(pos+1);

      if (scp->configFile->hasGroup(domain)) {
         readConfig(scp->configFile, domain.toLower(), &metaData);
         scp->host.insert(host, metaData);
      }
   }
   while (pos > 0);
}

class SlaveConfigSingleton
{
public:
  SlaveConfig instance;
};

Q_GLOBAL_STATIC(SlaveConfigSingleton, _self)

SlaveConfig *SlaveConfig::self()
{
   return &_self()->instance;
}

SlaveConfig::SlaveConfig()
	:d(new SlaveConfigPrivate)
{
  d->readGlobalConfig();
}

SlaveConfig::~SlaveConfig()
{
   qDeleteAll(d->protocol);
   delete d;
}

void SlaveConfig::setConfigData(const QString &protocol,
                                const QString &host,
                                const QString &key,
                                const QString &value )
{
   MetaData config;
   config.insert(key, value);
   setConfigData(protocol, host, config);
}

void SlaveConfig::setConfigData(const QString &protocol, const QString &host, const MetaData &config )
{
   if (protocol.isEmpty())
      d->global += config;
   else {
      SlaveConfigProtocol *scp = d->findProtocolConfig(protocol);
      if (host.isEmpty())
      {
         scp->global += config;
      }
      else
      {
         if (!scp->host.contains(host))
            d->readConfigProtocolHost(protocol, scp, host);

         MetaData hostConfig = scp->host.value(host);
         hostConfig += config;
         scp->host.insert(host, hostConfig);
      }
   }
}

MetaData SlaveConfig::configData(const QString &protocol, const QString &host)
{
   MetaData config = d->global;
   SlaveConfigProtocol *scp = d->findProtocolConfig(protocol);
   config += scp->global;
   if (host.isEmpty())
      return config;

   if (!scp->host.contains(host))
   {
      d->readConfigProtocolHost(protocol, scp, host);
      emit configNeeded(protocol, host);
   }
   MetaData hostConfig = scp->host.value(host);
   config += hostConfig;

   return config;
}

QString SlaveConfig::configData(const QString &protocol, const QString &host, const QString &key)
{
   return configData(protocol, host)[key];
}

void SlaveConfig::reset()
{
   qDeleteAll(d->protocol);
   d->protocol.clear();
   d->readGlobalConfig();
}

}

