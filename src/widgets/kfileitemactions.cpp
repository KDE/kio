/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kfileitemactions.h"
#include "kfileitemactions_p.h"
#include <KMimeTypeTrader>
#include <kdesktopfileactions.h>
#include <KLocalizedString>
#include <kurlauthorized.h>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KServiceTypeTrader>
#include <KAbstractFileItemActionPlugin>
#include <KPluginMetaData>
#include <KIO/ApplicationLauncherJob>
#include <KIO/JobUiDelegate>

#include <QFile>
#include <QMenu>
#include <QMimeDatabase>
#include <QtAlgorithms>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <kio_widgets_debug.h>
#include <algorithm>

static bool KIOSKAuthorizedAction(const KConfigGroup &cfg)
{
    const QStringList list = cfg.readEntry("X-KDE-AuthorizeAction", QStringList());
    return std::all_of(list.constBegin(), list.constEnd(), [](const QString &action) {
         return KAuthorized::authorize(action.trimmed());
    });
}

static bool mimeTypeListContains(const QStringList &list, const KFileItem &item)
{
    const QString itemMimeType = item.mimetype();
    return std::any_of(list.constBegin(), list.constEnd(), [itemMimeType, item](const QString &i) {
        if (i == itemMimeType || i == QLatin1String("all/all")) {
            return true;
        }
        if (item.isFile() && (i == QLatin1String("allfiles") ||
            i == QLatin1String("all/allfiles") || i == QLatin1String("application/octet-stream"))) {
            return true;
        }

        if (item.currentMimeType().inherits(i)) {
            return true;
        }

        const int iSlashPos = i.indexOf(QLatin1Char(QLatin1Char('/')));
        Q_ASSERT(iSlashPos > 0);
        const QStringRef iSubType = i.midRef(iSlashPos+1);

        if (iSubType == QLatin1String("*")) {
            const int itemSlashPos = itemMimeType.indexOf(QLatin1Char('/'));
            Q_ASSERT(itemSlashPos > 0);
            const QStringRef iTopLevelType = i.midRef(0, iSlashPos);
            const QStringRef itemTopLevelType = itemMimeType.midRef(0, itemSlashPos);

            return itemTopLevelType == iTopLevelType;
        }
        return false;
    });
}



// This helper class stores the .desktop-file actions and the servicemenus
// in order to support X-KDE-Priority and X-KDE-Submenu.
namespace KIO
{
class PopupServices
{
public:
    ServiceList &selectList(const QString &priority, const QString &submenuName);

    ServiceList builtin;
    ServiceList user, userToplevel, userPriority;
    QMap<QString, ServiceList> userSubmenus, userToplevelSubmenus, userPrioritySubmenus;
};

ServiceList &PopupServices::selectList(const QString &priority, const QString &submenuName)
{
    // we use the categories .desktop entry to define submenus
    // if none is defined, we just pop it in the main menu
    if (submenuName.isEmpty()) {
        if (priority == QLatin1String("TopLevel")) {
            return userToplevel;
        } else if (priority == QLatin1String("Important")) {
            return userPriority;
        }
    } else if (priority == QLatin1String("TopLevel")) {
        return userToplevelSubmenus[submenuName];
    } else if (priority == QLatin1String("Important")) {
        return userPrioritySubmenus[submenuName];
    } else {
        return userSubmenus[submenuName];
    }
    return user;
}
} // namespace

////

KFileItemActionsPrivate::KFileItemActionsPrivate(KFileItemActions *qq)
    : QObject(),
      q(qq),
      m_executeServiceActionGroup(static_cast<QWidget *>(nullptr)),
      m_runApplicationActionGroup(static_cast<QWidget *>(nullptr)),
      m_parentWidget(nullptr),
      m_config(QStringLiteral("kservicemenurc"), KConfig::NoGlobals)
{
    QObject::connect(&m_executeServiceActionGroup, &QActionGroup::triggered,
                     this, &KFileItemActionsPrivate::slotExecuteService);
    QObject::connect(&m_runApplicationActionGroup, &QActionGroup::triggered,
                     this, &KFileItemActionsPrivate::slotRunApplication);
}

KFileItemActionsPrivate::~KFileItemActionsPrivate()
{
}

int KFileItemActionsPrivate::insertServicesSubmenus(const QMap<QString, ServiceList> &submenus,
        QMenu *menu,
        bool isBuiltin)
{
    int count = 0;
    QMap<QString, ServiceList>::ConstIterator it;
    for (it = submenus.begin(); it != submenus.end(); ++it) {
        if (it.value().isEmpty()) {
            //avoid empty sub-menus
            continue;
        }

        QMenu *actionSubmenu = new QMenu(menu);
        actionSubmenu->setTitle(it.key());
        actionSubmenu->setIcon(QIcon::fromTheme(it.value().first().icon()));
        actionSubmenu->menuAction()->setObjectName(QStringLiteral("services_submenu")); // for the unittest
        menu->addMenu(actionSubmenu);
        count += insertServices(it.value(), actionSubmenu, isBuiltin);
    }

    return count;
}

int KFileItemActionsPrivate::insertServices(const ServiceList &list,
        QMenu *menu,
        bool isBuiltin)
{
    int count = 0;
    for (const KServiceAction &serviceAction : list) {
        if (serviceAction.isSeparator()) {
            const QList<QAction *> actions = menu->actions();
            if (!actions.isEmpty() && !actions.last()->isSeparator()) {
                menu->addSeparator();
            }
            continue;
        }

        if (isBuiltin || !serviceAction.noDisplay()) {
            QAction *act = new QAction(q);
            act->setObjectName(QStringLiteral("menuaction")); // for the unittest
            QString text = serviceAction.text();
            text.replace(QLatin1Char('&'), QLatin1String("&&"));
            act->setText(text);
            if (!serviceAction.icon().isEmpty()) {
                act->setIcon(QIcon::fromTheme(serviceAction.icon()));
            }
            act->setData(QVariant::fromValue(serviceAction));
            m_executeServiceActionGroup.addAction(act);

            menu->addAction(act); // Add to toplevel menu
            ++count;
        }
    }

    return count;
}

void KFileItemActionsPrivate::slotExecuteService(QAction *act)
{
    const KServiceAction serviceAction = act->data().value<KServiceAction>();
    if (KAuthorized::authorizeAction(serviceAction.name())) {
        KDesktopFileActions::executeService(m_props.urlList(), serviceAction);
    }
}

KFileItemActions::KFileItemActions(QObject *parent)
    : QObject(parent), d(new KFileItemActionsPrivate(this))
{
}

KFileItemActions::~KFileItemActions()
{
    delete d;
}

void KFileItemActions::setItemListProperties(const KFileItemListProperties &itemListProperties)
{
    d->m_props = itemListProperties;

    d->m_mimeTypeList.clear();
    const KFileItemList items = d->m_props.items();
    for (const KFileItem &item : items) {
        if (!d->m_mimeTypeList.contains(item.mimetype())) {
            d->m_mimeTypeList << item.mimetype();
        }
    }
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 79)
int KFileItemActions::addServiceActionsTo(QMenu *mainMenu)
{
    return d->addServiceActionsTo(mainMenu, {}, {}).first;
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 79)
int KFileItemActions::addPluginActionsTo(QMenu *mainMenu)
{
    return d->addPluginActionsTo(mainMenu, mainMenu, {});
}
#endif

void KFileItemActions::addActionsTo(QMenu *menu,
                                    MenuActionSources sources,
                                    const QList<QAction *> &additionalActions,
                                    const QStringList &excludeList)
{
    QMenu *actionsMenu = menu;
    if (sources & MenuActionSource::Services) {
        actionsMenu = d->addServiceActionsTo(menu, additionalActions, excludeList).second;
    } else {
        // Since we didn't call addServiceActionsTo(), we have to add additional actions manually
        for (QAction *action : additionalActions) {
            actionsMenu->addAction(action);
        }
    }
    if (sources & MenuActionSource::Plugins) {
        d->addPluginActionsTo(menu, actionsMenu, excludeList);
    }
}

// static
KService::List KFileItemActions::associatedApplications(const QStringList &mimeTypeList, const QString &traderConstraint)
{
    if (!KAuthorized::authorizeAction(QStringLiteral("openwith")) || mimeTypeList.isEmpty()) {
        return KService::List();
    }

    const KService::List firstOffers = KMimeTypeTrader::self()->query(mimeTypeList.first(), QStringLiteral("Application"), traderConstraint);

    QList<KFileItemActionsPrivate::ServiceRank> rankings;
    QStringList serviceList;

    // This section does two things.  First, it determines which services are common to all the given MIME types.
    // Second, it ranks them based on their preference level in the associated applications list.
    // The more often a service appear near the front of the list, the LOWER its score.

    rankings.reserve(firstOffers.count());
    serviceList.reserve(firstOffers.count());
    for (int i = 0; i < firstOffers.count(); ++i) {
        KFileItemActionsPrivate::ServiceRank tempRank;
        tempRank.service = firstOffers[i];
        tempRank.score = i;
        rankings << tempRank;
        serviceList << tempRank.service->storageId();
    }

    for (int j = 1; j < mimeTypeList.count(); ++j) {
        QStringList subservice; // list of services that support this MIME type
        const KService::List offers = KMimeTypeTrader::self()->query(mimeTypeList[j], QStringLiteral("Application"), traderConstraint);
        subservice.reserve(offers.count());
        for (int i = 0; i != offers.count(); ++i) {
            const QString serviceId = offers[i]->storageId();
            subservice << serviceId;
            const int idPos = serviceList.indexOf(serviceId);
            if (idPos != -1) {
                rankings[idPos].score += i;
            } // else: we ignore the services that didn't support the previous MIME types
        }

        // Remove services which supported the previous MIME types but don't support this one
        for (int i = 0; i < serviceList.count(); ++i) {
            if (!subservice.contains(serviceList[i])) {
                serviceList.removeAt(i);
                rankings.removeAt(i);
                --i;
            }
        }
        // Nothing left -> there is no common application for these MIME types
        if (rankings.isEmpty()) {
            return KService::List();
        }
    }

    std::sort(rankings.begin(), rankings.end(), KFileItemActionsPrivate::lessRank);

    KService::List result;
    result.reserve(rankings.size());
    for (const KFileItemActionsPrivate::ServiceRank &tempRank : qAsConst(rankings)) {
        result << tempRank.service;
    }

    return result;
}

// KMimeTypeTrader::preferredService doesn't take a constraint
static KService::Ptr preferredService(const QString &mimeType, const QString &constraint)
{
    const KService::List services = KMimeTypeTrader::self()->query(mimeType, QStringLiteral("Application"), constraint);
    return !services.isEmpty() ? services.first() : KService::Ptr();
}

void KFileItemActions::addOpenWithActionsTo(QMenu *topMenu, const QString &traderConstraint)
{
    insertOpenWithActionsTo(nullptr /* append actions */, topMenu, traderConstraint);
}

void KFileItemActions::insertOpenWithActionsTo(QAction *before, QMenu *topMenu, const QString &traderConstraint)
{
    if (!KAuthorized::authorizeAction(QStringLiteral("openwith"))) {
        return;
    }

    d->m_traderConstraint = traderConstraint;
    KService::List offers = associatedApplications(d->m_mimeTypeList, traderConstraint);

    //// Ok, we have everything, now insert

    const KFileItemList items = d->m_props.items();
    const KFileItem &firstItem = items.first();
    const bool isLocal = firstItem.url().isLocalFile();
    const bool isDir = d->m_props.isDirectory();
    // "Open With..." for folders is really not very useful, especially for remote folders.
    // (media:/something, or trash:/, or ftp://...).
    // Don't show "open with" actions for remote dirs only
    if (isDir && !isLocal) {
        return;
    }

    const QStringList serviceIdList = d->listPreferredServiceIds(d->m_mimeTypeList, traderConstraint);

    // When selecting files with multiple MIME types, offer either "open with <app for all>"
    // or a generic <open> (if there are any apps associated).
    if (d->m_mimeTypeList.count() > 1
            && !serviceIdList.isEmpty()
            && !(serviceIdList.count() == 1 && serviceIdList.first().isEmpty())) { // empty means "no apps associated"

        QAction *runAct = new QAction(this);
        if (serviceIdList.count() == 1) {
            const KService::Ptr app = preferredService(d->m_mimeTypeList.first(), traderConstraint);
            runAct->setText(i18n("&Open with %1", app->name()));
            runAct->setIcon(QIcon::fromTheme(app->icon()));

            // Remove that app from the offers list (#242731)
            for (int i = 0; i < offers.count(); ++i) {
                if (offers[i]->storageId() == app->storageId()) {
                    offers.removeAt(i);
                    break;
                }
            }
        } else {
            runAct->setText(i18n("&Open"));
        }

        QObject::connect(runAct, &QAction::triggered, d, &KFileItemActionsPrivate::slotRunPreferredApplications);
        topMenu->insertAction(before, runAct);

        d->m_traderConstraint = traderConstraint;
        d->m_fileOpenList = d->m_props.items();
    }

    QAction *openWithAct = new QAction(this);
    openWithAct->setText(i18nc("@title:menu", "&Open With..."));
    openWithAct->setObjectName(QStringLiteral("openwith_browse")); // For the unittest
    QObject::connect(openWithAct, &QAction::triggered, d, &KFileItemActionsPrivate::slotOpenWithDialog);

    if (!offers.isEmpty()) {
        // Show the top app inline for files, but not folders
        if (!isDir) {
            QAction *act = d->createAppAction(offers.takeFirst(), true);
            topMenu->insertAction(before, act);
        }

        // If there are still more apps, show them in a sub-menu
        if (!offers.isEmpty()) { // submenu 'open with'
            QMenu *subMenu = new QMenu(i18nc("@title:menu", "&Open With"), topMenu);
            subMenu->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
            subMenu->menuAction()->setObjectName(QStringLiteral("openWith_submenu")); // For the unittest
            // Add other apps to the sub-menu
            for (const KServicePtr &service : qAsConst(offers)) {
                QAction *act = d->createAppAction(service, false);
                subMenu->addAction(act);
            }

            subMenu->addSeparator();

            openWithAct->setText(i18nc("@action:inmenu Open With", "&Other Application..."));
            subMenu->addAction(openWithAct);

            topMenu->insertMenu(before, subMenu);
            topMenu->insertSeparator(before);
        } else { // No other apps
            topMenu->insertAction(before, openWithAct);
        }
    } else { // no app offers -> Open With...
        openWithAct->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
        openWithAct->setObjectName(QStringLiteral("openwith")); // For the unittest
        topMenu->insertAction(before, openWithAct);
    }
}

void KFileItemActionsPrivate::slotRunPreferredApplications()
{
    const KFileItemList fileItems = m_fileOpenList;
    const QStringList mimeTypeList = listMimeTypes(fileItems);
    const QStringList serviceIdList = listPreferredServiceIds(mimeTypeList, m_traderConstraint);

    for (const QString& serviceId : serviceIdList) {
        KFileItemList serviceItems;
        for (const KFileItem &item : fileItems) {
            const KService::Ptr serv = preferredService(item.mimetype(), m_traderConstraint);
            const QString preferredServiceId = serv ? serv->storageId() : QString();
            if (preferredServiceId == serviceId) {
                serviceItems << item;
            }
        }

        if (serviceId.isEmpty()) { // empty means: no associated app for this MIME type
            openWithByMime(serviceItems);
            continue;
        }

        const KService::Ptr servicePtr = KService::serviceByStorageId(serviceId); // can be nullptr
        auto *job = new KIO::ApplicationLauncherJob(servicePtr);
        job->setUrls(serviceItems.urlList());
        job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_parentWidget));
        job->start();
    }
}

void KFileItemActions::runPreferredApplications(const KFileItemList &fileOpenList, const QString &traderConstraint)
{
    d->m_fileOpenList = fileOpenList;
    d->m_traderConstraint = traderConstraint;
    d->slotRunPreferredApplications();
}

void KFileItemActionsPrivate::openWithByMime(const KFileItemList &fileItems)
{
    const QStringList mimeTypeList = listMimeTypes(fileItems);
    for (const QString& mimeType : mimeTypeList) {
        KFileItemList mimeItems;
        for (const KFileItem &item : fileItems) {
            if (item.mimetype() == mimeType) {
                mimeItems << item;
            }
        }
        // Show Open With dialog
        auto *job = new KIO::ApplicationLauncherJob();
        job->setUrls(mimeItems.urlList());
        job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_parentWidget));
        job->start();
    }
}

void KFileItemActionsPrivate::slotRunApplication(QAction *act)
{
    // Is it an application, from one of the "Open With" actions?
    KService::Ptr app = act->data().value<KService::Ptr>();
    Q_ASSERT(app);
    auto *job = new KIO::ApplicationLauncherJob(app);
    job->setUrls(m_props.urlList());
    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_parentWidget));
    job->start();
}

void KFileItemActionsPrivate::slotOpenWithDialog()
{
    // The item 'Other...' or 'Open With...' has been selected
    Q_EMIT q->openWithDialogAboutToBeShown();
    auto *job = new KIO::ApplicationLauncherJob();
    job->setUrls(m_props.urlList());
    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_parentWidget));
    job->start();
}

QStringList KFileItemActionsPrivate::listMimeTypes(const KFileItemList &items)
{
    QStringList mimeTypeList;
    for (const KFileItem &item : items) {
        if (!mimeTypeList.contains(item.mimetype())) {
            mimeTypeList << item.mimetype();
        }
    }
    return mimeTypeList;
}

QStringList KFileItemActionsPrivate::listPreferredServiceIds(const QStringList &mimeTypeList, const QString &traderConstraint)
{
    QStringList serviceIdList;
    serviceIdList.reserve(mimeTypeList.size());
    for (const QString &mimeType : mimeTypeList) {
        const KService::Ptr serv = preferredService(mimeType, traderConstraint);
        const QString newOffer = serv ? serv->storageId() : QString();
        serviceIdList << newOffer;
    }
    serviceIdList.removeDuplicates();
    return serviceIdList;
}

QAction *KFileItemActionsPrivate::createAppAction(const KService::Ptr &service, bool singleOffer)
{
    QString actionName(service->name().replace(QLatin1Char('&'), QLatin1String("&&")));
    if (singleOffer) {
        actionName = i18n("Open &with %1", actionName);
    } else {
        actionName = i18nc("@item:inmenu Open With, %1 is application name", "%1", actionName);
    }

    QAction *act = new QAction(q);
    act->setObjectName(QStringLiteral("openwith")); // for the unittest
    act->setIcon(QIcon::fromTheme(service->icon()));
    act->setText(actionName);
    act->setData(QVariant::fromValue(service));
    m_runApplicationActionGroup.addAction(act);
    return act;
}

bool KFileItemActionsPrivate::shouldDisplayServiceMenu(const KConfigGroup &cfg, const QString &protocol) const
{
    const QList<QUrl> urlList = m_props.urlList();
    if (!KIOSKAuthorizedAction(cfg)) {
        return false;
    }
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 76)
    if (cfg.hasKey("X-KDE-ShowIfRunning")) {
        qCWarning(KIO_WIDGETS) << "The property X-KDE-ShowIfRunning is deprecated and will be removed in future releases";
        const QString app = cfg.readEntry("X-KDE-ShowIfRunning");
        if (QDBusConnection::sessionBus().interface()->isServiceRegistered(app)) {
            return false;
        }
    }
    if (cfg.hasKey("X-KDE-ShowIfDBusCall")) {
        qCWarning(KIO_WIDGETS) << "The property X-KDE-ShowIfDBusCall is deprecated and will be removed in future releases";
        QString calldata = cfg.readEntry("X-KDE-ShowIfDBusCall");
        const QStringList parts = calldata.split(QLatin1Char(' '));
        const QString &app = parts.at(0);
        const QString &obj = parts.at(1);
        QString interface = parts.at(2);
        QString method;
        int pos = interface.lastIndexOf(QLatin1Char('.'));
        if (pos != -1) {
            method = interface.mid(pos + 1);
            interface.truncate(pos);
        }

        QDBusMessage reply = QDBusInterface(app, obj, interface).
            call(method, QUrl::toStringList(urlList));
        if (reply.arguments().count() < 1 || reply.arguments().at(0).type() != QVariant::Bool || !reply.arguments().at(0).toBool()) {
            return false;
        }

    }
#endif
    if (cfg.hasKey("X-KDE-Protocol")) {
        const QString theProtocol = cfg.readEntry("X-KDE-Protocol");
        if (theProtocol.startsWith(QLatin1Char('!'))) {
            const QStringRef excludedProtocol = theProtocol.midRef(1);
            if (excludedProtocol == protocol) {
                return false;
            }
        } else if (protocol != theProtocol) {
            return false;
        }
    } else if (cfg.hasKey("X-KDE-Protocols")) {
        const QStringList protocols = cfg.readEntry("X-KDE-Protocols", QStringList());
        if (!protocols.contains(protocol)) {
            return false;
        }
    } else if (protocol == QLatin1String("trash")) {
        // Require servicemenus for the trash to ask for protocol=trash explicitly.
        // Trashed files aren't supposed to be available for actions.
        // One might want a servicemenu for trash.desktop itself though.
        return false;
    }

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 76)
    if (cfg.hasKey("X-KDE-Require")) {
        qCWarning(KIO_WIDGETS) << "The property X-KDE-Require is deprecated and will be removed in future releases";
        const QStringList capabilities = cfg.readEntry("X-KDE-Require", QStringList());
        if (capabilities.contains(QLatin1String("Write")) && !m_props.supportsWriting()) {
            return false;
        }
    }
#endif

    const auto requiredNumbers = cfg.readEntry("X-KDE-RequiredNumberOfUrls", QList<int>());
    if (!requiredNumbers.isEmpty() && !requiredNumbers.contains(urlList.count())) {
        return false;
    }
    if (cfg.hasKey("X-KDE-MinNumberOfUrls")) {
        const int minNumber = cfg.readEntry("X-KDE-MinNumberOfUrls").toInt();
        if (urlList.count() < minNumber) {
            return false;
        }
    }
    if (cfg.hasKey("X-KDE-MaxNumberOfUrls")) {
        const int maxNumber = cfg.readEntry("X-KDE-MaxNumberOfUrls").toInt();
        if (urlList.count() > maxNumber) {
            return false;
        }
    }
    return true;
}

bool KFileItemActionsPrivate::checkTypesMatch(const KConfigGroup &cfg) const
{
    // Like KService, we support ServiceTypes, X-KDE-ServiceTypes, and MimeType.
    const QStringList types = cfg.readEntry("ServiceTypes", QStringList())
        << cfg.readEntry("X-KDE-ServiceTypes", QStringList())
        << cfg.readXdgListEntry("MimeType");

    if (types.isEmpty()) {
        return false;
    }

    const QStringList excludeTypes = cfg.readEntry("ExcludeServiceTypes", QStringList());
    const KFileItemList items = m_props.items();
    return std::all_of(items.constBegin(), items.constEnd(),
                                [&types, &excludeTypes](const KFileItem &i)
                                {
                                    return mimeTypeListContains(types, i) && !mimeTypeListContains(excludeTypes, i);
                                });

}

QPair<int, QMenu *> KFileItemActionsPrivate::addServiceActionsTo(QMenu *mainMenu,
    const QList<QAction *> &additionalActions,
    const QStringList &excludeList)
{
    const KFileItemList items = m_props.items();
    const KFileItem &firstItem = items.first();
    const QString protocol = firstItem.url().scheme(); // assumed to be the same for all items
    const bool isLocal = !firstItem.localPath().isEmpty();
    const bool isSingleLocal = items.count() == 1 && isLocal;
    const QList<QUrl> urlList = m_props.urlList();

    KIO::PopupServices s;

    // 1 - Look for builtin and user-defined services
    if (isSingleLocal && m_props.mimeType() == QLatin1String("application/x-desktop")) {
        // get builtin services, like mount/unmount
        const QString path = firstItem.localPath();
        s.builtin = KDesktopFileActions::builtinServices(QUrl::fromLocalFile(path));
        const KDesktopFile desktopFile(path);
        const KConfigGroup cfg = desktopFile.desktopGroup();
        const QString priority = cfg.readEntry("X-KDE-Priority");
        const QString submenuName = cfg.readEntry("X-KDE-Submenu");
        ServiceList &list = s.selectList(priority, submenuName);
        list = KDesktopFileActions::userDefinedServices(path, desktopFile, true /*isLocal*/);
    }

    // 2 - Look for "servicemenus" bindings (user-defined services)

    // first check the .directory if this is a directory
    if (m_props.isDirectory() && isSingleLocal) {
        const QString dotDirectoryFile = QUrl::fromLocalFile(firstItem.localPath()).path().append(QLatin1String("/.directory"));
        if (QFile::exists(dotDirectoryFile)) {
            const KDesktopFile desktopFile(dotDirectoryFile);
            const KConfigGroup cfg = desktopFile.desktopGroup();

            if (KIOSKAuthorizedAction(cfg)) {
                const QString priority = cfg.readEntry("X-KDE-Priority");
                const QString submenuName = cfg.readEntry("X-KDE-Submenu");
                ServiceList &list = s.selectList(priority, submenuName);
                list += KDesktopFileActions::userDefinedServices(dotDirectoryFile, desktopFile, true);
            }
        }
    }

    const KConfigGroup showGroup = m_config.group("Show");

    const QMimeDatabase db;
    const KService::List entries = KServiceTypeTrader::self()->query(QStringLiteral("KonqPopupMenu/Plugin"));
    for (const KServicePtr &entry : entries) {
        QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("kservices5/") + entry->entryPath());
        const KDesktopFile desktopFile(file);
        const KConfigGroup cfg = desktopFile.desktopGroup();

        if (!shouldDisplayServiceMenu(cfg, protocol)) {
            continue;
        }

        if (cfg.hasKey("Actions") || cfg.hasKey("X-KDE-GetActionMenu")) {
            if (!checkTypesMatch(cfg)) {
                continue;
            }

            const QString priority = cfg.readEntry("X-KDE-Priority");
            const QString submenuName = cfg.readEntry("X-KDE-Submenu");

            ServiceList &list = s.selectList(priority, submenuName);
            const ServiceList userServices = KDesktopFileActions::userDefinedServices(*entry, isLocal, urlList);
            for (const KServiceAction &action : userServices) {
                if (showGroup.readEntry(action.name(), true) && !excludeList.contains(action.name())) {
                    list += action;
                }
            }
        }
    }

    QMenu *actionMenu = mainMenu;
    int userItemCount = 0;
    if (s.user.count() + s.userSubmenus.count() +
        s.userPriority.count() + s.userPrioritySubmenus.count() + additionalActions.count() > 3) {
        // we have more than three items, so let's make a submenu
        actionMenu = new QMenu(i18nc("@title:menu", "&Actions"), mainMenu);
        actionMenu->setIcon(QIcon::fromTheme(QStringLiteral("view-more-symbolic")));
        actionMenu->menuAction()->setObjectName(QStringLiteral("actions_submenu")); // for the unittest
        mainMenu->addMenu(actionMenu);
    }

    userItemCount += additionalActions.count();
    for (QAction *action : additionalActions) {
        actionMenu->addAction(action);
    }
    userItemCount += insertServicesSubmenus(s.userPrioritySubmenus, actionMenu, false);
    userItemCount += insertServices(s.userPriority, actionMenu, false);
    userItemCount += insertServicesSubmenus(s.userSubmenus, actionMenu, false);
    userItemCount += insertServices(s.user, actionMenu, false);
    userItemCount += insertServices(s.builtin, mainMenu, true);
    userItemCount += insertServicesSubmenus(s.userToplevelSubmenus, mainMenu, false);
    userItemCount += insertServices(s.userToplevel, mainMenu, false);

    return {userItemCount, actionMenu};
}

int KFileItemActionsPrivate::addPluginActionsTo(QMenu *mainMenu,
    QMenu *actionsMenu,
    const QStringList &excludeList)
{
    QString commonMimeType = m_props.mimeType();
    if (commonMimeType.isEmpty() && m_props.isFile()) {
        commonMimeType = QStringLiteral("application/octet-stream");
    }

    QStringList addedPlugins;
    int itemCount = 0;

    const KConfigGroup showGroup = m_config.group("Show");
    const KService::List fileItemPlugins = KMimeTypeTrader::self()->query(commonMimeType, QStringLiteral("KFileItemAction/Plugin"), QStringLiteral("exist Library"));
    for(const auto &service : fileItemPlugins) {
        if (!showGroup.readEntry(service->desktopEntryName(), true)) {
            // The plugin has been disabled
            continue;
        }

        auto *abstractPlugin = service->createInstance<KAbstractFileItemActionPlugin>();
        if (abstractPlugin) {
            abstractPlugin->setParent(mainMenu);
            auto actions = abstractPlugin->actions(m_props, m_parentWidget);
            itemCount += actions.count();
            mainMenu->addActions(actions);
            addedPlugins.append(service->desktopEntryName());
        }
    }

    const QMimeDatabase db;
    const auto jsonPlugins = KPluginLoader::findPlugins(QStringLiteral("kf5/kfileitemaction"), [&db, commonMimeType](const KPluginMetaData& metaData) {
        if (!metaData.serviceTypes().contains(QLatin1String("KFileItemAction/Plugin"))) {
            return false;
        }

        auto mimeType = db.mimeTypeForName(commonMimeType);
        const QStringList list = metaData.mimeTypes();
        return std::any_of(list.constBegin(), list.constEnd(), [mimeType](const QString &supportedMimeType) {
            return mimeType.inherits(supportedMimeType);
        });
    });

    for (const auto &jsonMetadata : jsonPlugins) {
        // The plugin has been disabled
        const QString pluginId = jsonMetadata.pluginId();
        if (!showGroup.readEntry(pluginId, true) || excludeList.contains(pluginId)) {
            continue;
        }

        // The plugin also has a .desktop file and has already been added.
        if (addedPlugins.contains(jsonMetadata.pluginId())) {
            continue;
        }

        KPluginFactory *factory = KPluginLoader(jsonMetadata.fileName()).factory();
        if (!factory) {
            continue;
        }
        auto *abstractPlugin = factory->create<KAbstractFileItemActionPlugin>();
        if (abstractPlugin) {
            abstractPlugin->setParent(this);
            const QList<QAction *> actions = abstractPlugin->actions(m_props, m_parentWidget);
            itemCount += actions.count();
            const QString showInSubmenu = jsonMetadata.value(QStringLiteral("X-KDE-Show-In-Submenu"));
            if (showInSubmenu == QLatin1String("true")) {
                actionsMenu->addActions(actions);
            } else {
                mainMenu->addActions(actions);
            }
            addedPlugins.append(jsonMetadata.pluginId());
        }
    }

    return itemCount;
}

QAction *KFileItemActions::preferredOpenWithAction(const QString &traderConstraint)
{
    const KService::List offers = associatedApplications(d->m_mimeTypeList, traderConstraint);
    if (offers.isEmpty()) {
        return nullptr;
    }
    return d->createAppAction(offers.first(), true);
}

void KFileItemActions::setParentWidget(QWidget *widget)
{
    d->m_parentWidget = widget;
}
