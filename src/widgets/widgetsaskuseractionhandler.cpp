/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "widgetsaskuseractionhandler.h"

#include <kio_widgets_debug.h>
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

#include <QDialogButtonBox>
#include <QRegularExpression>
#include <QUrl>

class KIO::WidgetsAskUserActionHandlerPrivate
{
public:
    WidgetsAskUserActionHandlerPrivate(WidgetsAskUserActionHandler *qq)
        : q(qq)
    {
    }

    // Creates a KSslInfoDialog or falls back to a generic Information dialog
    void sslMessageBox(const QString &text, const KIO::MetaData &metaData, QWidget *parent);

    WidgetsAskUserActionHandler *const q;
};

KIO::WidgetsAskUserActionHandler::WidgetsAskUserActionHandler(QObject *parent)
    : KIO::AskUserActionInterface(parent), d(new WidgetsAskUserActionHandlerPrivate(this))
{
}

KIO::WidgetsAskUserActionHandler::~WidgetsAskUserActionHandler()
{
}

void KIO::WidgetsAskUserActionHandler::askUserRename(KJob *job,
                                                     const QString &caption,
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
    auto *dlg = new KIO::RenameDialog(KJobWidgets::window(job), caption,
                                      src, dest, options,
                                      sizeSrc, sizeDest,
                                      ctimeSrc, ctimeDest,
                                      mtimeSrc, mtimeDest);

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

void KIO::WidgetsAskUserActionHandler::askUserSkip(KJob *job,
                                                   KIO::SkipDialog_Options options,
                                                   const QString &errorText)
{
    auto *dlg = new KIO::SkipDialog(KJobWidgets::window(job), options, errorText);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowModality(Qt::WindowModal);

    connect(job, &KJob::finished, dlg, &QDialog::reject);
    connect(dlg, &QDialog::finished, this, [this, job](const int exitCode) {
        Q_EMIT askUserSkipResult(static_cast<KIO::SkipDialog_Result>(exitCode), job);
    });

    dlg->show();
}

void KIO::WidgetsAskUserActionHandler::askUserDelete(const QList<QUrl> &urls,
                                                     DeletionType deletionType,
                                                     ConfirmationType confirmationType,
                                                     QWidget *parent)
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

    KGuiItem acceptButton;
    QString text;
    QString caption = i18n("Delete Permanently");

    switch(deletionType) {
    case Delete: {
        text = xi18ncp("@info", "Do you really want to permanently delete this %1 item?<nl/><nl/>"
                       "<emphasis strong='true'>This action cannot be undone.</emphasis>",
                       "Do you really want to permanently delete these %1 items?<nl/><nl/>"
                       "<emphasis strong='true'>This action cannot be undone.</emphasis>", urlCount);
        acceptButton = KStandardGuiItem::del();
        break;
    }
    case EmptyTrash: {
        text = xi18nc("@info", "Do you want to permanently delete all items from the Trash?<nl/><nl/>"
                      "<emphasis strong='true'>This action cannot be undone.</emphasis>");
        acceptButton = KGuiItem(i18nc("@action:button", "Empty Trash"), QStringLiteral("user-trash"));
        break;
    }
    case Trash: {
        if (urlCount == 1) {
            text = xi18nc("@info", "Do you really want to move this item to the Trash?<nl/>"
                          "<filename>%1</filename>", prettyList.at(0));
        } else {
            text = xi18ncp("@info", "Do you really want to move this %1 item to the Trash?", "Do you really want to move these %1 items to the Trash?", urlCount);
        }
        caption = i18n("Move to Trash");
        acceptButton = KGuiItem(i18n("Move to Trash"), QStringLiteral("user-trash"));
        break;
    }
    default:
        break;
    }

    KMessageDialog *dlg = new KMessageDialog(KMessageDialog::QuestionYesNo, text, parent);

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setCaption(caption);
    dlg->setIcon(QIcon{});
    dlg->setButtons(acceptButton, KStandardGuiItem::cancel());
    if (urlCount > 1) {
        dlg->setListWidgetItems(prettyList);
    }
    dlg->setDontAskAgainText(i18nc("@option:checkbox", "Do not ask again"));
    // If we get here, !ask must be false
    dlg->setDontAskAgainChecked(!ask);

    connect(dlg, &QDialog::finished, this, [=](const int buttonCode) {
        const bool isDelete = buttonCode == QDialogButtonBox::Yes;

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
                                                             const QString &caption,
                                                             const QString &buttonYes,
                                                             const QString &buttonNo,
                                                             const QString &iconYes,
                                                             const QString &iconNo,
                                                             const QString &dontAskAgainName,
                                                             const QString &details,
                                                             const KIO::MetaData &metaData,
                                                             QWidget *parent)
{
    KSharedConfigPtr reqMsgConfig = KSharedConfig::openConfig(QStringLiteral("kioslaverc"));
    const bool ask = reqMsgConfig->group("Notification Messages").readEntry(dontAskAgainName, true);

    if (!ask) {
        Q_EMIT messageBoxResult(type == WarningContinueCancel ? KIO::SlaveBase::Continue : KIO::SlaveBase::Yes);
        return;
    }

    auto acceptButton = KGuiItem(buttonYes, iconYes);
    auto rejectButton = KGuiItem(buttonNo, iconNo);

    // It's "Do not ask again" every where except with Information
    QString dontAskAgainText = i18nc("@option:check", "Do not ask again");

    KMessageDialog::Type dlgType;

    switch (type) {
    case QuestionYesNo:
        dlgType = KMessageDialog::QuestionYesNo;
        break;
    case QuestionYesNoCancel:
        dlgType = KMessageDialog::QuestionYesNoCancel;
        break;
    case WarningYesNo:
        dlgType = KMessageDialog::WarningYesNo;
        break;
    case WarningYesNoCancel:
        dlgType = KMessageDialog::WarningYesNoCancel;
        break;
    case WarningContinueCancel:
        dlgType = KMessageDialog::WarningContinueCancel;
        break;
    case SSLMessageBox:
        d->sslMessageBox(text, metaData, parent);
        return;
    case Information:
        dlgType = KMessageDialog::Information;
        dontAskAgainText = i18nc("@option:check", "Do not show this message again");
        break;
    case Sorry:
        dlgType = KMessageDialog::Sorry;
        dontAskAgainText = QString{}; // No dontAskAgain checkbox
        break;
    case Error:
        dlgType = KMessageDialog::Error;
        dontAskAgainText = QString{}; // No dontAskAgain checkbox
        break;
    default:
        qCWarning(KIO_WIDGETS) << "Unknown message dialog type" << type;
        return;
    }

    auto *dialog = new KMessageDialog(dlgType, text, parent);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setCaption(caption);
    dialog->setIcon(QIcon{});
    // If a button has empty text, KMessageDialog will replace that button
    // with a suitable KStandardGuiItem
    dialog->setButtons(acceptButton, rejectButton);
    dialog->setDetails(details);
    dialog->setDontAskAgainText(dontAskAgainText);
    dialog->setDontAskAgainChecked(!ask);
    dialog->setOpenExternalLinks(true); // Allow opening external links in the text labels

    connect(dialog, &QDialog::finished, this, [=](const int result) {
        KIO::SlaveBase::ButtonCode btnCode;
        switch (result) {
        case QDialogButtonBox::Yes:
            if (dlgType == KMessageDialog::WarningContinueCancel) {
                btnCode = KIO::SlaveBase::Continue;
            } else {
                btnCode = KIO::SlaveBase::Yes;
            }
            break;
        case QDialogButtonBox::No:
            btnCode = KIO::SlaveBase::No;
            break;
        case QDialogButtonBox::Cancel:
            btnCode = KIO::SlaveBase::Cancel;
            break;
        case QDialogButtonBox::Ok:
            btnCode = KIO::SlaveBase::Ok;
            break;
        default:
            qCWarning(KIO_WIDGETS) << "Unknown message dialog result" << result;
            return;
        }

        Q_EMIT messageBoxResult(btnCode);

        if (result != QDialogButtonBox::Cancel) {
            KConfigGroup cg = reqMsgConfig->group("Notification Messages");
            cg.writeEntry(dontAskAgainName, !dialog->isDontAskAgainChecked());
            cg.sync();
        }
    });

    dialog->show();
}

void KIO::WidgetsAskUserActionHandlerPrivate::sslMessageBox(const QString &text,
                                                            const KIO::MetaData &metaData,
                                                            QWidget *parent)
{
    const QStringList sslList = metaData.value(QStringLiteral("ssl_peer_chain")).split(QLatin1Char('\x01'),
                                          Qt::SkipEmptyParts);

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
        ksslDlg->setSslInfo(
            certChain,
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
    auto *dialog = new KMessageDialog(KMessageDialog::Information,
                                      i18n("The peer SSL certificate chain appears to be corrupt."),
                                      parent);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setCaption(i18n("SSL"));
    // KMessageDialog will set a proper icon
    dialog->setIcon(QIcon{});
    dialog->setButtons(KStandardGuiItem::ok());

    QObject::connect(dialog, &QDialog::finished, q, [this](const int result) {
        Q_EMIT q->messageBoxResult(result == QDialogButtonBox::Ok ? KIO::SlaveBase::Ok : KIO::SlaveBase::Cancel);
    });

    dialog->show();
}
