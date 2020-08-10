/*
    SPDX-FileCopyrightText: 2001 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef USERAGENTSELECTORDLG_H
#define USERAGENTSELECTORDLG_H

// KDE
#include <QDialog>

#include "ui_useragentselectordlg.h"

// Forward declarations
class QDialogButtonBox;
class UserAgentInfo;

class UserAgentSelectorDlg : public QDialog
{
    Q_OBJECT

public:
    explicit UserAgentSelectorDlg(UserAgentInfo* info, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~UserAgentSelectorDlg();

    void setSiteName (const QString&);
    void setIdentity (const QString&);

    QString siteName();
    QString identity();
    QString alias();

protected Q_SLOTS:
    void onHostNameChanged (const QString&);
    void onAliasChanged (const QString&);

private:
    UserAgentInfo* mUserAgentInfo;
    Ui::UserAgentSelectorUI mUi;
    QDialogButtonBox *mButtonBox;
};

#endif // USERAGENTSELECTORDLG_H
