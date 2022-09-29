/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "widgetsaskuseractionhandler.h"

#include <KConfig>
#include <KConfigGroup>
#include <KGuiItem>
#include <KIO/SlaveBase>
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
    QWidget *m_parentWidget = nullptr;
};

bool KIO::WidgetsAskUserActionHandlerPrivate::gotPersistentUserReply(KIO::AskUserActionInterface::MessageDialogType type,
                                                                     const KConfigGroup &cg,
                                                                     const QString &dontAskAgainName)
{
    // storage values matching the logic of FrameworkIntegration's KMessageBoxDontAskAgainConfigStorage
    switch (type) {
    case KIO::AskUserActionInterface::QuestionYesNo:
    case KIO::AskUserActionInterface::QuestionYesNoCancel:
    case KIO::AskUserActionInterface::WarningYesNo:
    case KIO::AskUserActionInterface::WarningYesNoCancel: {
        // storage holds "true" if persistent reply is "Yes", "false" for persistent "No",
        // otherwise no persistent reply is present
        const QString value = cg.readEntry(dontAskAgainName, QString());
        if ((value.compare(QLatin1String("yes"), Qt::CaseInsensitive) == 0) || (value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0)) {
            Q_EMIT q->messageBoxResult(KIO::SlaveBase::Yes);
            return true;
        }
        if ((value.compare(QLatin1String("no"), Qt::CaseInsensitive) == 0) || (value.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0)) {
            Q_EMIT q->messageBoxResult(KIO::SlaveBase::No);
            return true;
        }
        break;
    }
    case KIO::AskUserActionInterface::WarningContinueCancel: {
        // storage holds "false" if persistent reply is "Continue"
        // otherwise no persistent reply is present
        const bool value = cg.readEntry(dontAskAgainName, true);
        if (value == false) {
            Q_EMIT q->messageBoxResult(KIO::SlaveBase::Continue);
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
    case KIO::AskUserActionInterface::QuestionYesNo:
    case KIO::AskUserActionInterface::QuestionYesNoCancel:
    case KIO::AskUserActionInterface::WarningYesNo:
    case KIO::AskUserActionInterface::WarningYesNoCancel:
        cg.writeEntry(dontAskAgainName, result == KIO::SlaveBase::Yes);
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
    QWidget *parentWidget = nullptr;

    if (job) {
        parentWidget = KJobWidgets::window(job);
    }

    if (!parentWidget) {
        parentWidget = d->m_parentWidget;
    }

    if (!parentWidget) {
        parentWidget = qApp->activeWindow();
    }

    auto *dlg = new KIO::RenameDialog(parentWidget, title, src, dest, options, sizeSrc, sizeDest, ctimeSrc, ctimeDest, mtimeSrc, mtimeDest);

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowModality(Qt::WindowModal);

    connect(job, &KJob::finished, dlg, &QDialog::reject);
    connect(dlg, &QDialog::finished, this, [this, job, dlg](const int exitCode) {
        KIO::RenameDialog_Result result = static_cast<RenameDialog_Result>(exitCode);
        const QUrl newUrl = result == Result_AutoRename ? dlg->autoDestUrl() : dlg->newDestUrl();
        Q_EMIT askUserRenameResult(result, newUrl, job);
    });

    dlg->show();
}

void KIO::WidgetsAskUserActionHandler::askUserSkip(KJob *job, KIO::SkipDialog_Options options, const QString &errorText)
{
    QWidget *parentWidget = nullptr;

    if (job) {
        parentWidget = KJobWidgets::window(job);
    }

    if (!parentWidget) {
        parentWidget = d->m_parentWidget;
    }

    if (!parentWidget) {
        parentWidget = qApp->activeWindow();
    }

    auto *dlg = new KIO::SkipDialog(parentWidget, options, errorText);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowModality(Qt::WindowModal);

    connect(job, &KJob::finished, dlg, &QDialog::reject);
    connect(dlg, &QDialog::finished, this, [this, job](const int exitCode) {
        Q_EMIT askUserSkipResult(static_cast<KIO::SkipDialog_Result>(exitCode), job);
    });

    dlg->show();
}

void KIO::WidgetsAskUserActionHandler::askUserDelete(const QList<QUrl> &urls, DeletionType deletionType, ConfirmationType confirmationType, QWidget *parent)
{
    KSharedConfigPtr kioConfig = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals);

    QString keyName;
    bool ask = (confirmationType == ForceConfirmation);
    if (!ask) {
        // The default value for confirmations is true for delete and false
        // for trash. If you change this, please also update:
        //      dolphin/src/settings/general/confirmationssettingspage.cpp
        bool defaultValue = true;

        switch (deletionType) {
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

        ask = kioConfig->group("Confirmations").readEntry(keyName, defaultValue);
    }

    if (!ask) {
        Q_EMIT askUserDeleteResult(true, urls, deletionType, parent);
        return;
    }

    QStringList prettyList;
    prettyList.reserve(urls.size());
    for (const QUrl &url : urls) {
        if (url.scheme() == QLatin1String("trash")) {
            QString path = url.path();
            // HACK (#98983): remove "0-foo". Note that it works better than
            // displaying KFileItem::name(), for files under a subdir.
            path.remove(QRegularExpression(QStringLiteral("^/[0-9]+-")));
            prettyList.append(path);
        } else {
            prettyList.append(url.toDisplayString(QUrl::PreferLocalFile));
        }
    }

    const int urlCount = prettyList.count();

    KMessageDialog::Type dialogType = KMessageDialog::QuestionTwoActions;
    KGuiItem acceptButton;
    QString text;
    QString title = i18n("Delete Permanently");

    switch (deletionType) {
    case Delete: {
        dialogType = KMessageDialog::WarningTwoActions;
        text = xi18ncp("@info",
                       "Do you really want to permanently delete this %1 item?<nl/><nl/>"
                       "<emphasis strong='true'>This action cannot be undone.</emphasis>",
                       "Do you really want to permanently delete these %1 items?<nl/><nl/>"
                       "<emphasis strong='true'>This action cannot be undone.</emphasis>",
                       urlCount);
        acceptButton = KStandardGuiItem::del();
        break;
    }
    case EmptyTrash: {
        dialogType = KMessageDialog::WarningTwoActions;
        text = xi18nc("@info",
                      "Do you want to permanently delete all items from the Trash?<nl/><nl/>"
                      "<emphasis strong='true'>This action cannot be undone.</emphasis>");
        acceptButton = KGuiItem(i18nc("@action:button", "Empty Trash"), QStringLiteral("user-trash"));
        break;
    }
    case Trash: {
        if (urlCount == 1) {
            text = xi18nc("@info",
                          "Do you really want to move this item to the Trash?<nl/>"
                          "<filename>%1</filename>",
                          prettyList.at(0));
        } else {
            text =
                xi18ncp("@info", "Do you really want to move this %1 item to the Trash?", "Do you really want to move these %1 items to the Trash?", urlCount);
        }
        title = i18n("Move to Trash");
        acceptButton = KGuiItem(title, QStringLiteral("user-trash"));
        break;
    }
    default:
        break;
    }

    KMessageDialog *dlg = new KMessageDialog(dialogType, text, parent);

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setCaption(title);
    dlg->setIcon(QIcon{});
    dlg->setButtons(acceptButton, KStandardGuiItem::cancel());
    if (urlCount > 1) {
        dlg->setListWidgetItems(prettyList);
    }
    dlg->setDontAskAgainText(i18nc("@option:checkbox", "Do not ask again"));
    // If we get here, !ask must be false
    dlg->setDontAskAgainChecked(!ask);

    connect(dlg, &QDialog::finished, this, [=](const int buttonCode) {
        const bool isDelete = (buttonCode == KMessageDialog::PrimaryAction);

        Q_EMIT askUserDeleteResult(isDelete, urls, deletionType, parent);

        if (isDelete) {
            KConfigGroup cg = kioConfig->group("Confirmations");
            cg.writeEntry(keyName, !dlg->isDontAskAgainChecked());
            cg.sync();
        }
    });

    dlg->setWindowModality(Qt::WindowModal);
    dlg->show();
}

void KIO::WidgetsAskUserActionHandler::requestUserMessageBox(MessageDialogType type,
                                                             const QString &text,
                                                             const QString &title,
                                                             const QString &primaryActionText,
                                                             const QString &secondatyActionText,
                                                             const QString &primaryActionIconName,
                                                             const QString &secondatyActionIconName,
                                                             const QString &dontAskAgainName,
                                                             const QString &details,
                                                             const KIO::MetaData &metaData,
                                                             QWidget *parent)
{
    KSharedConfigPtr reqMsgConfig = KSharedConfig::openConfig(QStringLiteral("kioslaverc"));

    if (d->gotPersistentUserReply(type, reqMsgConfig->group("Notification Messages"), dontAskAgainName)) {
        return;
    }

    const KGuiItem primaryActionButton(primaryActionText, primaryActionIconName);
    const KGuiItem secondaryActionButton(secondatyActionText, secondatyActionIconName);

    // It's "Do not ask again" every where except with Information
    QString dontAskAgainText = i18nc("@option:check", "Do not ask again");

    KMessageDialog::Type dlgType;
    bool hasCancelButton = false;

    switch (type) {
    case QuestionYesNo:
        dlgType = KMessageDialog::QuestionTwoActions;
        break;
    case QuestionYesNoCancel:
        dlgType = KMessageDialog::QuestionTwoActionsCancel;
        hasCancelButton = true;
        break;
    case WarningYesNo:
        dlgType = KMessageDialog::WarningTwoActions;
        break;
    case WarningYesNoCancel:
        dlgType = KMessageDialog::WarningTwoActionsCancel;
        hasCancelButton = true;
        break;
    case WarningContinueCancel:
        dlgType = KMessageDialog::WarningContinueCancel;
        hasCancelButton = true;
        break;
    case SSLMessageBox:
        d->sslMessageBox(text, metaData, parent);
        return;
    case Information:
        dlgType = KMessageDialog::Information;
        dontAskAgainText = i18nc("@option:check", "Do not show this message again");
        break;
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 97)
    case Sorry:
#if KWIDGETSADDONS_BUILD_DEPRECATED_SINCE(5, 97)
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        dlgType = KMessageDialog::Sorry;
        QT_WARNING_POP
        dontAskAgainText = QString{}; // No dontAskAgain checkbox
        break;
#else
#error "Cannot build KIOCore with KIO::AskUserActionInterface::Sorry with KMessageDialog::Sorry disabled"
#endif
#endif
    case Error:
        dlgType = KMessageDialog::Error;
        dontAskAgainText = QString{}; // No dontAskAgain checkbox
        break;
    default:
        qCWarning(KIO_WIDGETS) << "Unknown message dialog type" << type;
        return;
    }
    auto cancelButton = hasCancelButton ? KStandardGuiItem::cancel() : KGuiItem();

    auto *dialog = new KMessageDialog(dlgType, text, parent);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setCaption(title);
    dialog->setIcon(QIcon{});
    dialog->setButtons(primaryActionButton, secondaryActionButton, cancelButton);
    dialog->setDetails(details);
    dialog->setDontAskAgainText(dontAskAgainText);
    dialog->setDontAskAgainChecked(false);
    dialog->setOpenExternalLinks(true); // Allow opening external links in the text labels

    connect(dialog, &QDialog::finished, this, [=](const int result) {
        KIO::SlaveBase::ButtonCode btnCode;
        switch (result) {
        case KMessageDialog::PrimaryAction:
            if (dlgType == KMessageDialog::WarningContinueCancel) {
                btnCode = KIO::SlaveBase::Continue;
            } else {
                btnCode = KIO::SlaveBase::Yes;
            }
            break;
        case KMessageDialog::SecondaryAction:
            btnCode = KIO::SlaveBase::No;
            break;
        case KMessageDialog::Cancel:
            btnCode = KIO::SlaveBase::Cancel;
            break;
        case KMessageDialog::Ok:
            btnCode = KIO::SlaveBase::Ok;
            break;
        default:
            qCWarning(KIO_WIDGETS) << "Unknown message dialog result" << result;
            return;
        }

        Q_EMIT messageBoxResult(btnCode);

        if ((result != KMessageDialog::Cancel) && dialog->isDontAskAgainChecked()) {
            KConfigGroup cg = reqMsgConfig->group("Notification Messages");
            d->savePersistentUserReply(type, cg, dontAskAgainName, result);
        }
    });

    dialog->show();
}

void KIO::WidgetsAskUserActionHandlerPrivate::sslMessageBox(const QString &text, const KIO::MetaData &metaData, QWidget *parent)
{
    const QStringList sslList = metaData.value(QStringLiteral("ssl_peer_chain")).split(QLatin1Char('\x01'), Qt::SkipEmptyParts);

    QList<QSslCertificate> certChain;
    bool decodedOk = true;
    for (const QString &str : sslList) {
        certChain.append(QSslCertificate(str.toUtf8()));
        if (certChain.last().isNull()) {
            decodedOk = false;
            break;
        }
    }

    if (decodedOk) { // Use KSslInfoDialog
        KSslInfoDialog *ksslDlg = new KSslInfoDialog(parent);
        ksslDlg->setSslInfo(certChain,
                            metaData.value(QStringLiteral("ssl_peer_ip")),
                            text, // The URL
                            metaData.value(QStringLiteral("ssl_protocol_version")),
                            metaData.value(QStringLiteral("ssl_cipher")),
                            metaData.value(QStringLiteral("ssl_cipher_used_bits")).toInt(),
                            metaData.value(QStringLiteral("ssl_cipher_bits")).toInt(),
                            KSslInfoDialog::certificateErrorsFromString(metaData.value(QStringLiteral("ssl_cert_errors"))));

        // KSslInfoDialog deletes itself by setting Qt::WA_DeleteOnClose

        QObject::connect(ksslDlg, &QDialog::finished, q, [this]() {
            // KSslInfoDialog only has one button, QDialogButtonBox::Close
            Q_EMIT q->messageBoxResult(KIO::SlaveBase::Cancel);
        });

        ksslDlg->show();
        return;
    }

    // Fallback to a generic message box
    auto *dialog = new KMessageDialog(KMessageDialog::Information, i18n("The peer SSL certificate chain appears to be corrupt."), parent);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setCaption(i18n("SSL"));
    // KMessageDialog will set a proper icon
    dialog->setIcon(QIcon{});
    dialog->setButtons(KStandardGuiItem::ok());

    QObject::connect(dialog, &QDialog::finished, q, [this](const int result) {
        Q_EMIT q->messageBoxResult(result == KMessageDialog::Ok ? KIO::SlaveBase::Ok : KIO::SlaveBase::Cancel);
    });

    dialog->show();
}

void KIO::WidgetsAskUserActionHandler::setWindow(QWidget *window)
{
    d->m_parentWidget = window;
}
