/*
    SPDX-FileCopyrightText: 2022 Friedrich W. H. Kossebau <kossebau@kde.org>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef MESSAGEBOXWORKER_H
#define MESSAGEBOXWORKER_H

// KF
#include <KIO/WorkerBase>

// See README
class MessageBoxWorker : public KIO::WorkerBase
{
public:
    MessageBoxWorker(const QByteArray &pool_socket, const QByteArray &app_socket);
    ~MessageBoxWorker() override;

public: // KIO::WorkerBase API
    KIO::WorkerResult get(const QUrl &url) override;
    KIO::WorkerResult stat(const QUrl &url) override;
    KIO::WorkerResult listDir(const QUrl &url) override;
};

#endif
