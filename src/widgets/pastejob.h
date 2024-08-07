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
class CopyJob;
class PasteJobPrivate;
/*!
 * \class KIO::PasteJob
 * \inheaderfile KIO/PasteJob
 * \inmodule KIOWidgets
 *
 * \brief A KIO job that handles pasting the clipboard contents.
 *
 * If the clipboard contains URLs, they are copied to the destination URL.
 * If the clipboard contains data, it is saved into a file after asking
 * the user to choose a filename and the preferred data format.
 *
 * \sa KIO::paste
 * \since 5.4
 */
class KIOWIDGETS_EXPORT PasteJob : public Job
{
    Q_OBJECT

public:
    ~PasteJob() override;

Q_SIGNALS:
    /*!
     * Signals that a file or directory was created.
     */
    void itemCreated(const QUrl &url);

    /*!
     * Emitted when a copy job was started as subjob as part of pasting. Note that a
     * CopyJob isn't always started by PasteJob. For instance pasting image content will create a file.
     *
     * You can use \a job to monitor the progress of the copy/move/link operation.
     *
     * \a job the job started for moving, copying or symlinking files
     * \since 6.0
     */
    void copyJobStarted(KIO::CopyJob *job);

protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    KIOWIDGETS_NO_EXPORT explicit PasteJob(PasteJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(PasteJob)
};

/*!
 * \relates KIO::PasteJob
 *
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
 * \a mimeData the MIME data to paste, usually QApplication::clipboard()->mimeData()
 *
 * \a destDir The URL of the target directory
 *
 * \a flags passed to the sub job
 *
 * Returns a pointer to the job handling the operation.
 * \since 5.4
 */
KIOWIDGETS_EXPORT PasteJob *paste(const QMimeData *mimeData, const QUrl &destDir, JobFlags flags = DefaultFlags);

}

#endif
