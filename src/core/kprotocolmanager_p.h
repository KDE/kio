/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2000 Waldo Bastain <bastain@kde.org>
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2008 Jarosław Staniek <staniek@kde.org>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KPROTOCOLMANAGER_P_H
#define KPROTOCOLMANAGER_P_H

#include <kiocore_export.h>

#include <QMutex>
#include <QString>
#include <QUrl>

#include <KSharedConfig>

#include "kprotocolmanager.h"

class KIOCORE_EXPORT KProtocolManagerPrivate
{
public:
    KProtocolManagerPrivate();
    ~KProtocolManagerPrivate();
    void sync();

    // Whether a worker copies files from src to dest with KIO::BatchCopyWorker, from the
    // "batchCopy", "batchCopyFromFile" and "batchCopyToFile" keys of its metadata.
    static bool canBatchCopy(const QUrl &src, const QUrl &dest);

    QMutex mutex; // protects all member vars
    KSharedConfig::Ptr configPtr;

    QMap<QString /*mimetype*/, QString /*protocol*/> protocolForArchiveMimetypes;
};

#endif
