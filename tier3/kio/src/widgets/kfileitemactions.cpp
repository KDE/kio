/* This file is part of the KDE project
   Copyright (C) 1998-2009 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2 of the License or
   ( at your option ) version 3 or, at the discretion of KDE e.V.
   ( which shall act as a proxy as in section 14 of the GPLv3 ), any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kfileitemactions.h"
#include "kfileitemactions_p.h"
#include <krun.h>
#include <kmimetypetrader.h>
#include <kdesktopfileactions.h>
#include <klocalizedstring.h>
#include <kurlauthorized.h>
#include <kconfiggroup.h>
#include <kdesktopfile.h>
#include <kservicetypetrader.h>
#include <QFile>
#include <QMenu>
#include <qmimedatabase.h>
#include <QtAlgorithms>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>

static bool KIOSKAuthorizedAction(const KConfigGroup& cfg)
{
    if (!cfg.hasKey("X-KDE-AuthorizeAction")) {
        return true;
    }
    const QStringList list = cfg.readEntry("X-KDE-AuthorizeAction", QStringList());
    for(QStringList::ConstIterator it = list.constBegin();
        it != list.constEnd(); ++it) {
        if (!KAuthorized::authorize((*it).trimmed())) {
            return false;
        }
    }
    return true;
}

// This helper class stores the .desktop-file actions and the servicemenus
// in order to support X-KDE-Priority and X-KDE-Submenu.
namespace KIO {
class PopupServices
{
public:
    ServiceList& selectList(const QString& priority, const QString& submenuName);

    ServiceList builtin;
    ServiceList user, userToplevel, userPriority;
    QMap<QString, ServiceList> userSubmenus, userToplevelSubmenus, userPrioritySubmenus;
};

ServiceList& PopupServices::selectList(const QString& priority, const QString& submenuName)
{
    // we use the categories .desktop entry to define submenus
    // if none is defined, we just pop it in the main menu
    if (submenuName.isEmpty()) {
        if (priority == "TopLevel") {
            return userToplevel;
        } else if (priority == "Important") {
            return userPriority;
        }
    } else if (priority == "TopLevel") {
        return userToplevelSubmenus[submenuName];
    } else if (priority == "Important") {
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
      m_executeServiceActionGroup(static_cast<QWidget *>(0)),
      m_runApplicationActionGroup(static_cast<QWidget *>(0)),
      m_parentWidget(0)
{
    QObject::connect(&m_executeServiceActionGroup, SIGNAL(triggered(QAction*)),
                     this, SLOT(slotExecuteService(QAction*)));
    QObject::connect(&m_runApplicationActionGroup, SIGNAL(triggered(QAction*)),
                     this, SLOT(slotRunApplication(QAction*)));
}

KFileItemActionsPrivate::~KFileItemActionsPrivate()
{
    qDeleteAll(m_ownActions);
}

int KFileItemActionsPrivate::insertServicesSubmenus(const QMap<QString, ServiceList>& submenus,
                                                   QMenu* menu,
                                                   bool isBuiltin)
{
    int count = 0;
    QMap<QString, ServiceList>::ConstIterator it;
    for (it = submenus.begin(); it != submenus.end(); ++it) {
        if (it.value().isEmpty()) {
            //avoid empty sub-menus
            continue;
        }

        QMenu* actionSubmenu = new QMenu(menu);
        actionSubmenu->setTitle(it.key());
        actionSubmenu->menuAction()->setObjectName("services_submenu"); // for the unittest
        menu->addMenu(actionSubmenu);
        count += insertServices(it.value(), actionSubmenu, isBuiltin);
    }

    return count;
}

int KFileItemActionsPrivate::insertServices(const ServiceList& list,
                                           QMenu* menu,
                                           bool isBuiltin)
{
    int count = 0;
    ServiceList::const_iterator it = list.begin();
    for(; it != list.end(); ++it) {
        if ((*it).isSeparator()) {
            const QList<QAction*> actions = menu->actions();
            if (!actions.isEmpty() && !actions.last()->isSeparator()) {
                menu->addSeparator();
            }
            continue;
        }

        if (isBuiltin || !(*it).noDisplay()) {
            QAction *act = new QAction(m_parentWidget);
            m_ownActions.append(act);
            act->setObjectName("menuaction"); // for the unittest
            QString text = (*it).text();
            text.replace('&',"&&");
            act->setText(text);
            if (!(*it).icon().isEmpty()) {
                act->setIcon(QIcon::fromTheme((*it).icon()));
            }
            act->setData(QVariant::fromValue(*it));
            m_executeServiceActionGroup.addAction(act);

            menu->addAction(act); // Add to toplevel menu
            ++count;
        }
    }

    return count;
}

void KFileItemActionsPrivate::slotExecuteService(QAction* act)
{
    KServiceAction serviceAction = act->data().value<KServiceAction>();
    if (KAuthorized::authorizeKAction(serviceAction.name())) {
        KDesktopFileActions::executeService(m_props.urlList(), serviceAction);
    }
}

////

KFileItemActions::KFileItemActions(QObject* parent)
    : QObject(parent), d(new KFileItemActionsPrivate(this))
{
}


KFileItemActions::~KFileItemActions()
{
    delete d;
}

void KFileItemActions::setItemListProperties(const KFileItemListProperties& itemListProperties)
{
    d->m_props = itemListProperties;

    d->m_mimeTypeList.clear();
    const KFileItemList items = d->m_props.items();
    KFileItemList::const_iterator kit = items.constBegin();
    const KFileItemList::const_iterator kend = items.constEnd();
    for (; kit != kend; ++kit) {
        if (!d->m_mimeTypeList.contains((*kit).mimetype()))
            d->m_mimeTypeList << (*kit).mimetype();
    }
}

int KFileItemActions::addServiceActionsTo(QMenu* mainMenu)
{
    const KFileItemList items = d->m_props.items();
    const KFileItem firstItem = items.first();
    const QString protocol = firstItem.url().scheme(); // assumed to be the same for all items
    const bool isLocal = !firstItem.localPath().isEmpty();
    const bool isSingleLocal = items.count() == 1 && isLocal;
    const QList<QUrl> urlList = d->m_props.urlList();

    KIO::PopupServices s;

    // 1 - Look for builtin and user-defined services
    if (isSingleLocal && (d->m_props.mimeType() == "application/x-desktop" || // .desktop file
        d->m_props.mimeType() == "inode/blockdevice")) { // dev file
        // get builtin services, like mount/unmount
        const QString path = firstItem.localPath();
        s.builtin = KDesktopFileActions::builtinServices(QUrl::fromLocalFile(path));
        KDesktopFile desktopFile(path);
        KConfigGroup cfg = desktopFile.desktopGroup();
        const QString priority = cfg.readEntry("X-KDE-Priority");
        const QString submenuName = cfg.readEntry("X-KDE-Submenu");
#if 0
        if (cfg.readEntry("Type") == "Link") {
           d->m_url = cfg.readEntry("URL");
           // TODO: Do we want to make all the actions apply on the target
           // of the .desktop file instead of the .desktop file itself?
        }
#endif
        ServiceList& list = s.selectList(priority, submenuName);
        list = KDesktopFileActions::userDefinedServices(path, desktopFile, true /*isLocal*/);
    }

    // 2 - Look for "servicemenus" bindings (user-defined services)

    // first check the .directory if this is a directory
    if (d->m_props.isDirectory() && isSingleLocal) {
        QString dotDirectoryFile = QUrl::fromLocalFile(firstItem.localPath()).path().append("/.directory");
        if (QFile::exists(dotDirectoryFile)) {
            const KDesktopFile desktopFile(dotDirectoryFile);
            const KConfigGroup cfg = desktopFile.desktopGroup();

            if (KIOSKAuthorizedAction(cfg)) {
                const QString priority = cfg.readEntry("X-KDE-Priority");
                const QString submenuName = cfg.readEntry("X-KDE-Submenu");
                ServiceList& list = s.selectList(priority, submenuName);
                list += KDesktopFileActions::userDefinedServices(dotDirectoryFile, desktopFile, true);
            }
        }
    }

    const KConfig config("kservicemenurc", KConfig::NoGlobals);
    const KConfigGroup showGroup = config.group("Show");

    const QString commonMimeType = d->m_props.mimeType();
    const QString commonMimeGroup = d->m_props.mimeGroup();
    const QMimeDatabase db;
    const QMimeType mimeTypePtr = commonMimeType.isEmpty() ? QMimeType() : db.mimeTypeForName(commonMimeType);
    const KService::List entries = KServiceTypeTrader::self()->query("KonqPopupMenu/Plugin");
    KService::List::const_iterator eEnd = entries.end();
    for (KService::List::const_iterator it2 = entries.begin(); it2 != eEnd; ++it2) {
        QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("kde5/services/") + (*it2)->entryPath());
        KDesktopFile desktopFile(file);
        const KConfigGroup cfg = desktopFile.desktopGroup();

        if (!KIOSKAuthorizedAction(cfg)) {
            continue;
        }

        if (cfg.hasKey("X-KDE-ShowIfRunning")) {
            const QString app = cfg.readEntry("X-KDE-ShowIfRunning");
            if (QDBusConnection::sessionBus().interface()->isServiceRegistered(app)) {
                continue;
            }
        }
        if (cfg.hasKey("X-KDE-ShowIfDBusCall")) {
            QString calldata = cfg.readEntry("X-KDE-ShowIfDBusCall");
            const QStringList parts = calldata.split(' ');
            const QString &app = parts.at(0);
            const QString &obj = parts.at(1);
            QString interface = parts.at(2);
            QString method;
            int pos = interface.lastIndexOf(QLatin1Char('.'));
            if (pos != -1) {
                method = interface.mid(pos + 1);
                interface.truncate(pos);
            }

            //if (!QDBus::sessionBus().busService()->nameHasOwner(app))
            //    continue; //app does not exist so cannot send call

            QDBusMessage reply = QDBusInterface(app, obj, interface).
                                 call(method, QUrl::toStringList(urlList));
            if (reply.arguments().count() < 1 || reply.arguments().at(0).type() != QVariant::Bool || !reply.arguments().at(0).toBool()) {
                continue;
            }

        }
        if (cfg.hasKey("X-KDE-Protocol")) {
            const QString theProtocol = cfg.readEntry("X-KDE-Protocol");
            if (theProtocol.startsWith('!')) {
                const QString excludedProtocol = theProtocol.mid(1);
                if (excludedProtocol == protocol) {
                    continue;
                }
            } else if (protocol != theProtocol) {
                continue;
            }
        }
        else if (cfg.hasKey("X-KDE-Protocols")) {
            const QStringList protocols = cfg.readEntry("X-KDE-Protocols", QStringList());
            if (!protocols.contains(protocol)) {
                continue;
            }
        }
        else if (protocol == "trash") {
            // Require servicemenus for the trash to ask for protocol=trash explicitly.
            // Trashed files aren't supposed to be available for actions.
            // One might want a servicemenu for trash.desktop itself though.
            continue;
        }

        if (cfg.hasKey("X-KDE-Require")) {
            const QStringList capabilities = cfg.readEntry("X-KDE-Require" , QStringList());
            if (capabilities.contains("Write") && !d->m_props.supportsWriting()) {
                continue;
            }
        }
        if (cfg.hasKey("Actions") || cfg.hasKey("X-KDE-GetActionMenu")) {
            // Like KService, we support ServiceTypes, X-KDE-ServiceTypes, and MimeType.
            QStringList types = cfg.readEntry("ServiceTypes", QStringList());
            types += cfg.readEntry("X-KDE-ServiceTypes", QStringList());
            types += cfg.readXdgListEntry("MimeType");
            //qDebug() << file << types;

            if (types.isEmpty()) {
                continue;
            }
            const QStringList excludeTypes = cfg.readEntry("ExcludeServiceTypes" , QStringList());
            bool ok = false;

            // check for exact matches or a typeglob'd mimetype if we have a mimetype
            for (QStringList::ConstIterator it = types.constBegin();
                 it != types.constEnd() && !ok;
                 ++it)
            {
                // first check if we have an all mimetype
                bool checkTheMimetypes = false;
                if (*it == "all/all" ||
                    *it == "allfiles" /*compat with KDE up to 3.0.3*/) {
                    checkTheMimetypes = true;
                }

                // next, do we match all files?
                if (!ok &&
                    !d->m_props.isDirectory() &&
                    (*it == "all/allfiles" || *it == "application/octet-stream")) {
                    checkTheMimetypes = true;
                }

                // if we have a mimetype, see if we have an exact or a type globbed match
                if (!ok && (
                    (mimeTypePtr.inherits(*it)) ||
                    (!commonMimeGroup.isEmpty() &&
                     ((*it).right(1) == "*" &&
                      (*it).left((*it).indexOf('/')) == commonMimeGroup)))) {
                    checkTheMimetypes = true;
                }

                if (checkTheMimetypes) {
                    ok = true;
                    for (QStringList::ConstIterator itex = excludeTypes.constBegin(); itex != excludeTypes.constEnd(); ++itex) {
                        if(((*itex).endsWith('*') && (*itex).left((*itex).indexOf('/')) == commonMimeGroup) ||
                            ((*itex) == commonMimeType)) {
                            ok = false;
                            break;
                        }
                    }
                }
            }

            if (ok) {
                const QString priority = cfg.readEntry("X-KDE-Priority");
                const QString submenuName = cfg.readEntry("X-KDE-Submenu");

                ServiceList& list = s.selectList(priority, submenuName);
                const ServiceList userServices = KDesktopFileActions::userDefinedServices(*(*it2), isLocal, urlList);
                foreach (const KServiceAction& action, userServices) {
                    if (showGroup.readEntry(action.name(), true)) {
                        list += action;
                    }
                }
            }
        }
    }



    QMenu* actionMenu = mainMenu;
    int userItemCount = 0;
    if (s.user.count() + s.userSubmenus.count() +
        s.userPriority.count() + s.userPrioritySubmenus.count() > 1) {
        // we have more than one item, so let's make a submenu
        actionMenu = new QMenu(i18nc("@title:menu", "&Actions"), mainMenu);
        actionMenu->menuAction()->setObjectName("actions_submenu"); // for the unittest
        mainMenu->addMenu(actionMenu);
    }

    userItemCount += d->insertServicesSubmenus(s.userPrioritySubmenus, actionMenu, false);
    userItemCount += d->insertServices(s.userPriority, actionMenu, false);

    // see if we need to put a separator between our priority items and our regular items
    if (userItemCount > 0 &&
        (s.user.count() > 0 ||
         s.userSubmenus.count() > 0 ||
         s.builtin.count() > 0) &&
        !actionMenu->actions().last()->isSeparator()) {
        actionMenu->addSeparator();
    }
    userItemCount += d->insertServicesSubmenus(s.userSubmenus, actionMenu, false);
    userItemCount += d->insertServices(s.user, actionMenu, false);
    userItemCount += d->insertServices(s.builtin, mainMenu, true);
    userItemCount += d->insertServicesSubmenus(s.userToplevelSubmenus, mainMenu, false);
    userItemCount += d->insertServices(s.userToplevel, mainMenu, false);
    return userItemCount;
}


// static
KService::List KFileItemActions::associatedApplications(const QStringList& mimeTypeList, const QString& traderConstraint)
{
    if (!KAuthorized::authorizeKAction("openwith") || mimeTypeList.isEmpty()) {
        return KService::List();
    }

    const KService::List firstOffers = KMimeTypeTrader::self()->query(mimeTypeList.first(), "Application", traderConstraint);

    QList<KFileItemActionsPrivate::ServiceRank> rankings;
    QStringList serviceList;

    // This section does two things.  First, it determines which services are common to all the given mimetypes.
    // Second, it ranks them based on their preference level in the associated applications list.
    // The more often a service appear near the front of the list, the LOWER its score.

    for (int i = 0; i < firstOffers.count(); ++i) {
        KFileItemActionsPrivate::ServiceRank tempRank;
        tempRank.service = firstOffers[i];
        tempRank.score = i;
        rankings << tempRank;
        serviceList << tempRank.service->storageId();
    }

    for (int j = 1; j < mimeTypeList.count(); ++j) {
        QStringList subservice; // list of services that support this mimetype
        const KService::List offers = KMimeTypeTrader::self()->query(mimeTypeList[j], "Application", traderConstraint);
        for (int i = 0; i != offers.count(); ++i) {
            const QString serviceId = offers[i]->storageId();
            subservice << serviceId;
            const int idPos = serviceList.indexOf(serviceId);
            if (idPos != -1) {
                rankings[idPos].score += i;
            } // else: we ignore the services that didn't support the previous mimetypes
        }

        // Remove services which supported the previous mimetypes but don't support this one
        for (int i = 0; i < serviceList.count(); ++i) {
            if (!subservice.contains(serviceList[i])) {
                serviceList.removeAt(i);
                rankings.removeAt(i);
                --i;
            }
        }
        // Nothing left -> there is no common application for these mimetypes
        if (rankings.isEmpty()) {
            return KService::List();
        }
    }

    qSort(rankings.begin(), rankings.end(), KFileItemActionsPrivate::lessRank);

    KService::List result;
    Q_FOREACH(const KFileItemActionsPrivate::ServiceRank& tempRank, rankings) {
        result << tempRank.service;
    }

    return result;
}

// KMimeTypeTrader::preferredService doesn't take a constraint
static KService::Ptr preferredService(const QString& mimeType, const QString& constraint)
{
    const KService::List services = KMimeTypeTrader::self()->query(mimeType, QString::fromLatin1("Application"), constraint);
    return !services.isEmpty() ? services.first() : KService::Ptr();
}

void KFileItemActions::addOpenWithActionsTo(QMenu* topMenu, const QString& traderConstraint)
{
    if (!KAuthorized::authorizeKAction("openwith")) {
        return;
    }

    d->m_traderConstraint = traderConstraint;
    KService::List offers = associatedApplications(d->m_mimeTypeList, traderConstraint);

    //// Ok, we have everything, now insert

    const KFileItemList items = d->m_props.items();
    const KFileItem firstItem = items.first();
    const bool isLocal = firstItem.url().isLocalFile();
    // "Open With..." for folders is really not very useful, especially for remote folders.
    // (media:/something, or trash:/, or ftp://...)
    if (!d->m_props.isDirectory() || isLocal) {
        if (!topMenu->actions().isEmpty()) {
            topMenu->addSeparator();
        }

        QAction *runAct = new QAction(d->m_parentWidget);
        QString runActionName;


        const QStringList serviceIdList = d->listPreferredServiceIds(d->m_mimeTypeList, traderConstraint);
        //qDebug() << "serviceIdList=" << serviceIdList;

        // When selecting files with multiple mimetypes, offer either "open with <app for all>"
        // or a generic <open> (if there are any apps associated).
        if (d->m_mimeTypeList.count() > 1
            && !serviceIdList.isEmpty()
            && !(serviceIdList.count()==1 && serviceIdList.first().isEmpty())) { // empty means "no apps associated"

            d->m_ownActions.append(runAct);

            if (serviceIdList.count() == 1) {
                const KService::Ptr app = preferredService(d->m_mimeTypeList.first(), traderConstraint);
                runActionName = i18n("&Open with %1", app->name());
                runAct->setIcon(QIcon::fromTheme(app->icon()));

                // Remove that app from the offers list (#242731)
                for (int i = 0; i < offers.count() ; ++i) {
                    if (offers[i]->storageId() == app->storageId()) {
                        offers.removeAt(i);
                        break;
                    }
                }
            } else {
                runActionName = i18n("&Open");
            }

            runAct->setText(runActionName);

            d->m_traderConstraint = traderConstraint;
            d->m_fileOpenList = d->m_props.items();
            QObject::connect(runAct, SIGNAL(triggered()), d, SLOT(slotRunPreferredApplications()));
            topMenu->addAction(runAct);
        }

        if (!offers.isEmpty()) {
            QMenu* menu = topMenu;

            if (offers.count() > 1) { // submenu 'open with'
                menu = new QMenu(i18nc("@title:menu", "&Open With"), topMenu);
                menu->menuAction()->setObjectName("openWith_submenu"); // for the unittest
                topMenu->addMenu(menu);
            }
            //qDebug() << offers.count() << "offers" << topMenu << menu;

            KService::List::ConstIterator it = offers.constBegin();
            for(; it != offers.constEnd(); it++) {
                QAction* act = d->createAppAction(*it,
                                                  // no submenu -> prefix single offer
                                                  menu == topMenu);
                menu->addAction(act);
            }

            QString openWithActionName;
            if (menu != topMenu) { // submenu
                menu->addSeparator();
                openWithActionName = i18nc("@action:inmenu Open With", "&Other...");
            } else {
                openWithActionName = i18nc("@title:menu", "&Open With...");
            }
            QAction *openWithAct = new QAction(d->m_parentWidget);
            d->m_ownActions.append(openWithAct);
            openWithAct->setText(openWithActionName);
            openWithAct->setObjectName("openwith_browse"); // for the unittest
            QObject::connect(openWithAct, SIGNAL(triggered()), d, SLOT(slotOpenWithDialog()));
            menu->addAction(openWithAct);
        }
        else // no app offers -> Open With...
        {
            QAction *act = new QAction(d->m_parentWidget);
            d->m_ownActions.append(act);
            act->setText(i18nc("@title:menu", "&Open With..."));
            QObject::connect(act, SIGNAL(triggered()), d, SLOT(slotOpenWithDialog()));
            topMenu->addAction(act);
        }

    }
}

void KFileItemActionsPrivate::slotRunPreferredApplications()
{
    const KFileItemList fileItems = m_fileOpenList;

    const QStringList mimeTypeList = listMimeTypes(fileItems);
    const QStringList serviceIdList = listPreferredServiceIds(mimeTypeList, m_traderConstraint);

    foreach (const QString serviceId, serviceIdList) {
        KFileItemList serviceItems;
        foreach (const KFileItem& item, fileItems) {
            const KService::Ptr serv = preferredService(item.mimetype(), m_traderConstraint);
            const QString preferredServiceId = serv ? serv->storageId() : QString();
            if (preferredServiceId == serviceId) {
                serviceItems << item;
            }
        }

        if (serviceId.isEmpty()) { // empty means: no associated app for this mimetype
            openWithByMime(serviceItems);
            continue;
        }

        const KService::Ptr servicePtr = KService::serviceByStorageId(serviceId);
        if (!servicePtr) {
            KRun::displayOpenWithDialog(serviceItems.urlList(), m_parentWidget);
            continue;
        }
        KRun::run(*servicePtr, serviceItems.urlList(), m_parentWidget);
    }
}

void KFileItemActions::runPreferredApplications(const KFileItemList& fileOpenList, const QString& traderConstraint)
{
    d->m_fileOpenList = fileOpenList;
    d->m_traderConstraint = traderConstraint;
    d->slotRunPreferredApplications();
}

void KFileItemActionsPrivate::openWithByMime(const KFileItemList& fileItems)
{
    const QStringList mimeTypeList = listMimeTypes(fileItems);
    foreach (const QString mimeType, mimeTypeList) {
        KFileItemList mimeItems;
        foreach (const KFileItem& item, fileItems) {
            if (item.mimetype() == mimeType) {
                mimeItems << item;
            }
        }
        KRun::displayOpenWithDialog(mimeItems.urlList(), m_parentWidget);
    }
}

void KFileItemActionsPrivate::slotRunApplication(QAction* act)
{
    // Is it an application, from one of the "Open With" actions
    KService::Ptr app = act->data().value<KService::Ptr>();
    Q_ASSERT(app);
    if (app) {
        KRun::run(*app, m_props.urlList(), m_parentWidget);
    }
}

void KFileItemActionsPrivate::slotOpenWithDialog()
{
    // The item 'Other...' or 'Open With...' has been selected
    emit q->openWithDialogAboutToBeShown();
    KRun::displayOpenWithDialog(m_props.urlList(), m_parentWidget);
}

QStringList KFileItemActionsPrivate::listMimeTypes(const KFileItemList& items)
{
    QStringList mimeTypeList;
    foreach (const KFileItem& item, items) {
        if (!mimeTypeList.contains(item.mimetype()))
            mimeTypeList << item.mimetype();
    }
    return mimeTypeList;
}

QStringList KFileItemActionsPrivate::listPreferredServiceIds(const QStringList& mimeTypeList, const QString& traderConstraint)
{
    QStringList serviceIdList;
    Q_FOREACH(const QString& mimeType, mimeTypeList) {
        const KService::Ptr serv = preferredService(mimeType, traderConstraint);
        const QString newOffer = serv ? serv->storageId() : QString();
        serviceIdList << newOffer;
    }
    serviceIdList.removeDuplicates();
    return serviceIdList;
}

QAction* KFileItemActionsPrivate::createAppAction(const KService::Ptr& service, bool singleOffer)
{
    QString actionName(service->name().replace('&', "&&"));
    if (singleOffer) {
        actionName = i18n("Open &with %1", actionName);
    } else {
        actionName = i18nc("@item:inmenu Open With, %1 is application name", "%1", actionName);
    }

    QAction *act = new QAction(m_parentWidget);
    act->setObjectName("openwith"); // for the unittest
    m_ownActions.append(act);
    act->setIcon(QIcon::fromTheme(service->icon()));
    act->setText(actionName);
    act->setData(QVariant::fromValue(service));
    m_runApplicationActionGroup.addAction(act);
    return act;
}

QAction* KFileItemActions::preferredOpenWithAction(const QString& traderConstraint)
{
    const KService::List offers = associatedApplications(d->m_mimeTypeList, traderConstraint);
    if (offers.isEmpty()) {
        return 0;
    }
    return d->createAppAction(offers.first(), true);
}

void KFileItemActions::setParentWidget(QWidget* widget)
{
    d->m_parentWidget = widget;
}
