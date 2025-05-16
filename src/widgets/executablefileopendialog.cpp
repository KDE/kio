/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 Arjun A.K. <arjunak234@gmail.com>
    SPDX-FileCopyrightText: 2025 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "executablefileopendialog_p.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QMimeType>
#include <QPushButton>
#include <QShowEvent>

#include <KApplicationTrader>
#include <KFileItem>
#include <KIconLoader>
#include <KLocalizedString>
#include <KMessageDialog>

#include <KIO/PreviewJob>

ExecutableFileOpenDialog::ExecutableFileOpenDialog(const QUrl &url, const QMimeType &mimeType, ExecutableFileOpenDialog::Mode mode, QWidget *parent)
    : QDialog(parent)
{
    m_ui.setupUi(this);

    std::optional<KFileItem> fileItem;

    if (url.isValid()) {
        fileItem = KFileItem{url, mimeType.name()};

        m_ui.nameLabel->setText(fileItem->name());
        m_ui.nameLabel->setToolTip(url.toDisplayString(QUrl::PreferLocalFile));
    } else {
        m_ui.nameLabel->hide();
    }

    m_ui.mimeTypeLabel->setForegroundRole(QPalette::PlaceholderText);
    // Not using KFileItem::comment() since that also reads the Comment from the .desktop file
    // which could spoof the user.
    m_ui.mimeTypeLabel->setText(mimeType.comment());
    m_ui.mimeTypeLabel->setToolTip(mimeType.name());

    const QSize iconSize{KIconLoader::SizeHuge, KIconLoader::SizeHuge};
    QIcon icon;

    if (fileItem) {
        icon = QIcon::fromTheme(fileItem->iconName());

        auto *previewJob = KIO::filePreview({*fileItem}, iconSize);
        previewJob->setDevicePixelRatio(devicePixelRatioF());
        connect(previewJob, &KIO::PreviewJob::gotPreview, this, [this](const KFileItem &item, const QPixmap &pixmap) {
            Q_UNUSED(item);
            m_ui.iconLabel->setPixmap(pixmap);
        });
    }

    if (icon.isNull()) {
        icon = QIcon::fromTheme(mimeType.iconName());
    }
    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("unknown"));
    }

    m_ui.iconLabel->setPixmap(icon.pixmap(iconSize, devicePixelRatioF()));

    connect(m_ui.buttonBox, &QDialogButtonBox::rejected, this, &ExecutableFileOpenDialog::reject);

    QPushButton *launchButton = new QPushButton(i18nc("@action:button Launch script", "&Launch"), this);
    launchButton->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));

    // Script execution settings UI is in Dolphin, not KIO, only show the explanation
    // on how to undo "dont ask again" when Dolphin is the default file manager.
    const auto fileManagerService = KApplicationTrader::preferredService(QStringLiteral("inode/directory"));
    m_ui.dontAgainHelpButton->setVisible(fileManagerService && fileManagerService->desktopEntryName() == QLatin1String("org.kde.dolphin"));

    if (mode == OnlyExecute) {
        m_ui.dontAgainCheckBox->setText(i18nc("@option:check", "Launch executable files without asking"));
        connect(launchButton, &QPushButton::clicked, this, &ExecutableFileOpenDialog::executeFile);
    } else if (mode == OpenAsExecute) {
        m_ui.dontAgainCheckBox->setText(i18nc("@option:check Open in the associated app", "Open executable files in the default application without asking"));
        connect(launchButton, &QPushButton::clicked, this, &ExecutableFileOpenDialog::openFile);
    } else { // mode == OpenOrExecute
        m_ui.label->setText(i18n("What do you wish to do with this file?"));
        connect(launchButton, &QPushButton::clicked, this, &ExecutableFileOpenDialog::executeFile);

        QPushButton *openButton = new QPushButton(QIcon::fromTheme(QStringLiteral("document-preview")), i18nc("@action:button", "&Open"), this);
        if (KService::Ptr service = KApplicationTrader::preferredService(mimeType.name())) {
            openButton->setText(i18nc("@action:button", "&Open with %1", service->name()));
            const QIcon serviceIcon = QIcon::fromTheme(service->icon());
            if (!serviceIcon.isNull()) {
                openButton->setIcon(serviceIcon);
            }
        }
        m_ui.buttonBox->addButton(openButton, QDialogButtonBox::AcceptRole);

        connect(openButton, &QPushButton::clicked, this, &ExecutableFileOpenDialog::openFile);
    }

    // Add Execute button last so that Open is first in the button box
    m_ui.buttonBox->addButton(launchButton, QDialogButtonBox::AcceptRole);
    m_ui.buttonBox->button(QDialogButtonBox::Cancel)->setFocus();
}

bool ExecutableFileOpenDialog::isDontAskAgainChecked() const
{
    return m_ui.dontAgainCheckBox->isChecked();
}

void ExecutableFileOpenDialog::executeFile()
{
    done(ExecuteFile);
}

void ExecutableFileOpenDialog::openFile()
{
    done(OpenFile);
}

void ExecutableFileOpenDialog::showEvent(QShowEvent *event)
{
    if (!event->spontaneous()) {
        KMessageDialog::beep(KMessageDialog::QuestionTwoActionsCancel, m_ui.label->text(), this);
    }
    QDialog::showEvent(event);
}

#include "moc_executablefileopendialog_p.cpp"
