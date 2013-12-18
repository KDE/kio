/* This file is part of the KDE libraries
   Copyright (C) 2000-2005 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KIO_PASTE_H
#define KIO_PASTE_H

#include <kio/kiowidgets_export.h>
#include <QtCore/QString>
class QWidget;
class QUrl;
class QMimeData;

namespace KIO {
    class Job;
    class CopyJob;

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
   * @see pasteData()
   */
  KIOWIDGETS_EXPORT Job *pasteClipboard( const QUrl& destURL, QWidget* widget, bool move = false );

  /**
   * Pastes the given @p data to the given destination URL.
   * NOTE: This method is blocking (uses NetAccess for saving the data).
   * Please consider using pasteDataAsync instead.
   *
   * @param destURL the URL of the directory where the data will be pasted.
   * The filename to use in that directory is prompted by this method.
   * @param data the data to copy
   * @param widget parent widget to use for dialogs
   * @see pasteClipboard()
   *
   * This method is a candidate for disappearing in KDE5, email faure at kde.org if you
   * are using it in your application, then I'll reconsider.
   */
  KIOWIDGETS_EXPORT void pasteData( const QUrl& destURL, const QByteArray& data, QWidget* widget );


  /**
   * Pastes the given @p data to the given destination URL.
   * Note that this method requires the caller to have chosen the QByteArray
   * to paste before hand, unlike pasteClipboard and pasteMimeSource.
   *
   * @param destURL the URL of the directory where the data will be pasted.
   * The filename to use in that directory is prompted by this method.
   * @param data the data to copy
   * @param dialogText the text to show in the dialog
   * @see pasteClipboard()
   *
   * This method is a candidate for disappearing in KDE5, email faure at kde.org if you
   * are using it in your application, then I'll reconsider.
   */
  KIOWIDGETS_EXPORT CopyJob *pasteDataAsync( const QUrl& destURL, const QByteArray& data, QWidget *widget, const QString& dialogText = QString() );


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
   * @param clipboard whether the QMimeData comes from QClipboard. If you
   * use pasteClipboard for that case, you never have to worry about this parameter.
   *
   * @see pasteClipboard()
   */
  KIOWIDGETS_EXPORT Job* pasteMimeData(const QMimeData* data, const QUrl& destUrl,
                                const QString& dialogText, QWidget* widget);

  /**
   * @deprecated because it returns a CopyJob*, and this is better implemented
   * without a copy job. Use pasteMimeData instead.
   * Note that you'll have to tell the user in case of an error (no data to paste),
   * while pasteMimeSource did that.
   */
#ifndef KDE_NO_DEPRECATED
  KIOWIDGETS_DEPRECATED_EXPORT CopyJob* pasteMimeSource( const QMimeData* data, const QUrl& destURL,
                                       const QString& dialogText, QWidget* widget,
                                       bool clipboard = false );
#endif


  /**
   * Returns true if pasteMimeSource finds any interesting format in @p data.
   * You can use this method to enable/disable the paste action appropriately.
   * @since 4.3
   */
  KIOWIDGETS_EXPORT bool canPasteMimeSource(const QMimeData* data);

  /**
   * Returns the text to use for the Paste action, when the application supports
   * pasting files, urls, and clipboard data, using pasteClipboard().
   * @return a string suitable for KAction::setText, or an empty string if pasting
   * isn't possible right now.
   */
  KIOWIDGETS_EXPORT QString pasteActionText();
}

#endif
