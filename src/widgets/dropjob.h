/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef DROPJOB_H
#define DROPJOB_H

#include <QUrl>

#include "kiowidgets_export.h"
#include <kio/job_base.h>

class QAction;
class QDropEvent;
class KFileItemListProperties;

namespace KIO
{

/**
* Special flag of DropJob in addition to KIO::JobFlag
*
* @see DropJobFlags
* @since 5.67
*/
enum DropJobFlag
{
    DropJobDefaultFlags = 0,
    ShowMenuManually = 1, ///< show the menu manually with DropJob::showMenu
};
/**
 * Stores a combination of #DropJobFlag values.
 */
Q_DECLARE_FLAGS(DropJobFlags, DropJobFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(DropJobFlags)

class CopyJob;
class DropJobPrivate;

/**
 * @class KIO::DropJob dropjob.h <KIO/DropJob>
 *
 * A KIO job that handles dropping into a file-manager-like view.
 * @see KIO::drop
 *
 * The popupmenu that can appear on drop, can be customized with plugins,
 * see KIO::DndPopupMenuPlugin.
 *
 * @since 5.6
 */
class KIOWIDGETS_EXPORT DropJob : public Job
{
    Q_OBJECT

public:
    virtual ~DropJob();

    /**
     * Allows the application to set additional actions in the drop popup menu.
     * For instance, the application handling the desktop might want to add
     * "set as wallpaper" if the dropped url is an image file.
     * This can be called upfront, or for convenience, when popupMenuAboutToShow is emitted.
     */
    void setApplicationActions(const QList<QAction *> &actions);

    /**
     * Allows the application to show the menu manually.
     * DropJob instance has to be created with the KIO::ShowMenuManually flag
     *
     * @since 5.67
     */
    void showMenu(const QPoint &p, QAction *atAction = nullptr);

Q_SIGNALS:
    /**
     * Signals that a file or directory was created.
     */
    void itemCreated(const QUrl &url);

    /**
    * Emitted when a copy job was started as subjob after user selection.
    *
    * You can use @p job to monitor the progress of the copy/move/link operation. Note that a
    * CopyJob isn't always started by DropJob. For instance dropping files onto an executable will
    * simply launch the executable.
    *
    * @param job the job started for moving, copying or symlinking files
    * @since 5.30
    */
    void copyJobStarted(KIO::CopyJob *job);

    /**
     * Signals that the popup menu is about to be shown.
     * Applications can use the information provided about the dropped URLs
     * (e.g. the MIME type) to decide whether to call setApplicationActions.
     * @param itemProps properties of the dropped items
     */
    void popupMenuAboutToShow(const KFileItemListProperties &itemProps);

protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    DropJob(DropJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(DropJob)
    Q_PRIVATE_SLOT(d_func(), void slotStart())
};

/**
 * Drops the clipboard contents.
 *
 * If the mime data contains URLs, a popup appears to choose between
 *  Move, Copy, Link and Cancel
 * which is then implemented by the job, using KIO::move, KIO::copy or KIO::link
 * Additional actions provided by the application or by plugins can be shown in the popup.
 *
 * If the mime data contains data other than URLs, it is saved into a file after asking
 * the user to choose a filename and the preferred data format.
 *
 * This job takes care of recording the subjob in the FileUndoManager, and emits
 * itemCreated for every file or directory being created, so that the view can select
 * these items.
 *
 * @param dropEvent the drop event, from which the job will extract mimeData, dropAction, etc.
         The application should take care of calling dropEvent->acceptProposedAction().
 * @param destUrl The URL of the target file or directory
 * @param flags passed to the sub job
 *
 * @return A pointer to the job handling the operation.
 * @warning Don't forget to call KJobWidgets::setWindow() on this job, otherwise the popup
 *          menu won't be properly positioned with Wayland compositors.
 * @since 5.4
 */
KIOWIDGETS_EXPORT DropJob *drop(const QDropEvent *dropEvent, const QUrl &destUrl, JobFlags flags = DefaultFlags);

/**
 * Similar to KIO::drop
 *
 * @param dropEvent the drop event, from which the job will extract mimeData, dropAction, etc.
         The application should take care of calling dropEvent->acceptProposedAction().
 * @param destUrl The URL of the target file or directory
 * @param dropjobFlags Show the menu immediately or manually.
 * @param flags passed to the sub job
 *
 * @return A pointer to the job handling the operation.
 * @warning Don't forget to call DropJob::showMenu on this job, otherwise the popup will never be shown
 *
 * @since 5.67
 */
KIOWIDGETS_EXPORT DropJob *drop(const QDropEvent *dropEvent, const QUrl &destUrl, DropJobFlags dropjobFlags, JobFlags flags = DefaultFlags); // TODO KF6: merge with DropJobFlags dropjobFlag = DropJobDefaultFlags

}

#endif
