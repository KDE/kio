/* This file is part of the KDE libraries
    Copyright (c) 2020 David Faure <faure@kde.org>

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

#include "kopenwithdialog.h"
#include "openurljob.h"
#include "widgetsopenwithhandler.h"

#include <KConfigGroup>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QApplication>

#ifdef Q_OS_WIN
#include "widgetsopenwithhandler_win.cpp" // displayNativeOpenWithDialog
#endif

KIO::WidgetsOpenWithHandler::WidgetsOpenWithHandler()
    : KIO::OpenWithHandlerInterface()
{
}

KIO::WidgetsOpenWithHandler::~WidgetsOpenWithHandler() = default;

void KIO::WidgetsOpenWithHandler::promptUserForApplication(KJob *job, const QList<QUrl> &urls, const QString &mimeType)
{
    QWidget *parentWidget = job ? KJobWidgets::window(job) : qApp->activeWindow();

#ifdef Q_OS_WIN
        KConfigGroup cfgGroup(KSharedConfig::openConfig(), QStringLiteral("KOpenWithDialog Settings"));
        if (cfgGroup.readEntry("Native", true)) {
            // Implemented in applicationlauncherjob_win.cpp
            if (displayNativeOpenWithDialog(urls, parentWidget)) {
                Q_EMIT handled();
                return;
            } else {
                // Some error happened with the Windows-specific code. Fallback to the KDE one...
            }
        }
#endif

    KOpenWithDialog *dialog = new KOpenWithDialog(urls, mimeType, QString(), QString(), parentWidget);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [=]() {
        KService::Ptr service = dialog->service();
        if (!service) {
            service = KService::Ptr(new KService(QString() /*name*/, dialog->text(), QString() /*icon*/));
        }
        Q_EMIT serviceSelected(service);
    });
    connect(dialog, &QDialog::rejected, this, [this]() {
        Q_EMIT canceled();
    });
    dialog->show();
}
