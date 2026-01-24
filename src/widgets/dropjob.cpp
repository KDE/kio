/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "dropjob.h"

#include "job_p.h"
#include "jobuidelegate.h"
#include "jobuidelegateextension.h"
#include "kio_widgets_debug.h"
#include "pastejob.h"
#include "pastejob_p.h"

#include <KConfigGroup>
#include <KCoreDirLister>
#include <KDesktopFile>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KIO/ApplicationLauncherJob>
#include <KIO/CommandLauncherJob>
#include <KIO/CopyJob>
#include <KIO/DndPopupMenuPlugin>
#include <KIO/FileUndoManager>
#include <KJobWidgets>
#include <KJobWindows>
#include <KLocalizedString>
#include <KMountPoint>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <KProtocolManager>
#include <KService>
#include <KSharedConfig>
#include <KUrlMimeData>

#ifdef WITH_QTDBUS
#include <QDBusConnection>
#include <QDBusPendingCall>
#endif

#include <QDropEvent>
#include <QFileInfo>
#include <QMenu>
#include <QMetaEnum>
#include <QMimeData>
#include <QTimer>
#include <QWindow>

using namespace KIO;

Q_DECLARE_METATYPE(Qt::DropAction)

namespace KIO
{
class DropMenu;
}

class KIO::DropMenu : public QMenu
{
    Q_OBJECT
public:
    explicit DropMenu(QWidget *parent = nullptr);
    ~DropMenu() override;

    void addCancelAction();
    void addExtraActions(const QList<QAction *> &appActions, const QList<QAction *> &pluginActions);

private:
    QList<QAction *> m_appActions;
    QList<QAction *> m_pluginActions;
    QAction *m_lastSeparator;
    QAction *m_extraActionsSeparator;
    QAction *m_cancelAction;
};

static const QString s_applicationSlashXDashKDEDashArkDashDnDExtractDashService = //
    QStringLiteral("application/x-kde-ark-dndextract-service");
static const QString s_applicationSlashXDashKDEDashArkDashDnDExtractDashPath = //
    QStringLiteral("application/x-kde-ark-dndextract-path");

class KIO::DropJobPrivate : public KIO::JobPrivate
{
public:
    DropJobPrivate(const QDropEvent *dropEvent, const QUrl &destUrl, DropJobFlags dropjobFlags, JobFlags flags)
        : JobPrivate()
        , m_mimeData(dropEvent->mimeData()) // Extract everything from the dropevent, since it will be deleted before the job starts
        , m_urls(KUrlMimeData::urlsFromMimeData(m_mimeData, KUrlMimeData::PreferLocalUrls, &m_metaData))
        , m_dropAction(dropEvent->dropAction())
        , m_possibleActions(dropEvent->possibleActions())
        , m_relativePos(dropEvent->position().toPoint())
        , m_keyboardModifiers(dropEvent->modifiers())
        , m_hasArkFormat(m_mimeData->hasFormat(s_applicationSlashXDashKDEDashArkDashDnDExtractDashService)
                         && m_mimeData->hasFormat(s_applicationSlashXDashKDEDashArkDashDnDExtractDashPath))
        , m_destUrl(destUrl)
        , m_destItem(KCoreDirLister::cachedItemForUrl(destUrl))
        , m_flags(flags)
        , m_dropjobFlags(dropjobFlags)
        , m_triggered(false)
    {
        // Check for the drop of a bookmark -> we want a Link action
        if (m_mimeData->hasFormat(QStringLiteral("application/x-xbel"))) {
            m_keyboardModifiers |= Qt::KeyboardModifiers(Qt::ControlModifier | Qt::ShiftModifier);
            m_dropAction = Qt::LinkAction;
        }
        if (m_destItem.isNull() && m_destUrl.isLocalFile()) {
            m_destItem = KFileItem(m_destUrl);
        }

        if (m_hasArkFormat) {
            m_remoteArkDBusClient = QString::fromUtf8(m_mimeData->data(s_applicationSlashXDashKDEDashArkDashDnDExtractDashService));
            m_remoteArkDBusPath = QString::fromUtf8(m_mimeData->data(s_applicationSlashXDashKDEDashArkDashDnDExtractDashPath));
        }
    }

    bool destIsDirectory() const
    {
        if (!m_destItem.isNull()) {
            return m_destItem.isDir();
        }
        // We support local dir, remote dir, local desktop file, local executable.
        // So for remote URLs, we just assume they point to a directory, the user will get an error from KIO::copy if not.
        return true;
    }
    void handleCopyToDirectory();
    void slotDropActionDetermined(int error);
    void handleDropToDesktopFile();
    void handleDropToExecutable();
    void fillPopupMenu(KIO::DropMenu *popup);
    void addPluginActions(KIO::DropMenu *popup, const KFileItemListProperties &itemProps);
    void doCopyToDirectory();

    QWindow *transientParent();

    QPointer<const QMimeData> m_mimeData;
    const QList<QUrl> m_urls;
    QMap<QString, QString> m_metaData;
    Qt::DropAction m_dropAction;
    Qt::DropActions m_possibleActions;
    bool m_allSourcesAreHttpUrls;
    QPoint m_relativePos;
    Qt::KeyboardModifiers m_keyboardModifiers;
    KFileItemListProperties m_itemProps;
    bool m_hasArkFormat;
    QString m_remoteArkDBusClient;
    QString m_remoteArkDBusPath;
    QUrl m_destUrl;
    KFileItem m_destItem; // null for remote URLs not found in the dirlister cache
    const JobFlags m_flags;
    const DropJobFlags m_dropjobFlags;
    QList<QAction *> m_appActions;
    QList<QAction *> m_pluginActions;
    bool m_triggered; // Tracks whether an action has been triggered in the popup menu.
    QSet<KIO::DropMenu *> m_menus;
    QList<KIO::DndPopupMenuPlugin *> m_plugins;

    Q_DECLARE_PUBLIC(DropJob)

    void slotStart();
    void slotTriggered(QAction *);
    void slotAboutToHide();

    static inline DropJob *newJob(const QDropEvent *dropEvent, const QUrl &destUrl, DropJobFlags dropjobFlags, JobFlags flags)
    {
        DropJob *job = new DropJob(*new DropJobPrivate(dropEvent, destUrl, dropjobFlags, flags));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        // Note: never KIO::getJobTracker()->registerJob here.
        // We don't want a progress dialog during the copy/move/link popup, it would in fact close
        // the popup
        return job;
    }

    ~DropJobPrivate()
    {
        qDeleteAll(m_plugins);
    }
};

DropMenu::DropMenu(QWidget *parent)
    : QMenu(parent)
    , m_extraActionsSeparator(nullptr)
{
    m_cancelAction = new QAction(i18n("C&ancel") + QLatin1Char('\t') + QKeySequence(Qt::Key_Escape).toString(QKeySequence::NativeText), this);
    m_cancelAction->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));

    m_lastSeparator = new QAction(this);
    m_lastSeparator->setSeparator(true);
}

DropMenu::~DropMenu()
{
}

void DropMenu::addExtraActions(const QList<QAction *> &appActions, const QList<QAction *> &pluginActions)
{
    removeAction(m_lastSeparator);
    removeAction(m_cancelAction);

    removeAction(m_extraActionsSeparator);
    for (QAction *action : std::as_const(m_appActions)) {
        removeAction(action);
    }
    for (QAction *action : std::as_const(m_pluginActions)) {
        removeAction(action);
    }

    m_appActions = appActions;
    m_pluginActions = pluginActions;

    if (!m_appActions.isEmpty() || !m_pluginActions.isEmpty()) {
        QAction *firstExtraAction = m_appActions.value(0, m_pluginActions.value(0, nullptr));
        if (firstExtraAction && !firstExtraAction->isSeparator()) {
            if (!m_extraActionsSeparator) {
                m_extraActionsSeparator = new QAction(this);
                m_extraActionsSeparator->setSeparator(true);
            }
            addAction(m_extraActionsSeparator);
        }
        addActions(appActions);
        addActions(pluginActions);
    }

    addAction(m_lastSeparator);
    addAction(m_cancelAction);
}

DropJob::DropJob(DropJobPrivate &dd)
    : Job(dd)
{
    Q_D(DropJob);

    QTimer::singleShot(0, this, [d]() {
        d->slotStart();
    });
}

DropJob::~DropJob()
{
}

void DropJobPrivate::slotStart()
{
    Q_Q(DropJob);

#ifdef WITH_QTDBUS
    if (m_hasArkFormat) {
        QDBusMessage message = QDBusMessage::createMethodCall(m_remoteArkDBusClient,
                                                              m_remoteArkDBusPath,
                                                              QStringLiteral("org.kde.ark.DndExtract"),
                                                              QStringLiteral("extractSelectedFilesTo"));
        message.setArguments({m_destUrl.toDisplayString(QUrl::PreferLocalFile)});
        const auto pending = QDBusConnection::sessionBus().asyncCall(message);
        auto watcher = std::make_shared<QDBusPendingCallWatcher>(pending);
        QObject::connect(watcher.get(), &QDBusPendingCallWatcher::finished, q, [this, watcher] {
            Q_Q(DropJob);

            if (watcher->isError()) {
                q->setError(KIO::ERR_UNKNOWN);
            }
            q->emitResult();
        });

        return;
    }
#endif

    if (!m_urls.isEmpty()) {
        if (destIsDirectory()) {
            handleCopyToDirectory();
        } else { // local file
            const QString destFile = m_destUrl.toLocalFile();
            if (KDesktopFile::isDesktopFile(destFile)) {
                handleDropToDesktopFile();
            } else if (QFileInfo(destFile).isExecutable()) {
                handleDropToExecutable();
            } else {
                // should not happen, if KDirModel::flags is correct
                q->setError(KIO::ERR_ACCESS_DENIED);
                q->emitResult();
            }
        }
    } else if (m_mimeData) {
        // Dropping raw data
        KIO::PasteJob *job = KIO::PasteJobPrivate::newJob(m_mimeData, m_destUrl, KIO::HideProgressInfo, false /*not clipboard*/);
        QObject::connect(job, &KIO::PasteJob::itemCreated, q, &KIO::DropJob::itemCreated);
        q->addSubjob(job);
    }
}

void DropJobPrivate::fillPopupMenu(KIO::DropMenu *popup)
{
    const int separatorLength = QCoreApplication::translate("QShortcut", "+").size();
    QString seq = QKeySequence(Qt::ShiftModifier).toString(QKeySequence::NativeText);
    seq.chop(separatorLength); // chop superfluous '+'
    QAction *popupMoveAction = new QAction(i18n("&Move Here") + QLatin1Char('\t') + seq, popup);
    popupMoveAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-move"), QIcon::fromTheme(QStringLiteral("go-jump"))));
    popupMoveAction->setData(QVariant::fromValue(Qt::MoveAction));
    seq = QKeySequence(Qt::ControlModifier).toString(QKeySequence::NativeText);
    seq.chop(separatorLength);

    const QString copyActionName = m_allSourcesAreHttpUrls ? i18nc("@action:inmenu Download contents of URL here", "&Download Here") : i18n("&Copy Here");
    const QIcon copyActionIcon = QIcon::fromTheme(m_allSourcesAreHttpUrls ? QStringLiteral("download") : QStringLiteral("edit-copy"));
    QAction *popupCopyAction = new QAction(copyActionName + QLatin1Char('\t') + seq, popup);
    popupCopyAction->setIcon(copyActionIcon);
    popupCopyAction->setData(QVariant::fromValue(Qt::CopyAction));
    seq = QKeySequence(Qt::ControlModifier | Qt::ShiftModifier).toString(QKeySequence::NativeText);
    seq.chop(separatorLength);
    QAction *popupLinkAction = new QAction(i18n("&Link Here") + QLatin1Char('\t') + seq, popup);
    popupLinkAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-link")));
    popupLinkAction->setData(QVariant::fromValue(Qt::LinkAction));

    if (m_possibleActions & Qt::MoveAction) {
        popup->addAction(popupMoveAction);
    }

    if (m_possibleActions & Qt::CopyAction) {
        popup->addAction(popupCopyAction);
    }

    popup->addAction(popupLinkAction);

    if (m_dropjobFlags & DropJobFlag::ExcludePluginsActions) {
        // we must exclude plugins actions so we just call addExtraActions with both parameters as empty lists
        // to add some final common menu items prepared in that method (usually: last separator and "Cancel" action)
        QList<QAction *> emptyActionList;
        popup->addExtraActions(emptyActionList, emptyActionList);
    } else {
        // add plugins custom actions to drop popup menu
        addPluginActions(popup, m_itemProps);
    }
}

void DropJobPrivate::addPluginActions(KIO::DropMenu *popup, const KFileItemListProperties &itemProps)
{
    const QList<KPluginMetaData> plugin_offers = KPluginMetaData::findPlugins(QStringLiteral("kf6/kio_dnd"));
    for (const KPluginMetaData &data : plugin_offers) {
        if (auto plugin = KPluginFactory::instantiatePlugin<KIO::DndPopupMenuPlugin>(data).plugin) {
            const auto actions = plugin->setup(itemProps, m_destUrl);
            for (auto action : actions) {
                action->setParent(popup);
            }
            m_pluginActions += actions;
            m_plugins += plugin;
        }
    }

    popup->addExtraActions(m_appActions, m_pluginActions);
}

void DropJob::setApplicationActions(const QList<QAction *> &actions)
{
    Q_D(DropJob);

    d->m_appActions = actions;

    for (KIO::DropMenu *menu : std::as_const(d->m_menus)) {
        menu->addExtraActions(d->m_appActions, d->m_pluginActions);
    }
}

void DropJob::showMenu(const QPoint &p, QAction *atAction)
{
    Q_D(DropJob);

    if (!(d->m_dropjobFlags & KIO::ShowMenuManually)) {
        return;
    }

    for (KIO::DropMenu *menu : std::as_const(d->m_menus)) {
        menu->ensurePolished();
        if (QWindow *transientParent = d->transientParent()) {
            if (menu->winId()) {
                menu->windowHandle()->setTransientParent(transientParent);
            }
        }
        menu->popup(p, atAction);
    }
}

void DropJobPrivate::slotTriggered(QAction *action)
{
    Q_Q(DropJob);
    if (m_appActions.contains(action) || m_pluginActions.contains(action)) {
        q->emitResult();
        return;
    }
    const QVariant data = action->data();
    if (!data.canConvert<Qt::DropAction>()) {
        q->setError(KIO::ERR_USER_CANCELED);
        q->emitResult();
        return;
    }
    m_dropAction = data.value<Qt::DropAction>();
    doCopyToDirectory();
}

void DropJobPrivate::slotAboutToHide()
{
    Q_Q(DropJob);
    // QMenu emits aboutToHide before triggered.
    // So we need to give the menu time in case it needs to emit triggered.
    // If it does, the cleanup will be done by slotTriggered.
    QTimer::singleShot(0, q, [=, this]() {
        if (!m_triggered) {
            q->setError(KIO::ERR_USER_CANCELED);
            q->emitResult();
        }
    });
}

void DropJobPrivate::handleCopyToDirectory()
{
    Q_Q(DropJob);

    // Process m_dropAction as set by Qt at the time of the drop event
    if (!KProtocolManager::supportsWriting(m_destUrl)) {
        slotDropActionDetermined(KIO::ERR_CANNOT_WRITE);
        return;
    }

    if (!m_destItem.isNull() && !m_destItem.isWritable()) {
        slotDropActionDetermined(KIO::ERR_WRITE_ACCESS_DENIED);
        return;
    }

    // Check what the source can do
    KFileItemList fileItems;
    fileItems.reserve(m_urls.size());

    bool allItemsAreFromTrash = true;
    bool allItemsAreLocal = true;
    bool allItemsAreSameDevice = true;
    bool containsTrashRoot = false;
    bool equalDestination = true;
    m_allSourcesAreHttpUrls = true;
    // Check if the default behavior has been changed to MoveAction, read from kdeglobals
    const KConfigGroup g = KConfigGroup(KSharedConfig::openConfig(), QStringLiteral("KDE"));
    QMetaEnum metaEnum = QMetaEnum::fromType<DndBehavior>();
    QString configValue = g.readEntry("DndBehavior", metaEnum.valueToKey(DndBehavior::AlwaysAsk));
    bool defaultActionIsMove = metaEnum.keyToValue(configValue.toLocal8Bit().constData());

    KMountPoint::List mountPoints;
    bool destIsLocal = m_destUrl.isLocalFile();
    QString destDevice;
    if (defaultActionIsMove && destIsLocal) {
        // As getting the mount point can be slow, only do it when we need to.
        if (mountPoints.isEmpty()) {
            mountPoints = KMountPoint::currentMountPoints();
        }
        KMountPoint::Ptr destMountPoint = mountPoints.findByPath(m_destUrl.path());
        if (destMountPoint) {
            destDevice = destMountPoint->mountedFrom();
        } else {
            qCWarning(KIO_WIDGETS) << "Could not determine mount point for destination drop target " << m_destUrl;
        }
    } else {
        allItemsAreSameDevice = false;
    }

    for (const QUrl &url : m_urls) {
        const bool local = url.isLocalFile();
        if (!local) {
            allItemsAreLocal = false;
            allItemsAreSameDevice = false;
        }
#ifdef Q_OS_LINUX
        // Check if the file is already in the xdg trash folder, BUG:497390
        const QString xdgtrash = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash");
        if (!local /*optimization*/ && url.scheme() == QLatin1String("trash")) {
            if (url.path().isEmpty() || url.path() == QLatin1String("/")) {
                containsTrashRoot = true;
            }
        } else if (local || url.scheme() == QLatin1String("file")) {
            if (!url.toLocalFile().startsWith(xdgtrash)) {
                allItemsAreFromTrash = false;
            } else if (url.path().isEmpty() || url.path() == QLatin1String("/")) {
                containsTrashRoot = true;
            }
        } else {
            allItemsAreFromTrash = false;
        }
#else
        if (!local /*optimization*/ && url.scheme() == QLatin1String("trash")) {
            if (url.path().isEmpty() || url.path() == QLatin1String("/")) {
                containsTrashRoot = true;
            }
        } else {
            allItemsAreFromTrash = false;
        }
#endif

        if (equalDestination && !m_destUrl.matches(url.adjusted(QUrl::RemoveFilename), QUrl::StripTrailingSlash)) {
            equalDestination = false;
        }

        if (defaultActionIsMove && allItemsAreSameDevice) {
            // As getting the mount point can be slow, only do it when we need to.
            if (mountPoints.isEmpty()) {
                mountPoints = KMountPoint::currentMountPoints();
            }
            QString sourceDevice;
            KMountPoint::Ptr sourceMountPoint = mountPoints.findByPath(url.path());
            if (sourceMountPoint) {
                sourceDevice = sourceMountPoint->mountedFrom();
            } else {
                qCWarning(KIO_WIDGETS) << "Could not determine mount point for destination drag source " << url;
            }
            if (sourceDevice != destDevice && !KFileItem(url).isLink()) {
                allItemsAreSameDevice = false;
            }
            if (sourceDevice.isEmpty()) {
                // Sanity check in case we somehow have a local files that we can't get the mount points from.
                allItemsAreSameDevice = false;
            }
        }

        if (m_allSourcesAreHttpUrls && !url.scheme().startsWith(QStringLiteral("http"), Qt::CaseInsensitive)) {
            m_allSourcesAreHttpUrls = false;
        }

        fileItems.append(KFileItem(url));

        if (url.matches(m_destUrl, QUrl::StripTrailingSlash)) {
            slotDropActionDetermined(KIO::ERR_DROP_ON_ITSELF);
            return;
        }
    }
    m_itemProps.setItems(fileItems);

    m_possibleActions |= Qt::LinkAction;
    const bool sReading = m_itemProps.supportsReading();
    // For http URLs, even though technically the protocol supports deleting,
    // this never makes sense for a drag operation.
    const bool sDeleting = m_allSourcesAreHttpUrls ? false : m_itemProps.supportsDeleting();
    const bool sMoving = m_itemProps.supportsMoving();

    if (!sReading) {
        m_possibleActions &= ~Qt::CopyAction;
    }

    if (!(sMoving || (sReading && sDeleting)) || equalDestination) {
        m_possibleActions &= ~Qt::MoveAction;
    }

    const bool trashing = m_destUrl.scheme() == QLatin1String("trash");
    if (trashing) {
        if (allItemsAreFromTrash) {
            qCDebug(KIO_WIDGETS) << "Dropping items from trash to trash";
            slotDropActionDetermined(KIO::ERR_DROP_ON_ITSELF);
            return;
        }
        m_dropAction = Qt::MoveAction;

        auto *askUserInterface = KIO::delegateExtension<AskUserActionInterface *>(q);

        // No UI Delegate set for this job, or a delegate that doesn't implement
        // AskUserActionInterface, then just proceed with the job without asking.
        // This is useful for non-interactive usage, (which doesn't actually apply
        // here as a DropJob is always interactive), but this is useful for unittests,
        // which are typically non-interactive.
        if (!askUserInterface) {
            slotDropActionDetermined(KJob::NoError);
            return;
        }

        QObject::connect(askUserInterface, &KIO::AskUserActionInterface::askUserDeleteResult, q, [this](bool allowDelete) {
            if (allowDelete) {
                slotDropActionDetermined(KJob::NoError);
            } else {
                slotDropActionDetermined(KIO::ERR_USER_CANCELED);
            }
        });

        askUserInterface->askUserDelete(m_urls, KIO::AskUserActionInterface::Trash, KIO::AskUserActionInterface::DefaultConfirmation, KJobWidgets::window(q));
        return;
    }

    // If we can't determine the action below, we use ERR::UNKNOWN as we need to ask
    // the user via a popup menu.
    int err = KIO::ERR_UNKNOWN;
    const bool implicitCopy = m_destUrl.scheme() == QLatin1String("stash");
    if (implicitCopy) {
        m_dropAction = Qt::CopyAction;
        err = KJob::NoError; // Ok
    } else if (containsTrashRoot) {
        // Dropping a link to the trash: don't move the full contents, just make a link (#319660)
        m_dropAction = Qt::LinkAction;
        err = KJob::NoError; // Ok
    } else if (allItemsAreFromTrash) {
        // No point in asking copy/move/link when using dragging from the trash, just move the file out.
        m_dropAction = Qt::MoveAction;
        err = KJob::NoError; // Ok
    } else if (defaultActionIsMove && (m_possibleActions & Qt::MoveAction) && allItemsAreLocal && allItemsAreSameDevice) {
        if (m_keyboardModifiers == Qt::NoModifier) {
            m_dropAction = Qt::MoveAction;
            err = KJob::NoError; // Ok
        } else if (m_keyboardModifiers == Qt::ShiftModifier) {
            // the user requests to show the menu
            err = KIO::ERR_UNKNOWN;
        } else if (m_keyboardModifiers & (Qt::ControlModifier | Qt::AltModifier)) {
            // Qt determined m_dropAction from the modifiers
            err = KJob::NoError; // Ok
        }
    } else if (m_keyboardModifiers & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier)) {
        // Qt determined m_dropAction from the modifiers already
        err = KJob::NoError; // Ok
    }
    slotDropActionDetermined(err);
}

QWindow *DropJobPrivate::transientParent()
{
    Q_Q(DropJob);

    if (QWidget *widget = KJobWidgets::window(q)) {
        QWidget *window = widget->window();
        Q_ASSERT(window);
        return window->windowHandle();
    }

    if (QWindow *window = KJobWindows::window(q)) {
        return window;
    }

    return nullptr;
}

void DropJobPrivate::slotDropActionDetermined(int error)
{
    Q_Q(DropJob);

    if (error == KJob::NoError) {
        doCopyToDirectory();
        return;
    }

    // There was an error, handle it
    if (error == KIO::ERR_UNKNOWN) {
        KIO::DropMenu *menu = new KIO::DropMenu();
        QObject::connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);

        // If the user clicks outside the menu, it will be destroyed without emitting the triggered signal.
        QObject::connect(menu, &QMenu::aboutToHide, q, [this]() {
            slotAboutToHide();
        });

        fillPopupMenu(menu);
        Q_EMIT q->popupMenuAboutToShow(m_itemProps);
        QObject::connect(menu, &QMenu::triggered, q, [this](QAction *action) {
            m_triggered = true;
            slotTriggered(action);
        });

        if (!(m_dropjobFlags & KIO::ShowMenuManually)) {
            menu->ensurePolished();
            if (QWindow *parent = transientParent()) {
                if (menu->winId()) {
                    menu->windowHandle()->setTransientParent(parent);
                }
            }
            auto *window = KJobWidgets::window(q);
            menu->popup(window ? window->mapToGlobal(m_relativePos) : QCursor::pos());
        }
        m_menus.insert(menu);
        QObject::connect(menu, &QObject::destroyed, q, [this, menu]() {
            m_menus.remove(menu);
        });
    } else {
        q->setError(error);
        q->emitResult();
    }
}

void DropJobPrivate::doCopyToDirectory()
{
    Q_Q(DropJob);
    KIO::CopyJob *job = nullptr;
    switch (m_dropAction) {
    case Qt::MoveAction:
        job = KIO::move(m_urls, m_destUrl, m_flags);
        KIO::FileUndoManager::self()->recordJob(m_destUrl.scheme() == QLatin1String("trash") ? KIO::FileUndoManager::Trash : KIO::FileUndoManager::Move,
                                                m_urls,
                                                m_destUrl,
                                                job);
        break;
    case Qt::CopyAction:
        job = KIO::copy(m_urls, m_destUrl, m_flags);
        KIO::FileUndoManager::self()->recordCopyJob(job);
        break;
    case Qt::LinkAction:
        job = KIO::link(m_urls, m_destUrl, m_flags);
        KIO::FileUndoManager::self()->recordCopyJob(job);
        break;
    default:
        qCWarning(KIO_WIDGETS) << "Unknown drop action" << int(m_dropAction);
        q->setError(KIO::ERR_UNSUPPORTED_ACTION);
        q->emitResult();
        return;
    }
    Q_ASSERT(job);
    job->setParentJob(q);
    job->setMetaData(m_metaData);
    QObject::connect(job, &KIO::CopyJob::copyingDone, q, [q](KIO::Job *, const QUrl &, const QUrl &to) {
        Q_EMIT q->itemCreated(to);
    });
    QObject::connect(job, &KIO::CopyJob::copyingLinkDone, q, [q](KIO::Job *, const QUrl &, const QString &, const QUrl &to) {
        Q_EMIT q->itemCreated(to);
    });
    q->addSubjob(job);

    Q_EMIT q->copyJobStarted(job);
}

void DropJobPrivate::handleDropToDesktopFile()
{
    Q_Q(DropJob);
    const QString urlKey = QStringLiteral("URL");
    const QString destFile = m_destUrl.toLocalFile();
    const KDesktopFile desktopFile(destFile);
    const KConfigGroup desktopGroup = desktopFile.desktopGroup();
    if (desktopFile.hasApplicationType()) {
        // Drop to application -> start app with urls as argument
        KService::Ptr service(new KService(destFile));
        // Can't use setParentJob() because ApplicationLauncherJob isn't a KIO::Job,
        // instead pass q as parent so that KIO::delegateExtension() can find a delegate
        KIO::ApplicationLauncherJob *job = new KIO::ApplicationLauncherJob(service, q);
        job->setUrls(m_urls);
        QObject::connect(job, &KJob::result, q, [=]() {
            if (job->error()) {
                q->setError(KIO::ERR_CANNOT_LAUNCH_PROCESS);
                q->setErrorText(destFile);
            }
            q->emitResult();
        });
        job->start();
    } else if (desktopFile.hasLinkType() && desktopGroup.hasKey(urlKey)) {
        // Drop to link -> adjust destination directory
        m_destUrl = QUrl::fromUserInput(desktopGroup.readPathEntry(urlKey, QString()));
        handleCopyToDirectory();
    } else {
        if (desktopFile.hasDeviceType()) {
            qCWarning(KIO_WIDGETS) << "Not re-implemented; please email kde-frameworks-devel@kde.org if you need this.";
            // take code from libkonq's old konq_operations.cpp
            // for now, fallback
        }
        // Some other kind of .desktop file (service, servicetype...)
        q->setError(KIO::ERR_UNSUPPORTED_ACTION);
        q->emitResult();
    }
}

void DropJobPrivate::handleDropToExecutable()
{
    Q_Q(DropJob);
    const QString destFile = m_destUrl.toLocalFile();
    // Launch executable for each of the files
    QStringList args;
    args.reserve(m_urls.size());
    for (const QUrl &url : std::as_const(m_urls)) {
        args << url.toLocalFile(); // assume local files
    }
    auto *job = new KIO::CommandLauncherJob(destFile, args, q);
    QObject::connect(job, &KJob::result, q, [q, job, destFile] {
        if (job->error()) {
            q->setError(KIO::ERR_CANNOT_LAUNCH_PROCESS);
            q->setErrorText(destFile);
        }
        q->emitResult();
    });
    job->start();
}

void DropJob::slotResult(KJob *job)
{
    if (job->error()) {
        KIO::Job::slotResult(job); // will set the error and emit result(this)
        return;
    }
    removeSubjob(job);
    emitResult();
}

DropJob *KIO::drop(const QDropEvent *dropEvent, const QUrl &destUrl, JobFlags flags)
{
    return DropJobPrivate::newJob(dropEvent, destUrl, KIO::DropJobDefaultFlags, flags);
}

DropJob *KIO::drop(const QDropEvent *dropEvent, const QUrl &destUrl, DropJobFlags dropjobFlags, JobFlags flags)
{
    return DropJobPrivate::newJob(dropEvent, destUrl, dropjobFlags, flags);
}

#include "dropjob.moc"
#include "moc_dropjob.cpp"
