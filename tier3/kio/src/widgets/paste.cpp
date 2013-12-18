/* This file is part of the KDE libraries
   Copyright (C) 2000 David Faure <faure@kde.org>

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

#include "paste.h"
#include "clipboardupdater_p.h"
#include "pastedialog.h"

#include "kio/job.h"
#include "kio/copyjob.h"
#include "kio/deletejob.h"
#include "kio/global.h"
#include "kio/renamedialog.h"
#include "kio/kprotocolmanager.h"

#include <kjobwidgets.h>
#include <klocalizedstring.h>
#include <kmessagebox.h>
#include <kurlmimedata.h>

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <qtemporaryfile.h>
#include <qmimedatabase.h>
#include <qinputdialog.h>
#include <QDebug>

static bool decodeIsCutSelection(const QMimeData *mimeData)
{
    const QByteArray data = mimeData->data("application/x-kde-cutselection");
    return data.isEmpty() ? false : data.at(0) == '1';
}

// This could be made a public method, if there's a need for pasting only urls
// and not random data.
/**
 * Pastes URLs from the clipboard. This results in a copy or move job,
 * depending on whether the user has copied or cut the items.
 *
 * @param mimeData the mimeData to paste, usually QApplication::clipboard()->mimeData()
 * @param destDir Destination directory where the items will be copied/moved.
 * @param flags the flags are passed to KIO::copy or KIO::move.
 * @return the copy or move job handling the operation, or 0 if there is nothing to do
 * @since ...
 */
//KIOWIDGETS_EXPORT Job *pasteClipboardUrls(const QUrl& destDir, JobFlags flags = DefaultFlags);
static KIO::Job *pasteClipboardUrls(const QMimeData* mimeData, const QUrl& destDir, KIO::JobFlags flags = KIO::DefaultFlags)
{
    const QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(mimeData, KUrlMimeData::PreferLocalUrls);
    if (!urls.isEmpty()) {
        const bool move = decodeIsCutSelection(mimeData);
        KIO::Job *job = 0;
        if (move) {
            job = KIO::move(urls, destDir, flags);
            KIO::ClipboardUpdater* clipboardUpdater = job->findChild<KIO::ClipboardUpdater *>();
            if (clipboardUpdater) {
                clipboardUpdater->setMode(KIO::JobUiDelegateExtension::OverwriteContent);
            }
        }
        else
            job = KIO::copy(urls, destDir, flags);
        return job;
    }
    return 0;
}

static QUrl getNewFileName(const QUrl &u, const QString& text, const QString& suggestedFileName, QWidget *widget, bool delIfOverwrite)
{
  bool ok;
  QString dialogText( text );
  if ( dialogText.isEmpty() )
    dialogText = i18n( "Filename for clipboard content:" );
  QString file = QInputDialog::getText( widget, QString(), dialogText, QLineEdit::Normal, suggestedFileName, &ok );
  if ( !ok )
     return QUrl();

  QUrl myurl(u);
  myurl.setPath(myurl.path() + '/' + file);

  KIO::StatJob *job = KIO::stat(myurl, myurl.isLocalFile() ? KIO::HideProgressInfo : KIO::DefaultFlags);
  job->setDetails(0);
  job->setSide(KIO::StatJob::DestinationSide);
  KJobWidgets::setWindow(job, widget);

  // Check for existing destination file.
  // When we were using CopyJob, we couldn't let it do that (would expose
  // an ugly tempfile name as the source URL)
  // And now we're using a put job anyway, no destination checking included.
  if (job->exec()) {
      //qDebug() << "Paste will overwrite file.  Prompting...";
      KIO::RenameDialog_Result res = KIO::R_OVERWRITE;

      KIO::RenameDialog dlg( widget,
                          i18n("File Already Exists"),
                          u,
                          myurl,
                          (KIO::RenameDialog_Mode) (KIO::M_OVERWRITE | KIO::M_SINGLE) );
          res = static_cast<KIO::RenameDialog_Result>(dlg.exec());

          if ( res == KIO::R_RENAME )
          {
              myurl = dlg.newDestUrl();
          }
          else if ( res == KIO::R_CANCEL )
          {
              return QUrl();
          } else if (res == KIO::R_OVERWRITE)
          {
              // Old hack. With the put job we just pass Overwrite.
              if (delIfOverwrite) {
                  // Ideally we would just pass KIO::Overwrite to the job in pasteDataAsyncTo.
                  // But 1) CopyJob doesn't support that (it wouldn't really apply to multiple files) [not true anymore]
                  // 2) we can't use file_move because CopyJob* is everywhere in the API (see TODO)
                  // But well the simpler is really to delete the dest:
                  KIO::Job* delJob = KIO::del(myurl);
                  delJob->exec();
              }
          }
      }

      return myurl;
    }

    // Old solution
    // The final step: write _data to tempfile and move it to newUrl
    static KIO::CopyJob* pasteDataAsyncTo( const QUrl& newUrl, const QByteArray& _data )
    {
        // ### Bug: because we move from a tempfile to the destination,
        // if the user does "Undo" then we won't ask for confirmation, and we'll
        // move back to a tempfile, instead of just deleting.
        // A KIO::storedPut would be better but FileUndoManager would need to support it first.
        QTemporaryFile tempFile;
        tempFile.setAutoRemove(false);
        tempFile.open();
        tempFile.write(_data.data(), _data.size());
        tempFile.flush();
        QUrl origUrl = QUrl::fromLocalFile(tempFile.fileName());
        return KIO::move(origUrl, newUrl);
    }

    // New solution
    static KIO::Job* putDataAsyncTo(const QUrl& url, const QByteArray& data, QWidget* widget, KIO::JobFlags flags)
    {
        KIO::Job* job = KIO::storedPut(data, url, -1, flags);
        KJobWidgets::setWindow(job, widget);
        return job;
    }

    static QByteArray chooseFormatAndUrl(const QUrl& u, const QMimeData* mimeData,
                                         const QStringList& formats,
                                         const QString& text,
                                         const QString& suggestedFileName,
                                         QWidget* widget,
                                         bool clipboard,
                                         QUrl* newUrl)
    {
        QMimeDatabase db;
        QStringList formatLabels;
        for ( int i = 0; i < formats.size(); ++i ) {
            const QString& fmt = formats[i];
            QMimeType mime = db.mimeTypeForName(fmt);
            if (mime.isValid())
                formatLabels.append(i18n("%1 (%2)", mime.comment(), fmt));
            else
                formatLabels.append(fmt);
        }

        QString dialogText( text );
        if ( dialogText.isEmpty() )
            dialogText = i18n( "Filename for clipboard content:" );
        //using QString() instead of QString::null didn't compile (with gcc 3.2.3), because the ctor was mistaken as a function declaration, Alex //krazy:exclude=nullstrassign
        KIO::PasteDialog dlg( QString::null, dialogText, suggestedFileName, formatLabels, widget, clipboard ); //krazy:exclude=nullstrassign

    if ( dlg.exec() != QDialog::Accepted )
        return QByteArray();

    if ( clipboard && dlg.clipboardChanged() ) {
        KMessageBox::sorry( widget,
                            i18n( "The clipboard has changed since you used 'paste': "
                                  "the chosen data format is no longer applicable. "
                                  "Please copy again what you wanted to paste." ) );
        return QByteArray();
    }

    const QString result = dlg.lineEditText();
    const QString chosenFormat = formats[ dlg.comboItem() ];

    //qDebug() << " result=" << result << " chosenFormat=" << chosenFormat;
    *newUrl = u;
    newUrl->setPath(newUrl->path() + '/' + result);
    // if "data" came from QClipboard, then it was deleted already - by a nice 0-seconds timer
    // In that case, get it again. Let's hope the user didn't copy something else meanwhile :/
    // #### QT4/KDE4 TODO: check that this is still the case
    if ( clipboard ) {
        mimeData = QApplication::clipboard()->mimeData();
    }
    const QByteArray ba = mimeData->data( chosenFormat );
    return ba;
}

static QStringList extractFormats(const QMimeData* mimeData)
{
    QStringList formats;
    const QStringList allFormats = mimeData->formats();
    Q_FOREACH(const QString& format, allFormats) {
        if (format == QLatin1String("application/x-qiconlist")) // see QIconDrag
            continue;
        if (format == QLatin1String("application/x-kde-cutselection")) // see KonqDrag
            continue;
        if (format == QLatin1String("application/x-kde-suggestedfilename"))
            continue;
        if (format.startsWith(QLatin1String("application/x-qt-"))) // Qt-internal
            continue;
        if (format.startsWith(QLatin1String("x-kmail-drag/"))) // app-internal
            continue;
        if (!format.contains(QLatin1Char('/'))) // e.g. TARGETS, MULTIPLE, TIMESTAMP
            continue;
        formats.append(format);
    }
    return formats;
}

#ifndef KDE_NO_DEPRECATED
// The [old] main method for dropping
KIO::CopyJob* KIO::pasteMimeSource( const QMimeData* mimeData, const QUrl& destUrl,
                                    const QString& dialogText, QWidget* widget, bool clipboard )
{
  QByteArray ba;

  const QString suggestedFilename = QString::fromUtf8(mimeData->data("application/x-kde-suggestedfilename"));

  // Now check for plain text
  // We don't want to display a mimetype choice for a QTextDrag, those mimetypes look ugly.
  if ( mimeData->hasText() )
  {
      ba = mimeData->text().toLocal8Bit(); // encoding OK?
  }
  else
  {
      const QStringList formats = extractFormats(mimeData);
      if ( formats.size() == 0 )
          return 0;

      if ( formats.size() > 1 ) {
          QUrl newUrl;
          ba = chooseFormatAndUrl(destUrl, mimeData, formats, dialogText, suggestedFilename, widget, clipboard, &newUrl);
          KIO::CopyJob* job = pasteDataAsyncTo(newUrl, ba);
          KJobWidgets::setWindow(job, widget);
          return job;
      }
      ba = mimeData->data( formats.first() );
  }
  if ( ba.isEmpty() )
  {
    KMessageBox::sorry( widget, i18n("The clipboard is empty") );
    return 0;
  }

    const QUrl newUrl = getNewFileName(destUrl, dialogText, suggestedFilename, widget, true);
    if (newUrl.isEmpty())
        return 0;

    KIO::CopyJob* job = pasteDataAsyncTo(newUrl, ba);
    KJobWidgets::setWindow(job, widget);
    return job;
}
#endif

KIOWIDGETS_EXPORT bool KIO::canPasteMimeSource(const QMimeData* data)
{
    return data->hasText() || !extractFormats(data).isEmpty();
}

KIO::Job* pasteMimeDataImpl(const QMimeData* mimeData, const QUrl& destUrl,
                            const QString& dialogText, QWidget* widget,
                            bool clipboard)
{
    QByteArray ba;
    const QString suggestedFilename = QString::fromUtf8(mimeData->data("application/x-kde-suggestedfilename"));

    // Now check for plain text
    // We don't want to display a mimetype choice for a QTextDrag, those mimetypes look ugly.
    if (mimeData->hasText()) {
        ba = mimeData->text().toLocal8Bit(); // encoding OK?
    } else {
        const QStringList formats = extractFormats(mimeData);
        if (formats.isEmpty()) {
            return 0;
        } else if (formats.size() > 1) {
            QUrl newUrl;
            ba = chooseFormatAndUrl(destUrl, mimeData, formats, dialogText, suggestedFilename, widget, clipboard, &newUrl);
            if (ba.isEmpty()) {
                return 0;
            }
            return putDataAsyncTo(newUrl, ba, widget, KIO::Overwrite);
        }
        ba = mimeData->data(formats.first());
    }
    if (ba.isEmpty()) {
        return 0;
    }

    const QUrl newUrl = getNewFileName(destUrl, dialogText, suggestedFilename, widget, false);
    if (newUrl.isEmpty())
        return 0;

    return putDataAsyncTo(newUrl, ba, widget, KIO::Overwrite);
}

// The main method for pasting
KIOWIDGETS_EXPORT KIO::Job *KIO::pasteClipboard( const QUrl& destUrl, QWidget* widget, bool move )
{
    Q_UNUSED(move);

  if ( !destUrl.isValid() ) {
    KMessageBox::error(widget, i18n("Malformed URL\n%1", destUrl.errorString()));
    qWarning() << destUrl.errorString();
    return 0;
  }

  // TODO: if we passed mimeData as argument, we could write unittests that don't
  // mess up the clipboard and that don't need QtGui.
  const QMimeData *mimeData = QApplication::clipboard()->mimeData();

  if (mimeData->hasUrls()) {
      // We can ignore the bool move, KIO::paste decodes it
      KIO::Job* job = pasteClipboardUrls(mimeData, destUrl);
      if (job) {
          KJobWidgets::setWindow(job, widget);
          return job;
      }
  }

  return pasteMimeDataImpl(mimeData, destUrl, QString(), widget, true /*clipboard*/);
}


KIOWIDGETS_EXPORT void KIO::pasteData(const QUrl& u, const QByteArray& data, QWidget* widget)
{
    const QUrl newUrl = getNewFileName(u, QString(), QString(), widget, false);
    if (newUrl.isEmpty())
       return;

    KIO::Job* job = putDataAsyncTo(newUrl, data, widget, KIO::Overwrite);
    KJobWidgets::setWindow(job, widget);
    job->exec();
}

// KDE5: remove
KIOWIDGETS_EXPORT KIO::CopyJob* KIO::pasteDataAsync( const QUrl& u, const QByteArray& _data, QWidget *widget, const QString& text )
{
    QUrl newUrl = getNewFileName(u, text, QString(), widget, true);

    if (newUrl.isEmpty())
       return 0;

    KIO::CopyJob* job = pasteDataAsyncTo( newUrl, _data );
    KJobWidgets::setWindow(job, widget);
    return job;
}

// NOTE: DolphinView::pasteInfo() has a better version of this
// (but which requires KonqFileItemCapabilities)
// (KFileItemCapabilities exists now, but are missing the KFileItem for the dest dir)
KIOWIDGETS_EXPORT QString KIO::pasteActionText()
{
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    const QList<QUrl> urls = KUrlMimeData::urlsFromMimeData( mimeData );
    if ( !urls.isEmpty() ) {
        if ( urls.first().isLocalFile() )
            return i18np( "&Paste File", "&Paste %1 Files", urls.count() );
        else
            return i18np( "&Paste URL", "&Paste %1 URLs", urls.count() );
    } else if ( !mimeData->formats().isEmpty() ) {
        return i18n( "&Paste Clipboard Contents" );
    } else {
        return QString();
    }
}

// The [new] main method for dropping
KIOWIDGETS_EXPORT KIO::Job* KIO::pasteMimeData(const QMimeData* mimeData, const QUrl& destUrl,
                                        const QString& dialogText, QWidget* widget)
{
    return pasteMimeDataImpl(mimeData, destUrl, dialogText, widget, false /*not clipboard*/);
}
