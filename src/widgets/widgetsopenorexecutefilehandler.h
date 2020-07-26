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

#ifndef WIDGETSOPENOREXECUTEFILEHANDLER_H
#define WIDGETSOPENOREXECUTEFILEHANDLER_H

#include "openorexecutefileinterface.h"

namespace KIO {

// TODO KF6: Make KIO::JobUiDelegate inherit from WidgetsOpenOrExecuteFileHandler
// (or even merge the two classes)
// so that setDelegate(new KIO::JobUiDelegate) invokes the dialog boxes on error
// and when showing ExecutableFileOpenDialog.

class WidgetsOpenOrExecuteFileHandler : public OpenOrExecuteFileInterface
{
public:
    WidgetsOpenOrExecuteFileHandler();
    ~WidgetsOpenOrExecuteFileHandler() override;

    void promptUserOpenOrExecute(KJob *job, const QString &mimetype) override;

private:
    // Note: no d pointer because it's not exported at this point
};

}

#endif // WIDGETSOPENOREXECUTEFILEHANDLER_H
