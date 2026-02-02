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
/*!
 * Special flag of DropJob in addition to KIO::JobFlag
 *
 * \value DropJobDefaultFlags
 * \value ShowMenuManually Show the menu manually with DropJob::showMenu
 * \value[since 6.23] ExcludePluginsActions Exclude plugins actions from drop popup menu
 *
 * \since 5.67
 */
enum DropJobFlag {
    DropJobDefaultFlags = 0,
    ShowMenuManually = 1,
    ExcludePluginsActions = 2,
};
Q_DECLARE_FLAGS(DropJobFlags, DropJobFlag)

/*!
 * Setting flag to determine what the default behaviour should be when dropping items.
 *
 * \value AlwaysAsk
 * \value MoveIfSameDevice Move the dragged items without showing the options menu they are on the same device
 *
 * \since 6.14
 */
Q_NAMESPACE
enum DndBehavior : std::uint8_t {
    AlwaysAsk = 0,
    MoveIfSameDevice = 1,
};
Q_ENUM_NS(DndBehavior)
Q_DECLARE_OPERATORS_FOR_FLAGS(DropJobFlags)

class CopyJob;
class DropJobPrivate;

/*!
 * \class KIO::DropJob
 * \inheaderfile KIO/DropJob
 * \inmodule KIOWidgets
 *
 * \brief A KIO job that handles dropping into a file-manager-like view.
 * \sa KIO::drop
 *
 * The popupmenu that can appear on drop, can be customized with plugins,
 * see KIO::DndPopupMenuPlugin.
 *
 * \since 5.6
 */
class KIOWIDGETS_EXPORT DropJob : public Job
{
    Q_OBJECT

public:
    ~DropJob() override;

    /*!
     * Allows the application to set additional actions in the drop popup menu.
     * For instance, the application handling the desktop might want to add
     * "set as wallpaper" if the dropped url is an image file.
     * This can be called upfront, or for convenience, when popupMenuAboutToShow is emitted.
     */
    void setApplicationActions(const QList<QAction *> &actions);

    /*!
     * Allows the application to show the menu manually.
     * DropJob instance has to be created with the KIO::ShowMenuManually flag
     *
     * \since 5.67
     */
    void showMenu(const QPoint &p, QAction *atAction = nullptr);

Q_SIGNALS:
    /*!
     * Signals that a file or directory was created.
     */
    void itemCreated(const QUrl &url);

    /*!
     * Emitted when a copy job was started as subjob after user selection.
     *
     * You can use \a job to monitor the progress of the copy/move/link operation. Note that a
     * CopyJob isn't always started by DropJob. For instance dropping files onto an executable will
     * simply launch the executable.
     *
     * \a job the job started for moving, copying or symlinking files
     * \since 5.30
     */
    void copyJobStarted(KIO::CopyJob *job);

    /*!
     * Signals that the popup menu is about to be shown.
     * Applications can use the information provided about the dropped URLs
     * (e.g. the MIME type) to decide whether to call setApplicationActions.
     *
     * \a itemProps properties of the dropped items
     */
    void popupMenuAboutToShow(const KFileItemListProperties &itemProps);

protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    KIOWIDGETS_NO_EXPORT explicit DropJob(DropJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(DropJob)
};

/*!
 * \relates KIO::DropJob
 *
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
 * \a dropEvent the drop event, from which the job will extract mimeData, dropAction, etc.
 *        The application should take care of calling dropEvent->acceptProposedAction().
 *
 * \a destUrl The URL of the target file or directory
 *
 * \a flags passed to the sub job
 *
 * Returns A pointer to the job handling the operation.
 *
 * \warning Don't forget to call KJobWidgets::setWindow() on this job, otherwise the popup
 *          menu won't be properly positioned with Wayland compositors.
 * \since 5.4
 */
KIOWIDGETS_EXPORT DropJob *drop(const QDropEvent *dropEvent, const QUrl &destUrl, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::DropJob
 *
 * Similar to KIO::drop
 *
 * \a dropEvent the drop event, from which the job will extract mimeData, dropAction, etc.
 *        The application should take care of calling dropEvent->acceptProposedAction().
 *
 * \a destUrl The URL of the target file or directory
 *
 * \a dropjobFlags Show the menu immediately or manually.
 *
 * \a flags passed to the sub job
 *
 * Returns A pointer to the job handling the operation.
 * \warning Don't forget to call DropJob::showMenu on this job, otherwise the popup will never be shown
 *
 * \since 5.67
 */
KIOWIDGETS_EXPORT DropJob *drop(const QDropEvent *dropEvent,
                                const QUrl &destUrl,
                                DropJobFlags dropjobFlags,
                                JobFlags flags = DefaultFlags); // TODO KF6: merge with DropJobFlags dropjobFlag = DropJobDefaultFlags

}

#endif
