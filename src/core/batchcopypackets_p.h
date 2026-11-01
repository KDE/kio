/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_BATCHCOPYPACKETS_P_H
#define KIO_BATCHCOPYPACKETS_P_H

#include <QtGlobal>

namespace KIO
{
// The packets that the batch copy command of the file worker sends on the data channel. Each packet
// starts with its kind.
enum BatchCopyPacketKind : qint32 {
    BatchCopyPacketProgress = 0, // qint32 current, QList<qint32> finished since the previous packet
    BatchCopyPacketAnswer = 1, // QList<qint32>: the files the worker did not copy
};
}

#endif
