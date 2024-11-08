/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2005 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KIO_PASTE_H
#define KIO_PASTE_H

#include "kiowidgets_export.h"
#include <QString>
class QWidget;
class QUrl;
class QMimeData;
class KFileItem;

namespace KIO
{
class Job;
class CopyJob;

// TODO qdoc header

/*!
 * Returns true if pasteMimeData will find any interesting format in \a data.
 * You can use this method to enable/disable the paste action appropriately.
 * \since 5.0 (was called canPasteMimeSource before)
 */
KIOWIDGETS_EXPORT bool canPasteMimeData(const QMimeData *data);

/*!
 * Returns the text to use for the Paste action, when the application supports
 * pasting files, urls, and clipboard data, using pasteClipboard().
 *
 * \a mimeData the mime data, usually QApplication::clipboard()->mimeData().
 *
 * \a enable output parameter, to be passed to QAction::setEnabled.
 *      The pointer must be non-null, and in return the function will always set its value.
 *
 * \a destItem item representing the directory into which the clipboard data
 *        or items would be pasted. Used to find out about permissions in that directory.
 *
 * Returns a string suitable for QAction::setText
 * \since 5.4
 */
KIOWIDGETS_EXPORT QString pasteActionText(const QMimeData *mimeData, bool *enable, const KFileItem &destItem);

/*!
 * Add the information whether the files were cut, into the mimedata.
 *
 * \a mimeData pointer to the mimeData object to be populated. Must not be null.
 *
 * \a cut if true, the user selected "cut" (saved as application/x-kde-cutselection in the mimedata).
 * \since 5.2
 */
KIOWIDGETS_EXPORT void setClipboardDataCut(QMimeData *mimeData, bool cut);

/*!
 * Returns true if the URLs in \a mimeData were cut by the user.
 * This should be called when pasting, to choose between moving and copying.
 * \since 5.2
 */
KIOWIDGETS_EXPORT bool isClipboardDataCut(const QMimeData *mimeData);

}

#endif
