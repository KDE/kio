/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef WIDGETSUNTRUSTEDPROGRAMHANDLER_H
#define WIDGETSUNTRUSTEDPROGRAMHANDLER_H

#include "untrustedprogramhandlerinterface.h"

#include <memory>

class QDialog;
class QWidget;

namespace KIO
{
// TODO KF6: Make KIO::JobUiDelegate inherit from WidgetsUntrustedProgramHandler
// (or even merge the two classes)
// so that setDelegate(new KIO::JobUiDelegate) provides both dialog boxes on error
// and the messagebox for handling untrusted programs.
// Then port those users of ApplicationLauncherJob which were setting a KDialogJobUiDelegate
// to set a KIO::JobUiDelegate instead.
class WidgetsUntrustedProgramHandlerPrivate;

class WidgetsUntrustedProgramHandler : public UntrustedProgramHandlerInterface
{
    Q_OBJECT
public:
    explicit WidgetsUntrustedProgramHandler(QObject *parent = nullptr);
    ~WidgetsUntrustedProgramHandler() override;

    void showUntrustedProgramWarning(KJob *job, const QString &programName) override;

    // Compat code for KRun::runUrl. Will disappear before KF6
    bool execUntrustedProgramWarning(QWidget *window, const QString &programName);

    void setWindow(QWidget *window);

private:
    QDialog *createDialog(QWidget *parentWidget, const QString &programName);

    std::unique_ptr<WidgetsUntrustedProgramHandlerPrivate> d;
};

}

#endif // WIDGETSUNTRUSTEDPROGRAMHANDLER_H
