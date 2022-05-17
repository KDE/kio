/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef WIDGETSOPENWITHHANDLER_H
#define WIDGETSOPENWITHHANDLER_H

#include "openwithhandlerinterface.h"

class QDialog;
class QWidget;

namespace KIO
{
// TODO KF6: Make KIO::JobUiDelegate inherit from WidgetsOpenWithHandler
// (or even merge the two classes)
// so that setDelegate(new KIO::JobUiDelegate) provides both dialog boxes on error
// and the open-with dialog.

class WidgetsOpenWithHandler : public OpenWithHandlerInterface
{
    Q_OBJECT
public:
    explicit WidgetsOpenWithHandler(QObject *parent = nullptr);
    ~WidgetsOpenWithHandler() override;

    void promptUserForApplication(KJob *job, const QList<QUrl> &urls, const QString &mimeType) override;

    void setWindow(QWidget *widget);

private:
    // Note: no d pointer because not exported at this point
    QWidget *m_parentWidget = nullptr;
};

}

#endif // WIDGETSOPENWITHHANDLER_H
