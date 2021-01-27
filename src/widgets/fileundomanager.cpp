/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000 Simon Hausmann <hausmann@kde.org>
    SPDX-FileCopyrightText: 2006, 2008 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "fileundomanager.h"
#include "fileundomanager_p.h"
#include "clipboardupdater_p.h"
#include "fileundomanager_adaptor.h"

#include <kdirnotify.h>
#include <kio/copyjob.h>
#include <kio/job.h>
#include <kio/mkdirjob.h>
#include <kio/mkpathjob.h>
#include <kio/batchrenamejob.h>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KJobTrackerInterface>
#include <kio/jobuidelegate.h>
#include <job_p.h>
#include "kio_widgets_debug.h"
#include "askuseractioninterface.h"

#include <QDateTime>
#include <QDBusConnection>
#include <QFileInfo>
#include <QLocale>

using namespace KIO;

static const char *undoStateToString(UndoState state)
{
    static const char *const s_undoStateToString[] = { "MAKINGDIRS", "MOVINGFILES", "STATINGFILE", "REMOVINGDIRS", "REMOVINGLINKS" };
    return s_undoStateToString[state];
}

static QDataStream &operator<<(QDataStream &stream, const KIO::BasicOperation &op)
{
    stream << op.m_valid << (qint8)op.m_type << op.m_renamed
           << op.m_src << op.m_dst << op.m_target << qint64(op.m_mtime.toMSecsSinceEpoch() / 1000);
    return stream;
}
static QDataStream &operator>>(QDataStream &stream, BasicOperation &op)
{
    qint8 type;
    qint64 mtime;
    stream >> op.m_valid >> type >> op.m_renamed
           >> op.m_src >> op.m_dst >> op.m_target >> mtime;
    op.m_type = static_cast<BasicOperation::Type>(type);
    op.m_mtime = QDateTime::fromMSecsSinceEpoch(1000 * mtime, Qt::UTC);
    return stream;
}

static QDataStream &operator<<(QDataStream &stream, const UndoCommand &cmd)
{
    stream << cmd.m_valid << (qint8)cmd.m_type << cmd.m_opQueue << cmd.m_src << cmd.m_dst;
    return stream;
}

static QDataStream &operator>>(QDataStream &stream, UndoCommand &cmd)
{
    qint8 type;
    stream >> cmd.m_valid >> type >> cmd.m_opQueue >> cmd.m_src >> cmd.m_dst;
    cmd.m_type = static_cast<FileUndoManager::CommandType>(type);
    return stream;
}

QDebug operator<<(QDebug dbg, const BasicOperation &op)
{
    if (op.m_valid) {
        static const char* s_types[] = { "File", "Link", "Directory" };
        dbg << "BasicOperation: type" << s_types[op.m_type] << "src" << op.m_src << "dest" << op.m_dst << "target" << op.m_target << "renamed" << op.m_renamed;
    } else {
        dbg << "Invalid BasicOperation";
    }
    return dbg;
}
/**
 * checklist:
 * copy dir -> overwrite -> works
 * move dir -> overwrite -> works
 * copy dir -> rename -> works
 * move dir -> rename -> works
 *
 * copy dir -> works
 * move dir -> works
 *
 * copy files -> works
 * move files -> works (TODO: optimize (change FileCopyJob to use the renamed arg for copyingDone)
 *
 * copy files -> overwrite -> works (sorry for your overwritten file...)
 * move files -> overwrite -> works (sorry for your overwritten file...)
 *
 * copy files -> rename -> works
 * move files -> rename -> works
 *
 * -> see also fileundomanagertest, which tests some of the above (but not renaming).
 *
 */

class KIO::UndoJob : public KIO::Job
{
    Q_OBJECT
public:
    UndoJob(bool showProgressInfo) : KIO::Job()
    {
        if (showProgressInfo) {
            KIO::getJobTracker()->registerJob(this);
        }

        d_ptr->m_privilegeExecutionEnabled = true;
        d_ptr->m_operationType = d_ptr->Other;
        d_ptr->m_caption = i18n("Undo Changes");
        d_ptr->m_message = i18n("Undoing this operation requires root privileges. Do you want to continue?");
    }
    ~UndoJob() override {}

    virtual void kill(bool) // TODO should be doKill
    {
        FileUndoManager::self()->d->stopUndo(true);
        KIO::Job::doKill();
    }

    void emitCreatingDir(const QUrl &dir)
    {
        Q_EMIT description(this, i18n("Creating directory"),
                         qMakePair(i18n("Directory"), dir.toDisplayString()));
    }
    void emitMoving(const QUrl &src, const QUrl &dest)
    {
        Q_EMIT description(this, i18n("Moving"),
                         qMakePair(i18nc("The source of a file operation", "Source"), src.toDisplayString()),
                         qMakePair(i18nc("The destination of a file operation", "Destination"), dest.toDisplayString()));
    }
    void emitDeleting(const QUrl &url)
    {
        Q_EMIT description(this, i18n("Deleting"),
                         qMakePair(i18n("File"), url.toDisplayString()));
    }
    void emitResult()
    {
        KIO::Job::emitResult();
    }
};

CommandRecorder::CommandRecorder(FileUndoManager::CommandType op, const QList<QUrl> &src, const QUrl &dst, KIO::Job *job)
    : QObject(job),
      m_cmd(op, src, dst, FileUndoManager::self()->newCommandSerialNumber())
{
    connect(job, &KJob::result,
            this, &CommandRecorder::slotResult);
    if (auto *copyJob = qobject_cast<KIO::CopyJob*>(job)) {
        connect(copyJob, &KIO::CopyJob::copyingDone,
                this, &CommandRecorder::slotCopyingDone);
        connect(copyJob, &KIO::CopyJob::copyingLinkDone,
                this, &CommandRecorder::slotCopyingLinkDone);
    } else if (auto *mkpathJob = qobject_cast<KIO::MkpathJob *>(job)) {
        connect(mkpathJob, &KIO::MkpathJob::directoryCreated,
                this, &CommandRecorder::slotDirectoryCreated);
    } else if (auto *batchRenameJob = qobject_cast<KIO::BatchRenameJob *>(job)) {
        connect(batchRenameJob, &KIO::BatchRenameJob::fileRenamed,
                this, &CommandRecorder::slotBatchRenamingDone);
    }
}

void CommandRecorder::slotResult(KJob *job)
{
    if (job->error()) {
        qWarning() << job->errorString();
        return;
    }

    FileUndoManager::self()->d->addCommand(m_cmd);
}

void CommandRecorder::slotCopyingDone(KIO::Job *, const QUrl &from, const QUrl &to, const QDateTime &mtime, bool directory, bool renamed)
{
    const BasicOperation::Type type = directory ? BasicOperation::Directory : BasicOperation::File;
    m_cmd.m_opQueue.enqueue(BasicOperation(type, renamed, from, to, mtime));
}

void CommandRecorder::slotCopyingLinkDone(KIO::Job *, const QUrl &from, const QString &target, const QUrl &to)
{
    m_cmd.m_opQueue.enqueue(BasicOperation(BasicOperation::Link, false, from, to, {}, target));
}

void CommandRecorder::slotDirectoryCreated(const QUrl &dir)
{
    m_cmd.m_opQueue.enqueue(BasicOperation(BasicOperation::Directory, false, QUrl{}, dir, {}));
}

void CommandRecorder::slotBatchRenamingDone(const QUrl &from, const QUrl &to)
{
    m_cmd.m_opQueue.enqueue(BasicOperation(BasicOperation::Directory, true, from, to, {}));
}

////

class KIO::FileUndoManagerSingleton
{
public:
    FileUndoManager self;
};
Q_GLOBAL_STATIC(KIO::FileUndoManagerSingleton, globalFileUndoManager)

FileUndoManager *FileUndoManager::self()
{
    return &globalFileUndoManager()->self;
}

// m_nextCommandIndex is initialized to a high number so that konqueror can
// assign low numbers to closed items loaded "on-demand" from a config file
// in KonqClosedWindowsManager::readConfig and thus maintaining the real
// order of the undo items.
FileUndoManagerPrivate::FileUndoManagerPrivate(FileUndoManager *qq)
    : m_uiInterface(new FileUndoManager::UiInterface()),
      m_nextCommandIndex(1000), q(qq)
{
    (void) new KIOFileUndoManagerAdaptor(this);
    const QString dbusPath = QStringLiteral("/FileUndoManager");
    const QString dbusInterface = QStringLiteral("org.kde.kio.FileUndoManager");

    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(dbusPath, this);
    dbus.connect(QString(), dbusPath, dbusInterface, QStringLiteral("lock"), this, SLOT(slotLock()));
    dbus.connect(QString(), dbusPath, dbusInterface, QStringLiteral("pop"), this, SLOT(slotPop()));
    dbus.connect(QString(), dbusPath, dbusInterface, QStringLiteral("push"), this, SLOT(slotPush(QByteArray)));
    dbus.connect(QString(), dbusPath, dbusInterface, QStringLiteral("unlock"), this, SLOT(slotUnlock()));
}

FileUndoManager::FileUndoManager()
    : d(new FileUndoManagerPrivate(this))
{
}

FileUndoManager::~FileUndoManager()
{
}

void FileUndoManager::recordJob(CommandType op, const QList<QUrl> &src, const QUrl &dst, KIO::Job *job)
{
    // This records what the job does and calls addCommand when done
    (void) new CommandRecorder(op, src, dst, job);
    Q_EMIT jobRecordingStarted(op);
}

void FileUndoManager::recordCopyJob(KIO::CopyJob *copyJob)
{
    CommandType commandType;
    switch (copyJob->operationMode()) {
    case CopyJob::Copy:
        commandType = Copy;
        break;
    case CopyJob::Move:
        commandType = Move;
        break;
    case CopyJob::Link:
        commandType = Link;
        break;
    default:
        Q_UNREACHABLE();
    }
    recordJob(commandType, copyJob->srcUrls(), copyJob->destUrl(), copyJob);
}

void FileUndoManagerPrivate::addCommand(const UndoCommand &cmd)
{
    pushCommand(cmd);
    Q_EMIT q->jobRecordingFinished(cmd.m_type);
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 79)
bool FileUndoManager::undoAvailable() const
{
    return isUndoAvailable();
}
#endif

bool FileUndoManager::isUndoAvailable() const
{
    return !d->m_commands.isEmpty() && !d->m_lock;
}

QString FileUndoManager::undoText() const
{
    if (d->m_commands.isEmpty()) {
        return i18n("Und&o");
    }

    FileUndoManager::CommandType t = d->m_commands.top().m_type;
    switch (t) {
    case FileUndoManager::Copy:
        return i18n("Und&o: Copy");
    case FileUndoManager::Link:
        return i18n("Und&o: Link");
    case FileUndoManager::Move:
        return i18n("Und&o: Move");
    case FileUndoManager::Rename:
        return i18n("Und&o: Rename");
    case FileUndoManager::Trash:
        return i18n("Und&o: Trash");
    case FileUndoManager::Mkdir:
        return i18n("Und&o: Create Folder");
    case FileUndoManager::Mkpath:
        return i18n("Und&o: Create Folder(s)");
    case FileUndoManager::Put:
        return i18n("Und&o: Create File");
    case FileUndoManager::BatchRename:
        return i18n("Und&o: Batch Rename");
    }
    /* NOTREACHED */
    return QString();
}

quint64 FileUndoManager::newCommandSerialNumber()
{
    return ++(d->m_nextCommandIndex);
}

quint64 FileUndoManager::currentCommandSerialNumber() const
{
    if (!d->m_commands.isEmpty()) {
        const UndoCommand &cmd = d->m_commands.top();
        Q_ASSERT(cmd.m_valid);
        return cmd.m_serialNumber;
    } else {
        return 0;
    }
}

void FileUndoManager::undo()
{
    Q_ASSERT(!d->m_commands.isEmpty()); // forgot to record before calling undo?

    // Make a copy of the command to undo before slotPop() pops it.
    UndoCommand cmd = d->m_commands.last();
    Q_ASSERT(cmd.m_valid);
    d->m_current = cmd;
    const CommandType commandType = cmd.m_type;

    // Note that m_opQueue is empty for simple operations like Mkdir.
    const auto &opQueue = d->m_current.m_opQueue;

    // Let's first ask for confirmation if we need to delete any file (#99898)
    QList<QUrl> itemsToDelete;
    for (auto it = opQueue.crbegin(); it != opQueue.crend(); ++it) {
        const BasicOperation &op = *it;
        const auto destination = op.m_dst;
        if (op.m_type == BasicOperation::File && commandType == FileUndoManager::Copy) {
            if (destination.isLocalFile() && !QFileInfo::exists(destination.toLocalFile())) {
                continue;
            }
            itemsToDelete.append(destination);
        } else if (commandType == FileUndoManager::Mkpath) {
            itemsToDelete.append(destination);
        }
    }
    if (commandType == FileUndoManager::Mkdir || commandType == FileUndoManager::Put) {
        itemsToDelete.append(d->m_current.m_dst);
    }
    if (!itemsToDelete.isEmpty()) {
        AskUserActionInterface *askUserInterface = nullptr;
        d->m_uiInterface->virtual_hook(UiInterface::HookGetAskUserActionInterface, &askUserInterface);
        if (askUserInterface) {
            if (!d->m_connectedToAskUserInterface) {
                d->m_connectedToAskUserInterface = true;
                QObject::connect(askUserInterface, &KIO::AskUserActionInterface::askUserDeleteResult,
                                 this, [=](bool allowDelete) {
                    if (allowDelete) {
                        d->startUndo();
                    }
                });
            }

            // Because undo can happen with an accidental Ctrl-Z, we want to always confirm.
            askUserInterface->askUserDelete(itemsToDelete, KIO::AskUserActionInterface::Delete,
                                            KIO::AskUserActionInterface::ForceConfirmation,
                                            d->m_uiInterface->parentWidget());
            return;
        }
    }

    d->startUndo();
}

void FileUndoManagerPrivate::startUndo()
{
    slotPop();
    slotLock();

    m_dirCleanupStack.clear();
    m_dirStack.clear();
    m_dirsToUpdate.clear();

    m_undoState = MOVINGFILES;

    // Let's have a look at the basic operations we need to undo.
    auto &opQueue = m_current.m_opQueue;
    for (auto it = opQueue.rbegin(); it != opQueue.rend(); ++it) {
        const BasicOperation::Type type = (*it).m_type;
        if (type == BasicOperation::Directory && !(*it).m_renamed) {
            // If any directory has to be created/deleted, we'll start with that
            m_undoState = MAKINGDIRS;
            // Collect all the dirs that have to be created in case of a move undo.
            if (m_current.isMoveCommand()) {
                m_dirStack.push((*it).m_src);
            }
            // Collect all dirs that have to be deleted
            // from the destination in both cases (copy and move).
            m_dirCleanupStack.prepend((*it).m_dst);
        } else if (type == BasicOperation::Link) {
            m_fileCleanupStack.prepend((*it).m_dst);
        }
    }
    auto isBasicOperation = [this](const BasicOperation &op) {
        return (op.m_type == BasicOperation::Directory && !op.m_renamed)
                || (op.m_type == BasicOperation::Link && !m_current.isMoveCommand());
    };
    opQueue.erase(std::remove_if(opQueue.begin(), opQueue.end(), isBasicOperation), opQueue.end());

    const FileUndoManager::CommandType commandType = m_current.m_type;
    if (commandType == FileUndoManager::Put) {
        m_fileCleanupStack.append(m_current.m_dst);
    }

    qCDebug(KIO_WIDGETS) << "starting with" << undoStateToString(m_undoState);
    m_undoJob = new UndoJob(m_uiInterface->showProgressInfo());
    QMetaObject::invokeMethod(this, "undoStep", Qt::QueuedConnection);
}

void FileUndoManagerPrivate::stopUndo(bool step)
{
    m_current.m_opQueue.clear();
    m_dirCleanupStack.clear();
    m_fileCleanupStack.clear();
    m_undoState = REMOVINGDIRS;
    m_undoJob = nullptr;

    if (m_currentJob) {
        m_currentJob->kill();
    }

    m_currentJob = nullptr;

    if (step) {
        undoStep();
    }
}

void FileUndoManagerPrivate::slotResult(KJob *job)
{
    m_currentJob = nullptr;
    if (job->error()) {
        qWarning() << job->errorString();
        m_uiInterface->jobError(static_cast<KIO::Job *>(job));
        delete m_undoJob;
        stopUndo(false);
    } else if (m_undoState == STATINGFILE) {
        const BasicOperation op = m_current.m_opQueue.head();
        //qDebug() << "stat result for " << op.m_dst;
        KIO::StatJob *statJob = static_cast<KIO::StatJob *>(job);
        const QDateTime mtime = QDateTime::fromMSecsSinceEpoch(1000 * statJob->statResult().numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, -1), Qt::UTC);
        if (mtime != op.m_mtime) {
            qCDebug(KIO_WIDGETS) << op.m_dst << "was modified after being copied. Initial timestamp" << mtime << "now" << op.m_mtime;
            QDateTime srcTime = op.m_mtime.toLocalTime();
            QDateTime destTime = mtime.toLocalTime();
            if (!m_uiInterface->copiedFileWasModified(op.m_src, op.m_dst, srcTime, destTime)) {
                stopUndo(false);
            }
        }
    }

    undoStep();
}

void FileUndoManagerPrivate::addDirToUpdate(const QUrl &url)
{
    if (!m_dirsToUpdate.contains(url)) {
        m_dirsToUpdate.prepend(url);
    }
}

void FileUndoManagerPrivate::undoStep()
{
    m_currentJob = nullptr;

    if (m_undoState == MAKINGDIRS) {
        stepMakingDirectories();
    }

    if (m_undoState == MOVINGFILES || m_undoState == STATINGFILE) {
        stepMovingFiles();
    }

    if (m_undoState == REMOVINGLINKS) {
        stepRemovingLinks();
    }

    if (m_undoState == REMOVINGDIRS) {
        stepRemovingDirectories();
    }

    if (m_currentJob) {
        if (m_uiInterface) {
            KJobWidgets::setWindow(m_currentJob, m_uiInterface->parentWidget());
        }
        QObject::connect(m_currentJob, &KJob::result,
                         this, &FileUndoManagerPrivate::slotResult);
    }
}

void FileUndoManagerPrivate::stepMakingDirectories()
{
    if (!m_dirStack.isEmpty()) {
        QUrl dir = m_dirStack.pop();
        //qDebug() << "creatingDir" << dir;
        m_currentJob = KIO::mkdir(dir);
        m_currentJob->setParentJob(m_undoJob);
        m_undoJob->emitCreatingDir(dir);
    } else {
        m_undoState = MOVINGFILES;
    }
}

// Misnamed method: It moves files back, but it also
// renames directories back, recreates symlinks,
// deletes copied files, and restores trashed files.
void FileUndoManagerPrivate::stepMovingFiles()
{
    if (!m_current.m_opQueue.isEmpty()) {
        const BasicOperation op = m_current.m_opQueue.head();
        Q_ASSERT(op.m_valid);
        if (op.m_type == BasicOperation::Directory) {
            Q_ASSERT(op.m_renamed);
            //qDebug() << "rename" << op.m_dst << op.m_src;
            m_currentJob = KIO::rename(op.m_dst, op.m_src, KIO::HideProgressInfo);
            m_undoJob->emitMoving(op.m_dst, op.m_src);
        } else if (op.m_type == BasicOperation::Link) {
            //qDebug() << "symlink" << op.m_target << op.m_src;
            m_currentJob = KIO::symlink(op.m_target, op.m_src, KIO::Overwrite | KIO::HideProgressInfo);
        } else if (m_current.m_type == FileUndoManager::Copy) {
            if (m_undoState == MOVINGFILES) { // dest not stat'ed yet
                // Before we delete op.m_dst, let's check if it was modified (#20532)
                //qDebug() << "stat" << op.m_dst;
                m_currentJob = KIO::stat(op.m_dst, KIO::HideProgressInfo);
                m_undoState = STATINGFILE; // temporarily
                return; // no pop() yet, we'll finish the work in slotResult
            } else { // dest was stat'ed, and the deletion was approved in slotResult
                m_currentJob = KIO::file_delete(op.m_dst, KIO::HideProgressInfo);
                m_undoJob->emitDeleting(op.m_dst);
                m_undoState = MOVINGFILES;
            }
        } else if (m_current.isMoveCommand()
                   || m_current.m_type == FileUndoManager::Trash) {
            //qDebug() << "file_move" << op.m_dst << op.m_src;
            m_currentJob = KIO::file_move(op.m_dst, op.m_src, -1, KIO::HideProgressInfo);
            m_currentJob->uiDelegateExtension()->createClipboardUpdater(m_currentJob, JobUiDelegateExtension::UpdateContent);
            m_undoJob->emitMoving(op.m_dst, op.m_src);
        }

        if (m_currentJob) {
            m_currentJob->setParentJob(m_undoJob);
        }

        m_current.m_opQueue.dequeue();
        // The above KIO jobs are lowlevel, they don't trigger KDirNotify notification
        // So we need to do it ourselves (but schedule it to the end of the undo, to compress them)
        QUrl url = op.m_dst.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
        addDirToUpdate(url);

        url = op.m_src.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
        addDirToUpdate(url);
    } else {
        m_undoState = REMOVINGLINKS;
    }
}

void FileUndoManagerPrivate::stepRemovingLinks()
{
    //qDebug() << "REMOVINGLINKS";
    if (!m_fileCleanupStack.isEmpty()) {
        const QUrl file = m_fileCleanupStack.pop();
        //qDebug() << "file_delete" << file;
        m_currentJob = KIO::file_delete(file, KIO::HideProgressInfo);
        m_currentJob->setParentJob(m_undoJob);
        m_undoJob->emitDeleting(file);

        const QUrl url = file.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
        addDirToUpdate(url);
    } else {
        m_undoState = REMOVINGDIRS;

        if (m_dirCleanupStack.isEmpty() && m_current.m_type == FileUndoManager::Mkdir) {
            m_dirCleanupStack << m_current.m_dst;
        }
    }
}

void FileUndoManagerPrivate::stepRemovingDirectories()
{
    if (!m_dirCleanupStack.isEmpty()) {
        QUrl dir = m_dirCleanupStack.pop();
        //qDebug() << "rmdir" << dir;
        m_currentJob = KIO::rmdir(dir);
        m_currentJob->setParentJob(m_undoJob);
        m_undoJob->emitDeleting(dir);
        addDirToUpdate(dir);
    } else {
        m_current.m_valid = false;
        m_currentJob = nullptr;
        if (m_undoJob) {
            //qDebug() << "deleting undojob";
            m_undoJob->emitResult();
            m_undoJob = nullptr;
        }
        for (const QUrl &url : qAsConst(m_dirsToUpdate)) {
            //qDebug() << "Notifying FilesAdded for " << url;
            org::kde::KDirNotify::emitFilesAdded(url);
        }
        Q_EMIT q->undoJobFinished();
        slotUnlock();
    }
}

// const ref doesn't work due to QDataStream
void FileUndoManagerPrivate::slotPush(QByteArray data)
{
    QDataStream strm(&data, QIODevice::ReadOnly);
    UndoCommand cmd;
    strm >> cmd;
    pushCommand(cmd);
}

void FileUndoManagerPrivate::pushCommand(const UndoCommand &cmd)
{
    m_commands.push(cmd);
    Q_EMIT q->undoAvailable(true);
    Q_EMIT q->undoTextChanged(q->undoText());
}

void FileUndoManagerPrivate::slotPop()
{
    m_commands.pop();
    Q_EMIT q->undoAvailable(q->isUndoAvailable());
    Q_EMIT q->undoTextChanged(q->undoText());
}

void FileUndoManagerPrivate::slotLock()
{
//  Q_ASSERT(!m_lock);
    m_lock = true;
    Q_EMIT q->undoAvailable(q->isUndoAvailable());
}

void FileUndoManagerPrivate::slotUnlock()
{
//  Q_ASSERT(m_lock);
    m_lock = false;
    Q_EMIT q->undoAvailable(q->isUndoAvailable());
}

QByteArray FileUndoManagerPrivate::get() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << m_commands;
    return data;
}

void FileUndoManager::setUiInterface(UiInterface *ui)
{
    delete d->m_uiInterface;
    d->m_uiInterface = ui;
}

FileUndoManager::UiInterface *FileUndoManager::uiInterface() const
{
    return d->m_uiInterface;
}

////

class Q_DECL_HIDDEN FileUndoManager::UiInterface::UiInterfacePrivate
{
public:
    UiInterfacePrivate()
        : m_parentWidget(nullptr), m_showProgressInfo(true)
    {}
    QWidget *m_parentWidget;
    bool m_showProgressInfo;
};

FileUndoManager::UiInterface::UiInterface()
    : d(new UiInterfacePrivate)
{
}

FileUndoManager::UiInterface::~UiInterface()
{
    delete d;
}

void FileUndoManager::UiInterface::jobError(KIO::Job *job)
{
    job->uiDelegate()->showErrorMessage();
}

bool FileUndoManager::UiInterface::copiedFileWasModified(const QUrl &src, const QUrl &dest, const QDateTime &srcTime, const QDateTime &destTime)
{
    Q_UNUSED(srcTime); // not sure it should appear in the msgbox
    // Possible improvement: only show the time if date is today
    const QString timeStr = QLocale().toString(destTime, QLocale::ShortFormat);
    return KMessageBox::warningContinueCancel(
               d->m_parentWidget,
               i18n("The file %1 was copied from %2, but since then it has apparently been modified at %3.\n"
                    "Undoing the copy will delete the file, and all modifications will be lost.\n"
                    "Are you sure you want to delete %4?", dest.toDisplayString(QUrl::PreferLocalFile), src.toDisplayString(QUrl::PreferLocalFile), timeStr, dest.toDisplayString(QUrl::PreferLocalFile)),
               i18n("Undo File Copy Confirmation"),
               KStandardGuiItem::cont(),
               KStandardGuiItem::cancel(),
               QString(),
               KMessageBox::Options(KMessageBox::Notify) | KMessageBox::Dangerous) == KMessageBox::Continue;
}

bool FileUndoManager::UiInterface::confirmDeletion(const QList<QUrl> &files)
{
    KIO::JobUiDelegate uiDelegate;
    uiDelegate.setWindow(d->m_parentWidget);
    // Because undo can happen with an accidental Ctrl-Z, we want to always confirm.
    return uiDelegate.askDeleteConfirmation(files, KIO::JobUiDelegate::Delete, KIO::JobUiDelegate::ForceConfirmation);
}

QWidget *FileUndoManager::UiInterface::parentWidget() const
{
    return d->m_parentWidget;
}

void FileUndoManager::UiInterface::setParentWidget(QWidget *parentWidget)
{
    d->m_parentWidget = parentWidget;
}

void FileUndoManager::UiInterface::setShowProgressInfo(bool b)
{
    d->m_showProgressInfo = b;
}

bool FileUndoManager::UiInterface::showProgressInfo() const
{
    return d->m_showProgressInfo;
}

void FileUndoManager::UiInterface::virtual_hook(int id, void *data)
{
    if (id == HookGetAskUserActionInterface) {
        auto *p = static_cast<AskUserActionInterface**>(data);
        static KJobUiDelegate *delegate = KIO::createDefaultJobUiDelegate();
        static auto *askUserInterface = delegate ? delegate->findChild<AskUserActionInterface *>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
        *p = askUserInterface;
    }
}

#include "moc_fileundomanager_p.cpp"
#include "moc_fileundomanager.cpp"
#include "fileundomanager.moc"
