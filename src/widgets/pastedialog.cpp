/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2005 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "pastedialog_p.h"

#include <KIconLoader>
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

using namespace Qt::StringLiterals;

KIO::PasteDialog::PasteDialog(const QString &title, const QString &label, const QString &suggestedFileName, const QStringList &formats, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setModal(true);

    QVBoxLayout *topLayout = new QVBoxLayout(this);

    auto hLayout = new QHBoxLayout;
    topLayout->addLayout(hLayout);

    m_iconLabel = new QLabel(this);
    hLayout->addWidget(m_iconLabel, {}, Qt::AlignVCenter);

    auto innerLayout = new QVBoxLayout;
    hLayout->addLayout(innerLayout);

    m_label = new QLabel(label);
    m_label->setWordWrap(true);
    innerLayout->addWidget(m_label);

    m_lineEdit = new QLineEdit(suggestedFileName);
    innerLayout->addWidget(m_lineEdit);

    m_lineEdit->setFocus();
    m_label->setBuddy(m_lineEdit);

    if (formats.isEmpty()) {
        setIcon(QIcon());
    } else {
        QMimeDatabase db;

        auto *comboBoxHeading = new QLabel(i18nc("@label", "Data format:"));
        comboBoxHeading->setVisible(formats.size() > 1);
        innerLayout->addWidget(comboBoxHeading);

        m_comboBox = new QComboBox;
        m_comboBox->setVisible(formats.size() > 1);

        // Populate the combobox with nice human-readable labels
        for (const QString &format : formats) {
            QMimeType mime = db.mimeTypeForName(format);
            auto iconName = mime.iconName();
            if (iconName.isEmpty()) {
                // An icon might be available, but not registered with QMimeDatabase
                iconName = format;
                iconName.replace(u'/', u'-');
            }
            const auto icon = QIcon::fromTheme(iconName);
            if (mime.isValid()) {
                auto label = i18n("%1 (%2)", mime.comment(), format);
                m_comboBox->addItem(icon, label, mime.name());
            } else {
                m_comboBox->addItem(icon, format, format);
            }
        }

        m_lastValidComboboxFormat = formats.value(comboItem());

        const QMimeType mimeType = db.mimeTypeForName(m_lastValidComboboxFormat);
        setIcon(QIcon::fromTheme(mimeType.iconName()));

        // Get fancy: if the user changes the format, try to replace the filename extension
        connect(m_comboBox, &QComboBox::activated, this, [this, formats]() {
            const auto format = formats.value(comboItem());
            const auto currentText = m_lineEdit->text();

            QMimeDatabase db;
            const QMimeType oldMimetype = db.mimeTypeForName(m_lastValidComboboxFormat);
            const QMimeType newMimetype = db.mimeTypeForName(format);

            const QString newExtension = newMimetype.preferredSuffix();
            const QString oldExtension = oldMimetype.preferredSuffix();

            setIcon(QIcon::fromTheme(newMimetype.iconName()));

            m_lastValidComboboxFormat = format;
            if (newMimetype.isValid()) {
                if (oldMimetype.isValid() && currentText.endsWith(oldMimetype.preferredSuffix())) {
                    m_lineEdit->setText(m_lineEdit->text().replace(oldExtension, newExtension));
                } else {
                    m_lineEdit->setText(currentText + QLatin1String(".") + newMimetype.preferredSuffix());
                }
                m_lineEdit->setSelection(0, m_lineEdit->text().length() - newExtension.length() - 1);
                m_lineEdit->setFocus();
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

        innerLayout->addWidget(m_comboBox);

        const auto mimetype = db.mimeTypeForName(formats.value(comboItem()));
        const auto validMimeType = mimetype.isValid();
        const auto currentSuffix = db.suffixForFileName(suggestedFileName);
        QStringView currentBaseName = suggestedFileName;
        if (auto suffixSize = currentSuffix.size()) {
            currentBaseName.chop(suffixSize + 1); // remove '.' too
        }
        // Replace the current suffix if it doesn't match the mimetype
        if (validMimeType) {
            const auto preferredSuffix = mimetype.preferredSuffix();
            if (!preferredSuffix.isEmpty() && !mimetype.suffixes().contains(currentSuffix, Qt::CaseInsensitive)) {
                m_lineEdit->setText(currentBaseName % u'.' % preferredSuffix);
            } else if (preferredSuffix.isEmpty()) {
                m_lineEdit->setText(currentBaseName.toString());
            }
        } else if (!currentSuffix.isEmpty() && !validMimeType) {
            m_lineEdit->setText(currentBaseName.toString());
        }
        // Select the base name and move the text cursor to the end of it
        m_lineEdit->setSelection(0, currentBaseName.length());
    }

    topLayout->addStretch();
    m_lineEdit->setFocus();

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
    if (!m_comboBox) {
        return {};
    }
    return m_comboBox->currentIndex();
}

void KIO::PasteDialog::setIcon(const QIcon &icon)
{
    QIcon effectiveIcon = icon;
    if (effectiveIcon.isNull()) {
        effectiveIcon = QIcon::fromTheme(QIcon::ThemeIcon::EditPaste);
    }
    m_iconLabel->setPixmap(effectiveIcon.pixmap(KIconLoader::SizeHuge));
}

#include "moc_pastedialog_p.cpp"
