/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 Arjun A.K. <arjunak234@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef EXECUTABLEFILEOPENDIALOG_H
#define EXECUTABLEFILEOPENDIALOG_H

#include <QDialog>

class QCheckBox;

/**
 * @brief Dialog shown when opening an executable file
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

    explicit ExecutableFileOpenDialog(Mode mode, QWidget* parent = nullptr);
    explicit ExecutableFileOpenDialog(QWidget* parent = nullptr);

    bool isDontAskAgainChecked() const;

private:
    QCheckBox *m_dontAskAgain;
};

#endif // EXECUTABLEFILEOPENDIALOG_H
