/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2005 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "pastedialog_p.h"

#include <KLocalizedString>

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QClipboard>
#include <QVBoxLayout>

KIO::PasteDialog::PasteDialog(const QString &caption, const QString &label,
                              const QString &value, const QStringList &items,
                              QWidget *parent,
                              bool clipboard)
    : QDialog(parent)
{
    setWindowTitle(caption);
    setModal(true);

    QVBoxLayout *topLayout = new QVBoxLayout(this);

    QFrame *frame = new QFrame(this);
    topLayout->addWidget(frame);

    QVBoxLayout *layout = new QVBoxLayout(frame);

    m_label = new QLabel(label, frame);
    layout->addWidget(m_label);

    m_lineEdit = new QLineEdit(value, frame);
    layout->addWidget(m_lineEdit);

    m_lineEdit->setFocus();
    m_label->setBuddy(m_lineEdit);

    layout->addWidget(new QLabel(i18n("Data format:"), frame));
    m_comboBox = new QComboBox(frame);
    m_comboBox->addItems(items);
    layout->addWidget(m_comboBox);

    layout->addStretch();

    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    topLayout->addWidget(buttonBox);

    setMinimumWidth(350);

    m_clipboardChanged = false;
    if (clipboard)
        connect(QApplication::clipboard(), &QClipboard::dataChanged,
                this, &PasteDialog::slotClipboardDataChanged);
}

void KIO::PasteDialog::slotClipboardDataChanged()
{
    m_clipboardChanged = true;
}

QString KIO::PasteDialog::lineEditText() const
{
    return m_lineEdit->text();
}

int KIO::PasteDialog::comboItem() const
{
    return m_comboBox->currentIndex();
}

