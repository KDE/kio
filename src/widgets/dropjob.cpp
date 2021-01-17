/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "dropjob.h"

#include "job_p.h"
#include "pastejob.h"
#include "pastejob_p.h"
#include "jobuidelegate.h"
#include "jobuidelegateextension.h"
#include "kio_widgets_debug.h"

#include <KConfigGroup>
#include <KCoreDirLister>
#include <KDesktopFile>
#include <KIO/ApplicationLauncherJob>
#include <KIO/CopyJob>
#include <KIO/DndPopupMenuPlugin>
#include <KIO/FileUndoManager>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KPluginMetaData>
#include <KPluginLoader>
#include <KProtocolManager>
#include <KUrlMimeData>
#include <KService>

#include <QDropEvent>
#include <QFileInfo>
#include <QMenu>
#include <QMimeData>
#include <QProcess>
#include <QTimer>

using namespace KIO;

Q_DECLARE_METATYPE(Qt::DropAction)

namespace KIO {
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

class KIO::DropJobPrivate : public KIO::JobPrivate
{
public:
    DropJobPrivate(const QDropEvent *dropEvent, const QUrl &destUrl, DropJobFlags dropjobFlags, JobFlags flags)
        : JobPrivate(),
          // Extract everything from the dropevent, since it will be deleted before the job starts
          m_mimeData(dropEvent->mimeData()),
          m_urls(KUrlMimeData::urlsFromMimeData(m_mimeData, KUrlMimeData::PreferLocalUrls, &m_metaData)),
          m_dropAction(dropEvent->dropAction()),
          m_relativePos(dropEvent->pos()),
          m_keyboardModifiers(dropEvent->keyboardModifiers()),
          m_destUrl(destUrl),
          m_destItem(KCoreDirLister::cachedItemForUrl(destUrl)),
          m_flags(flags),
          m_dropjobFlags(dropjobFlags),
          m_triggered(false)
    {
        // Check for the drop of a bookmark -> we want a Link action
        if (m_mimeData->hasFormat(QStringLiteral("application/x-xbel"))) {
            m_keyboardModifiers |= Qt::KeyboardModifiers(Qt::ControlModifier | Qt::ShiftModifier);
            m_dropAction = Qt::LinkAction;
        }
        if (m_destItem.isNull() && m_destUrl.isLocalFile()) {
            m_destItem = KFileItem(m_destUrl);
        }

        if (!(m_flags & KIO::NoPrivilegeExecution)) {
            m_privilegeExecutionEnabled = true;
            switch (m_dropAction) {
            case Qt::CopyAction:
                m_operationType = Copy;
                break;
            case Qt::MoveAction:
                m_operationType = Move;
                break;
            case Qt::LinkAction:
                m_operationType = Symlink;
                break;
            default:
                m_operationType = Other;
                break;
            }
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

    const QMimeData *m_mimeData;
    const QList<QUrl> m_urls;
    QMap<QString, QString> m_metaData;
    Qt::DropAction m_dropAction;
    QPoint m_relativePos;
    Qt::KeyboardModifiers m_keyboardModifiers;
    QUrl m_destUrl;
    KFileItem m_destItem; // null for remote URLs not found in the dirlister cache
    const JobFlags m_flags;
    const DropJobFlags m_dropjobFlags;
    QList<QAction *> m_appActions;
    QList<QAction *> m_pluginActions;
    bool m_triggered;  // Tracks whether an action has been triggered in the popup menu.
    QSet<KIO::DropMenu *> m_menus;

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

};

DropMenu::DropMenu(QWidget *parent)
    : QMenu(parent),
      m_extraActionsSeparator(nullptr)
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
    for (QAction *action : qAsConst(m_appActions)) {
        removeAction(action);
    }
    for (QAction *action : qAsConst(m_pluginActions)) {
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
    QTimer::singleShot(0, this, SLOT(slotStart()));
}

DropJob::~DropJob()
{
}

void DropJobPrivate::slotStart()
{
    Q_Q(DropJob);
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
    } else {
        // Dropping raw data
        KIO::PasteJob *job = KIO::PasteJobPrivate::newJob(m_mimeData, m_destUrl, KIO::HideProgressInfo, false /*not clipboard*/);
        QObject::connect(job, &KIO::PasteJob::itemCreated, q, &KIO::DropJob::itemCreated);
        q->addSubjob(job);
    }
}

void DropJobPrivate::fillPopupMenu(KIO::DropMenu *popup)
{
    Q_Q(DropJob);

    // Check what the source can do
    // TODO: Determining the MIME type of the source URLs is difficult for remote URLs,
    // we would need to KIO::stat each URL in turn, asynchronously....
    KFileItemList fileItems;
    fileItems.reserve(m_urls.size());
    for (const QUrl &url : m_urls) {
        fileItems.append(KFileItem(url));
    }
    const KFileItemListProperties itemProps(fileItems);

    Q_EMIT q->popupMenuAboutToShow(itemProps);

    const bool sReading = itemProps.supportsReading();
    const bool sDeleting = itemProps.supportsDeleting();
    const bool sMoving = itemProps.supportsMoving();

    const int separatorLength = QCoreApplication::translate("QShortcut", "+").size();
    QString seq = QKeySequence(Qt::ShiftModifier).toString(QKeySequence::NativeText);
    seq.chop(separatorLength); // chop superfluous '+'
    QAction* popupMoveAction = new QAction(i18n("&Move Here") + QLatin1Char('\t') + seq, popup);
    popupMoveAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-move"), QIcon::fromTheme(QStringLiteral("go-jump"))));
    popupMoveAction->setData(QVariant::fromValue(Qt::MoveAction));
    seq = QKeySequence(Qt::ControlModifier).toString(QKeySequence::NativeText);
    seq.chop(separatorLength);
    QAction* popupCopyAction = new QAction(i18n("&Copy Here") + QLatin1Char('\t') + seq, popup);
    popupCopyAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    popupCopyAction->setData(QVariant::fromValue(Qt::CopyAction));
    seq = QKeySequence(Qt::ControlModifier | Qt::ShiftModifier).toString(QKeySequence::NativeText);
    seq.chop(separatorLength);
    QAction* popupLinkAction = new QAction(i18n("&Link Here") + QLatin1Char('\t') + seq, popup);
    popupLinkAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-link")));
    popupLinkAction->setData(QVariant::fromValue(Qt::LinkAction));

    if (sMoving || (sReading && sDeleting)) {
        bool equalDestination = true;
        for (const QUrl &src : m_urls) {
            if (!m_destUrl.matches(src.adjusted(QUrl::RemoveFilename), QUrl::StripTrailingSlash)) {
                equalDestination = false;
                break;
            }
        }

        if (!equalDestination) {
            popup->addAction(popupMoveAction);
        }
    }

    if (sReading) {
        popup->addAction(popupCopyAction);
    }

    popup->addAction(popupLinkAction);

    addPluginActions(popup, itemProps);
}

void DropJobPrivate::addPluginActions(KIO::DropMenu *popup, const KFileItemListProperties &itemProps)
{
    const QVector<KPluginMetaData> plugin_offers = KPluginLoader::findPlugins(QStringLiteral("kf5/kio_dnd"));
    for (const KPluginMetaData &service : plugin_offers) {
        KPluginFactory *factory = KPluginLoader(service.fileName()).factory();
        if (factory) {
            KIO::DndPopupMenuPlugin *plugin = factory->create<KIO::DndPopupMenuPlugin>();
            if (plugin) {
                const auto actions = plugin->setup(itemProps, m_destUrl);
                for (auto action : actions) {
                    action->setParent(popup);
                }
                m_pluginActions += actions;
            }
        }
    }

    popup->addExtraActions(m_appActions, m_pluginActions);
}

void DropJob::setApplicationActions(const QList<QAction *> &actions)
{
    Q_D(DropJob);

    d->m_appActions = actions;

    for (KIO::DropMenu *menu : qAsConst(d->m_menus)) {
        menu->addExtraActions(d->m_appActions, d->m_pluginActions);
    }

}

void DropJob::showMenu(const QPoint &p, QAction *atAction)
{
    Q_D(DropJob);

    if (!(d->m_dropjobFlags & KIO::ShowMenuManually)) {
        return;
    }

    for (KIO::DropMenu *menu : qAsConst(d->m_menus)) {
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
    QTimer::singleShot(0, q, [=]() {
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

    if (!m_destItem.isNull() && !m_destItem.isWritable() && (m_flags & KIO::NoPrivilegeExecution)) {
        slotDropActionDetermined(KIO::ERR_WRITE_ACCESS_DENIED);
        return;
    }

    bool allItemsAreFromTrash = true;
    bool containsTrashRoot = false;
    for (const QUrl &url : m_urls) {
        const bool local = url.isLocalFile();
        if (!local /*optimization*/ && url.scheme() == QLatin1String("trash")) {
            if (url.path().isEmpty() || url.path() == QLatin1String("/")) {
                containsTrashRoot = true;
            }
        } else {
            allItemsAreFromTrash = false;
        }
        if (url.matches(m_destUrl, QUrl::StripTrailingSlash)) {
            slotDropActionDetermined(KIO::ERR_DROP_ON_ITSELF);
            return;
        }
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

        QObject::connect(askUserInterface, &KIO::AskUserActionInterface::askUserDeleteResult,
                         q, [this](bool allowDelete) {
            if (allowDelete) {
                slotDropActionDetermined(KJob::NoError);
            } else {
                slotDropActionDetermined(KIO::ERR_USER_CANCELED);
            }
        });

        askUserInterface->askUserDelete(m_urls, KIO::AskUserActionInterface::Trash,
                                        KIO::AskUserActionInterface::DefaultConfirmation,
                                        KJobWidgets::window(q));
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
    } else if (m_keyboardModifiers & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier)) {
        // Qt determined m_dropAction from the modifiers already
        err = KJob::NoError; // Ok
    }
    slotDropActionDetermined(err);
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
        auto *window = KJobWidgets::window(q);
        KIO::DropMenu *menu = new KIO::DropMenu(window);
        QObject::connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);

        // If the user clicks outside the menu, it will be destroyed without emitting the triggered signal.
        QObject::connect(menu, &QMenu::aboutToHide, q, [this]() { slotAboutToHide(); });

        fillPopupMenu(menu);
        QObject::connect(menu, &QMenu::triggered, q, [this](QAction* action) {
            m_triggered = true;
            slotTriggered(action);
        });

        if (!(m_dropjobFlags & KIO::ShowMenuManually)) {
            menu->popup(window ? window->mapToGlobal(m_relativePos) : QCursor::pos());
        }
        m_menus.insert(menu);
        QObject::connect(menu, &QObject::destroyed, q, [this, menu]() { m_menus.remove(menu); });
    } else {
        q->setError(error);
        q->emitResult();
    }
}

void DropJobPrivate::doCopyToDirectory()
{
    Q_Q(DropJob);
    KIO::CopyJob * job = nullptr;
    switch (m_dropAction) {
    case Qt::MoveAction:
        job = KIO::move(m_urls, m_destUrl, m_flags);
        KIO::FileUndoManager::self()->recordJob(
            m_destUrl.scheme() == QLatin1String("trash") ? KIO::FileUndoManager::Trash : KIO::FileUndoManager::Move,
            m_urls, m_destUrl, job);
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
    job->setUiDelegate(q->uiDelegate());
    job->setParentJob(q);
    job->setMetaData(m_metaData);
    QObject::connect(job, &KIO::CopyJob::copyingDone, q, [q](KIO::Job*, const QUrl &, const QUrl &to) {
            Q_EMIT q->itemCreated(to);
    });
    QObject::connect(job, &KIO::CopyJob::copyingLinkDone, q, [q](KIO::Job*, const QUrl&, const QString&, const QUrl &to) {
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
        KIO::ApplicationLauncherJob *job = new KIO::ApplicationLauncherJob(service);
        job->setUrls(m_urls);
        job->setUiDelegate(q->uiDelegate());
        job->start();
        QObject::connect(job, &KJob::result, q, [=]() {
            if (job->error()) {
                q->setError(KIO::ERR_CANNOT_LAUNCH_PROCESS);
                q->setErrorText(destFile);
            }
            q->emitResult();
        });
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
    // Launch executable for each of the files
    QStringList args;
    args.reserve(m_urls.size());
    for (const QUrl &url : qAsConst(m_urls)) {
        args << url.toLocalFile(); // assume local files
    }
    QProcess::startDetached(m_destUrl.toLocalFile(), args);
    q->emitResult();
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

DropJob * KIO::drop(const QDropEvent *dropEvent, const QUrl &destUrl, JobFlags flags)
{
    return DropJobPrivate::newJob(dropEvent, destUrl, KIO::DropJobDefaultFlags, flags);
}

DropJob * KIO::drop(const QDropEvent *dropEvent, const QUrl &destUrl, DropJobFlags dropjobFlags, JobFlags flags)
{
    return DropJobPrivate::newJob(dropEvent, destUrl, dropjobFlags, flags);
}

#include "moc_dropjob.cpp"
#include "dropjob.moc"
