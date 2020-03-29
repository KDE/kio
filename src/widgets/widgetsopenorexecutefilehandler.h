/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
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
    explicit WidgetsOpenOrExecuteFileHandler(QObject *parent = nullptr);
    ~WidgetsOpenOrExecuteFileHandler() override;

    void promptUserOpenOrExecute(KJob *job, const QString &mimetype) override;

private:
    // Note: no d pointer because it's not exported at this point
};

}

#endif // WIDGETSOPENOREXECUTEFILEHANDLER_H
