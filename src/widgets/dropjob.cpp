/* This file is part of the KDE libraries
    Copyright (C) 2014 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or (at
    your option) version 3 or, at the discretion of KDE e.V. (which shall
    act as a proxy as in section 14 of the GPLv3), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "dropjob.h"

#include "job_p.h"
#include "pastejob.h"
#include "pastejob_p.h"
#include "jobuidelegate.h"
#include "jobuidelegateextension.h"

#include <KConfigGroup>
#include <KCoreDirLister>
#include <KDesktopFile>
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
#include <KRun>
#include <KService>

#include <QApplication>
#include <QDebug>
#include <QDropEvent>
#include <QFileInfo>
#include <QMenu>
#include <QMimeData>
#include <QProcess>
#include <QTimer>

using namespace KIO;

Q_DECLARE_METATYPE(Qt::DropAction);

class KIO::DropJobPrivate : public KIO::JobPrivate
{
public:
    DropJobPrivate(const QDropEvent *dropEvent, const QUrl &destUrl, JobFlags flags)
        : JobPrivate(),
          // Extract everything from the dropevent, since it will be deleted before the job starts
          m_mimeData(dropEvent->mimeData()),
          m_urls(KUrlMimeData::urlsFromMimeData(m_mimeData, KUrlMimeData::PreferLocalUrls, &m_metaData)),
          m_dropAction(dropEvent->dropAction()),
          m_globalPos(QCursor::pos()), // record mouse pos at time of drop
          m_keyboardModifiers(dropEvent->keyboardModifiers()),
          m_destUrl(destUrl),
          m_destItem(KCoreDirLister::cachedItemForUrl(destUrl)),
          m_flags(flags)
    {
        // Check for the drop of a bookmark -> we want a Link action
        if (m_mimeData->hasFormat(QStringLiteral("application/x-xbel"))) {
            m_keyboardModifiers |= Qt::KeyboardModifiers(Qt::ControlModifier | Qt::ShiftModifier);
            m_dropAction = Qt::LinkAction;
        }
        if (m_destItem.isNull() && m_destUrl.isLocalFile()) {
            m_destItem = KFileItem(m_destUrl);
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
    void handleDropToDesktopFile();
    void handleDropToExecutable();
    int determineDropAction();
    void fillPopupMenu(QMenu *popup);
    void addPluginActions(QMenu *popup, const KFileItemListProperties &itemProps);
    void doCopyToDirectory();

    const QMimeData *m_mimeData;
    const QList<QUrl> m_urls;
    QMap<QString, QString> m_metaData;
    Qt::DropAction m_dropAction;
    QPoint m_globalPos;
    Qt::KeyboardModifiers m_keyboardModifiers;
    QUrl m_destUrl;
    KFileItem m_destItem; // null for remote URLs not found in the dirlister cache
    const JobFlags m_flags;
    QList<QAction *> m_appActions;
    QList<QAction *> m_pluginActions;

    Q_DECLARE_PUBLIC(DropJob)

    void slotStart();
    void slotTriggered(QAction *);

    static inline DropJob *newJob(const QDropEvent *dropEvent, const QUrl &destUrl, JobFlags flags)
    {
        DropJob *job = new DropJob(*new DropJobPrivate(dropEvent, destUrl, flags));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        // Note: never KIO::getJobTracker()->registerJob here.
        // We don't want a progress dialog during the copy/move/link popup, it would in fact close
        // the popup
        return job;
    }

};

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

// Input: m_dropAction as set by Qt at the time of the drop event
// Output: m_dropAction possibly modified
// Returns a KIO error code, in case of error.
int DropJobPrivate::determineDropAction()
{
    Q_Q(DropJob);

    if (!KProtocolManager::supportsWriting(m_destUrl)) {
        return KIO::ERR_CANNOT_WRITE;
    }
    if (!m_destItem.isNull() && !m_destItem.isWritable()) {
        return KIO::ERR_WRITE_ACCESS_DENIED;
    }

    bool allItemsAreFromTrash = true;
    bool containsTrashRoot = false;
    foreach (const QUrl &url, m_urls) {
        const bool local = url.isLocalFile();
        if (!local /*optimization*/ && url.scheme() == QLatin1String("trash")) {
            if (url.path().isEmpty() || url.path() == QLatin1String("/")) {
                containsTrashRoot = true;
            }
        } else {
            allItemsAreFromTrash = false;
        }
        if (url.matches(m_destUrl, QUrl::StripTrailingSlash)) {
            return KIO::ERR_DROP_ON_ITSELF;
        }
    }

    const bool trashing = m_destUrl.scheme() == QLatin1String("trash");
    if (trashing) {
        m_dropAction = Qt::MoveAction;
        if (!q->uiDelegateExtension()->askDeleteConfirmation(m_urls, KIO::JobUiDelegate::Trash, KIO::JobUiDelegate::DefaultConfirmation)) {
            return KIO::ERR_USER_CANCELED;
        }
        return KJob::NoError; // ok
    }
    if (containsTrashRoot) {
        // Dropping a link to the trash: don't move the full contents, just make a link (#319660)
        m_dropAction = Qt::LinkAction;
        return KJob::NoError; // ok
    }
    if (allItemsAreFromTrash) {
        // No point in asking copy/move/link when using dragging from the trash, just move the file out.
        m_dropAction = Qt::MoveAction;
        return KJob::NoError; // ok
    }
    if (m_keyboardModifiers & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier)) {
        // Qt determined m_dropAction from the modifiers already
        return KJob::NoError; // ok
    }

    // We need to ask the user with a popup menu. Let the caller know.
    return KIO::ERR_UNKNOWN;
}

void DropJobPrivate::fillPopupMenu(QMenu *popup)
{
    Q_Q(DropJob);

    // Check what the source can do
    // TODO: Determining the mimetype of the source URLs is difficult for remote URLs,
    // we would need to KIO::stat each URL in turn, asynchronously....
    KFileItemList fileItems;
    foreach (const QUrl &url, m_urls) {
        fileItems.append(KFileItem(url));
    }
    const KFileItemListProperties itemProps(fileItems);

    emit q->popupMenuAboutToShow(fileItems);

    const bool sReading = itemProps.supportsReading();
    const bool sDeleting = itemProps.supportsDeleting();
    const bool sMoving = itemProps.supportsMoving();

    QString seq = QKeySequence(Qt::ShiftModifier).toString();
    Q_ASSERT(seq.endsWith('+'));
    seq.chop(1); // chop superfluous '+'
    QAction* popupMoveAction = new QAction(i18n("&Move Here") + '\t' + seq, popup);
    popupMoveAction->setIcon(QIcon::fromTheme(QStringLiteral("go-jump")));
    popupMoveAction->setData(QVariant::fromValue(Qt::MoveAction));
    seq = QKeySequence(Qt::ControlModifier).toString();
    Q_ASSERT(seq.endsWith('+'));
    seq.chop(1);
    QAction* popupCopyAction = new QAction(i18n("&Copy Here") + '\t' + seq, popup);
    popupCopyAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    popupCopyAction->setData(QVariant::fromValue(Qt::CopyAction));
    seq = QKeySequence(Qt::ControlModifier + Qt::ShiftModifier).toString();
    Q_ASSERT(seq.endsWith('+'));
    seq.chop(1);
    QAction* popupLinkAction = new QAction(i18n("&Link Here") + '\t' + seq, popup);
    popupLinkAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-link")));
    popupLinkAction->setData(QVariant::fromValue(Qt::LinkAction));
    QAction* popupCancelAction = new QAction(i18n("C&ancel") + '\t' + QKeySequence(Qt::Key_Escape).toString(), popup);
    popupCancelAction->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));

    if (sMoving || (sReading && sDeleting)) {
        bool equalDestination = true;
        foreach (const QUrl &src, m_urls) {
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

    popup->addSeparator();
    popup->addAction(popupCancelAction);
}

void DropJobPrivate::addPluginActions(QMenu *popup, const KFileItemListProperties &itemProps)
{
    Q_Q(DropJob);

    const QVector<KPluginMetaData> plugin_offers = KPluginLoader::findPlugins("kf5/kio_dnd");
    foreach (const KPluginMetaData &service, plugin_offers) {
        KPluginFactory *factory = KPluginLoader(service.fileName()).factory();
        if (factory) {
            KIO::DndPopupMenuPlugin *plugin = factory->create<KIO::DndPopupMenuPlugin>();
            if (plugin) {
                m_pluginActions += plugin->setup(itemProps, m_destUrl);
            }
        }
    }

    if (!m_appActions.isEmpty() || !m_pluginActions.isEmpty()) {
        popup->addSeparator();
        popup->addActions(m_appActions);
        popup->addActions(m_pluginActions);
    }

}

void DropJob::setApplicationActions(const QList<QAction *> &actions)
{
    Q_D(DropJob);
    d->m_appActions = actions;
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

void DropJobPrivate::handleCopyToDirectory()
{
    Q_Q(DropJob);

    if (int error = determineDropAction()) {
        if (error == KIO::ERR_UNKNOWN) {
            QMenu *menu = new QMenu(KJobWidgets::window(q));
            QObject::connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
            fillPopupMenu(menu);
            QObject::connect(menu, &QMenu::triggered, q, [this](QAction* action) { slotTriggered(action); });
            menu->popup(m_globalPos);
        } else {
            q->setError(error);
            q->emitResult();
        }
    } else {
        doCopyToDirectory();
    }
}

void DropJobPrivate::doCopyToDirectory()
{
    Q_Q(DropJob);
    KIO::CopyJob * job = 0;
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
        qWarning() << "Unknown drop action" << (int)m_dropAction;
        q->setError(KIO::ERR_UNSUPPORTED_ACTION);
        q->emitResult();
        return;
    }
    Q_ASSERT(job);
    job->setMetaData(m_metaData);
    QObject::connect(job, &KIO::CopyJob::copyingDone, q, [q](KIO::Job*, const QUrl &, const QUrl &to) {
            emit q->itemCreated(to);
    });
    QObject::connect(job, &KIO::CopyJob::copyingLinkDone, q, [q](KIO::Job*, const QUrl&, const QString&, const QUrl &to) {
            emit q->itemCreated(to);
    });
    q->addSubjob(job);
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
        KService service(destFile);
        if (!KRun::runService(service, m_urls, KJobWidgets::window(q))) {
            q->setError(KIO::ERR_CANNOT_LAUNCH_PROCESS);
            q->setErrorText(destFile);
        }
        q->emitResult();
    } else if (desktopFile.hasLinkType() && desktopGroup.hasKey(urlKey)) {
        // Drop to link -> adjust destination directory
        m_destUrl = QUrl::fromUserInput(desktopGroup.readPathEntry(urlKey, QString()));
        handleCopyToDirectory();
    } else {
        if (desktopFile.hasDeviceType()) {
            qWarning() << "Not re-implemented; please email kde-frameworks-devel@kde.org if you need this.";
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
    // Launch executable for each of the files
    QStringList args;
    Q_FOREACH(const QUrl &url, m_urls) {
        args << url.toLocalFile(); // assume local files
    }
    QProcess::startDetached(m_destUrl.toLocalFile(), args);
}

void DropJob::slotResult(KJob *job)
{
    Q_D(DropJob);
    if (job->error()) {
        KIO::Job::slotResult(job); // will set the error and emit result(this)
        return;
    }
    removeSubjob(job);
    emitResult();
}

DropJob * KIO::drop(const QDropEvent *dropEvent, const QUrl &destUrl, JobFlags flags)
{
    return DropJobPrivate::newJob(dropEvent, destUrl, flags);
}

#include "moc_dropjob.cpp"
