/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 Kevin Ottens <ervin ipsquad net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_REMOTE_H
#define KIO_REMOTE_H

#include "remoteimpl.h"
#include <KIO/WorkerBase>

class RemoteProtocol : public KIO::WorkerBase
{
public:
    RemoteProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app);
    ~RemoteProtocol() override;

    KIO::WorkerResult listDir(const QUrl &url) override;
    KIO::WorkerResult stat(const QUrl &url) override;
    KIO::WorkerResult del(const QUrl &url, bool isFile) override;
    KIO::WorkerResult get(const QUrl &url) override;
    KIO::WorkerResult rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags) override;
    KIO::WorkerResult symlink(const QString &target, const QUrl &dest, KIO::JobFlags flags) override;

private:
    KIO::WorkerResult listRoot();

    RemoteImpl m_impl;
};

#endif
