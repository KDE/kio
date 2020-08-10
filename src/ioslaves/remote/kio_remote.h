/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 Kevin Ottens <ervin ipsquad net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_REMOTE_H
#define KIO_REMOTE_H

#include <KIO/SlaveBase>
#include "remoteimpl.h"

class RemoteProtocol : public KIO::SlaveBase
{
public:
    RemoteProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app);
    ~RemoteProtocol() override;
    void listDir(const QUrl &url) override;
    void stat(const QUrl &url) override;
    void del(const QUrl &url, bool isFile) override;
    void get(const QUrl &url) override;
    void rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags) override;
    void symlink(const QString &target, const QUrl &dest, KIO::JobFlags flags) override;

private:
    void listRoot();

    RemoteImpl m_impl;
};

#endif
