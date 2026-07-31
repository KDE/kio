/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "renamefilewarning.h"

#include <KConfigGroup>
#include <KGuiItem>
#include <KIconUtils>
#include <KLocalizedString>
#include <KMessageDialog>
#include <KSharedConfig>
#include <KStandardGuiItem>

#include <QMimeDatabase>

namespace KIO
{

RenameFileWarning::RenameFileWarning(const KFileItem &item, const QString &newName, QWidget *parent, bool hiddenFilesVisible)
    : QObject(parent)
    , m_item(item)
    , m_newName(newName)
    , m_parent(parent)
    , m_hiddenFilesVisible(hiddenFilesVisible)
{
}

RenameFileWarning::~RenameFileWarning() = default;

void RenameFileWarning::exec()
{
    const bool becomesHidden = m_newName.startsWith(QLatin1Char('.')) && !m_item.name().startsWith(QLatin1Char('.'));

    KSharedConfig::Ptr kioConfig = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals);
    KConfigGroup confirmGroup(kioConfig, QStringLiteral("Confirmations"));

    if (!m_hiddenFilesVisible && becomesHidden) {
        if (!confirmGroup.readEntry("ConfirmHide", true)) {
            Q_EMIT result(true);
            deleteLater();
            return;
        }

        const KGuiItem renameAndHideGuiItem(i18nc("@action:button", "Rename and Hide"), QStringLiteral("view-hidden"));
        const QString message = m_item.isDir()
            ? i18nc("@info", "Adding a dot to the beginning of this folder's name will hide it from view.\nDo you still want to rename it?")
            : i18nc("@info", "Adding a dot to the beginning of this file's name will hide it from view.\nDo you still want to rename it?");
        const QString title = m_item.isDir() ? i18nc("@title:window", "Hide this Folder?") : i18nc("@title:window", "Hide this File?");

        auto *dialog = new KMessageDialog(KMessageDialog::QuestionTwoActions, message, m_parent);
        dialog->setWindowTitle(title);
        dialog->setButtons(renameAndHideGuiItem, KStandardGuiItem::cancel());
        dialog->setIcon(QIcon::fromTheme(QStringLiteral("dialog-question")));
        dialog->setDontAskAgainText(i18nc("@option:check", "Do not ask again"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowModality(Qt::WindowModal);

        connect(dialog, &QDialog::finished, this, [this, dialog, confirmGroup](int dialogResult) mutable {
            if (dialogResult == KMessageDialog::PrimaryAction) {
                if (dialog->isDontAskAgainChecked()) {
                    confirmGroup.writeEntry("ConfirmHide", false);
                }
                Q_EMIT result(true);
            } else {
                Q_EMIT result(false);
            }
            deleteLater();
        });
        dialog->show();
        return;
    }

    if (m_item.isFile() && !becomesHidden) {
        if (!confirmGroup.readEntry("ConfirmRenameFileType", true)) {
            Q_EMIT result(true);
            deleteLater();
            return;
        }

        QMimeDatabase db;
        const QMimeType oldMimeType = db.mimeTypeForFile(m_item.name(), QMimeDatabase::MatchExtension);
        const QMimeType newMimeType = db.mimeTypeForFile(m_newName, QMimeDatabase::MatchExtension);

        if (oldMimeType.isValid() && !oldMimeType.isDefault() && newMimeType.isValid() && newMimeType != oldMimeType) {
            const KGuiItem renameGuiItem(i18nc("@action:button", "Rename"), QStringLiteral("edit-rename"));
            const QIcon mimeTypeIcon = QIcon::fromTheme(newMimeType.iconName(), QIcon::fromTheme(QStringLiteral("unknown")));
            const QIcon warningBadge = QIcon::fromTheme(QStringLiteral("emblem-warning"), QIcon::fromTheme(QStringLiteral("emblem-important")));
            const QIcon messageBoxIcon = KIconUtils::addOverlay(mimeTypeIcon, warningBadge, Qt::BottomRightCorner);
            const QString prompt = newMimeType.isDefault() ? i18nc("@info",
                                                                   "This will make the file type unknown.\n"
                                                                   "The file's content won't change but applications may no longer recognize it.\n"
                                                                   "Do you still want to rename it?")
                                                           : i18nc("@info",
                                                                   "This will change the file type from \"%1\" to \"%2\".\n"
                                                                   "The file's content won't change but applications may no longer recognize it.\n"
                                                                   "Do you still want to rename it?",
                                                                   oldMimeType.comment(),
                                                                   newMimeType.comment());

            auto *dialog = new KMessageDialog(KMessageDialog::QuestionTwoActions, prompt, m_parent);
            dialog->setWindowTitle(i18nc("@title:window", "Change File Type"));
            dialog->setButtons(renameGuiItem, KStandardGuiItem::cancel());
            dialog->setIcon(messageBoxIcon);
            dialog->setDontAskAgainText(i18nc("@option:check", "Do not ask again"));
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setWindowModality(Qt::WindowModal);

            connect(dialog, &QDialog::finished, this, [this, dialog, confirmGroup](int dialogResult) mutable {
                if (dialogResult == KMessageDialog::PrimaryAction) {
                    if (dialog->isDontAskAgainChecked()) {
                        confirmGroup.writeEntry("ConfirmRenameFileType", false);
                    }
                    Q_EMIT result(true);
                } else {
                    Q_EMIT result(false);
                }
                deleteLater();
            });
            dialog->show();
            return;
        }
    }

    Q_EMIT result(true);
    deleteLater();
}

} // namespace KIO
