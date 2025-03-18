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
    for (const QString &format : formats) {
        QMimeType mime = db.mimeTypeForName(format);
        if (mime.isValid()) {
            auto label = i18n("%1 (%2)", mime.comment(), format);
            m_comboBox->addItem(label, mime.name());
        } else {
            m_comboBox->addItem(format);
        }
    }

    m_lastValidComboboxFormat = formats.value(comboItem());

    // Get fancy: if the user changes the format, try to replace the filename extension
    connect(m_comboBox, &QComboBox::activated, this, [this, formats]() {
        const auto format = formats.value(comboItem());
        const auto currentText = m_lineEdit->text();

        QMimeDatabase db;
        const QMimeType oldMimetype = db.mimeTypeForName(m_lastValidComboboxFormat);
        const QMimeType newMimetype = db.mimeTypeForName(format);

        const QString newExtension = newMimetype.preferredSuffix();
        const QString oldExtension = oldMimetype.preferredSuffix();

        m_lastValidComboboxFormat = format;
        if (newMimetype.isValid()) {
            if (oldMimetype.isValid() && currentText.endsWith(oldMimetype.preferredSuffix())) {
                m_lineEdit->setFocus();
                m_lineEdit->setText(m_lineEdit->text().replace(oldExtension, newExtension));
                m_lineEdit->setFocus();
                m_lineEdit->setSelection(0, m_lineEdit->text().length() - newExtension.length() - 1);
            } else {
                m_lineEdit->setText(currentText + QLatin1String(".") + newMimetype.preferredSuffix());
                m_lineEdit->setSelection(0, m_lineEdit->text().length() - newExtension.length() - 1);
                m_lineEdit->setFocus();
            }
        } else if (oldMimetype.isValid() && currentText.endsWith(oldMimetype.preferredSuffix())) {
            // remove the extension
            m_lineEdit->setText(currentText.chopped(oldExtension.length() + 1));
            m_lineEdit->setFocus();
        }
    });

    // update the selected format depending on the text
    connect(m_lineEdit, &QLineEdit::textChanged, this, [this, formats]() {
        const auto format = formats.value(comboItem());
        const auto currentText = m_lineEdit->text();

        QMimeDatabase db;
        const QMimeType oldMimetype = db.mimeTypeForName(m_lastValidComboboxFormat);
        QMimeType newMimetype = db.mimeTypeForFile(currentText, QMimeDatabase::MatchMode::MatchExtension);
        if (newMimetype.isValid() && newMimetype != oldMimetype && formats.contains(newMimetype.name())) {
            auto idxMime = m_comboBox->findData(newMimetype.name(), Qt::UserRole);
            if (idxMime != -1) {
                m_lastValidComboboxFormat = format;
                m_comboBox->setCurrentIndex(idxMime);
            }
        }
    });

    layout->addWidget(m_comboBox);

    layout->addStretch();

    auto textValue = value;
    if (textValue.isEmpty()) {
        textValue = i18nc("A default file name excluding extension", "pasted file");
    }
    // Pre-fill the filename extension and select everything before it, or just
    // move the cursor appropriately
    const QMimeType mimetype = db.mimeTypeForName(formats.value(comboItem()));
    if (mimetype.isValid() && !textValue.endsWith(mimetype.preferredSuffix())) {
        m_lineEdit->setText(textValue + QLatin1String(".") + mimetype.preferredSuffix());

        m_lineEdit->setSelection(0, textValue.length());
        m_lineEdit->setFocus();
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
