/*
    SPDX-FileCopyrightText: 1997 Torben Weis <weis@stud.uni-frankfurt.de>
    SPDX-FileCopyrightText: 1999 Dirk Mueller <mueller@kde.org>
    Portions SPDX-FileCopyrightText: 1999 Preston Brown <pbrown@kde.org>
    SPDX-FileCopyrightText: 2007 Pino Toscano <pino@kde.org>
    SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QString>

#include <KService>

#include "kiocore_export.h"

namespace KIO
{

/**
 * Core class for open with style dialog handling. This only implements core functionality. For an actual open with
 * implementation see KOpenWithDialog. For a way to trigger open with dialogs see OpenUrlJob and OpenWithHandlerInterface.
 */
class KIOCORE_EXPORT OpenWith
{
public:
    struct [[nodiscard]] AcceptResult {
        /// Whether the accept was successful (if not error is set)
        bool accept;
        /// The error message if the acccept failed
        QString error;
        /// Whether the sycoca needs rebuilding (e.g. call KBuildSycocaProgressDialog::rebuildKSycoca)
        bool rebuildSycoca = false;
    };
    /**
     * Accept an openwith request with the provided arguments as context.
     * This function may have side effects to do with accepting, such as setting the default application for the
     * mimetype if @p remember is true.
     * @returns an AcceptResult
     */
    static AcceptResult accept(KService::Ptr &service,
                               const QString &typedExec,
                               bool remember,
                               const QString &mimeType,
                               bool openInTerminal,
                               bool lingerTerminal,
                               bool saveNewApps);
};

} // namespace KIO
