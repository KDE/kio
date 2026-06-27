/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_CONNECTIONBACKEND_P_H
#define KIO_CONNECTIONBACKEND_P_H

#include <QObject>
#include <QUrl>

namespace KIO
{
struct Task {
    int cmd = -1;
    QByteArray data{};
};

/*!
 * \internal
 *
 * Abstract transport used by Connection to exchange Tasks (command + payload)
 * with a peer. The peer can live in another process (SocketConnectionBackend,
 * over a QLocalSocket) or in another thread of the same process
 * (ThreadConnectionBackend, over direct queued signals/slots).
 */
class ConnectionBackend : public QObject
{
    Q_OBJECT

public:
    enum State {
        Idle,
        Listening,
        Connected,
    };
    State state = Idle;
    QUrl address;
    QString errorString;

    explicit ConnectionBackend(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~ConnectionBackend() override = default;

    virtual void setSuspended(bool enable) = 0;
    virtual void close() = 0;
    virtual bool waitForIncomingTask(int ms) = 0;
    virtual bool sendCommand(int command, const QByteArray &data) = 0;

Q_SIGNALS:
    void disconnected();
    void commandReceived(const KIO::Task &task);
};
}

#endif
