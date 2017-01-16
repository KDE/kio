/* This file is part of the KDE libraries
    Copyright (C) 2014 Arjun A.K. <arjunak234@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
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
        OpenFile = 42, ExecuteFile
    };

    explicit ExecutableFileOpenDialog(QWidget* parent = nullptr);

    bool isDontAskAgainChecked() const;

private:
    QCheckBox *m_dontAskAgain;
};

#endif // EXECUTABLEFILEOPENDIALOG_H
