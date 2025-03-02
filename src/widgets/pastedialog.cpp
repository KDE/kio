/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2005 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "pastedialog_p.h"

#include <KLocalizedString>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMimeDatabase>
#include <QMimeType>
#include <QVBoxLayout>

KIO::PasteDialog::PasteDialog(const QString &title, const QString &label, const QString &value, const QStringList &formats, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setModal(true);

    QVBoxLayout *topLayout = new QVBoxLayout(this);

    QFrame *frame = new QFrame(this);
    topLayout->addWidget(frame);

    QVBoxLayout *layout = new QVBoxLayout(frame);

    m_label = new QLabel(label, frame);
    m_label->setWordWrap(true);
    layout->addWidget(m_label);

    m_lineEdit = new QLineEdit(value, frame);
    layout->addWidget(m_lineEdit);

    m_lineEdit->setFocus();
    m_label->setBuddy(m_lineEdit);

    layout->addWidget(new QLabel(i18n("Data format:"), frame));
    m_comboBox = new QComboBox(frame);

    // Populate the combobox with nice human-readable labels
    QMimeDatabase db;
    QStringList formatLabels;
    formatLabels.reserve(formats.size());
    for (const QString &format : formats) {
        QMimeType mime = db.mimeTypeForName(format);
        if (mime.isValid()) {
            formatLabels.append(i18n("%1 (%2)", mime.comment(), format));
        } else {
            formatLabels.append(format);
        }
    }

    m_comboBox->addItems(formatLabels);
    m_lastValidComboboxFormat = formats.value(comboItem());

    // Get fancy: if the user changes the format, try to replace the filename extension
    connect(m_comboBox, &QComboBox::activated, this, [this, formats]() {
        QMimeDatabase db;
        const QMimeType oldMimetype = db.mimeTypeForName(m_lastValidComboboxFormat);

        if (oldMimetype.isValid()) {
            const QMimeType newMimetype = db.mimeTypeForName(formats.value(comboItem()));

            if (newMimetype.isValid()) {
                const QString oldExtension = oldMimetype.preferredSuffix();
                const QString newExtension = newMimetype.preferredSuffix();
                m_lineEdit->setFocus();
                m_lineEdit->setText(m_lineEdit->text().replace(oldExtension, newExtension));
                m_lastValidComboboxFormat = formats.value(comboItem());
                m_lineEdit->setFocus();
                m_lineEdit->setSelection(0, m_lineEdit->text().length() - newExtension.length() - 1);
            }
        }
    });

    layout->addWidget(m_comboBox);

    layout->addStretch();

    // Pre-fill the filename extension and select everything before it, or just
    // move the cursor appropriately
    const QMimeType mimetype = db.mimeTypeForName(formats[comboItem()]);
    if (mimetype.isValid() && !value.endsWith(mimetype.preferredSuffix())) {
        m_lineEdit->setText(value + QLatin1String(".") + mimetype.preferredSuffix());

        if (value.isEmpty()) {
            m_lineEdit->setFocus();
            // FIXME: Why do these two lines of code do nothing?
            m_lineEdit->deselect();
            m_lineEdit->setCursorPosition(0);
        } else {
            m_lineEdit->setFocus();
            m_lineEdit->setSelection(0, value.length());
        }
    }

    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    topLayout->addWidget(buttonBox);

    setMinimumWidth(350);
}

QString KIO::PasteDialog::lineEditText() const
{
    return m_lineEdit->text();
}

int KIO::PasteDialog::comboItem() const
{
    return m_comboBox->currentIndex();
}

#include "moc_pastedialog_p.cpp"
