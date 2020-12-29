/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef WIDGETSASKUSERACTIONHANDLER_H
#define WIDGETSASKUSERACTIONHANDLER_H

#include "kiowidgets_export.h"
#include <kio/jobuidelegateextension.h>
#include <kio/askuseractioninterface.h>
#include <kio/skipdialog.h>
#include <kio/renamedialog.h>
#include <kio/global.h>

namespace KIO {
// TODO KF6: Handle this the same way we end up handling WidgetsUntrustedProgramHandler.

/**
 * @class KIO::WidgetsAskUserActionHandler widgetsaskuseractionhandler.h <KIO/WidgetsAskUserActionHandler>
 *
 * This implements KIO::AskUserActionInterface.
 * @see KIO::AskUserActionInterface()
 *
 * @sa KIO::JobUiDelegateExtension()
 *
 * @since 5.78
 */

class WidgetsAskUserActionHandlerPrivate;

class KIOWIDGETS_EXPORT WidgetsAskUserActionHandler : public AskUserActionInterface
{
public:
    explicit WidgetsAskUserActionHandler(QObject *parent = nullptr);

    ~WidgetsAskUserActionHandler() override;

    /**
     * @copydoc KIO::AskUserActionInterface::askUserRename()
     */
    void askUserRename(KJob *job,
                       const QString &caption,
                       const QUrl &src,
                       const QUrl &dest,
                       KIO::RenameDialog_Options options,
                       KIO::filesize_t sizeSrc = KIO::filesize_t(-1),
                       KIO::filesize_t sizeDest = KIO::filesize_t(-1),
                       const QDateTime &ctimeSrc = QDateTime(),
                       const QDateTime &ctimeDest = QDateTime(),
                       const QDateTime &mtimeSrc = QDateTime(),
                       const QDateTime &mtimeDest = QDateTime()) override;

    /**
     * @copydoc KIO::AskUserActionInterface::askUserSkip()
     */
    void askUserSkip(KJob *job,
                     KIO::SkipDialog_Options options,
                     const QString &error_text) override;

    /**
     * @copydoc KIO::AskUserActionInterface::askUserDelete()
     */
    void askUserDelete(const QList<QUrl> &urls,
                       DeletionType deletionType,
                       ConfirmationType confirmationType,
                       QWidget *parent = nullptr) override;

    /**
     * @copydoc KIO::AskUserActionInterface::requestUserMessageBox()
     */
    void requestUserMessageBox(MessageDialogType type,
                               const QString &text,
                               const QString &caption,
                               const QString &buttonYes,
                               const QString &buttonNo,
                               const QString &iconYes = QString(),
                               const QString &iconNo = QString(),
                               const QString &dontAskAgainName = QString(),
                               const QString &details = QString(),
                               const KIO::MetaData &metaData = KIO::MetaData(),
                               QWidget *parent = nullptr) override;

private:
    std::unique_ptr<WidgetsAskUserActionHandlerPrivate> d;
};

} // namespace KIO

#endif // WIDGETSASKUSERACTIONHANDLER_H
