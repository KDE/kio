/* This file is part of the KDE libraries

    Copyright (c) 2020 Ahmad Samir <a.samirh78@gmail.com>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "widgetsopenorexecutefilehandler.h"

#include "executablefileopendialog_p.h"

#include <KConfigGroup>
#include <KJobWidgets>
#include <KSharedConfig>

#include <QApplication>
#include <QMimeDatabase>

KIO::WidgetsOpenOrExecuteFileHandler::WidgetsOpenOrExecuteFileHandler()
    : KIO::OpenOrExecuteFileInterface()
{
}

KIO::WidgetsOpenOrExecuteFileHandler::~WidgetsOpenOrExecuteFileHandler() = default;

static ExecutableFileOpenDialog::Mode promptMode(const QMimeType &mime)
{
    // Note that ExecutableFileOpenDialog::OpenAsExecute isn't useful here as
    // OpenUrlJob treats .exe (application/x-ms-dos-executable) files as executables
    // that are only opened using the default application associated with that mime type
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
