/* This file is part of the KDE libraries
   Copyright (C) 2000 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kio/skipdialog.h"

#include <stdio.h>
#include <assert.h>

#include <QDialogButtonBox>
#include <QPushButton>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

#include <KLocalizedString>
#include <kio/jobuidelegateextension.h>

using namespace KIO;

SkipDialog::SkipDialog(QWidget *parent, KIO::SkipDialog_Options options, const QString &_error_text)
    : QDialog(parent), d(nullptr)
{
    setWindowTitle(i18n("Information"));

    QVBoxLayout *layout = new QVBoxLayout;
    setLayout(layout);

    layout->addWidget(new QLabel(_error_text, this));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    layout->addWidget(buttonBox);

    QPushButton *retryButton = new QPushButton(i18n("Retry"));
    connect(retryButton, &QAbstractButton::clicked, this, &SkipDialog::retryPressed);
    buttonBox->addButton(retryButton, QDialogButtonBox::ActionRole);

    if (options & SkipDialog_MultipleItems) {
        QPushButton *skipButton = new QPushButton(i18n("Skip"));
        connect(skipButton, &QAbstractButton::clicked, this, &SkipDialog::skipPressed);
        buttonBox->addButton(skipButton, QDialogButtonBox::ActionRole);

        QPushButton *autoSkipButton = new QPushButton(i18n("Skip All"));
        connect(autoSkipButton, &QAbstractButton::clicked, this, &SkipDialog::autoSkipPressed);
        buttonBox->addButton(autoSkipButton, QDialogButtonBox::ActionRole);
    }

    buttonBox->addButton(QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SkipDialog::cancelPressed);

    resize(sizeHint());
}

SkipDialog::~SkipDialog()
{
}

void SkipDialog::cancelPressed()
{
    done(KIO::Result_Cancel);
}

void SkipDialog::skipPressed()
{
    done(KIO::Result_Skip);
}

void SkipDialog::autoSkipPressed()
{
    done(KIO::Result_AutoSkip);
}

void SkipDialog::retryPressed()
{
    done(KIO::Result_Retry);
}

