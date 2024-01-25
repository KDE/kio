/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998-2009 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2021 Alexander Lohnau <alexander.lohnau@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kfileitemactions.h"
#include "kfileitemactions_p.h"
#include <KAbstractFileItemActionPlugin>
#include <KApplicationTrader>
#include <KAuthorized>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KDesktopFileAction>
#include <KFileUtils>
#include <KIO/ApplicationLauncherJob>
#include <KIO/JobUiDelegate>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <KSandbox>
#include <jobuidelegatefactory.h>
#include <kapplicationtrader.h>
#include <kdirnotify.h>
#include <kurlauthorized.h>

#include <QFile>
#include <QMenu>
#include <QMimeDatabase>
#include <QtAlgorithms>

#ifdef WITH_QTDBUS
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#endif
#include <algorithm>
#include <kio_widgets_debug.h>
#include <set>

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
    return std::any_of(list.cbegin(), list.cend(), [&](const QString &mt) {
        if (mt == itemMimeType || mt == QLatin1String("all/all")) {
            return true;
        }

        if (item.isFile() //
            && (mt == QLatin1String("allfiles") || mt == QLatin1String("all/allfiles") || mt == QLatin1String("application/octet-stream"))) {
            return true;
        }

        if (item.currentMimeType().inherits(mt)) {
            return true;
        }

        if (mt.endsWith(QLatin1String("/*"))) {
            const int slashPos = mt.indexOf(QLatin1Char('/'));
            const auto topLevelType = QStringView(mt).mid(0, slashPos);
            return itemMimeType.startsWith(topLevelType);
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

    ServiceList user;
    ServiceList userToplevel;
    ServiceList userPriority;

    QMap<QString, ServiceList> userSubmenus;
    QMap<QString, ServiceList> userToplevelSubmenus;
    QMap<QString, ServiceList> userPrioritySubmenus;
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
    : QObject()
    , q(qq)
    , m_executeServiceActionGroup(static_cast<QWidget *>(nullptr))
    , m_runApplicationActionGroup(static_cast<QWidget *>(nullptr))
    , m_parentWidget(nullptr)
    , m_config(QStringLiteral("kservicemenurc"), KConfig::NoGlobals)
{
    QObject::connect(&m_executeServiceActionGroup, &QActionGroup::triggered, this, &KFileItemActionsPrivate::slotExecuteService);
    QObject::connect(&m_runApplicationActionGroup, &QActionGroup::triggered, this, &KFileItemActionsPrivate::slotRunApplication);
}

KFileItemActionsPrivate::~KFileItemActionsPrivate()
{
}

int KFileItemActionsPrivate::insertServicesSubmenus(const QMap<QString, ServiceList> &submenus, QMenu *menu)
{
    int count = 0;
    QMap<QString, ServiceList>::ConstIterator it;
    for (it = submenus.begin(); it != submenus.end(); ++it) {
        if (it.value().isEmpty()) {
            // avoid empty sub-menus
            continue;
        }

        QMenu *actionSubmenu = new QMenu(menu);
        const int servicesAddedCount = insertServices(it.value(), actionSubmenu);

        if (servicesAddedCount > 0) {
            count += servicesAddedCount;
            actionSubmenu->setTitle(it.key());
            actionSubmenu->setIcon(QIcon::fromTheme(it.value().first().icon()));
            actionSubmenu->menuAction()->setObjectName(QStringLiteral("services_submenu")); // for the unittest
            menu->addMenu(actionSubmenu);
        } else {
            // avoid empty sub-menus
            delete actionSubmenu;
        }
    }

    return count;
}

int KFileItemActionsPrivate::insertServices(const ServiceList &list, QMenu *menu)
{
    // Temporary storage for current group and all groups
    ServiceList currentGroup;
    std::vector<ServiceList> allGroups;

    // Grouping
    for (const KDesktopFileAction &serviceAction : std::as_const(list)) {
        if (serviceAction.isSeparator()) {
            if (!currentGroup.empty()) {
                allGroups.push_back(currentGroup);
                currentGroup.clear();
            }
            // Push back a dummy list to represent a separator for later
            allGroups.push_back(ServiceList());
        } else {
            currentGroup.push_back(serviceAction);
        }
    }
    // Don't forget to add the last group if it exists
    if (!currentGroup.empty()) {
        allGroups.push_back(currentGroup);
    }

    // Sort each group
    for (ServiceList &group : allGroups) {
        std::sort(group.begin(), group.end(), [](const KDesktopFileAction &a1, const KDesktopFileAction &a2) {
            return a1.name() < a2.name();
        });
    }

    int count = 0;
    for (const ServiceList &group : allGroups) {
        // Check if the group is a separator
        if (group.empty()) {
            const QList<QAction *> actions = menu->actions();
            if (!actions.isEmpty() && !actions.last()->isSeparator()) {
                menu->addSeparator();
            }
            continue;
        }

        // Insert sorted actions for current group
        for (const KDesktopFileAction &serviceAction : group) {
            QAction *act = new QAction(q);
            act->setObjectName(QStringLiteral("menuaction")); // for the unittest
            QString text = serviceAction.name();
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
    const KDesktopFileAction serviceAction = act->data().value<KDesktopFileAction>();
    if (KAuthorized::authorizeAction(serviceAction.name())) {
        auto *job = new KIO::ApplicationLauncherJob(serviceAction);
        job->setUrls(m_props.urlList());
        job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_parentWidget));
        job->start();
    }
}

KFileItemActions::KFileItemActions(QObject *parent)
    : QObject(parent)
    , d(new KFileItemActionsPrivate(this))
{
}

KFileItemActions::~KFileItemActions() = default;

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

void KFileItemActions::addActionsTo(QMenu *menu, MenuActionSources sources, const QList<QAction *> &additionalActions, const QStringList &excludeList)
{
    QMenu *actionsMenu = menu;
    if (sources & MenuActionSource::Services) {
        actionsMenu = d->addServiceActionsTo(menu, additionalActions, excludeList).menu;
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
KService::List KFileItemActions::associatedApplications(const QStringList &mimeTypeList)
{
    return KFileItemActionsPrivate::associatedApplications(mimeTypeList, QStringList{});
}

static KService::Ptr preferredService(const QString &mimeType, const QStringList &excludedDesktopEntryNames)
{
    KService::List services = KApplicationTrader::queryByMimeType(mimeType, [&](const KService::Ptr &serv) {
        return !excludedDesktopEntryNames.contains(serv->desktopEntryName());
    });
    return services.isEmpty() ? KService::Ptr() : services.first();
}

void KFileItemActions::insertOpenWithActionsTo(QAction *before, QMenu *topMenu, const QStringList &excludedDesktopEntryNames)
{
    d->insertOpenWithActionsTo(before, topMenu, excludedDesktopEntryNames);
}

void KFileItemActionsPrivate::slotRunPreferredApplications()
{
    const KFileItemList fileItems = m_fileOpenList;
    const QStringList mimeTypeList = listMimeTypes(fileItems);
    const QStringList serviceIdList = listPreferredServiceIds(mimeTypeList, QStringList());

    for (const QString &serviceId : serviceIdList) {
        KFileItemList serviceItems;
        for (const KFileItem &item : fileItems) {
            const KService::Ptr serv = preferredService(item.mimetype(), QStringList());
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
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_parentWidget));
        job->start();
    }
}

void KFileItemActions::runPreferredApplications(const KFileItemList &fileOpenList)
{
    d->m_fileOpenList = fileOpenList;
    d->slotRunPreferredApplications();
}

void KFileItemActionsPrivate::openWithByMime(const KFileItemList &fileItems)
{
    const QStringList mimeTypeList = listMimeTypes(fileItems);
    for (const QString &mimeType : mimeTypeList) {
        KFileItemList mimeItems;
        for (const KFileItem &item : fileItems) {
            if (item.mimetype() == mimeType) {
                mimeItems << item;
            }
        }
        // Show Open With dialog
        auto *job = new KIO::ApplicationLauncherJob();
        job->setUrls(mimeItems.urlList());
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_parentWidget));
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
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_parentWidget));
    job->start();
}

void KFileItemActionsPrivate::slotOpenWithDialog()
{
    // The item 'Other...' or 'Open With...' has been selected
    Q_EMIT q->openWithDialogAboutToBeShown();
    auto *job = new KIO::ApplicationLauncherJob();
    job->setUrls(m_props.urlList());
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_parentWidget));
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

QStringList KFileItemActionsPrivate::listPreferredServiceIds(const QStringList &mimeTypeList, const QStringList &excludedDesktopEntryNames)
{
    QStringList serviceIdList;
    serviceIdList.reserve(mimeTypeList.size());
    for (const QString &mimeType : mimeTypeList) {
        const KService::Ptr serv = preferredService(mimeType, excludedDesktopEntryNames);
        serviceIdList << (serv ? serv->storageId() : QString()); // empty string means mimetype has no associated apps
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
    if (cfg.hasKey("X-KDE-Protocol")) {
        const QString theProtocol = cfg.readEntry("X-KDE-Protocol");
        if (theProtocol.startsWith(QLatin1Char('!'))) { // Is it excluded?
            if (QStringView(theProtocol).mid(1) == protocol) {
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
    QStringList types = cfg.readXdgListEntry("MimeType");
    if (types.isEmpty()) {
        types = cfg.readEntry("ServiceTypes", QStringList());
        types.removeAll(QStringLiteral("KonqPopupMenu/Plugin"));

        if (types.isEmpty()) {
            return false;
        }
    }

    const QStringList excludeTypes = cfg.readEntry("ExcludeServiceTypes", QStringList());
    const KFileItemList items = m_props.items();
    return std::all_of(items.constBegin(), items.constEnd(), [&types, &excludeTypes](const KFileItem &i) {
        return mimeTypeListContains(types, i) && !mimeTypeListContains(excludeTypes, i);
    });
}

KFileItemActionsPrivate::ServiceActionInfo
KFileItemActionsPrivate::addServiceActionsTo(QMenu *mainMenu, const QList<QAction *> &additionalActions, const QStringList &excludeList)
{
    const KFileItemList items = m_props.items();
    const KFileItem &firstItem = items.first();
    const QString protocol = firstItem.url().scheme(); // assumed to be the same for all items
    const bool isLocal = !firstItem.localPath().isEmpty();

    KIO::PopupServices s;

    // 2 - Look for "servicemenus" bindings (user-defined services)

    // first check the .directory if this is a directory
    const bool isSingleLocal = items.count() == 1 && isLocal;
    if (m_props.isDirectory() && isSingleLocal) {
        const QString dotDirectoryFile = QUrl::fromLocalFile(firstItem.localPath()).path().append(QLatin1String("/.directory"));
        if (QFile::exists(dotDirectoryFile)) {
            const KDesktopFile desktopFile(dotDirectoryFile);
            const KConfigGroup cfg = desktopFile.desktopGroup();

            if (KIOSKAuthorizedAction(cfg)) {
                const QString priority = cfg.readEntry("X-KDE-Priority");
                const QString submenuName = cfg.readEntry("X-KDE-Submenu");
                ServiceList &list = s.selectList(priority, submenuName);
                list += desktopFile.actions();
            }
        }
    }

    const KConfigGroup showGroup = m_config.group(QStringLiteral("Show"));

    const QMimeDatabase db;
    const QStringList files = serviceMenuFilePaths();
    for (const QString &file : files) {
        const KDesktopFile desktopFile(file);
        const KConfigGroup cfg = desktopFile.desktopGroup();
        if (!shouldDisplayServiceMenu(cfg, protocol)) {
            continue;
        }

        const QList<KDesktopFileAction> actions = desktopFile.actions();
        if (!actions.isEmpty()) {
            if (!checkTypesMatch(cfg)) {
                continue;
            }

            const QString priority = cfg.readEntry("X-KDE-Priority");
            const QString submenuName = cfg.readEntry("X-KDE-Submenu");

            ServiceList &list = s.selectList(priority, submenuName);
            std::copy_if(actions.cbegin(), actions.cend(), std::back_inserter(list), [&excludeList, &showGroup](const KDesktopFileAction &srvAction) {
                return showGroup.readEntry(srvAction.actionsKey(), true) && !excludeList.contains(srvAction.actionsKey());
            });
        }
    }

    QMenu *actionMenu = mainMenu;
    int userItemCount = 0;
    if (s.user.count() + s.userSubmenus.count() + s.userPriority.count() + s.userPrioritySubmenus.count() + additionalActions.count() > 3) {
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
    userItemCount += insertServicesSubmenus(s.userPrioritySubmenus, actionMenu);
    userItemCount += insertServices(s.userPriority, actionMenu);
    userItemCount += insertServicesSubmenus(s.userSubmenus, actionMenu);
    userItemCount += insertServices(s.user, actionMenu);

    userItemCount += insertServicesSubmenus(s.userToplevelSubmenus, mainMenu);
    userItemCount += insertServices(s.userToplevel, mainMenu);

    return {userItemCount, actionMenu};
}

int KFileItemActionsPrivate::addPluginActionsTo(QMenu *mainMenu, QMenu *actionsMenu, const QStringList &excludeList)
{
    QString commonMimeType = m_props.mimeType();
    if (commonMimeType.isEmpty() && m_props.isFile()) {
        commonMimeType = QStringLiteral("application/octet-stream");
    }

    int itemCount = 0;

    const KConfigGroup showGroup = m_config.group(QStringLiteral("Show"));

    const QMimeDatabase db;
    const auto jsonPlugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/kfileitemaction"), [&db, commonMimeType](const KPluginMetaData &metaData) {
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

        KAbstractFileItemActionPlugin *abstractPlugin = m_loadedPlugins.value(pluginId);
        if (!abstractPlugin) {
            abstractPlugin = KPluginFactory::instantiatePlugin<KAbstractFileItemActionPlugin>(jsonMetadata, this).plugin;
            m_loadedPlugins.insert(pluginId, abstractPlugin);
        }
        if (abstractPlugin) {
            connect(abstractPlugin, &KAbstractFileItemActionPlugin::error, q, &KFileItemActions::error);
            const QList<QAction *> actions = abstractPlugin->actions(m_props, m_parentWidget);
            itemCount += actions.count();
            const QString showInSubmenu = jsonMetadata.value(QStringLiteral("X-KDE-Show-In-Submenu"));
            if (showInSubmenu == QLatin1String("true")) {
                actionsMenu->addActions(actions);
            } else {
                mainMenu->addActions(actions);
            }
        }
    }

    return itemCount;
}

KService::List KFileItemActionsPrivate::associatedApplications(const QStringList &mimeTypeList, const QStringList &excludedDesktopEntryNames)
{
    if (!KAuthorized::authorizeAction(QStringLiteral("openwith")) || mimeTypeList.isEmpty()) {
        return KService::List();
    }

    KService::List firstOffers = KApplicationTrader::queryByMimeType(mimeTypeList.first(), [excludedDesktopEntryNames](const KService::Ptr &service) {
        return !excludedDesktopEntryNames.contains(service->desktopEntryName());
    });

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
        KService::List offers = KApplicationTrader::queryByMimeType(mimeTypeList[j], [excludedDesktopEntryNames](const KService::Ptr &service) {
            return !excludedDesktopEntryNames.contains(service->desktopEntryName());
        });

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
    for (const KFileItemActionsPrivate::ServiceRank &tempRank : std::as_const(rankings)) {
        result << tempRank.service;
    }

    return result;
}

void KFileItemActionsPrivate::insertOpenWithActionsTo(QAction *before, QMenu *topMenu, const QStringList &excludedDesktopEntryNames)
{
    if (!KAuthorized::authorizeAction(QStringLiteral("openwith"))) {
        return;
    }

    // TODO Overload with excludedDesktopEntryNames, but this method in public API and will be handled in a new MR
    KService::List offers = associatedApplications(m_mimeTypeList, excludedDesktopEntryNames);

    //// Ok, we have everything, now insert

    const KFileItemList items = m_props.items();
    const KFileItem &firstItem = items.first();
    const bool isLocal = firstItem.url().isLocalFile();
    const bool isDir = m_props.isDirectory();
    // "Open With..." for folders is really not very useful, especially for remote folders.
    // (media:/something, or trash:/, or ftp://...).
    // Don't show "open with" actions for remote dirs only
    if (isDir && !isLocal) {
        return;
    }

    const auto makeOpenWithAction = [this, isDir] {
        auto action = new QAction(this);
        action->setText(isDir ? i18nc("@action:inmenu", "&Open Folder With…") : i18nc("@action:inmenu", "&Open With…"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
        action->setObjectName(QStringLiteral("openwith_browse")); // For the unittest
        return action;
    };

#ifdef WITH_QTDBUS
    if (KSandbox::isInside() && !m_fileOpenList.isEmpty()) {
        auto openWithAction = makeOpenWithAction();
        QObject::connect(openWithAction, &QAction::triggered, this, [this] {
            const auto &items = m_fileOpenList;
            for (const auto &fileItem : items) {
                QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String("org.freedesktop.portal.Desktop"),
                                                                      QLatin1String("/org/freedesktop/portal/desktop"),
                                                                      QLatin1String("org.freedesktop.portal.OpenURI"),
                                                                      QLatin1String("OpenURI"));
                message << QString() << fileItem.url() << QVariantMap{};
                QDBusConnection::sessionBus().asyncCall(message);
            }
        });
        topMenu->insertAction(before, openWithAction);
        return;
    }
    if (KSandbox::isInside()) {
        return;
    }
#endif

    QStringList serviceIdList = listPreferredServiceIds(m_mimeTypeList, excludedDesktopEntryNames);

    // When selecting files with multiple MIME types, offer either "open with <app for all>"
    // or a generic <open> (if there are any apps associated).
    if (m_mimeTypeList.count() > 1 && !serviceIdList.isEmpty()
        && !(serviceIdList.count() == 1 && serviceIdList.first().isEmpty())) { // empty means "no apps associated"

        QAction *runAct = new QAction(this);
        if (serviceIdList.count() == 1) {
            const KService::Ptr app = preferredService(m_mimeTypeList.first(), excludedDesktopEntryNames);
            runAct->setText(isDir ? i18n("&Open folder with %1", app->name()) : i18n("&Open with %1", app->name()));
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

        QObject::connect(runAct, &QAction::triggered, this, &KFileItemActionsPrivate::slotRunPreferredApplications);
        topMenu->insertAction(before, runAct);

        m_fileOpenList = m_props.items();
    }

    auto openWithAct = makeOpenWithAction();
    QObject::connect(openWithAct, &QAction::triggered, this, &KFileItemActionsPrivate::slotOpenWithDialog);

    if (!offers.isEmpty()) {
        // Show the top app inline for files, but not folders
        if (!isDir) {
            QAction *act = createAppAction(offers.takeFirst(), true);
            topMenu->insertAction(before, act);
        }

        // If there are still more apps, show them in a sub-menu
        if (!offers.isEmpty()) { // submenu 'open with'
            QMenu *subMenu = new QMenu(isDir ? i18nc("@title:menu", "&Open Folder With") : i18nc("@title:menu", "&Open With"), topMenu);
            subMenu->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
            subMenu->menuAction()->setObjectName(QStringLiteral("openWith_submenu")); // For the unittest
            // Add other apps to the sub-menu
            for (const KServicePtr &service : std::as_const(offers)) {
                QAction *act = createAppAction(service, false);
                subMenu->addAction(act);
            }

            subMenu->addSeparator();

            openWithAct->setText(i18nc("@action:inmenu Open With", "&Other Application…"));
            subMenu->addAction(openWithAct);

            topMenu->insertMenu(before, subMenu);
        } else { // No other apps
            topMenu->insertAction(before, openWithAct);
        }
    } else { // no app offers -> Open With...
        openWithAct->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
        openWithAct->setObjectName(QStringLiteral("openwith")); // For the unittest
        topMenu->insertAction(before, openWithAct);
    }

    if (m_props.mimeType() == QLatin1String("application/x-desktop")) {
        const QString path = firstItem.localPath();
        const ServiceList services = KDesktopFile(path).actions();
        for (const KDesktopFileAction &serviceAction : services) {
            QAction *action = new QAction(this);
            action->setText(serviceAction.name());
            action->setIcon(QIcon::fromTheme(serviceAction.icon()));

            connect(action, &QAction::triggered, this, [serviceAction] {
                if (KAuthorized::authorizeAction(serviceAction.name())) {
                    auto *job = new KIO::ApplicationLauncherJob(serviceAction);
                    job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
                    job->start();
                }
            });

            topMenu->addAction(action);
        }
    }

    topMenu->insertSeparator(before);
}

QStringList KFileItemActionsPrivate::serviceMenuFilePaths()
{
    QStringList filePaths;

    std::set<QString> uniqueFileNames;

    // Load servicemenus from new install location
    const QStringList paths =
        QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("kio/servicemenus"), QStandardPaths::LocateDirectory);
    QStringList fromDisk = KFileUtils::findAllUniqueFiles(paths, QStringList(QStringLiteral("*.desktop")));

    // Also search in kservices5 for compatibility with older existing files
    const QStringList legacyPaths =
        QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("kservices5"), QStandardPaths::LocateDirectory);
    const QStringList legacyFiles = KFileUtils::findAllUniqueFiles(legacyPaths, QStringList(QStringLiteral("*.desktop")));

    for (const QString &path : legacyFiles) {
        KDesktopFile file(path);

        const QStringList serviceTypes = file.desktopGroup().readEntry("ServiceTypes", QStringList());
        if (serviceTypes.contains(QStringLiteral("KonqPopupMenu/Plugin"))) {
            fromDisk << path;
        }
    }

    for (const QString &fileFromDisk : std::as_const(fromDisk)) {
        if (auto [_, inserted] = uniqueFileNames.insert(fileFromDisk.split(QLatin1Char('/')).last()); inserted) {
            filePaths << fileFromDisk;
        }
    }
    return filePaths;
}

void KFileItemActions::setParentWidget(QWidget *widget)
{
    d->m_parentWidget = widget;
}

#include "moc_kfileitemactions.cpp"
#include "moc_kfileitemactions_p.cpp"
