// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2001 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KIO_WORKER_CONFIG_H
#define KIO_WORKER_CONFIG_H

#include "metadata.h"
#include <QObject>

#include <memory>

namespace KIO
{
class WorkerConfigPrivate;
/**
 * This class manages the configuration for KIO workers based on protocol
 * and host. The Scheduler makes use of this class to configure the worker
 * whenever it has to connect to a new host.
 *
 * You only need to use this class if you want to override specific
 * configuration items of an KIO worker when the worker is used by
 * your application.
 *
 * Normally KIO workers are being configured by "kio_<protocol>rc"
 * configuration files. Groups defined in such files are treated as host
 * or domain specification. Configuration items defined in a group are
 * only applied when the worker is connecting with a host that matches with
 * the host and/or domain specified by the group.
 */
class WorkerConfig : public QObject
{
    Q_OBJECT

public:
    static WorkerConfig *self();
    ~WorkerConfig() override;
    /**
     * Configure workers of type @p protocol by setting @p key to @p value.
     * If @p host is specified the configuration only applies when dealing
     * with @p host.
     *
     * Changes made to the worker configuration only apply to workers
     * used by the current process.
     */
    void setConfigData(const QString &protocol, const QString &host, const QString &key, const QString &value);

    /**
     * Configure workers of type @p protocol with @p config.
     * If @p host is specified the configuration only applies when dealing
     * with @p host.
     *
     * Changes made to the worker configuration only apply to workers
     * used by the current process.
     */
    void setConfigData(const QString &protocol, const QString &host, const MetaData &config);

    /**
     * Query worker configuration for workers of type @p protocol when
     * dealing with @p host.
     */
    MetaData configData(const QString &protocol, const QString &host);

    /**
     * Query a specific configuration key for workers of type @p protocol when
     * dealing with @p host.
     */
    QString configData(const QString &protocol, const QString &host, const QString &key);

    /**
     * Undo any changes made by calls to setConfigData.
     */
    void reset();

Q_SIGNALS:
    /**
     * This signal is raised when a worker of type @p protocol deals
     * with @p host for the first time.
     *
     * Your application can use this signal to make some last minute
     * configuration changes with setConfigData based on the
     * host.
     */
    void configNeeded(const QString &protocol, const QString &host);

protected:
    WorkerConfig();

private:
    std::unique_ptr<WorkerConfigPrivate> const d;
    friend class WorkerConfigSingleton;
};
}

#endif
