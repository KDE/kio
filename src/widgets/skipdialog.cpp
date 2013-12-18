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
#include <QLayout>
#include <QLabel>

#include <klocalizedstring.h>
#include <kio/jobuidelegateextension.h>

using namespace KIO;

SkipDialog::SkipDialog(QWidget *parent, bool _multi, const QString& _error_text )
  : QDialog(parent), d(0)
{
  setWindowTitle(i18n("Information"));

  QVBoxLayout *layout = new QVBoxLayout;
  setLayout(layout);

  layout->addWidget(new QLabel(_error_text, this));

  QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
  layout->addWidget(buttonBox);

  QPushButton *retryButton = new QPushButton(i18n("Retry"));
  connect(retryButton, SIGNAL(clicked()), SLOT(retryPressed()));
  buttonBox->addButton(retryButton, QDialogButtonBox::ActionRole);

  if (_multi) {
    QPushButton *skipButton = new QPushButton(i18n("Skip"));
    connect(skipButton, SIGNAL(clicked()), SLOT(skipPressed()));
    buttonBox->addButton(skipButton, QDialogButtonBox::ActionRole);

    QPushButton *autoSkipButton = new QPushButton(i18n("AutoSkip"));
    connect(autoSkipButton, SIGNAL(clicked()), SLOT(autoSkipPressed()));
    buttonBox->addButton(autoSkipButton, QDialogButtonBox::ActionRole);
  }

  buttonBox->addButton(QDialogButtonBox::Cancel);
  connect(buttonBox, SIGNAL(rejected()), SLOT(cancelPressed()) );

  resize( sizeHint() );
}

SkipDialog::~SkipDialog()
{
}

void SkipDialog::cancelPressed()
{
  done(KIO::S_CANCEL);
}

void SkipDialog::skipPressed()
{
  done(KIO::S_SKIP);
}

void SkipDialog::autoSkipPressed()
{
  done(KIO::S_AUTO_SKIP);
}

void SkipDialog::retryPressed()
{
  done(KIO::S_RETRY);
}

