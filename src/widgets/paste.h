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

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 4)
/**
 * Pastes the content of the clipboard to the given destination URL.
 * URLs are treated separately (performing a file copy)
 * from other data (which is saved into a file after asking the user
 * to choose a filename and the preferred data format)
 *
 * @param destURL the URL to receive the data
 * @param widget parent widget to use for dialogs
 * @param move true to move the data, false to copy -- now ignored and handled automatically
 * @return the job that handles the operation
 * @deprecated since 5.4, use KIO::paste() from <KIO/PasteJob> (which takes care of undo/redo too)
 */
KIOWIDGETS_DEPRECATED_VERSION(5, 4, "Use KIO::paste(...) from <KIO/PasteJob>")
KIOWIDGETS_EXPORT Job *pasteClipboard(const QUrl &destURL, QWidget *widget, bool move = false);
#endif

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 4)
/**
 * Save the given mime @p data to the given destination URL
 * after offering the user to choose a data format.
 * This is the method used when handling drops (of anything else than URLs)
 * onto dolphin and konqueror.
 *
 * @param data the QMimeData, usually from a QDropEvent
 * @param destUrl the URL of the directory where the data will be pasted.
 * The filename to use in that directory is prompted by this method.
 * @param dialogText the text to show in the dialog
 * @param widget parent widget to use for dialogs
 *
 * @see pasteClipboard()
 * @deprecated since 5.4, use KIO::paste() from <KIO/PasteJob> (which takes care of undo/redo too)
 */
KIOWIDGETS_DEPRECATED_VERSION(5, 4, "Use KIO::paste(...) from <KIO/PasteJob>")
KIOWIDGETS_EXPORT Job *pasteMimeData(const QMimeData *data, const QUrl &destUrl,
                                     const QString &dialogText, QWidget *widget);
#endif

/**
 * Returns true if pasteMimeData will find any interesting format in @p data.
 * You can use this method to enable/disable the paste action appropriately.
 * @since 5.0 (was called canPasteMimeSource before)
 */
KIOWIDGETS_EXPORT bool canPasteMimeData(const QMimeData *data);

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 4)
/**
 * Returns the text to use for the Paste action, when the application supports
 * pasting files, urls, and clipboard data, using pasteClipboard().
 * @return a string suitable for QAction::setText, or an empty string if pasting
 * isn't possible right now.
 * @deprecated since 5.4, use pasteActionText(const QMimeData *, bool*, const KFileItem &)
 */
KIOWIDGETS_DEPRECATED_VERSION(5, 4, "Use KIO::pasteActionText(const QMimeData *, bool*, const KFileItem &)")
KIOWIDGETS_EXPORT QString pasteActionText();
#endif

/**
 * Returns the text to use for the Paste action, when the application supports
 * pasting files, urls, and clipboard data, using pasteClipboard().
 * @param mimeData the mime data, usually QApplication::clipboard()->mimeData().
 * @param enable output parameter, to be passed to QAction::setEnabled.
 *      The pointer must be non-null, and in return the function will always set its value.
 * @param destItem item representing the directory into which the clipboard data
 *        or items would be pasted. Used to find out about permissions in that directory.
 * @return a string suitable for QAction::setText
 * @since 5.4
 */
KIOWIDGETS_EXPORT QString pasteActionText(const QMimeData *mimeData, bool *enable, const KFileItem &destItem);

/**
 * Add the information whether the files were cut, into the mimedata.
 * @param mimeData pointer to the mimeData object to be populated. Must not be null.
 * @param cut if true, the user selected "cut" (saved as application/x-kde-cutselection in the mimedata).
 * @since 5.2
 */
KIOWIDGETS_EXPORT void setClipboardDataCut(QMimeData* mimeData, bool cut);

/**
 * Returns true if the URLs in @p mimeData were cut by the user.
 * This should be called when pasting, to choose between moving and copying.
 * @since 5.2
 */
KIOWIDGETS_EXPORT bool isClipboardDataCut(const QMimeData *mimeData);

}

#endif
