/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 Arjun A.K. <arjunak234@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef EXECUTABLEFILEOPENDIALOG_H
#define EXECUTABLEFILEOPENDIALOG_H

#include <QDialog>

#include "ui_executablefileopendialog.h"

class QMimeType;

/*!
 * Dialog shown when opening an executable file
 *
 * \internal
 */
class ExecutableFileOpenDialog : public QDialog
{
    Q_OBJECT

public:
    enum ReturnCode {
        OpenFile = 42,
        ExecuteFile,
    };

    enum Mode {
        // For executable scripts
        OpenOrExecute,
        // For native binary executables
        OnlyExecute,
        // For *.exe files, open with WINE is like execute the file
        // In this case, openAsExecute is true, we hide "Open" button and connect
        // "Execute" button to OpenFile action.
        OpenAsExecute,
    };

    explicit ExecutableFileOpenDialog(const QUrl &url, const QMimeType &mimeType, Mode mode, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void executeFile();
    void openFile();

    Ui_ExecutableFileOpenDialog m_ui;
};

#endif // EXECUTABLEFILEOPENDIALOG_H
