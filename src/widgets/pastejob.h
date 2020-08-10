/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef PASTEJOB_H
#define PASTEJOB_H

#include <QUrl>

#include "kiowidgets_export.h"
#include <kio/job_base.h>

class QMimeData;

namespace KIO
{

class PasteJobPrivate;
/**
 * @class KIO::PasteJob pastejob.h <KIO/PasteJob>
 *
 * A KIO job that handles pasting the clipboard contents.
 *
 * If the clipboard contains URLs, they are copied to the destination URL.
 * If the clipboard contains data, it is saved into a file after asking
 * the user to choose a filename and the preferred data format.
 *
 * @see KIO::pasteClipboard
 * @since 5.4
 */
class KIOWIDGETS_EXPORT PasteJob : public Job
{
    Q_OBJECT

public:
    virtual ~PasteJob();

Q_SIGNALS:
    /**
     * Signals that a file or directory was created.
     */
    void itemCreated(const QUrl &url);

protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    PasteJob(PasteJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(PasteJob)
    Q_PRIVATE_SLOT(d_func(), void slotStart())
    Q_PRIVATE_SLOT(d_func(), void slotCopyingDone(KIO::Job*, const QUrl &, const QUrl &to))
    Q_PRIVATE_SLOT(d_func(), void slotCopyingLinkDone(KIO::Job*, const QUrl &, const QString &, const QUrl &to))
};

/**
 * Pastes the clipboard contents.
 *
 * If the clipboard contains URLs, they are copied (or moved) to the destination URL,
 * using a KIO::CopyJob subjob.
 * Otherwise, the data from the clipboard is saved into a file using KIO::storedPut,
 * after asking the user to choose a filename and the preferred data format.
 *
 * This takes care of recording the subjob in the FileUndoManager, and emits
 * itemCreated for every file or directory being created, so that the view can select
 * these items.
 *
 * @param mimeData the MIME data to paste, usually QApplication::clipboard()->mimeData()
 * @param destDir The URL of the target directory
 * @param flags passed to the sub job
 *
 * @return A pointer to the job handling the operation.
 * @since 5.4
 */
KIOWIDGETS_EXPORT PasteJob *paste(const QMimeData *mimeData, const QUrl &destDir, JobFlags flags = DefaultFlags);

}

#endif
