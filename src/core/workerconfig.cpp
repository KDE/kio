// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2001 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "workerconfig.h"

#include <QHash>
#include <QThreadStorage>

#include <KConfig>
#include <KSharedConfig>
#include <kprotocolinfo.h>
#include <kprotocolmanager.h>

using namespace KIO;

namespace KIO
{
class WorkerConfigProtocol
{
public:
    WorkerConfigProtocol() = default;
    ~WorkerConfigProtocol()
    {
        delete configFile;
    }

    WorkerConfigProtocol(const WorkerConfigProtocol &) = delete;
    WorkerConfigProtocol &operator=(const WorkerConfigProtocol &) = delete;

public:
    MetaData global;
    QHash<QString, MetaData> host;
    KConfig *configFile;
};

static void readConfig(KConfig *config, const QString &group, MetaData *metaData)
{
    *metaData += config->entryMap(group);
}

class WorkerConfigPrivate
{
public:
    void readGlobalConfig();
    WorkerConfigProtocol *readProtocolConfig(const QString &_protocol);
    WorkerConfigProtocol *findProtocolConfig(const QString &_protocol);
    void readConfigProtocolHost(const QString &_protocol, WorkerConfigProtocol *scp, const QString &host);

public:
    MetaData global;
    QHash<QString, WorkerConfigProtocol *> protocol;
};

void WorkerConfigPrivate::readGlobalConfig()
{
    global.clear();
    // Read stuff...
    readConfig(KSharedConfig::openConfig().data(), QStringLiteral("Socks"), &global); // Socks settings.
    global += KProtocolManager::entryMap(QStringLiteral("<default>"));
}

WorkerConfigProtocol *WorkerConfigPrivate::readProtocolConfig(const QString &_protocol)
{
    WorkerConfigProtocol *scp = protocol.value(_protocol, nullptr);
    if (!scp) {
        QString filename = KProtocolInfo::config(_protocol);
        scp = new WorkerConfigProtocol;
        scp->configFile = new KConfig(filename, KConfig::NoGlobals);
        protocol.insert(_protocol, scp);
    }
    // Read global stuff...
    readConfig(scp->configFile, QStringLiteral("<default>"), &(scp->global));
    return scp;
}

WorkerConfigProtocol *WorkerConfigPrivate::findProtocolConfig(const QString &_protocol)
{
    WorkerConfigProtocol *scp = protocol.value(_protocol, nullptr);
    if (!scp) {
        scp = readProtocolConfig(_protocol);
    }
    return scp;
}

void WorkerConfigPrivate::readConfigProtocolHost(const QString &, WorkerConfigProtocol *scp, const QString &host)
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

class WorkerConfigSingleton
{
public:
    WorkerConfig instance;
};

template<typename T>
T *perThreadGlobalStatic()
{
    static QThreadStorage<T *> s_storage;
    if (!s_storage.hasLocalData()) {
        s_storage.setLocalData(new T);
    }
    return s_storage.localData();
}
// Q_GLOBAL_STATIC(WorkerConfigSingleton, _self)
// TODO: export symbol here, or make compile unit local by "static"?
WorkerConfigSingleton *_workerConfigSelf()
{
    return perThreadGlobalStatic<WorkerConfigSingleton>();
}

WorkerConfig *WorkerConfig::self()
{
    return &_workerConfigSelf()->instance;
}

WorkerConfig::WorkerConfig()
    : d(new WorkerConfigPrivate)
{
    d->readGlobalConfig();
}

WorkerConfig::~WorkerConfig()
{
    qDeleteAll(d->protocol);
}

void WorkerConfig::setConfigData(const QString &protocol, const QString &host, const QString &key, const QString &value)
{
    MetaData config;
    config.insert(key, value);
    setConfigData(protocol, host, config);
}

void WorkerConfig::setConfigData(const QString &protocol, const QString &host, const MetaData &config)
{
    if (protocol.isEmpty()) {
        d->global += config;
    } else {
        WorkerConfigProtocol *scp = d->findProtocolConfig(protocol);
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

MetaData WorkerConfig::configData(const QString &protocol, const QString &host)
{
    MetaData config = d->global;
    WorkerConfigProtocol *scp = d->findProtocolConfig(protocol);
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

QString WorkerConfig::configData(const QString &protocol, const QString &host, const QString &key)
{
    return configData(protocol, host).value(key);
}

void WorkerConfig::reset()
{
    qDeleteAll(d->protocol);
    d->protocol.clear();
    d->readGlobalConfig();
}

}
