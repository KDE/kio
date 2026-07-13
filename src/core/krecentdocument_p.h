/*
    SPDX-FileCopyrightText: 2026 Tag Howard <tag@jthoward.dev>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KRECENTDOCUMENT_P_H
#define KRECENTDOCUMENT_P_H

#include "kiocore_export.h"

#include <QString>
#include <QUrl>

// \internal not installed, for use by KIOGui to avoid re-detecting an already-known mimetype (which can require reading the file's content).
namespace KRecentDocumentPrivate
{
// TODO KF7: merge with KRecentDocument::add with a default mimetype = {} argument
KIOCORE_EXPORT void addWithKnownMimeType(const QUrl &url, const QString &desktopEntryName, const QString &mimeType);
}

#endif
