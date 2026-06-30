/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_BATCHCOPYPACKETS_P_H
#define KIO_BATCHCOPYPACKETS_P_H

#include <QDataStream>
#include <QIODevice>
#include <QList>
#include <QString>

#include <optional>
#include <variant>

namespace KIO
{
// The special() sub-command of the file worker that copies a list of local files.
constexpr qint32 BatchCopyCommand = 4;

// The most files one batch copy request may name. A larger copy takes several requests.
constexpr int BatchCopyMaxFiles = 512;

// The flags of a batch copy request.
enum BatchCopyFlag : qint32 {
    BatchCopyFlagReflink = 0x1, // the destination filesystem supports reflink (FICLONE)
};

// The packets that the batch copy command of the file worker sends on the data channel. Each packet
// starts with its kind.
enum BatchCopyPacketKind : qint32 {
    BatchCopyPacketProgress = 0, // BatchCopyProgress
    BatchCopyPacketAnswer = 1, // BatchCopyAnswer
};

// One file of a batch copy request.
struct BatchCopyFile {
    QString source;
    QString destination;
};

// A batch copy request: its flags and the files to copy.
struct BatchCopyRequest {
    qint32 flags = 0;
    QList<BatchCopyFile> files;
};

// The request as special() takes it, the sub-command first.
inline QByteArray writeBatchCopyRequest(const BatchCopyRequest &request)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << BatchCopyCommand << request.flags << qint32(request.files.size());
    for (const BatchCopyFile &file : request.files) {
        stream << file.source << file.destination;
    }
    return data;
}

// Reads what follows the sub-command. Empty when the request does not decode, or names more than
// BatchCopyMaxFiles files.
inline std::optional<BatchCopyRequest> readBatchCopyRequest(QDataStream &stream)
{
    BatchCopyRequest request;
    qint32 count = 0;
    stream >> request.flags >> count;
    if (stream.status() != QDataStream::Ok || count < 0 || count > BatchCopyMaxFiles) {
        return std::nullopt;
    }
    request.files.reserve(count);
    for (qint32 i = 0; i < count; ++i) {
        BatchCopyFile file;
        stream >> file.source >> file.destination;
        if (stream.status() != QDataStream::Ok) {
            return std::nullopt;
        }
        request.files.append(file);
    }
    return request;
}

// Sent while the batch runs: the index of the file being copied, or -1, and the files finished
// since the previous progress packet.
struct BatchCopyProgress {
    qint32 current = -1;
    QList<qint32> finished;
};

// Sent once, before the result of the command: the files that the worker deferred to the job.
struct BatchCopyAnswer {
    QList<qint32> deferred;
};

// std::monostate for a packet that does not decode, or of a kind that this version does not know.
using BatchCopyPacket = std::variant<std::monostate, BatchCopyProgress, BatchCopyAnswer>;

inline QByteArray writeBatchCopyPacket(const BatchCopyProgress &progress)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << qint32(BatchCopyPacketProgress) << progress.current << progress.finished;
    return data;
}

inline QByteArray writeBatchCopyPacket(const BatchCopyAnswer &answer)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << qint32(BatchCopyPacketAnswer) << answer.deferred;
    return data;
}

inline BatchCopyPacket readBatchCopyPacket(const QByteArray &data)
{
    QDataStream stream(data);
    qint32 kind = -1;
    stream >> kind;
    BatchCopyPacket packet;
    if (kind == BatchCopyPacketProgress) {
        BatchCopyProgress progress;
        stream >> progress.current >> progress.finished;
        packet = progress;
    } else if (kind == BatchCopyPacketAnswer) {
        BatchCopyAnswer answer;
        stream >> answer.deferred;
        packet = answer;
    }
    if (stream.status() != QDataStream::Ok) {
        return {};
    }
    return packet;
}
}

#endif
