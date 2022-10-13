/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2022 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef DELETEORTRASHJOB_H
#define DELETEORTRASHJOB_H

#include <KIO/AskUserActionInterface>
#include <kiowidgets_export.h>

#include <KCompositeJob>

#include <memory>

namespace KIO
{

class DeleteOrTrashJobPrivate;

/**
 * @class DeleteOrTrashJob deleteortrashjob.h <KIO/DeleteOrTrashJob>
 *
 * This job asks the user for confirmation to delete or move to Trash
 * a list of URLs; or if the job is constructed with
 * AskUserActionInterface::EmptyTrash, to empty the Trash.
 *
 * A KIO::WidgetAskUserActionHandler will be used by default, unless a
 * KJobUiDelegate that implements KIO::AskUserActionInterface is set with
 * setUiDelegate().
 *
 * In the case of moving items to Trash, this job records the
 * operation using KIO::FileUndoManager.
 *
 * To start the job after constructing it, you must call start().
 *
 * @since 5.100
 */
class KIOWIDGETS_EXPORT DeleteOrTrashJob : public KCompositeJob
{
    Q_OBJECT
public:
    /**
     * Creates a DeleteOrTrashJob.
     * @param urls the list of urls to delete, move to Trash, or an empty list
     * in the case of AskUserActionInterface::EmptyTrash (in the latter case,
     * the list of urls is ignored)
     * @param deletionType one of AskUserActionInterface::DeletionType
     * @param confirm one of AskUserActionInterface::ConfirmationType
     * @param parent parent object, e.g. a QWidget for widget-based applications
     */
    explicit DeleteOrTrashJob(const QList<QUrl> &urls,
                              AskUserActionInterface::DeletionType deletionType,
                              AskUserActionInterface::ConfirmationType confirm,
                              QObject *parent);

    /**
     * Destructor
     *
     * Note that jobs auto-delete themselves after emitting result
     */
    ~DeleteOrTrashJob() override;

    /**
     * You must call this to actually start the job.
     */
    void start() override;

private:
    void slotResult(KJob *job) override;

    friend DeleteOrTrashJobPrivate;
    std::unique_ptr<DeleteOrTrashJobPrivate> d;
};

} // namespace KIO

#endif // DELETEORTRASHJOB_H
