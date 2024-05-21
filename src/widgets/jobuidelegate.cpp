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

#ifdef WITH_QTDBUS
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
#ifdef WITH_QTDBUS
            QDBusInterface(QStringLiteral("org.kde.kded6"), QStringLiteral("/kded"), QStringLiteral("org.kde.kded6"))
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
#ifdef WITH_QTDBUS
        QDBusInterface(QStringLiteral("org.kde.kded6"), QStringLiteral("/kded"), QStringLiteral("org.kde.kded6"))
            .call(QDBus::NoBlock, QStringLiteral("unregisterWindowId"), qlonglong(windowId));
#endif
    }

private:
    QMap<QObject *, WId> m_windowList;
};

Q_GLOBAL_STATIC(JobUiDelegateStatic, s_static)

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

        ask = kioConfig->group(QStringLiteral("Confirmations")).readEntry(keyName, defaultValue);
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
            KConfigGroup notificationGroup(config, QStringLiteral("Notification Messages"));
            if (!notificationGroup.readEntry(keyName, true)) {
                notificationGroup.writeEntry(keyName, true);
                notificationGroup.sync();

                KSharedConfigPtr kioConfig = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals);
                kioConfig->group(QStringLiteral("Confirmations")).writeEntry(keyName, false);
            }
        }
        return (result == KMessageBox::Continue);
    }
    return true;
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

KIO::JobUiDelegate::JobUiDelegate(KJobUiDelegate::Flags flags, QWidget *window, const QList<QObject *> &ifaces)
    : KDialogJobUiDelegate(flags, window)
    , d(new JobUiDelegatePrivate(this, ifaces))
{
    // TODO KF6: change the API to accept QWindows rather than QWidgets (this also carries through to the Interfaces)
    if (window) {
        s_static()->registerWindow(window);
        setWindow(window);
    }
}

class KIOWidgetJobUiDelegateFactory : public KIO::JobUiDelegateFactory
{
public:
    using KIO::JobUiDelegateFactory::JobUiDelegateFactory;

    KJobUiDelegate *createDelegate() const override
    {
        return new KIO::JobUiDelegate;
    }

    KJobUiDelegate *createDelegate(KJobUiDelegate::Flags flags, QWidget *window) const override
    {
        return new KIO::JobUiDelegate(flags, window);
    }

    static void registerJobUiDelegate()
    {
        static KIOWidgetJobUiDelegateFactory factory;
        KIO::setDefaultJobUiDelegateFactory(&factory);

        static KIO::JobUiDelegate delegate;
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
#include "moc_jobuidelegate.cpp"
