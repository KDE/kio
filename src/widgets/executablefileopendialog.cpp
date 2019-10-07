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

#include "executablefileopendialog_p.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QVBoxLayout>

#include <KLocalizedString>

ExecutableFileOpenDialog::ExecutableFileOpenDialog(ExecutableFileOpenDialog::Mode mode, QWidget *parent) :
    QDialog(parent)
{
    QLabel *label = new QLabel(i18n("What do you wish to do with this executable file?"), this);

    m_dontAskAgain = new QCheckBox(this);
    m_dontAskAgain->setText(i18n("Do not ask again"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);

    QPushButton *openButton;
    if (mode == OpenOrExecute) {
        openButton = new QPushButton(i18n("&Open"), this);
        openButton->setIcon(QIcon::fromTheme(QStringLiteral("document-preview")));
        buttonBox->addButton(openButton, QDialogButtonBox::AcceptRole);
    }

    QPushButton *executeButton = new QPushButton(i18n("&Execute"), this);
    executeButton->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
    buttonBox->addButton(executeButton, QDialogButtonBox::AcceptRole);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addWidget(m_dontAskAgain);
    layout->addWidget(buttonBox);
    setLayout(layout);


    if (mode == OnlyExecute) {
        connect(executeButton, &QPushButton::clicked, [=]{done(ExecuteFile);});
    } else if (mode == OpenAsExecute) {
        connect(executeButton, &QPushButton::clicked, [=]{done(OpenFile);});
    } else {
        connect(openButton, &QPushButton::clicked, [=]{done(OpenFile);});
        connect(executeButton, &QPushButton::clicked, [=]{done(ExecuteFile);});
    }
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ExecutableFileOpenDialog::reject);
}

ExecutableFileOpenDialog::ExecutableFileOpenDialog(QWidget *parent) :
    ExecutableFileOpenDialog(ExecutableFileOpenDialog::OpenOrExecute, parent) { }

bool ExecutableFileOpenDialog::isDontAskAgainChecked() const
{
    return m_dontAskAgain->isChecked();
}

