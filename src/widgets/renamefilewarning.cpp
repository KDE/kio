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

void confirmRenameWarning(const KFileItem &item, const QString &newName, QWidget *parent, std::function<void(bool proceed)> callback, bool hiddenFilesVisible)
{
    const bool becomesHidden = newName.startsWith(QLatin1Char('.')) && !item.name().startsWith(QLatin1Char('.'));

    KSharedConfig::Ptr kioConfig = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals);
    KConfigGroup confirmGroup(kioConfig, QStringLiteral("Confirmations"));

    if (!hiddenFilesVisible && becomesHidden) {
        if (!confirmGroup.readEntry("ConfirmHide", true)) {
            callback(true);
            return;
        }

        const KGuiItem renameAndHideGuiItem(i18nc("@action:button", "Rename and Hide"), QStringLiteral("view-hidden"));
        const QString message = item.isDir()
            ? i18nc("@info", "Adding a dot to the beginning of this folder's name will hide it from view.\nDo you still want to rename it?")
            : i18nc("@info", "Adding a dot to the beginning of this file's name will hide it from view.\nDo you still want to rename it?");
        const QString title = item.isDir() ? i18nc("@title:window", "Hide this Folder?") : i18nc("@title:window", "Hide this File?");

        auto *dialog = new KMessageDialog(KMessageDialog::QuestionTwoActions, message, parent);
        dialog->setWindowTitle(title);
        dialog->setButtons(renameAndHideGuiItem, KStandardGuiItem::cancel());
        dialog->setIcon(QIcon::fromTheme(QStringLiteral("dialog-question")));
        dialog->setDontAskAgainText(i18nc("@option:check", "Do not ask again"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setModal(false);

        QObject::connect(dialog, &QDialog::finished, dialog, [dialog, kioConfig, confirmGroup, callback](int result) mutable {
            if (result == KMessageDialog::PrimaryAction) {
                if (dialog->isDontAskAgainChecked()) {
                    confirmGroup.writeEntry("ConfirmHide", false);
                    kioConfig->sync();
                }
                callback(true);
            } else {
                callback(false);
            }
        });
        dialog->open();
        return;
    }

    if (item.isFile() && !becomesHidden) {
        if (!confirmGroup.readEntry("ConfirmRenameFileType", true)) {
            callback(true);
            return;
        }

        QMimeDatabase db;
        const QMimeType oldMimeType = db.mimeTypeForFile(item.name(), QMimeDatabase::MatchExtension);
        const QMimeType newMimeType = db.mimeTypeForFile(newName, QMimeDatabase::MatchExtension);

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

            auto *dialog = new KMessageDialog(KMessageDialog::QuestionTwoActions, prompt, parent);
            dialog->setWindowTitle(i18nc("@title:window", "Change File Type"));
            dialog->setButtons(renameGuiItem, KStandardGuiItem::cancel());
            dialog->setIcon(messageBoxIcon);
            dialog->setDontAskAgainText(i18nc("@option:check", "Do not ask again"));
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setModal(false);

            QObject::connect(dialog, &QDialog::finished, dialog, [dialog, kioConfig, confirmGroup, callback](int result) mutable {
                if (result == KMessageDialog::PrimaryAction) {
                    if (dialog->isDontAskAgainChecked()) {
                        confirmGroup.writeEntry("ConfirmRenameFileType", false);
                        kioConfig->sync();
                    }
                    callback(true);
                } else {
                    callback(false);
                }
            });
            dialog->open();
            return;
        }
    }

    callback(true);
}

} // namespace KIO
