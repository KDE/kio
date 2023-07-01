/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 Arjun A.K. <arjunak234@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "executablefileopendialog_p.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <KLocalizedString>

ExecutableFileOpenDialog::ExecutableFileOpenDialog(ExecutableFileOpenDialog::Mode mode, QWidget *parent)
    : QDialog(parent)
{
    QLabel *label = new QLabel(i18n("What do you wish to do with this file?"), this);

    m_dontAskAgain = new QCheckBox(this);
    m_dontAskAgain->setText(i18n("Do not ask again"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ExecutableFileOpenDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addWidget(m_dontAskAgain);
    layout->addWidget(buttonBox);

    QPushButton *executeButton = new QPushButton(i18n("&Execute"), this);
    executeButton->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));

    if (mode == OnlyExecute) {
        connect(executeButton, &QPushButton::clicked, this, &ExecutableFileOpenDialog::executeFile);
    } else if (mode == OpenAsExecute) {
        connect(executeButton, &QPushButton::clicked, this, &ExecutableFileOpenDialog::openFile);
    } else { // mode == OpenOrExecute
        connect(executeButton, &QPushButton::clicked, this, &ExecutableFileOpenDialog::executeFile);

        QPushButton *openButton = new QPushButton(i18n("&Open"), this);
        openButton->setIcon(QIcon::fromTheme(QStringLiteral("document-preview")));
        buttonBox->addButton(openButton, QDialogButtonBox::AcceptRole);

        connect(openButton, &QPushButton::clicked, this, &ExecutableFileOpenDialog::openFile);
    }

    // Add Execute button last so that Open is first in the button box
    buttonBox->addButton(executeButton, QDialogButtonBox::AcceptRole);
    buttonBox->button(QDialogButtonBox::Cancel)->setFocus();
}

ExecutableFileOpenDialog::ExecutableFileOpenDialog(QWidget *parent)
    : ExecutableFileOpenDialog(ExecutableFileOpenDialog::OpenOrExecute, parent)
{
}

bool ExecutableFileOpenDialog::isDontAskAgainChecked() const
{
    return m_dontAskAgain->isChecked();
}

void ExecutableFileOpenDialog::executeFile()
{
    done(ExecuteFile);
}

void ExecutableFileOpenDialog::openFile()
{
    done(OpenFile);
}

#include "moc_executablefileopendialog_p.cpp"
