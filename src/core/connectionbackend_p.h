/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_CONNECTIONBACKEND_P_H
#define KIO_CONNECTIONBACKEND_P_H

#include "metadata.h"
#include "udsentry.h"

#include <QDataStream>
#include <QObject>
#include <QUrl>

#include <utility>
#include <variant>

namespace KIO
{
// The two values an error carries.
struct TaskError {
    qint32 code = 0;
    QString text;
};

// What a task carries. A peer sharing this process is handed the objects themselves, a peer in
// another process is handed the bytes they are written to, which is all a socket can carry, so the
// bytes are one of the kinds. std::monostate is a message carrying nothing.
using TaskPayload = std::variant<std::monostate, QByteArray, quint32, quint64, QString, QUrl, UDSEntry, UDSEntryList, MetaData, TaskError>;

// The bytes a payload carries, which is what it carries whenever it came off a socket. Empty when it
// carries an object instead.
inline QByteArray payloadBytes(const TaskPayload &payload)
{
    const QByteArray *value = std::get_if<QByteArray>(&payload);
    return value ? *value : QByteArray();
}

// What a message carries: the object the peer handed over, when it shares this process, and what the
// bytes it sent stand for otherwise. Taking the object leaves the payload empty of it.
template<typename T>
T carried(TaskPayload &payload, QDataStream &stream)
{
    if (T *value = std::get_if<T>(&payload)) {
        return std::exchange(*value, T{});
    }
    T value{};
    stream >> value;
    return value;
}

struct Task {
    int cmd = -1;
    TaskPayload payload{};

    QByteArray bytes() const
    {
        return payloadBytes(payload);
    }
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

    /*!
     * Hands what a message carries to the peer. This writes it down and sends the bytes, which is
     * what a peer in another process can be given, and bytes are sent as they are. A backend whose
     * peer shares this process overrides this to hand every kind over as it is.
     */
    virtual bool sendPayload(int command, const TaskPayload &payload);

Q_SIGNALS:
    void disconnected();
    void commandReceived(const KIO::Task &task);
};
}

#endif
