/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_BATCHCOPY_P_H
#define KIO_BATCHCOPY_P_H

#include "batchcopyworker.h"

class QDataStream;

namespace KIO
{
// The arguments of CMD_BATCH_COPY.
struct BatchCopyRequest {
    QList<BatchCopyItem> items;
    JobFlags flags;
};

// Far above the largest batch a job sends. A count above it is a malformed request.
inline constexpr qint32 s_maxBatchCopyItems = 8192;

KIOCORE_EXPORT QDataStream &operator<<(QDataStream &stream, const BatchCopyRequest &request);
// Sets the stream status to QDataStream::ReadCorruptData on a malformed request.
KIOCORE_EXPORT QDataStream &operator>>(QDataStream &stream, BatchCopyRequest &request);
}

#endif
