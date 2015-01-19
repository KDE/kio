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

ExecutableFileOpenDialog::ExecutableFileOpenDialog(QWidget *parent) :
    QDialog(parent)
{
    QLabel *label = new QLabel(i18n("What do you wish to do with this executable file?"), this);

    m_dontAskAgain = new QCheckBox(this);
    m_dontAskAgain->setText(i18n("Do not ask again"));

    QPushButton *openButton = new QPushButton(i18n("&Open"), this);
    QPushButton *executeButton = new QPushButton(i18n("&Execute"), this);

    openButton->setIcon(QIcon::fromTheme("text-plain"));
    executeButton->setIcon(QIcon::fromTheme("application-x-executable"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttonBox->addButton(openButton, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(executeButton, QDialogButtonBox::AcceptRole);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addWidget(m_dontAskAgain);
    layout->addWidget(buttonBox);
    setLayout(layout);

    connect(openButton, &QPushButton::clicked, [=]{done(OpenFile);});
    connect(executeButton, &QPushButton::clicked, [=]{done(ExecuteFile);});
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ExecutableFileOpenDialog::reject);
}

bool ExecutableFileOpenDialog::isDontAskAgainChecked() const
{
    return m_dontAskAgain->isChecked();
}

