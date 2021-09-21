/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kio/skipdialog.h"

#include <assert.h>
#include <stdio.h>

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <KGuiItem>
#include <KLocalizedString>
#include <KStandardGuiItem>

using namespace KIO;

SkipDialog::SkipDialog(QWidget *parent, KIO::SkipDialog_Options options, const QString &_error_text)
    : QDialog(parent)
    , d(nullptr)
{
    setWindowTitle(i18n("Information"));

    QVBoxLayout *layout = new QVBoxLayout(this);

    auto *label = new QLabel(_error_text, this);
    label->setWordWrap(true);
    layout->addWidget(label);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    layout->addWidget(buttonBox);

    const bool isMultiple = options & SkipDialog_MultipleItems;
    const bool isInvalidChars = options & SkipDialog_Replace_Invalid_Chars;
    const bool hideRetry = options & SkipDialog_Hide_Retry;

    // Retrying to e.g. copy a file with "*" in the name to a fat32
    // partition will always fail
    if (isInvalidChars) {
        QPushButton *replaceCharButton = new QPushButton(i18n("Replace"));
        connect(replaceCharButton, &QAbstractButton::clicked, this, [this]() {
            done(KIO::Result_ReplaceInvalidChars);
        });
        buttonBox->addButton(replaceCharButton, QDialogButtonBox::ActionRole);
    } else if (!hideRetry) {
        QPushButton *retryButton = new QPushButton(i18n("Retry"));
        connect(retryButton, &QAbstractButton::clicked, this, &SkipDialog::retryPressed);
        buttonBox->addButton(retryButton, QDialogButtonBox::ActionRole);
    }

    if (isMultiple) {
        if (isInvalidChars) {
            QPushButton *autoReplaceButton = new QPushButton(i18n("Replace All"));
            connect(autoReplaceButton, &QAbstractButton::clicked, this, [this]() {
                done(KIO::Result_ReplaceAllInvalidChars);
            });
            buttonBox->addButton(autoReplaceButton, QDialogButtonBox::ActionRole);
        }

        QPushButton *skipButton = new QPushButton(i18n("Skip"));
        connect(skipButton, &QAbstractButton::clicked, this, &SkipDialog::skipPressed);
        buttonBox->addButton(skipButton, QDialogButtonBox::ActionRole);

        QPushButton *autoSkipButton = new QPushButton(i18n("Skip All"));
        connect(autoSkipButton, &QAbstractButton::clicked, this, &SkipDialog::autoSkipPressed);
        buttonBox->addButton(autoSkipButton, QDialogButtonBox::ActionRole);
    }

    auto *cancelBtn = buttonBox->addButton(QDialogButtonBox::Cancel);
    // If it's one item and the Retry button is hidden, replace the Cancel
    // button text with OK
    if (hideRetry && !isMultiple) {
        KGuiItem::assign(cancelBtn, KStandardGuiItem::ok());
    }
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
