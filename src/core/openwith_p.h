/*
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QString>

#include "kiocore_export.h"

namespace KIO
{
/*!
 * Whether \a extension can name a MIME type of its own, as createExtensionMimeType() makes.
 * \internal
 */
KIOCORE_EXPORT bool isValidExtensionForMimeType(const QString &extension);

/*!
 * The result of createExtensionMimeType(): the MIME type, or an error message when the type cannot be
 * defined. To become std::expected<QString, QString> once KIO builds with C++23.
 * \internal
 */
struct [[nodiscard]] ExtensionMimeTypeResult {
    // Empty when the type could not be defined.
    QString mimeType = {};
    QString error = {};
};

/*!
 * Returns the MIME type of the files whose name ends with ".extension", which the user associates
 * with an application while the system knows no type for them. It is application/x-extension-<extension>,
 * which GLib creates too, and it is defined in the user's MIME database if it does not exist yet.
 * \internal
 */
KIOCORE_EXPORT ExtensionMimeTypeResult createExtensionMimeType(const QString &extension);
}
