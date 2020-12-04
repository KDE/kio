/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
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

using namespace KIO;

SkipDialog::SkipDialog(QWidget *parent, KIO::SkipDialog_Options options, const QString &_error_text)
    : QDialog(parent), d(nullptr)
{
    setWindowTitle(i18n("Information"));

    QVBoxLayout *layout = new QVBoxLayout(this);

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

