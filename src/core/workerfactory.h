/*
    SPDX-FileCopyrightText: 2022 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_WORKERFACTORY_H
#define KIO_WORKERFACTORY_H

#include "kiocore_export.h"
#include <QObject>
#include <memory>

namespace KIO
{

class SlaveBase;
class WorkerBase;

class KIOCORE_EXPORT WorkerFactory : public QObject
{
    Q_OBJECT
public:
    explicit WorkerFactory(QObject *parent = nullptr);

    virtual std::unique_ptr<SlaveBase> createWorker(const QByteArray &pool, const QByteArray &app) = 0;
};

class KIOCORE_EXPORT RealWorkerFactory : public WorkerFactory
{
    Q_OBJECT
public:
    using WorkerFactory::WorkerFactory;
    virtual std::unique_ptr<WorkerBase> createRealWorker(const QByteArray &pool, const QByteArray &app) = 0;
};

} // namespace KIO

#endif // KIO_WORKERFACTORY_H
