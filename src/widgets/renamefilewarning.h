/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_RENAMEFILEWARNING_H
#define KIO_RENAMEFILEWARNING_H

#include "kiowidgets_export.h"

#include <KFileItem>

#include <functional>

class QWidget;

namespace KIO
{

/*!
 * \since 6.27
 *
 * Checks whether renaming \a item to \a newName requires user confirmation
 * and shows the appropriate dialog if so.
 *
 * The \a callback is called with \c true if the rename should proceed,
 * or \c false if the user cancelled. If no confirmation is needed, the
 * callback is called immediately with \c true.
 *
 * \param item      The file item being renamed.
 * \param newName   The proposed new name.
 * \param parent    Parent widget for any dialog that may be shown.
 * \param callback  Called with the user's decision.
 * \param hiddenFilesVisible  Pass \c true when the view already shows hidden
 *                  files. In that case the warning about renaming a file to a
 *                  hidden name is suppressed, because the file will remain
 *                  visible after the rename.
 */
KIOWIDGETS_EXPORT void confirmRenameWarning(const KFileItem &item,
                                            const QString &newName,
                                            QWidget *parent,
                                            std::function<void(bool proceed)> callback,
                                            bool hiddenFilesVisible = false);

} // namespace KIO

#endif // KIO_RENAMEFILEWARNING_H
