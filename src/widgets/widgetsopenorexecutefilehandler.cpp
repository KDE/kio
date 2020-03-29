/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "widgetsopenorexecutefilehandler.h"

#include "executablefileopendialog_p.h"

#include <KConfigGroup>
#include <KJobWidgets>
#include <KSharedConfig>

#include <QApplication>
#include <QMimeDatabase>

KIO::WidgetsOpenOrExecuteFileHandler::WidgetsOpenOrExecuteFileHandler(QObject *parent)
    : KIO::OpenOrExecuteFileInterface(parent)
{
}

KIO::WidgetsOpenOrExecuteFileHandler::~WidgetsOpenOrExecuteFileHandler() = default;

static ExecutableFileOpenDialog::Mode promptMode(const QMimeType &mime)
{
    // Note that ExecutableFileOpenDialog::OpenAsExecute isn't useful here as
    // OpenUrlJob treats .exe (application/x-ms-dos-executable) files as executables
    // that are only opened using the default application associated with that MIME type
    // e.g. WINE

    if (mime.inherits(QStringLiteral("text/plain"))) {
        return ExecutableFileOpenDialog::OpenOrExecute;
    }
    return ExecutableFileOpenDialog::OnlyExecute;
}

void KIO::WidgetsOpenOrExecuteFileHandler::promptUserOpenOrExecute(KJob *job, const QString &mimetype)
{
    KConfigGroup cfgGroup(KSharedConfig::openConfig(QStringLiteral("kiorc")), "Executable scripts");
    const QString value = cfgGroup.readEntry("behaviourOnLaunch", "alwaysAsk");

    if (value != QLatin1String("alwaysAsk")) {
        Q_EMIT executeFile(value == QLatin1String("execute"));
        return;
    }

    QWidget *parentWidget = job ? KJobWidgets::window(job) : qApp->activeWindow();

    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForName(mimetype);

    ExecutableFileOpenDialog *dialog = new ExecutableFileOpenDialog(promptMode(mime), parentWidget);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &QDialog::finished, this, [this, dialog, mime](const int result) {
        if (result == ExecutableFileOpenDialog::Rejected) {
            Q_EMIT canceled();
            return;
        }

        const bool isExecute = result == ExecutableFileOpenDialog::ExecuteFile;
        Q_EMIT executeFile(isExecute);

        if (dialog->isDontAskAgainChecked()) {
            KConfigGroup cfgGroup(KSharedConfig::openConfig(QStringLiteral("kiorc")), "Executable scripts");
            cfgGroup.writeEntry("behaviourOnLaunch", isExecute ? "execute" : "open");
        }
    });

    dialog->show();
}
