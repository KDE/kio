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

#ifndef WIDGETSUNTRUSTEDPROGRAMHANDLER_H
#define WIDGETSUNTRUSTEDPROGRAMHANDLER_H

#include "untrustedprogramhandlerinterface.h"

class QDialog;
class QWidget;

namespace KIO {

// TODO KF6: Make KIO::JobUiDelegate inherit from WidgetsUntrustedProgramHandler
// (or even merge the two classes)
// so that setDelegate(new KIO::JobUiDelegate) provides both dialog boxes on error
// and the messagebox for handling untrusted programs.
// Then port those users of ApplicationLauncherJob which were setting a KDialogJobUiDelegate
// to set a KIO::JobUiDelegate instead.

class WidgetsUntrustedProgramHandler : public UntrustedProgramHandlerInterface
{
public:
    WidgetsUntrustedProgramHandler();
    ~WidgetsUntrustedProgramHandler() override;

    void showUntrustedProgramWarning(KJob *job, const QString &programName) override;

    // Compat code for KRun::runUrl. Will disappear before KF6
    bool execUntrustedProgramWarning(QWidget *window, const QString &programName);

private:
    QDialog *createDialog(QWidget *parentWidget, const QString &programName);

    class Private;
    Private *const d;
};

}

#endif // WIDGETSUNTRUSTEDPROGRAMHANDLER_H
