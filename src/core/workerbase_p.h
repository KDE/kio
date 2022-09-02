/*
    SPDX-License-Identifier: LGPL-2.0-or-later
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2019-2022 Harald Sitter <sitter@kde.org>
*/

#ifndef WORKERBASE_P_H
#define WORKERBASE_P_H

#include "workerbase.h"

#include <commands_p.h>
#include <slavebase.h>

namespace KIO
{

// Bridges new worker API to legacy slave API. Overrides all SlaveBase virtual functions and redirects them at the
// fronting WorkerBase implementation. The WorkerBase implementation then returns Result objects which we translate
// back to the appropriate signal calls (error, finish, opened, etc.).
// When starting the dispatchLoop it actually runs inside the SlaveBase, so the SlaveBase is in the driver seat
// until KF6 when we can fully remove the SlaveBase in favor of the WorkerBase (means moving the dispatch and
// dispatchLoop functions into the WorkerBase and handling the signaling in the dispatch function rather than
// this intermediate Bridge object).
class WorkerSlaveBaseBridge : public SlaveBase
{
    void finalize(const WorkerResult &result)
    {
        if (!result.success()) {
            error(result.error(), result.errorString());
            return;
        }
        finished();
    }

    void maybeError(const WorkerResult &result)
    {
        if (!result.success()) {
            error(result.error(), result.errorString());
        }
    }

public:
    using SlaveBase::SlaveBase;

    void setHost(const QString &host, quint16 port, const QString &user, const QString &pass) final
    {
        base->setHost(host, port, user, pass);
    }

    void openConnection() final
    {
        const WorkerResult result = base->openConnection();
        if (!result.success()) {
            error(result.error(), result.errorString());
            return;
        }
        connected();
    }

    void closeConnection() final
    {
        base->closeConnection(); // not allowed to error but also not finishing
    }

    void get(const QUrl &url) final
    {
        finalize(base->get(url));
    }

    void open(const QUrl &url, QIODevice::OpenMode mode) final
    {
        const WorkerResult result = base->open(url, mode);
        if (!result.success()) {
            error(result.error(), result.errorString());
            return;
        }
        opened();
    }

    void read(KIO::filesize_t size) final
    {
        maybeError(base->read(size));
    }

    void write(const QByteArray &data) final
    {
        maybeError(base->write(data));
    }

    void seek(KIO::filesize_t offset) final
    {
        maybeError(base->seek(offset));
    }

    void close() final
    {
        finalize(base->close());
    }

    void put(const QUrl &url, int permissions, JobFlags flags) final
    {
        finalize(base->put(url, permissions, flags));
    }

    void stat(const QUrl &url) final
    {
        finalize(base->stat(url));
    }

    void mimetype(const QUrl &url) final
    {
        finalize(base->mimetype(url));
    }

    void listDir(const QUrl &url) final
    {
        finalize(base->listDir(url));
    }

    void mkdir(const QUrl &url, int permissions) final
    {
        finalize(base->mkdir(url, permissions));
    }

    void rename(const QUrl &src, const QUrl &dest, JobFlags flags) final
    {
        finalize(base->rename(src, dest, flags));
    }

    void symlink(const QString &target, const QUrl &dest, JobFlags flags) final
    {
        finalize(base->symlink(target, dest, flags));
    }

    void chmod(const QUrl &url, int permissions) final
    {
        finalize(base->chmod(url, permissions));
    }

    void chown(const QUrl &url, const QString &owner, const QString &group) final
    {
        finalize(base->chown(url, owner, group));
    }

    void setModificationTime(const QUrl &url, const QDateTime &mtime) final
    {
        finalize(base->setModificationTime(url, mtime));
    }

    void copy(const QUrl &src, const QUrl &dest, int permissions, JobFlags flags) final
    {
        finalize(base->copy(src, dest, permissions, flags));
    }

    void del(const QUrl &url, bool isfile) final
    {
        finalize(base->del(url, isfile));
    }

    void special(const QByteArray &data) final
    {
        finalize(base->special(data));
    }

    void multiGet(const QByteArray &data) final
    {
        finalize(base->multiGet(data));
    }

    void slave_status() final
    {
        base->worker_status(); // this only requests an update and isn't able to error or finish whatsoever
    }

    void reparseConfiguration() final
    {
        base->reparseConfiguration();
        SlaveBase::reparseConfiguration();
    }

    void setIncomingMetaData(const KIO::MetaData &metaData)
    {
        mIncomingMetaData = metaData;
    }

    WorkerBase *base = nullptr;

protected:
    void virtual_hook(int id, void *data) override
    {
        switch (id) {
        case SlaveBase::AppConnectionMade:
            base->appConnectionMade();
            return;
        case SlaveBase::GetFileSystemFreeSpace:
            finalize(base->fileSystemFreeSpace(*static_cast<QUrl *>(data)));
            return;
        case SlaveBase::Truncate:
            maybeError(base->truncate(*static_cast<KIO::filesize_t *>(data)));
            return;
        }

        maybeError(WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), id)));
    }
};

class WorkerBasePrivate
{
public:
    WorkerBasePrivate(const QByteArray &protocol, const QByteArray &poolSocket, const QByteArray &appSocket, WorkerBase *base)
        : bridge(protocol, poolSocket, appSocket)
    {
        bridge.base = base;
    }

    WorkerSlaveBaseBridge bridge;

    inline QString protocolName() const
    {
        return bridge.protocolName();
    }
};

} // namespace KIO

#endif
