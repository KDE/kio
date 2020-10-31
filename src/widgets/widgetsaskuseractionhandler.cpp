/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "widgetsaskuseractionhandler.h"

#include <KJob>
#include <KJobWidgets>

#include <QUrl>

KIO::WidgetsAskUserActionHandler::WidgetsAskUserActionHandler()
    : KIO::AskUserActionInterface()
{
}

KIO::WidgetsAskUserActionHandler::~WidgetsAskUserActionHandler()
{
}

void KIO::WidgetsAskUserActionHandler::askUserRename(KJob *job,
                                                     const QString &caption,
                                                     const QUrl &src,
                                                     const QUrl &dest,
                                                     KIO::RenameDialog_Options options,
                                                     KIO::filesize_t sizeSrc,
                                                     KIO::filesize_t sizeDest,
                                                     const QDateTime &ctimeSrc,
                                                     const QDateTime &ctimeDest,
                                                     const QDateTime &mtimeSrc,
                                                     const QDateTime &mtimeDest)
{
    auto *dlg = new KIO::RenameDialog(KJobWidgets::window(job), caption,
                                      src, dest, options,
                                      sizeSrc, sizeDest,
                                      ctimeSrc, ctimeDest,
                                      mtimeSrc, mtimeDest);

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowModality(Qt::WindowModal);

    connect(job, &KJob::finished, dlg, &QDialog::reject);
    connect(dlg, &QDialog::finished, this, [this, job, dlg](const int exitCode) {
        KIO::RenameDialog_Result result = static_cast<RenameDialog_Result>(exitCode);
        const QUrl newUrl = result == Result_AutoRename ? dlg->autoDestUrl() : dlg->newDestUrl();
        emit askUserRenameResult(result, newUrl, job);
    });

    dlg->show();
}

void KIO::WidgetsAskUserActionHandler::askUserSkip(KJob *job,
                                                   KIO::SkipDialog_Options options,
                                                   const QString &errorText)
{
    auto *dlg = new KIO::SkipDialog(KJobWidgets::window(job), options, errorText);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowModality(Qt::WindowModal);

    connect(job, &KJob::finished, dlg, &QDialog::reject);
    connect(dlg, &QDialog::finished, this, [this, job](const int exitCode) {
        emit askUserSkipResult(static_cast<KIO::SkipDialog_Result>(exitCode), job);
    });

    dlg->show();
}
