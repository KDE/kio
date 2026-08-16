/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "connectionbackend_p.h"

#include <QDataStream>
#include <QIODevice>

// ConnectionBackend is an abstract base. The concrete backends live in
// socketconnectionbackend.cpp and threadconnectionbackend.cpp.

using namespace KIO;

// What a message carries, written down. This is what a peer in another process is given, and what
// it reads back, so every kind is written the way the sender used to write it.
bool ConnectionBackend::sendPayload(int command, const TaskPayload &payload)
{
    // Bytes are what the peer is given, so they go as they are and nothing is allocated for them.
    if (const QByteArray *bytes = std::get_if<QByteArray>(&payload)) {
        return sendCommand(command, *bytes);
    }

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    std::visit(
        [&stream](const auto &value) {
            using Type = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Type, std::monostate> || std::is_same_v<Type, QByteArray>) {
                // Nothing to write, the bytes went above.
            } else if constexpr (std::is_same_v<Type, UDSEntryList>) {
                for (const UDSEntry &entry : value) {
                    stream << entry;
                }
            } else if constexpr (std::is_same_v<Type, TaskError>) {
                stream << value.code << value.text;
            } else {
                stream << value;
            }
        },
        payload);
    return sendCommand(command, data);
}

#include "moc_connectionbackend_p.cpp"
