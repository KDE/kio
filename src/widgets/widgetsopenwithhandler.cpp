/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
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

KIO::WidgetsOpenWithHandler::WidgetsOpenWithHandler(QObject *parent)
    : KIO::OpenWithHandlerInterface(parent)
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
