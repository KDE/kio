/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "paste.h"
#include "kio_widgets_debug.h"

#include "../utils_p.h"
#include "kio/copyjob.h"
#include "kio/deletejob.h"
#include "kio/global.h"
#include "kio/renamedialog.h"
#include "kio/statjob.h"
#include "pastedialog_p.h"
#include <kdirnotify.h>
#include <kfileitem.h>
#include <kfileitemlistproperties.h>
#include <kio/storedtransferjob.h>

#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KUrlMimeData>

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFileInfo>
#include <QInputDialog>
#include <QMimeData>
#include <QMimeDatabase>
#include <QTemporaryFile>

static QUrl getDestinationUrl(const QUrl &srcUrl, const QUrl &destUrl, QWidget *widget)
{
    KIO::StatJob *job = KIO::stat(destUrl, destUrl.isLocalFile() ? KIO::HideProgressInfo : KIO::DefaultFlags);
    job->setDetails(KIO::StatBasic);
    job->setSide(KIO::StatJob::DestinationSide);
    KJobWidgets::setWindow(job, widget);

    // Check for existing destination file.
    // When we were using CopyJob, we couldn't let it do that (would expose
    // an ugly tempfile name as the source URL)
    // And now we're using a put job anyway, no destination checking included.
    if (job->exec()) {
        KIO::RenameDialog dlg(widget, i18n("File Already Exists"), srcUrl, destUrl, KIO::RenameDialog_Overwrite);
        KIO::RenameDialog_Result res = static_cast<KIO::RenameDialog_Result>(dlg.exec());

        if (res == KIO::Result_Rename) {
            return dlg.newDestUrl();
        } else if (res == KIO::Result_Cancel) {
            return QUrl();
        } else if (res == KIO::Result_Overwrite) {
            return destUrl;
        }
    }

    return destUrl;
}

static QUrl getNewFileName(const QUrl &u, const QString &text, const QString &suggestedFileName, QWidget *widget)
{
    bool ok;
    QString dialogText(text);
    if (dialogText.isEmpty()) {
        dialogText = i18n("Filename for clipboard content:");
    }
    QString file = QInputDialog::getText(widget, QString(), dialogText, QLineEdit::Normal, suggestedFileName, &ok);
    if (!ok) {
        return QUrl();
    }

    QUrl myurl(u);
    myurl.setPath(Utils::concatPaths(myurl.path(), file));

    return getDestinationUrl(u, myurl, widget);
}

static KIO::Job *putDataAsyncTo(const QUrl &url, const QByteArray &data, QWidget *widget, KIO::JobFlags flags)
{
    KIO::Job *job = KIO::storedPut(data, url, -1, flags);
    QObject::connect(job, &KIO::Job::result, [url](KJob *job) {
        if (job->error() == KJob::NoError) {
#ifdef WITH_QTDBUS
            org::kde::KDirNotify::emitFilesAdded(url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash));
#endif
        }
    });
    KJobWidgets::setWindow(job, widget);
    return job;
}

static QByteArray chooseFormatAndUrl(const QUrl &u,
                                     const QMimeData *mimeData,
                                     const QStringList &formats,
                                     const QString &text,
                                     const QString &suggestedFileName,
                                     QWidget *widget,
                                     bool clipboard,
                                     QUrl *newUrl)
{
    QMimeDatabase db;
    QStringList formatLabels;
    formatLabels.reserve(formats.size());
    for (int i = 0; i < formats.size(); ++i) {
        const QString &fmt = formats[i];
        QMimeType mime = db.mimeTypeForName(fmt);
        if (mime.isValid()) {
            formatLabels.append(i18n("%1 (%2)", mime.comment(), fmt));
        } else {
            formatLabels.append(fmt);
        }
    }

    QString dialogText(text);
    if (dialogText.isEmpty()) {
        dialogText = i18n("Filename for clipboard content:");
    }

    KIO::PasteDialog dlg(QString(), dialogText, suggestedFileName, formatLabels, widget);

    if (dlg.exec() != QDialog::Accepted) {
        return QByteArray();
    }

    const QString chosenFormat = formats[dlg.comboItem()];
    if (clipboard && !qApp->clipboard()->mimeData()->hasFormat(chosenFormat)) {
        KMessageBox::information(widget,
                                 i18n("The clipboard has changed since you used 'paste': "
                                      "the chosen data format is no longer applicable. "
                                      "Please copy again what you wanted to paste."));
        return QByteArray();
    }

    const QString result = dlg.lineEditText();

    // qDebug() << " result=" << result << " chosenFormat=" << chosenFormat;
    *newUrl = u;
    newUrl->setPath(Utils::concatPaths(newUrl->path(), result));

    const QUrl destUrl = getDestinationUrl(u, *newUrl, widget);
    *newUrl = destUrl;

    // In Qt3, the result of clipboard()->mimeData() only existed until the next
    // event loop run (see dlg.exec() above), so we re-fetched it.
    // TODO: This should not be necessary with Qt5; remove this conditional
    // and test that it still works.
    if (clipboard) {
        mimeData = QApplication::clipboard()->mimeData();
    }
    const QByteArray ba = mimeData->data(chosenFormat);
    return ba;
}

static QStringList extractFormats(const QMimeData *mimeData)
{
    QStringList formats;
    const QStringList allFormats = mimeData->formats();
    for (const QString &format : allFormats) {
        if (format == QLatin1String("application/x-qiconlist")) { // Q3IconView and kde4's libkonq
            continue;
        }
        if (format == QLatin1String("application/x-kde-cutselection")) { // see isClipboardDataCut
            continue;
        }
        if (format == QLatin1String("application/x-kde-suggestedfilename")) {
            continue;
        }
        if (format == QLatin1String("application/x-kde-onlyReplaceEmpty")) { // Prevents emptying Klipper via selection
            continue;
        }
        if (format.startsWith(QLatin1String("application/x-qt-"))) { // Qt-internal
            continue;
        }
        if (format.startsWith(QLatin1String("x-kmail-drag/"))) { // app-internal
            continue;
        }
        if (!format.contains(QLatin1Char('/'))) { // e.g. TARGETS, MULTIPLE, TIMESTAMP
            continue;
        }
        formats.append(format);
    }
    return formats;
}

KIOWIDGETS_EXPORT bool KIO::canPasteMimeData(const QMimeData *data)
{
    return data->hasText() || !extractFormats(data).isEmpty();
}

KIO::Job *pasteMimeDataImpl(const QMimeData *mimeData, const QUrl &destUrl, const QString &dialogText, QWidget *widget, bool clipboard)
{
    QByteArray ba;
    const QString suggestedFilename = QString::fromUtf8(mimeData->data(QStringLiteral("application/x-kde-suggestedfilename")));

    // Now check for plain text
    // We don't want to display a MIME type choice for a QTextDrag, those MIME type look ugly.
    if (mimeData->hasText()) {
        ba = mimeData->text().toLocal8Bit(); // encoding OK?
    } else {
        auto formats = extractFormats(mimeData);
        const auto firstFormat = formats.value(0);
        // Remove formats that shouldn't be exposed to the user
        erase_if(formats, [](const QString &string) -> bool {
            return string.startsWith(u"application/x-kde-");
        });
        if (formats.isEmpty() && firstFormat.isEmpty()) {
            return nullptr;
        } else if (formats.size() > 1) {
            QUrl newUrl;
            ba = chooseFormatAndUrl(destUrl, mimeData, formats, dialogText, suggestedFilename, widget, clipboard, &newUrl);
            if (ba.isEmpty() || newUrl.isEmpty()) {
                return nullptr;
            }
            return putDataAsyncTo(newUrl, ba, widget, KIO::Overwrite);
        }
        ba = mimeData->data(firstFormat);
    }
    if (ba.isEmpty()) {
        return nullptr;
    }

    const QUrl newUrl = getNewFileName(destUrl, dialogText, suggestedFilename, widget);
    if (newUrl.isEmpty()) {
        return nullptr;
    }

    return putDataAsyncTo(newUrl, ba, widget, KIO::Overwrite);
}

KIOWIDGETS_EXPORT QString KIO::pasteActionText(const QMimeData *mimeData, bool *enable, const KFileItem &destItem)
{
    bool canPasteData = false;
    QList<QUrl> urls;

    // mimeData can be 0 according to https://bugs.kde.org/show_bug.cgi?id=335053
    if (mimeData) {
        canPasteData = KIO::canPasteMimeData(mimeData);
        urls = KUrlMimeData::urlsFromMimeData(mimeData);
    } else {
        qCWarning(KIO_WIDGETS) << "QApplication::clipboard()->mimeData() is 0!";
    }

    QString text;
    if (!urls.isEmpty() || canPasteData) {
        // disable the paste action if no writing is supported
        if (!destItem.isNull()) {
            if (destItem.url().isEmpty()) {
                *enable = false;
            } else {
                *enable = destItem.isWritable();
            }
        } else {
            *enable = false;
        }

        if (urls.count() == 1 && urls.first().isLocalFile()) {
            const bool isDir = QFileInfo(urls.first().toLocalFile()).isDir();
            text = isDir ? i18nc("@action:inmenu", "Paste One Folder") : i18nc("@action:inmenu", "Paste One File");
        } else if (!urls.isEmpty()) {
            text = i18ncp("@action:inmenu", "Paste One Item", "Paste %1 Items", urls.count());
        } else {
            text = i18nc("@action:inmenu", "Paste Clipboard Contents…");
        }
    } else {
        *enable = false;
        text = i18nc("@action:inmenu", "Paste");
    }
    return text;
}

KIOWIDGETS_EXPORT void KIO::setClipboardDataCut(QMimeData *mimeData, bool cut)
{
    const QByteArray cutSelectionData = cut ? "1" : "0";
    mimeData->setData(QStringLiteral("application/x-kde-cutselection"), cutSelectionData);
}

KIOWIDGETS_EXPORT bool KIO::isClipboardDataCut(const QMimeData *mimeData)
{
    const QByteArray a = mimeData->data(QStringLiteral("application/x-kde-cutselection"));
    return (!a.isEmpty() && a.at(0) == '1');
}
