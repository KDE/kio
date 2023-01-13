/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2013 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "jobuidelegate.h"
#include "kio_widgets_debug.h"
#include "kiogui_export.h"
#include "widgetsaskuseractionhandler.h"
#include "widgetsopenorexecutefilehandler.h"
#include "widgetsopenwithhandler.h"
#include "widgetsuntrustedprogramhandler.h"
#include <kio/jobuidelegatefactory.h>

#include <KConfigGroup>
#include <KJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSharedConfig>
#include <clipboardupdater_p.h>
#include <ksslinfodialog.h>

#ifndef KIO_ANDROID_STUB
#include <QDBusInterface>
#endif
#include <QGuiApplication>
#include <QIcon>
#include <QPointer>
#include <QRegularExpression>
#include <QUrl>
#include <QWidget>

class KIO::JobUiDelegatePrivate
{
public:
    JobUiDelegatePrivate(KIO::JobUiDelegate *qq, const QList<QObject *> &ifaces)
    {
        for (auto iface : ifaces) {
            iface->setParent(qq);
            if (auto obj = qobject_cast<UntrustedProgramHandlerInterface *>(iface)) {
                m_untrustedProgramHandler = obj;
            } else if (auto obj = qobject_cast<OpenWithHandlerInterface *>(iface)) {
                m_openWithHandler = obj;
            } else if (auto obj = qobject_cast<OpenOrExecuteFileInterface *>(iface)) {
                m_openOrExecuteFileHandler = obj;
            } else if (auto obj = qobject_cast<AskUserActionInterface *>(iface)) {
                m_askUserActionHandler = obj;
            }
        }

        if (!m_untrustedProgramHandler) {
            m_untrustedProgramHandler = new WidgetsUntrustedProgramHandler(qq);
        }
        if (!m_openWithHandler) {
            m_openWithHandler = new WidgetsOpenWithHandler(qq);
        }
        if (!m_openOrExecuteFileHandler) {
            m_openOrExecuteFileHandler = new WidgetsOpenOrExecuteFileHandler(qq);
        }
        if (!m_askUserActionHandler) {
            m_askUserActionHandler = new WidgetsAskUserActionHandler(qq);
        }
    }

    UntrustedProgramHandlerInterface *m_untrustedProgramHandler = nullptr;
    OpenWithHandlerInterface *m_openWithHandler = nullptr;
    OpenOrExecuteFileInterface *m_openOrExecuteFileHandler = nullptr;
    AskUserActionInterface *m_askUserActionHandler = nullptr;
};

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 98)
KIO::JobUiDelegate::JobUiDelegate()
    : JobUiDelegate(Version::V2)
{
}
#endif

KIO::JobUiDelegate::~JobUiDelegate() = default;

/*
  Returns the top most window associated with widget.

  Unlike QWidget::window(), this function does its best to find and return the
  main application window associated with the given widget.

  If widget itself is a dialog or its parent is a dialog, and that dialog has a
  parent widget then this function will iterate through all those widgets to
  find the top most window, which most of the time is the main window of the
  application. By contrast, QWidget::window() would simply return the first
  file dialog it encountered since it is the "next ancestor widget that has (or
  could have) a window-system frame".
*/
static QWidget *topLevelWindow(QWidget *widget)
{
    QWidget *w = widget;
    while (w && w->parentWidget()) {
        w = w->parentWidget();
    }
    return (w ? w->window() : nullptr);
}

class JobUiDelegateStatic : public QObject
{
    Q_OBJECT
public:
    void registerWindow(QWidget *wid)
    {
        if (!wid) {
            return;
        }

        QWidget *window = topLevelWindow(wid);
        QObject *obj = static_cast<QObject *>(window);
        if (!m_windowList.contains(obj)) {
            // We must store the window Id because by the time
            // the destroyed signal is emitted we can no longer
            // access QWidget::winId() (already destructed)
            WId windowId = window->winId();
            m_windowList.insert(obj, windowId);
            connect(window, &QObject::destroyed, this, &JobUiDelegateStatic::slotUnregisterWindow);
#ifndef KIO_ANDROID_STUB
            QDBusInterface(QStringLiteral("org.kde.kded5"), QStringLiteral("/kded"), QStringLiteral("org.kde.kded5"))
                .call(QDBus::NoBlock, QStringLiteral("registerWindowId"), qlonglong(windowId));
#endif
        }
    }
public Q_SLOTS:
    void slotUnregisterWindow(QObject *obj)
    {
        if (!obj) {
            return;
        }

        QMap<QObject *, WId>::Iterator it = m_windowList.find(obj);
        if (it == m_windowList.end()) {
            return;
        }
        WId windowId = it.value();
        disconnect(it.key(), &QObject::destroyed, this, &JobUiDelegateStatic::slotUnregisterWindow);
        m_windowList.erase(it);
#ifndef KIO_ANDROID_STUB
        QDBusInterface(QStringLiteral("org.kde.kded5"), QStringLiteral("/kded"), QStringLiteral("org.kde.kded5"))
            .call(QDBus::NoBlock, QStringLiteral("unregisterWindowId"), qlonglong(windowId));
#endif
    }

private:
    QMap<QObject *, WId> m_windowList;
};

Q_GLOBAL_STATIC(JobUiDelegateStatic, s_static)

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 98)
KIO::JobUiDelegate::JobUiDelegate(KJobUiDelegate::Flags flags, QWidget *window)
    : JobUiDelegate(Version::V2, flags, window)
{
    setWindow(window);
}
#endif

void KIO::JobUiDelegate::setWindow(QWidget *window)
{
    KDialogJobUiDelegate::setWindow(window);

    if (auto obj = qobject_cast<WidgetsUntrustedProgramHandler *>(d->m_openWithHandler)) {
        obj->setWindow(window);
    }
    if (auto obj = qobject_cast<WidgetsOpenWithHandler *>(d->m_untrustedProgramHandler)) {
        obj->setWindow(window);
    }
    if (auto obj = qobject_cast<WidgetsOpenOrExecuteFileHandler *>(d->m_openOrExecuteFileHandler)) {
        obj->setWindow(window);
    }
    if (auto obj = qobject_cast<WidgetsAskUserActionHandler *>(d->m_askUserActionHandler)) {
        obj->setWindow(window);
    }

    s_static()->registerWindow(window);
}

void KIO::JobUiDelegate::unregisterWindow(QWidget *window)
{
    s_static()->slotUnregisterWindow(window);
}

KIO::RenameDialog_Result KIO::JobUiDelegate::askFileRename(KJob *job,
                                                           const QString &title,
                                                           const QUrl &src,
                                                           const QUrl &dest,
                                                           KIO::RenameDialog_Options options,
                                                           QString &newDest,
                                                           KIO::filesize_t sizeSrc,
                                                           KIO::filesize_t sizeDest,
                                                           const QDateTime &ctimeSrc,
                                                           const QDateTime &ctimeDest,
                                                           const QDateTime &mtimeSrc,
                                                           const QDateTime &mtimeDest)
{
    // qDebug() << "job=" << job;
    // We now do it in process, so that opening the rename dialog
    // doesn't start uiserver for nothing if progressId=0 (e.g. F2 in konq)
    KIO::RenameDialog dlg(KJobWidgets::window(job), title, src, dest, options, sizeSrc, sizeDest, ctimeSrc, ctimeDest, mtimeSrc, mtimeDest);
    dlg.setWindowModality(Qt::WindowModal);
    connect(job, &KJob::finished, &dlg, &QDialog::reject); // #192976
    KIO::RenameDialog_Result res = static_cast<RenameDialog_Result>(dlg.exec());
    if (res == Result_AutoRename) {
        newDest = dlg.autoDestUrl().path();
    } else {
        newDest = dlg.newDestUrl().path();
    }
    return res;
}

KIO::SkipDialog_Result KIO::JobUiDelegate::askSkip(KJob *job, KIO::SkipDialog_Options options, const QString &error_text)
{
    KIO::SkipDialog dlg(KJobWidgets::window(job), options, error_text);
    dlg.setWindowModality(Qt::WindowModal);
    connect(job, &KJob::finished, &dlg, &QDialog::reject); // #192976
    return static_cast<KIO::SkipDialog_Result>(dlg.exec());
}

bool KIO::JobUiDelegate::askDeleteConfirmation(const QList<QUrl> &urls, DeletionType deletionType, ConfirmationType confirmationType)
{
    QString keyName;
    bool ask = (confirmationType == ForceConfirmation);
    if (!ask) {
        KSharedConfigPtr kioConfig = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals);

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
    if (ask) {
        QStringList prettyList;
        prettyList.reserve(urls.size());
        for (const QUrl &url : urls) {
            if (url.scheme() == QLatin1String("trash")) {
                QString path = url.path();
                // HACK (#98983): remove "0-foo". Note that it works better than
                // displaying KFileItem::name(), for files under a subdir.
                path.remove(QRegularExpression(QStringLiteral("^/[0-9]*-")));
                prettyList.append(path);
            } else {
                prettyList.append(url.toDisplayString(QUrl::PreferLocalFile));
            }
        }

        int result;
        QWidget *widget = window();
        const KMessageBox::Options options(KMessageBox::Notify | KMessageBox::WindowModal);
        switch (deletionType) {
        case Delete:
            if (prettyList.count() == 1) {
                result = KMessageBox::warningContinueCancel(
                    widget,
                    xi18nc("@info",
                           "Do you really want to permanently delete this item?<nl/><filename>%1</filename><nl/><nl/><emphasis strong='true'>This action "
                           "cannot be undone.</emphasis>",
                           prettyList.first()),
                    i18n("Delete Permanently"),
                    KGuiItem(i18nc("@action:button", "Delete Permanently"), QStringLiteral("edit-delete")),
                    KStandardGuiItem::cancel(),
                    keyName,
                    options);
            } else {
                result = KMessageBox::warningContinueCancelList(
                    widget,
                    xi18ncp(
                        "@info",
                        "Do you really want to permanently delete this item?<nl/><nl/><emphasis strong='true'>This action cannot be undone.</emphasis>",
                        "Do you really want to permanently delete these %1 items?<nl/><nl/><emphasis strong='true'>This action cannot be undone.</emphasis>",
                        prettyList.count()),
                    prettyList,
                    i18n("Delete Permanently"),
                    KGuiItem(i18nc("@action:button", "Delete Permanently"), QStringLiteral("edit-delete")),
                    KStandardGuiItem::cancel(),
                    keyName,
                    options);
            }
            break;
        case EmptyTrash:
            result = KMessageBox::warningContinueCancel(
                widget,
                xi18nc("@info",
                       "Do you want to permanently delete all items from the Trash?<nl/><nl/><emphasis strong='true'>This action cannot be undone.</emphasis>"),
                i18n("Delete Permanently"),
                KGuiItem(i18nc("@action:button", "Empty Trash"), QIcon::fromTheme(QStringLiteral("user-trash"))),
                KStandardGuiItem::cancel(),
                keyName,
                options);
            break;
        case Trash:
        default:
            if (prettyList.count() == 1) {
                result = KMessageBox::warningContinueCancel(
                    widget,
                    xi18nc("@info", "Do you really want to move this item to the Trash?<nl/><filename>%1</filename>", prettyList.first()),
                    i18n("Move to Trash"),
                    KGuiItem(i18n("Move to Trash"), QStringLiteral("user-trash")),
                    KStandardGuiItem::cancel(),
                    keyName,
                    options);
            } else {
                result = KMessageBox::warningContinueCancelList(
                    widget,
                    i18np("Do you really want to move this item to the Trash?", "Do you really want to move these %1 items to the Trash?", prettyList.count()),
                    prettyList,
                    i18n("Move to Trash"),
                    KGuiItem(i18n("Move to Trash"), QStringLiteral("user-trash")),
                    KStandardGuiItem::cancel(),
                    keyName,
                    options);
            }
        }
        if (!keyName.isEmpty()) {
            // Check kmessagebox setting... erase & copy to konquerorrc.
            KSharedConfig::Ptr config = KSharedConfig::openConfig();
            KConfigGroup notificationGroup(config, "Notification Messages");
            if (!notificationGroup.readEntry(keyName, true)) {
                notificationGroup.writeEntry(keyName, true);
                notificationGroup.sync();

                KSharedConfigPtr kioConfig = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals);
                kioConfig->group("Confirmations").writeEntry(keyName, false);
            }
        }
        return (result == KMessageBox::Continue);
    }
    return true;
}

int KIO::JobUiDelegate::requestMessageBox(KIO::JobUiDelegate::MessageBoxType type,
                                          const QString &text,
                                          const QString &title,
                                          const QString &primaryActionText,
                                          const QString &secondaryActionText,
                                          const QString &primaryActionIconName,
                                          const QString &secondaryActionIconName,
                                          const QString &dontAskAgainName,
                                          const KIO::MetaData &metaData)
{
    int result = -1;

    // qDebug() << type << text << "title=" << title;

    KConfig config(QStringLiteral("kioslaverc"));
    KMessageBox::setDontShowAgainConfig(&config);

#if KWIDGETSADDONS_BUILD_DEPRECATED_SINCE(5, 100)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    KGuiItem primaryActionTextGui = KStandardGuiItem::yes();
    KGuiItem secondaryActionTextGui = KStandardGuiItem::no();
    QT_WARNING_POP

    if (!primaryActionText.isEmpty()) {
        primaryActionTextGui.setText(primaryActionText);
    }
    if (!primaryActionIconName.isNull()) {
        primaryActionTextGui.setIconName(primaryActionIconName);
    }

    if (!secondaryActionText.isEmpty()) {
        secondaryActionTextGui.setText(secondaryActionText);
    }
    if (!secondaryActionIconName.isNull()) {
        secondaryActionTextGui.setIconName(secondaryActionIconName);
    }
#else
    KGuiItem primaryActionTextGui(primaryActionText, primaryActionIconName);
    KGuiItem secondaryActionTextGui(secondaryActionText, secondaryActionIconName);
#endif

    KMessageBox::Options options(KMessageBox::Notify | KMessageBox::WindowModal);

    switch (type) {
    case QuestionTwoActions:
        result = KMessageBox::questionTwoActions(window(), text, title, primaryActionTextGui, secondaryActionTextGui, dontAskAgainName, options);
        break;
    case WarningTwoActions:
        result = KMessageBox::warningTwoActions(window(),
                                                text,
                                                title,
                                                primaryActionTextGui,
                                                secondaryActionTextGui,
                                                dontAskAgainName,
                                                options | KMessageBox::Dangerous);
        break;
    case WarningTwoActionsCancel:
        result = KMessageBox::warningTwoActionsCancel(window(),
                                                      text,
                                                      title,
                                                      primaryActionTextGui,
                                                      secondaryActionTextGui,
                                                      KStandardGuiItem::cancel(),
                                                      dontAskAgainName,
                                                      options);
        break;
    case WarningContinueCancel:
        result = KMessageBox::warningContinueCancel(window(), text, title, primaryActionTextGui, KStandardGuiItem::cancel(), dontAskAgainName, options);
        break;
    case Information:
        KMessageBox::information(window(), text, title, dontAskAgainName, options);
        result = 1; // whatever
        break;
    case SSLMessageBox: {
        QPointer<KSslInfoDialog> kid(new KSslInfoDialog(window()));
        // ### this is boilerplate code and appears in khtml_part.cpp almost unchanged!
        const QStringList sl = metaData.value(QStringLiteral("ssl_peer_chain")).split(QLatin1Char('\x01'), Qt::SkipEmptyParts);
        QList<QSslCertificate> certChain;
        bool decodedOk = true;
        for (const QString &s : sl) {
            certChain.append(QSslCertificate(s.toLatin1())); // or is it toLocal8Bit or whatever?
            if (certChain.last().isNull()) {
                decodedOk = false;
                break;
            }
        }

        if (decodedOk) {
            result = 1; // whatever
            kid->setSslInfo(certChain,
                            metaData.value(QStringLiteral("ssl_peer_ip")),
                            text, // the URL
                            metaData.value(QStringLiteral("ssl_protocol_version")),
                            metaData.value(QStringLiteral("ssl_cipher")),
                            metaData.value(QStringLiteral("ssl_cipher_used_bits")).toInt(),
                            metaData.value(QStringLiteral("ssl_cipher_bits")).toInt(),
                            KSslInfoDialog::certificateErrorsFromString(metaData.value(QStringLiteral("ssl_cert_errors"))));
            kid->exec();
        } else {
            result = -1;
            KMessageBox::information(window(), i18n("The peer SSL certificate chain appears to be corrupt."), i18n("SSL"), QString(), options);
        }
        // KSslInfoDialog deletes itself (Qt::WA_DeleteOnClose).
        delete kid;
        break;
    }
    case WarningContinueCancelDetailed: {
        const QString details = metaData.value(QStringLiteral("privilege_conf_details"));
        result = KMessageBox::warningContinueCancelDetailed(window(),
                                                            text,
                                                            title,
                                                            KStandardGuiItem::cont(),
                                                            KStandardGuiItem::cancel(),
                                                            dontAskAgainName,
                                                            options | KMessageBox::Dangerous,
                                                            details);
        break;
    }
    default:
        qCWarning(KIO_WIDGETS) << "Unknown type" << type;
        result = 0;
        break;
    }
    KMessageBox::setDontShowAgainConfig(nullptr);
    return result;
}

KIO::ClipboardUpdater *KIO::JobUiDelegate::createClipboardUpdater(Job *job, ClipboardUpdaterMode mode)
{
    if (qobject_cast<QGuiApplication *>(qApp)) {
        return new KIO::ClipboardUpdater(job, mode);
    }
    return nullptr;
}

void KIO::JobUiDelegate::updateUrlInClipboard(const QUrl &src, const QUrl &dest)
{
    if (qobject_cast<QGuiApplication *>(qApp)) {
        KIO::ClipboardUpdater::update(src, dest);
    }
}

KIO::JobUiDelegate::JobUiDelegate(Version version, KJobUiDelegate::Flags /*flags*/, QWidget *window, const QList<QObject *> &ifaces)
    : d(new JobUiDelegatePrivate(this, ifaces))
{
    // TODO KF6: drop the version argument and replace the deprecated constructor
    // TODO KF6: change the API to accept QWindows rather than QWidgets (this also carries through to the Interfaces)
    if (window) {
        s_static()->registerWindow(window);
        setWindow(window);
    }

    Q_UNUSED(version); // only serves to disambiguate constructors
}

class KIOWidgetJobUiDelegateFactory : public KIO::JobUiDelegateFactoryV2
{
public:
    using KIO::JobUiDelegateFactoryV2::JobUiDelegateFactoryV2;

    KJobUiDelegate *createDelegate() const override
    {
        return new KIO::JobUiDelegate(KIO::JobUiDelegate::Version::V2);
    }

    KJobUiDelegate *createDelegate(KJobUiDelegate::Flags flags, QWidget *window) const override
    {
        return new KIO::JobUiDelegate(KIO::JobUiDelegate::Version::V2, flags, window);
    }

    static void registerJobUiDelegate()
    {
        static KIOWidgetJobUiDelegateFactory factory;
        KIO::setDefaultJobUiDelegateFactoryV2(&factory);

        static KIO::JobUiDelegate delegate(KIO::JobUiDelegate::Version::V2);
        KIO::setDefaultJobUiDelegateExtension(&delegate);
    }
};

// Simply linking to this library, creates a GUI job delegate and delegate extension for all KIO jobs
static void registerJobUiDelegate()
{
    // Inside the factory class so it is a friend of the delegate and can construct it.
    KIOWidgetJobUiDelegateFactory::registerJobUiDelegate();
}

Q_CONSTRUCTOR_FUNCTION(registerJobUiDelegate)

#include "jobuidelegate.moc"
