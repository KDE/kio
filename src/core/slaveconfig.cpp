// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2001 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "slaveconfig.h"

#include <QHash>
#include <QThreadStorage>

#include <KConfig>
#include <KSharedConfig>
#include <kprotocolinfo.h>
#include <kprotocolmanager.h>

using namespace KIO;

namespace KIO
{

class SlaveConfigProtocol
{
public:
    SlaveConfigProtocol() {}
    ~SlaveConfigProtocol()
    {
        delete configFile;
    }

    SlaveConfigProtocol(const SlaveConfigProtocol &) = delete;
    SlaveConfigProtocol &operator=(const SlaveConfigProtocol &) = delete;

public:
    MetaData global;
    QHash<QString, MetaData> host;
    KConfig *configFile;
};

static void readConfig(KConfig *config, const QString &group, MetaData *metaData)
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
    QHash<QString, SlaveConfigProtocol *> protocol;
};

void SlaveConfigPrivate::readGlobalConfig()
{
    global.clear();
    // Read stuff...
    readConfig(KSharedConfig::openConfig().data(), QStringLiteral("Socks"), &global); // Socks settings.
    global += KProtocolManager::entryMap(QStringLiteral("<default>"));
}

SlaveConfigProtocol *SlaveConfigPrivate::readProtocolConfig(const QString &_protocol)
{
    SlaveConfigProtocol *scp = protocol.value(_protocol, nullptr);
    if (!scp) {
        QString filename = KProtocolInfo::config(_protocol);
        scp = new SlaveConfigProtocol;
        scp->configFile = new KConfig(filename, KConfig::NoGlobals);
        protocol.insert(_protocol, scp);
    }
    // Read global stuff...
    readConfig(scp->configFile, QStringLiteral("<default>"), &(scp->global));
    return scp;
}

SlaveConfigProtocol *SlaveConfigPrivate::findProtocolConfig(const QString &_protocol)
{
    SlaveConfigProtocol *scp = protocol.value(_protocol, nullptr);
    if (!scp) {
        scp = readProtocolConfig(_protocol);
    }
    return scp;
}

void SlaveConfigPrivate::readConfigProtocolHost(const QString &, SlaveConfigProtocol *scp, const QString &host)
{
    MetaData metaData;
    scp->host.insert(host, metaData);

    // Read stuff
    // Break host into domains
    QString domain = host;

    if (!domain.contains(QLatin1Char('.'))) {
        // Host without domain.
        if (scp->configFile->hasGroup("<local>")) {
            readConfig(scp->configFile, QStringLiteral("<local>"), &metaData);
            scp->host.insert(host, metaData);
        }
    }

    int pos = 0;
    do {
        pos = host.lastIndexOf(QLatin1Char('.'), pos - 1);

        if (pos < 0) {
            domain = host;
        } else {
            domain = host.mid(pos + 1);
        }

        if (scp->configFile->hasGroup(domain)) {
            readConfig(scp->configFile, domain.toLower(), &metaData);
            scp->host.insert(host, metaData);
        }
    } while (pos > 0);
}

class SlaveConfigSingleton
{
public:
    SlaveConfig instance;
};

template <typename T>
T * perThreadGlobalStatic()
{
    static QThreadStorage<T *> s_storage;
    if (!s_storage.hasLocalData()) {
        s_storage.setLocalData(new T);
    }
    return s_storage.localData();
}
//Q_GLOBAL_STATIC(SlaveConfigSingleton, _self)
SlaveConfigSingleton *_self() { return perThreadGlobalStatic<SlaveConfigSingleton>(); }


SlaveConfig *SlaveConfig::self()
{
    return &_self()->instance;
}

SlaveConfig::SlaveConfig()
    : d(new SlaveConfigPrivate)
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
                                const QString &value)
{
    MetaData config;
    config.insert(key, value);
    setConfigData(protocol, host, config);
}

void SlaveConfig::setConfigData(const QString &protocol, const QString &host, const MetaData &config)
{
    if (protocol.isEmpty()) {
        d->global += config;
    } else {
        SlaveConfigProtocol *scp = d->findProtocolConfig(protocol);
        if (host.isEmpty()) {
            scp->global += config;
        } else {
            if (!scp->host.contains(host)) {
                d->readConfigProtocolHost(protocol, scp, host);
            }

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
    if (host.isEmpty()) {
        return config;
    }

    if (!scp->host.contains(host)) {
        d->readConfigProtocolHost(protocol, scp, host);
        Q_EMIT configNeeded(protocol, host);
    }
    MetaData hostConfig = scp->host.value(host);
    config += hostConfig;

    return config;
}

QString SlaveConfig::configData(const QString &protocol, const QString &host, const QString &key)
{
    return configData(protocol, host).value(key);
}

void SlaveConfig::reset()
{
    qDeleteAll(d->protocol);
    d->protocol.clear();
    d->readGlobalConfig();
}

}

