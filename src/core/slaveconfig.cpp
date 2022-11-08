// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2001 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "slaveconfig.h"

#include "workerconfig.h"
// Qt
#include <QGlobalStatic>

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 102)

namespace KIO
{

class SlaveConfigPrivate
{
};

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

SlaveConfig::SlaveConfig() = default;

SlaveConfig::~SlaveConfig() = default;

void SlaveConfig::setConfigData(const QString &protocol, const QString &host, const QString &key, const QString &value)
{
    WorkerConfig::self()->setConfigData(protocol, host, key, value);
}

void SlaveConfig::setConfigData(const QString &protocol, const QString &host, const MetaData &config)
{
    WorkerConfig::self()->setConfigData(protocol, host, config);
}

MetaData SlaveConfig::configData(const QString &protocol, const QString &host)
{
    return WorkerConfig::self()->configData(protocol, host);
}

QString SlaveConfig::configData(const QString &protocol, const QString &host, const QString &key)
{
    return WorkerConfig::self()->configData(protocol, host, key);
}

void SlaveConfig::reset()
{
    WorkerConfig::self()->reset();
}

}

#endif
