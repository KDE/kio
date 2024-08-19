/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "widgetsaskuseractionhandler.h"

#include <KConfig>
#include <KConfigGroup>
#include <KGuiItem>
#include <KIO/WorkerBase>
#include <KJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageDialog>
#include <KSharedConfig>
#include <KSslInfoDialog>
#include <KStandardGuiItem>
#include <kio_widgets_debug.h>

#include <QApplication>
#include <QDialogButtonBox>
#include <QPointer>
#include <QRegularExpression>
#include <QUrl>

class KIO::WidgetsAskUserActionHandlerPrivate
{
public:
    explicit WidgetsAskUserActionHandlerPrivate(WidgetsAskUserActionHandler *qq)
        : q(qq)
    {
    }

    // Creates a KSslInfoDialog or falls back to a generic Information dialog
    void sslMessageBox(const QString &text, const KIO::MetaData &metaData, QWidget *parent);

    bool gotPersistentUserReply(KIO::AskUserActionInterface::MessageDialogType type, const KConfigGroup &cg, const QString &dontAskAgainName);
    void savePersistentUserReply(KIO::AskUserActionInterface::MessageDialogType type, KConfigGroup &cg, const QString &dontAskAgainName, int result);

    WidgetsAskUserActionHandler *const q;
    QPointer<QWidget> m_parentWidget = nullptr;

    QWidget *getParentWidget(KJob *job);
    QWidget *getParentWidget(QWidget *widget);
};

bool KIO::WidgetsAskUserActionHandlerPrivate::gotPersistentUserReply(KIO::AskUserActionInterface::MessageDialogType type,
                                                                     const KConfigGroup &cg,
                                                                     const QString &dontAskAgainName)
{
    // storage values matching the logic of FrameworkIntegration's KMessageBoxDontAskAgainConfigStorage
    switch (type) {
    case KIO::AskUserActionInterface::QuestionTwoActions:
    case KIO::AskUserActionInterface::QuestionTwoActionsCancel:
    case KIO::AskUserActionInterface::WarningTwoActions:
    case KIO::AskUserActionInterface::WarningTwoActionsCancel: {
        // storage holds "true" if persistent reply is "Yes", "false" for persistent "No",
        // otherwise no persistent reply is present
        const QString value = cg.readEntry(dontAskAgainName, QString());
        if ((value.compare(QLatin1String("yes"), Qt::CaseInsensitive) == 0) || (value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0)) {
            Q_EMIT q->messageBoxResult(KIO::WorkerBase::PrimaryAction);
            return true;
        }
        if ((value.compare(QLatin1String("no"), Qt::CaseInsensitive) == 0) || (value.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0)) {
            Q_EMIT q->messageBoxResult(KIO::WorkerBase::SecondaryAction);
            return true;
        }
        break;
    }
    case KIO::AskUserActionInterface::WarningContinueCancel: {
        // storage holds "false" if persistent reply is "Continue"
        // otherwise no persistent reply is present
        const bool value = cg.readEntry(dontAskAgainName, true);
        if (value == false) {
            Q_EMIT q->messageBoxResult(KIO::WorkerBase::Continue);
            return true;
        }
        break;
    }
    default:
        break;
    }

    return false;
}

void KIO::WidgetsAskUserActionHandlerPrivate::savePersistentUserReply(KIO::AskUserActionInterface::MessageDialogType type,
                                                                      KConfigGroup &cg,
                                                                      const QString &dontAskAgainName,
                                                                      int result)
{
    // see gotPersistentUserReply for values stored and why
    switch (type) {
    case KIO::AskUserActionInterface::QuestionTwoActions:
    case KIO::AskUserActionInterface::QuestionTwoActionsCancel:
    case KIO::AskUserActionInterface::WarningTwoActions:
    case KIO::AskUserActionInterface::WarningTwoActionsCancel:
        cg.writeEntry(dontAskAgainName, result == KIO::WorkerBase::PrimaryAction);
        cg.sync();
        break;
    case KIO::AskUserActionInterface::WarningContinueCancel:
        cg.writeEntry(dontAskAgainName, false);
        cg.sync();
        break;
    default:
        break;
    }
}

QWidget *KIO::WidgetsAskUserActionHandlerPrivate::getParentWidget(KJob *job)
{
    QWidget *parentWidget = nullptr;

    if (job) {
        auto parentWindow = KJobWidgets::window(job);
        if (parentWindow) {
            parentWidget = parentWindow;
        }
    }

    return getParentWidget(parentWidget);
}

QWidget *KIO::WidgetsAskUserActionHandlerPrivate::getParentWidget(QWidget *widget)
{
    QWidget *parentWidget = widget;

    if (!parentWidget) {
        parentWidget = this->m_parentWidget;
    }

    if (!parentWidget) {
        parentWidget = qApp->activeWindow();
    }

    return parentWidget;
}

KIO::WidgetsAskUserActionHandler::WidgetsAskUserActionHandler(QObject *parent)
    : KIO::AskUserActionInterface(parent)
    , d(new WidgetsAskUserActionHandlerPrivate(this))
{
}

KIO::WidgetsAskUserActionHandler::~WidgetsAskUserActionHandler()
{
}

void KIO::WidgetsAskUserActionHandler::askUserRename(KJob *job,
                                                     const QString &title,
                                                     const QUrl &src,
                                                     const QUrl &dest,
                                                     KIO::RenameDialog_Options options,
                                                     KIO::filesize_t sizeSrc,
                                                     KIO::filesize_t sizeDest,
                                                     const QDateTime &ctimeSrc,
                                                     const QDateTime &ctimeDest,
                                                     const QDateTime &mtimeSrc,
                                                     const QDateTime &mtimeDest)
{
    QMetaObject::invokeMethod(qGuiApp, [=, this] {
        auto *dlg = new KIO::RenameDialog(d->getParentWidget(job), title, src, dest, options, sizeSrc, sizeDest, ctimeSrc, ctimeDest, mtimeSrc, mtimeDest);

        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowModality(Qt::WindowModal);

        connect(job, &KJob::finished, dlg, &QDialog::reject);
        connect(dlg, &QDialog::finished, this, [this, job, dlg](const int exitCode) {
            KIO::RenameDialog_Result result = static_cast<RenameDialog_Result>(exitCode);
            const QUrl newUrl = result == Result_AutoRename ? dlg->autoDestUrl() : dlg->newDestUrl();
            Q_EMIT askUserRenameResult(result, newUrl, job);
        });

        dlg->show();
    });
}

void KIO::WidgetsAskUserActionHandler::askUserSkip(KJob *job, KIO::SkipDialog_Options options, const QString &errorText)
{
    QMetaObject::invokeMethod(qGuiApp, [=, this] {
        auto *dlg = new KIO::SkipDialog(d->getParentWidget(job), options, errorText);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowModality(Qt::WindowModal);

        connect(job, &KJob::finished, dlg, &QDialog::reject);
        connect(dlg, &QDialog::finished, this, [this, job](const int exitCode) {
            Q_EMIT askUserSkipResult(static_cast<KIO::SkipDialog_Result>(exitCode), job);
        });

        dlg->show();
    });
}

struct ProcessAskDeleteResult {
    QStringList prettyList;
    KMessageDialog::Type dialogType = KMessageDialog::QuestionTwoActions;
    KGuiItem acceptButton;
    QString text;
    QIcon icon;
    QString title = i18n("Delete Permanently");
    bool isSingleUrl = false;
};

using AskIface = KIO::AskUserActionInterface;
static ProcessAskDeleteResult processAskDelete(const QList<QUrl> &urls, AskIface::DeletionType deletionType)
{
    ProcessAskDeleteResult res;
    res.prettyList.reserve(urls.size());
    std::transform(urls.cbegin(), urls.cend(), std::back_inserter(res.prettyList), [](const auto &url) {
        if (url.scheme() == QLatin1String("trash")) {
            QString path = url.path();
            // HACK (#98983): remove "0-foo". Note that it works better than
            // displaying KFileItem::name(), for files under a subdir.
            static const QRegularExpression re(QStringLiteral("^/[0-9]+-"));
            path.remove(re);
            return path;
        } else {
            return url.toDisplayString(QUrl::PreferLocalFile);
        }
    });

    const int urlCount = res.prettyList.size();
    res.isSingleUrl = urlCount == 1;

    switch (deletionType) {
    case AskIface::Delete: {
        res.dialogType = KMessageDialog::QuestionTwoActions; // Using Question* so the Delete button is pre-selected. Bug 462845
        res.icon = QIcon::fromTheme(QStringLiteral("dialog-warning"));
        if (res.isSingleUrl) {
            res.text = xi18nc("@info",
                              "Do you really want to permanently delete this item?<nl/><nl/>"
                              "<filename>%1</filename><nl/><nl/>"
                              "<emphasis strong='true'>This action cannot be undone.</emphasis>",
                              res.prettyList.at(0));
        } else {
            res.text = xi18ncp("@info",
                               "Do you really want to permanently delete this %1 item?<nl/><nl/>"
                               "<emphasis strong='true'>This action cannot be undone.</emphasis>",
                               "Do you really want to permanently delete these %1 items?<nl/><nl/>"
                               "<emphasis strong='true'>This action cannot be undone.</emphasis>",
                               urlCount);
        }
        res.acceptButton = KGuiItem(i18nc("@action:button", "Delete Permanently"), QStringLiteral("edit-delete"));
        break;
    }
    case AskIface::DeleteInsteadOfTrash: {
        res.dialogType = KMessageDialog::WarningTwoActions;
        if (res.isSingleUrl) {
            res.text = xi18nc("@info",
                              "Moving this item to Trash failed as it is too large."
                              " Permanently delete it instead?<nl/><nl/>"
                              "<filename>%1</filename><nl/><nl/>"
                              "<emphasis strong='true'>This action cannot be undone.</emphasis>",
                              res.prettyList.at(0));
        } else {
            res.text = xi18ncp("@info",
                               "Moving this %1 item to Trash failed as it is too large."
                               " Permanently delete it instead?<nl/>"
                               "<emphasis strong='true'>This action cannot be undone.</emphasis>",
                               "Moving these %1 items to Trash failed as they are too large."
                               " Permanently delete them instead?<nl/><nl/>"
                               "<emphasis strong='true'>This action cannot be undone.</emphasis>",
                               urlCount);
        }
        res.acceptButton = KGuiItem(i18nc("@action:button", "Delete Permanently"), QStringLiteral("edit-delete"));
        break;
    }
    case AskIface::EmptyTrash: {
        res.dialogType = KMessageDialog::QuestionTwoActions; // Using Question* so the Delete button is pre-selected.
        res.icon = QIcon::fromTheme(QStringLiteral("dialog-warning"));
        res.text = xi18nc("@info",
                          "Do you want to permanently delete all items from the Trash?<nl/><nl/>"
                          "<emphasis strong='true'>This action cannot be undone.</emphasis>");
        res.acceptButton = KGuiItem(i18nc("@action:button", "Empty Trash"), QStringLiteral("user-trash"));
        break;
    }
    case AskIface::Trash: {
        if (res.isSingleUrl) {
            res.text = xi18nc("@info",
                              "Do you really want to move this item to the Trash?<nl/>"
                              "<filename>%1</filename>",
                              res.prettyList.at(0));
        } else {
            res.text =
                xi18ncp("@info", "Do you really want to move this %1 item to the Trash?", "Do you really want to move these %1 items to the Trash?", urlCount);
        }
        res.title = i18n("Move to Trash");
        res.acceptButton = KGuiItem(res.title, QStringLiteral("user-trash"));
        break;
    }
    default:
        break;
    }
    return res;
}

void KIO::WidgetsAskUserActionHandler::askUserDelete(const QList<QUrl> &urls, DeletionType deletionType, ConfirmationType confirmationType, QWidget *parent)
{
    QString keyName;
    bool ask = (confirmationType == ForceConfirmation);
    if (!ask) {
        // The default value for confirmations is true for delete and false
        // for trash. If you change this, please also update:
        //      dolphin/src/settings/general/confirmationssettingspage.cpp
        bool defaultValue = true;

        switch (deletionType) {
        case DeleteInsteadOfTrash:
        case Delete:
            keyName = QStringLiteral("ConfirmDelete");
            break;
        case Trash:
            keyName = QStringLiteral("ConfirmTrash");
            defaultValue = false;
            break;
        case EmptyTrash:
            keyName = QStringLiteral("ConfirmEmptyTrash");
            break;
        }

        KSharedConfigPtr kioConfig = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals);
        ask = kioConfig->group(QStringLiteral("Confirmations")).readEntry(keyName, defaultValue);
    }

    if (!ask) {
        Q_EMIT askUserDeleteResult(true, urls, deletionType, parent);
        return;
    }

    QMetaObject::invokeMethod(qGuiApp, [=, this] {
        const auto &[prettyList, dialogType, acceptButton, text, icon, title, singleUrl] = processAskDelete(urls, deletionType);
        KMessageDialog *dlg = new KMessageDialog(dialogType, text, parent);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setCaption(title);
        dlg->setIcon(icon);
        dlg->setButtons(acceptButton, KStandardGuiItem::cancel());
        if (!singleUrl) {
            dlg->setListWidgetItems(prettyList);
        }
        dlg->setDontAskAgainText(i18nc("@option:checkbox", "Do not ask again"));
        // If we get here, !ask must be false
        dlg->setDontAskAgainChecked(!ask);

        connect(dlg, &QDialog::finished, this, [=, this](const int buttonCode) {
            const bool isDelete = (buttonCode == KMessageDialog::PrimaryAction);

            Q_EMIT askUserDeleteResult(isDelete, urls, deletionType, parent);

            if (isDelete) {
                KSharedConfigPtr kioConfig = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals);
                KConfigGroup cg = kioConfig->group(QStringLiteral("Confirmations"));
                cg.writeEntry(keyName, !dlg->isDontAskAgainChecked());
                cg.sync();
            }
        });

        dlg->setWindowModality(Qt::WindowModal);
        dlg->show();
    });
}

void KIO::WidgetsAskUserActionHandler::requestUserMessageBox(MessageDialogType type,
                                                             const QString &text,
                                                             const QString &title,
                                                             const QString &primaryActionText,
                                                             const QString &secondaryActionText,
                                                             const QString &primaryActionIconName,
                                                             const QString &secondaryActionIconName,
                                                             const QString &dontAskAgainName,
                                                             const QString &details,
                                                             QWidget *parent)
{
    if (d->gotPersistentUserReply(type,
                                  KSharedConfig::openConfig(QStringLiteral("kioslaverc"))->group(QStringLiteral("Notification Messages")),
                                  dontAskAgainName)) {
        return;
    }

    const KGuiItem primaryActionButton(primaryActionText, primaryActionIconName);
    const KGuiItem secondaryActionButton(secondaryActionText, secondaryActionIconName);

    // It's "Do not ask again" every where except with Information
    QString dontAskAgainText = i18nc("@option:check", "Do not ask again");

    KMessageDialog::Type dlgType;
    bool hasCancelButton = false;

    switch (type) {
    case AskUserActionInterface::QuestionTwoActions:
        dlgType = KMessageDialog::QuestionTwoActions;
        break;
    case AskUserActionInterface::QuestionTwoActionsCancel:
        dlgType = KMessageDialog::QuestionTwoActionsCancel;
        hasCancelButton = true;
        break;
    case AskUserActionInterface::WarningTwoActions:
        dlgType = KMessageDialog::WarningTwoActions;
        break;
    case AskUserActionInterface::WarningTwoActionsCancel:
        dlgType = KMessageDialog::WarningTwoActionsCancel;
        hasCancelButton = true;
        break;
    case AskUserActionInterface::WarningContinueCancel:
        dlgType = KMessageDialog::WarningContinueCancel;
        hasCancelButton = true;
        break;
    case AskUserActionInterface::Information:
        dlgType = KMessageDialog::Information;
        dontAskAgainText = i18nc("@option:check", "Do not show this message again");
        break;
    case AskUserActionInterface::Error:
        dlgType = KMessageDialog::Error;
        dontAskAgainText = QString{}; // No dontAskAgain checkbox
        break;
    default:
        qCWarning(KIO_WIDGETS) << "Unknown message dialog type" << type;
        return;
    }

    QMetaObject::invokeMethod(qGuiApp, [=, this]() {
        auto cancelButton = hasCancelButton ? KStandardGuiItem::cancel() : KGuiItem();
        auto *dialog = new KMessageDialog(dlgType, text, d->getParentWidget(parent));

        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setCaption(title);
        dialog->setIcon(QIcon{});
        dialog->setButtons(primaryActionButton, secondaryActionButton, cancelButton);
        dialog->setDetails(details);
        dialog->setDontAskAgainText(dontAskAgainText);
        dialog->setDontAskAgainChecked(false);
        dialog->setOpenExternalLinks(true); // Allow opening external links in the text labels

        connect(dialog, &QDialog::finished, this, [=, this](const int result) {
            KIO::WorkerBase::ButtonCode btnCode;
            switch (result) {
            case KMessageDialog::PrimaryAction:
                if (dlgType == KMessageDialog::WarningContinueCancel) {
                    btnCode = KIO::WorkerBase::Continue;
                } else {
                    btnCode = KIO::WorkerBase::PrimaryAction;
                }
                break;
            case KMessageDialog::SecondaryAction:
                btnCode = KIO::WorkerBase::SecondaryAction;
                break;
            case KMessageDialog::Cancel:
                btnCode = KIO::WorkerBase::Cancel;
                break;
            case KMessageDialog::Ok:
                btnCode = KIO::WorkerBase::Ok;
                break;
            default:
                qCWarning(KIO_WIDGETS) << "Unknown message dialog result" << result;
                return;
            }

            Q_EMIT messageBoxResult(btnCode);

            if ((result != KMessageDialog::Cancel) && dialog->isDontAskAgainChecked()) {
                KSharedConfigPtr reqMsgConfig = KSharedConfig::openConfig(QStringLiteral("kioslaverc"));
                KConfigGroup cg = reqMsgConfig->group(QStringLiteral("Notification Messages"));
                d->savePersistentUserReply(type, cg, dontAskAgainName, result);
            }
        });

        dialog->show();
    });
}

void KIO::WidgetsAskUserActionHandler::setWindow(QWidget *window)
{
    d->m_parentWidget = window;
}

void KIO::WidgetsAskUserActionHandler::askIgnoreSslErrors(const QVariantMap &sslErrorData, QWidget *parent)
{
    QWidget *parentWidget = d->getParentWidget(parent);

    QString message = i18n("The server failed the authenticity check (%1).\n\n", sslErrorData[QLatin1String("hostname")].toString());

    message += sslErrorData[QLatin1String("sslError")].toString();

    auto *dialog = new KMessageDialog(KMessageDialog::WarningTwoActionsCancel, message, parentWidget);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setCaption(i18n("Server Authentication"));
    dialog->setIcon(QIcon{});
    dialog->setButtons(KGuiItem{i18n("&Details"), QStringLiteral("documentinfo")}, KStandardGuiItem::cont(), KStandardGuiItem::cancel());

    connect(dialog, &KMessageDialog::finished, this, [this, parentWidget, sslErrorData](int result) {
        if (result == KMessageDialog::PrimaryAction) {
            showSslDetails(sslErrorData, parentWidget);
        } else if (result == KMessageDialog::SecondaryAction) {
            // continue();
            Q_EMIT askIgnoreSslErrorsResult(1);
        } else if (result == KMessageDialog::Cancel) {
            // cancel();
            Q_EMIT askIgnoreSslErrorsResult(0);
        }
    });

    dialog->show();
}

void KIO::WidgetsAskUserActionHandler::showSslDetails(const QVariantMap &sslErrorData, QWidget *parentWidget)
{
    const QStringList sslList = sslErrorData[QLatin1String("peerCertChain")].toStringList();

    QList<QSslCertificate> certChain;
    bool decodedOk = true;
    for (const QString &str : sslList) {
        certChain.append(QSslCertificate(str.toUtf8()));
        if (certChain.last().isNull()) {
            decodedOk = false;
            break;
        }
    }

    QMetaObject::invokeMethod(qGuiApp, [=, this] {
        if (decodedOk) { // Use KSslInfoDialog
            KSslInfoDialog *ksslDlg = new KSslInfoDialog(parentWidget);
            ksslDlg->setSslInfo(
                certChain,
                QString(),
                sslErrorData[QLatin1String("hostname")].toString(),
                sslErrorData[QLatin1String("protocol")].toString(),
                sslErrorData[QLatin1String("cipher")].toString(),
                sslErrorData[QLatin1String("usedBits")].toInt(),
                sslErrorData[QLatin1String("bits")].toInt(),
                KSslInfoDialog::certificateErrorsFromString(sslErrorData[QLatin1String("certificateErrors")].toStringList().join(QLatin1Char('\n'))));

            // KSslInfoDialog deletes itself by setting Qt::WA_DeleteOnClose

            QObject::connect(ksslDlg, &QDialog::finished, this, [this, sslErrorData, parentWidget]() {
                // KSslInfoDialog only has one button, QDialogButtonBox::Close
                askIgnoreSslErrors(sslErrorData, parentWidget);
            });

            ksslDlg->show();
            return;
        }

        // Fallback to a generic message box
        auto *dialog = new KMessageDialog(KMessageDialog::Information, i18n("The peer SSL certificate chain appears to be corrupt."), parentWidget);

        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setCaption(i18n("SSL"));
        dialog->setButtons(KStandardGuiItem::ok());

        QObject::connect(dialog, &QDialog::finished, this, [this](const int result) {
            Q_EMIT askIgnoreSslErrorsResult(result == KMessageDialog::Ok ? 1 : 0);
        });

        dialog->show();
    });
}

#include "moc_widgetsaskuseractionhandler.cpp"
