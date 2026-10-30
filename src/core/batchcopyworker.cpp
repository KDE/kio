/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "batchcopy_p.h"

#include <QDataStream>

using namespace KIO;

BatchCopyWorker::~BatchCopyWorker() = default;

QDataStream &KIO::operator<<(QDataStream &stream, const BatchCopyRequest &request)
{
    stream << qint32(request.flags.toInt()) << qint32(request.items.size());
    for (const BatchCopyItem &item : request.items) {
        stream << item.source << item.destination << qint32(item.permissions) << item.modificationTime << quint64(item.size);
    }
    return stream;
}

QDataStream &KIO::operator>>(QDataStream &stream, BatchCopyRequest &request)
{
    qint32 flags = 0;
    qint32 count = 0;
    stream >> flags >> count;
    if (stream.status() != QDataStream::Ok || count < 0 || count > s_maxBatchCopyItems) {
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    request.flags = JobFlags::fromInt(flags);
    request.items.clear();
    request.items.reserve(count);
    for (qint32 i = 0; i < count; ++i) {
        BatchCopyItem item;
        qint32 permissions = -1;
        quint64 size = 0;
        stream >> item.source >> item.destination >> permissions >> item.modificationTime >> size;
        if (stream.status() != QDataStream::Ok) {
            request.items.clear();
            return stream;
        }
        item.permissions = permissions;
        item.size = size;
        request.items.append(item);
    }
    return stream;
}
