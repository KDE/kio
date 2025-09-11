/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000 Simon Hausmann <hausmann@kde.org>
    SPDX-FileCopyrightText: 2006, 2008 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "fileundomanager.h"
#include "askuseractioninterface.h"
#include "clipboardupdater_p.h"
#ifdef WITH_QTDBUS
#include "fileundomanager_adaptor.h"
#endif
#include "fileundomanager_p.h"
#include "kio_widgets_debug.h"
#include <job_p.h>
#include <kdirnotify.h>
#include <kio/batchrenamejob.h>
#include <kio/copyjob.h>
#include <kio/filecopyjob.h>
#include <kio/jobuidelegate.h>
#include <kio/mkdirjob.h>
#include <kio/mkpathjob.h>
#include <kio/statjob.h>

#include <KJobTrackerInterface>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>

#ifdef WITH_QTDBUS
#include <QDBusConnection>
#endif

#include <QDateTime>
#include <QFileInfo>
#include <QLocale>

using namespace KIO;

static const char *undoStateToString(UndoState state)
{
    static const char *const s_undoStateToString[] = {"MAKINGDIRS", "MOVINGFILES", "STATINGFILE", "MOVINGLINK", "TRASHINGFILES", "REMOVINGDIRS"};
    return s_undoStateToString[state];
}

static QDataStream &operator<<(QDataStream &stream, const KIO::BasicOperation &op)
{
    stream << op.m_valid << (qint8)op.m_type << op.m_renamed << op.m_src << op.m_dst << op.m_target << qint64(op.m_mtime.toMSecsSinceEpoch() / 1000);
    return stream;
}
static QDataStream &operator>>(QDataStream &stream, BasicOperation &op)
{
    qint8 type;
    qint64 mtime;
    stream >> op.m_valid >> type >> op.m_renamed >> op.m_src >> op.m_dst >> op.m_target >> mtime;
    op.m_type = static_cast<BasicOperation::Type>(type);
    op.m_mtime = QDateTime::fromSecsSinceEpoch(mtime, QTimeZone::UTC);
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
        static const char *s_types[] = {"File", "Link", "Directory"};
        dbg << "BasicOperation: type" << s_types[op.m_type] << "src" << op.m_src << "dest" << op.m_dst << "target" << op.m_target << "renamed" << op.m_renamed;
    } else {
        dbg << "Invalid BasicOperation";
    }
    return dbg;
}
/*
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
    UndoJob(bool showProgressInfo)
        : KIO::Job()
    {
        if (showProgressInfo) {
            KIO::getJobTracker()->registerJob(this);
        }
    }

    ~UndoJob() override = default;

    void emitCreatingDir(const QUrl &dir)
    {
        Q_EMIT description(this, i18n("Creating directory"), qMakePair(i18n("Directory"), dir.toDisplayString()));
    }

    void emitCopying(const QUrl &src, const QUrl &dst)
    {
        Q_EMIT description(this, i18n("Copying"), qMakePair(i18n("Source"), src.toDisplayString()), qMakePair(i18n("Destination"), dst.toDisplayString()));
    }

    void emitMovingOrRenaming(const QUrl &src, const QUrl &dest, FileUndoManager::CommandType cmdType)
    {
        static const QString srcMsg(i18nc("The source of a file operation", "Source"));
        static const QString destMsg(i18nc("The destination of a file operation", "Destination"));

        Q_EMIT description(this, //
                           cmdType == FileUndoManager::Move ? i18n("Moving") : i18n("Renaming"),
                           {srcMsg, src.toDisplayString()},
                           {destMsg, dest.toDisplayString()});
    }

    void emitTrashing()
    {
        Q_EMIT description(this, i18n("Moving to Trash"));
    }

    void emitDeleting(const QUrl &url)
    {
        Q_EMIT description(this, i18n("Deleting"), qMakePair(i18n("File"), url.toDisplayString()));
    }

    void emitResult()
    {
        KIO::Job::emitResult();
    }

protected:
    bool doKill() override
    {
        FileUndoManager::self()->d->stopUndoOrRedo(true);
        return KIO::Job::doKill();
    }
};

CommandRecorder::CommandRecorder(FileUndoManager::CommandType op,
                                 const QList<QUrl> &src,
                                 const QUrl &dst,
                                 std::function<void(UndoCommand)> onFinished,
                                 KIO::Job *job)
    : QObject(job)
    , m_cmd(op, src, dst, FileUndoManager::self()->newCommandSerialNumber())
    , m_onFinished(onFinished)
{
    connect(job, &KJob::result, this, &CommandRecorder::slotResult);
    if (auto *copyJob = qobject_cast<KIO::CopyJob *>(job)) {
        connect(copyJob, &KIO::CopyJob::copyingDone, this, &CommandRecorder::slotCopyingDone);
        connect(copyJob, &KIO::CopyJob::copyingLinkDone, this, &CommandRecorder::slotCopyingLinkDone);
    } else if (auto *mkpathJob = qobject_cast<KIO::MkpathJob *>(job)) {
        connect(mkpathJob, &KIO::MkpathJob::directoryCreated, this, &CommandRecorder::slotDirectoryCreated);
    } else if (auto *batchRenameJob = qobject_cast<KIO::BatchRenameJob *>(job)) {
        connect(batchRenameJob, &KIO::BatchRenameJob::fileRenamed, this, &CommandRecorder::slotBatchRenamingDone);
    }
}

void CommandRecorder::slotResult(KJob *job)
{
    const int err = job->error();
    if (err) {
        if (err != KIO::ERR_USER_CANCELED) {
            qCDebug(KIO_WIDGETS) << "CommandRecorder::slotResult:" << job->errorString() << " - no undo command will be added";
        }
        return;
    }

    // For CopyJob, don't add an undo command unless the job actually did something,
    // e.g. if user selected to skip all, there is nothing to undo.
    // Note: this doesn't apply to other job types, e.g. for Mkdir m_opQueue is
    // expected to be empty
    if (qobject_cast<KIO::CopyJob *>(job)) {
        if (!m_cmd.m_opQueue.isEmpty()) {
            m_onFinished(m_cmd);
        }
        return;
    }

    m_onFinished(m_cmd);
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
    m_cmd.m_opQueue.enqueue(BasicOperation(BasicOperation::Item, true, from, to, {}));
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
    : m_uiInterface(new FileUndoManager::UiInterface())
    , m_nextCommandIndex(1000)
    , q(qq)
{
#ifdef WITH_QTDBUS
    (void)new KIOFileUndoManagerAdaptor(this);
    const QString dbusPath = QStringLiteral("/FileUndoManager");
    const QString dbusInterface = QStringLiteral("org.kde.kio.FileUndoManager");

    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(dbusPath, this);
    dbus.connect(QString(), dbusPath, dbusInterface, QStringLiteral("lock"), this, SLOT(slotLock()));
    dbus.connect(QString(), dbusPath, dbusInterface, QStringLiteral("pop"), this, SLOT(slotPopUndoCommand()));
    dbus.connect(QString(), dbusPath, dbusInterface, QStringLiteral("push"), this, SLOT(slotPushUndoCommand(QByteArray)));
    dbus.connect(QString(), dbusPath, dbusInterface, QStringLiteral("unlock"), this, SLOT(slotUnlock()));
#endif
}

FileUndoManager::FileUndoManager()
    : d(new FileUndoManagerPrivate(this))
{
}

FileUndoManager::~FileUndoManager() = default;

void FileUndoManager::recordJob(CommandType op, const QList<QUrl> &src, const QUrl &dst, KIO::Job *job)
{
    // This records what the job does and calls addUndoCommand when done
    auto onFinished = [this](UndoCommand cmd) {
        d->addUndoCommand(cmd);
    };
    (void)new CommandRecorder(op, src, dst, onFinished, job);
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

void FileUndoManagerPrivate::addUndoCommand(const UndoCommand &cmd)
{
    clearRedoStack();
    pushUndoCommand(cmd);
    Q_EMIT q->jobRecordingFinished(cmd.m_type);
}

bool FileUndoManager::isUndoAvailable() const
{
    return !d->m_undoCommands.isEmpty() && !d->m_lock;
}

bool FileUndoManager::isRedoAvailable() const
{
    return !d->m_redoCommands.isEmpty() && !d->m_lock;
}

QString FileUndoManager::undoText() const
{
    if (d->m_undoCommands.isEmpty()) {
        return i18n("Und&o");
    }

    FileUndoManager::CommandType t = d->m_undoCommands.top().m_type;
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

QString FileUndoManager::redoText() const
{
    if (d->m_redoCommands.isEmpty()) {
        return i18n("&Redo");
    }

    FileUndoManager::CommandType t = d->m_redoCommands.top().m_type;
    switch (t) {
    case FileUndoManager::Copy:
        return i18n("&Redo: Copy");
    case FileUndoManager::Link:
        return i18n("&Redo: Link");
    case FileUndoManager::Move:
        return i18n("&Redo: Move");
    case FileUndoManager::Rename:
        return i18n("&Redo: Rename");
    case FileUndoManager::Trash:
        return i18n("&Redo: Trash");
    case FileUndoManager::Mkdir:
        return i18n("&Redo: Create Folder");
    case FileUndoManager::Mkpath:
        return i18n("&Redo: Create Folder(s)");
    case FileUndoManager::Put:
        return i18n("&Redo: Create File");
    case FileUndoManager::BatchRename:
        return i18n("&Redo: Batch Rename");
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
    if (!d->m_undoCommands.isEmpty()) {
        const UndoCommand &cmd = d->m_undoCommands.top();
        Q_ASSERT(cmd.m_valid);
        return cmd.m_serialNumber;
    }

    return 0;
}

void FileUndoManager::undo()
{
    Q_ASSERT(!d->m_undoCommands.isEmpty()); // forgot to record before calling undo?

    // Make a copy of the command to undo before slotPopUndoCommand() pops it.
    UndoCommand cmd = d->m_undoCommands.last();
    Q_ASSERT(cmd.m_valid);
    d->m_currentCmd = d->m_cmdToBePushed = cmd;

    d->startUndoOrRedo(false);
}

void FileUndoManager::redo()
{
    Q_ASSERT(!d->m_redoCommands.isEmpty()); // forgot to record before calling redo?

    // Make a copy of the command to redo before slotPopRedoCommand() pops it.
    UndoCommand cmd = d->m_redoCommands.last();
    Q_ASSERT(cmd.m_valid);
    d->m_currentCmd = d->m_cmdToBePushed = cmd;

    d->startUndoOrRedo(true);
}

void FileUndoManagerPrivate::startUndoOrRedo(bool redo)
{
    slotLock();
    if (redo) {
        popRedoCommand();
    } else {
        slotPopUndoCommand();
    }

    m_dirCleanupStack.clear();
    m_dirStack.clear();
    m_dirsToUpdate.clear();

    m_undoState = MOVINGFILES;

    // Let's have a look at the basic operations we need to undo.
    auto &opQueue = m_currentCmd.m_opQueue;
    for (auto it = opQueue.rbegin(); it != opQueue.rend(); ++it) {
        const BasicOperation::Type type = (*it).m_type;
        if (type == BasicOperation::Directory && !(*it).m_renamed) {
            // If any directory has to be created/deleted, we'll start with that
            m_undoState = MAKINGDIRS;
            // Collect all the dirs that have to be created in case of a move undo.
            if (m_currentCmd.isMoveOrRename()) {
                if (redo) {
                    m_dirCleanupStack.prepend((*it).m_src);
                } else {
                    m_dirStack.push((*it).m_src);
                }
            }
            // Collect all dirs that have to be deleted
            // from the destination in both cases (copy and move).
            if (redo) {
                m_dirStack.push((*it).m_dst);
            } else {
                m_dirCleanupStack.prepend((*it).m_dst);
            }
        }
    }
    auto isBasicOperation = [](const BasicOperation &op) {
        return (op.m_type == BasicOperation::Directory && !op.m_renamed);
    };
    opQueue.erase(std::remove_if(opQueue.begin(), opQueue.end(), isBasicOperation), opQueue.end());

    const FileUndoManager::CommandType commandType = m_currentCmd.m_type;
    if (commandType == FileUndoManager::Put) {
        if (redo) {
            m_cmdToBePushed.m_opQueue.clear();
        } else {
            m_fileTrashStack.append(m_currentCmd.m_dst);
        }
    } else if (commandType == FileUndoManager::Mkdir) {
        if (redo) {
            m_undoState = MAKINGDIRS;
            m_dirStack.push(m_currentCmd.m_dst);
        } else {
            m_dirCleanupStack.push(m_currentCmd.m_dst);
        }
    } else if (commandType == FileUndoManager::Trash && redo) {
        m_fileTrashStack.append(m_currentCmd.m_src);
        m_currentCmd.m_opQueue.clear();
    }

    qCDebug(KIO_WIDGETS) << "starting with" << undoStateToString(m_undoState);
    m_undoJob = new UndoJob(m_uiInterface->showProgressInfo());
    auto func = [this, redo]() {
        processStep(redo);
    };
    auto onFinished = [this, redo](KJob *job) {
        if (!job->error()) {
            if (redo) {
                pushUndoCommand(m_cmdToBePushed);
            } else {
                pushRedoCommand(m_cmdToBePushed);
            }
        }
    };
    connect(m_undoJob, &KIO::UndoJob::result, this, onFinished);
    QMetaObject::invokeMethod(this, func, Qt::QueuedConnection);
}

void FileUndoManagerPrivate::stopUndoOrRedo(bool step)
{
    m_currentCmd.m_opQueue.clear();
    m_dirCleanupStack.clear();
    m_fileTrashStack.clear();
    m_undoState = REMOVINGDIRS;
    m_undoJob = nullptr;

    if (m_currentJob) {
        m_currentJob->kill();
    }

    m_currentJob = nullptr;

    if (step) {
        processStep(false);
    }
}

void FileUndoManagerPrivate::slotUndoResult(KJob *job)
{
    m_currentJob = nullptr;
    if (job->error()) {
        qWarning() << job->errorString();
        m_uiInterface->jobError(static_cast<KIO::Job *>(job));
        delete m_undoJob;
        stopUndoOrRedo(false);
    } else if (m_undoState == STATINGFILE) {
        const BasicOperation op = m_currentCmd.m_opQueue.head();
        // qDebug() << "stat result for " << op.m_dst;
        KIO::StatJob *statJob = static_cast<KIO::StatJob *>(job);
        const QDateTime mtime = QDateTime::fromSecsSinceEpoch(statJob->statResult().numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, -1), QTimeZone::UTC);
        if (mtime != op.m_mtime) {
            qCDebug(KIO_WIDGETS) << op.m_dst << "was modified after being copied. Initial timestamp" << mtime << "now" << op.m_mtime;
            QDateTime srcTime = op.m_mtime.toLocalTime();
            QDateTime destTime = mtime.toLocalTime();
            if (!m_uiInterface->copiedFileWasModified(op.m_src, op.m_dst, srcTime, destTime)) {
                stopUndoOrRedo(false);
            }
        }
    } else if (m_undoState == TRASHINGFILES) {
        Q_ASSERT(m_currentCmd.m_type == FileUndoManager::Put && m_cmdToBePushed.m_src.size() == 1);
        std::swap(m_cmdToBePushed.m_src.front(), m_cmdToBePushed.m_dst);
        for (BasicOperation &op : m_cmdToBePushed.m_opQueue) {
            std::swap(op.m_src, op.m_dst);
        }
    }

    processStep(false);
}

void FileUndoManagerPrivate::slotRedoResult(KJob *job)
{
    m_currentJob = nullptr;
    if (job->error()) {
        qWarning() << job->errorString();
        m_uiInterface->jobError(static_cast<KIO::Job *>(job));
        delete m_undoJob;
        stopUndoOrRedo(false);
    }

    processStep(true);
}

void FileUndoManagerPrivate::addDirToUpdate(const QUrl &url)
{
    if (!m_dirsToUpdate.contains(url)) {
        m_dirsToUpdate.prepend(url);
    }
}

void FileUndoManagerPrivate::processStep(bool redo)
{
    m_currentJob = nullptr;

    if (m_undoState == MAKINGDIRS) {
        stepMakingDirectories();
    }

    if (m_undoState == MOVINGFILES || m_undoState == STATINGFILE || m_undoState == MOVINGLINK) {
        if (redo) {
            redoStepMovingFiles();
        } else {
            undoStepMovingFiles();
        }
    }

    if (m_undoState == TRASHINGFILES) {
        stepTrashingFiles(redo);
    }

    if (m_undoState == REMOVINGDIRS) {
        stepRemovingDirectories();
    }

    if (m_currentJob) {
        if (m_uiInterface) {
            KJobWidgets::setWindow(m_currentJob, m_uiInterface->parentWidget());
        }
        QObject::connect(m_currentJob, &KJob::result, this, redo ? &FileUndoManagerPrivate::slotRedoResult : &FileUndoManagerPrivate::slotUndoResult);
    }
}

void FileUndoManagerPrivate::stepMakingDirectories()
{
    if (!m_dirStack.isEmpty()) {
        QUrl dir = m_dirStack.pop();
        // qDebug() << "creatingDir" << dir;
        m_currentJob = KIO::mkdir(dir);
        m_currentJob->setParentJob(m_undoJob);
        m_undoJob->emitCreatingDir(dir);
    } else {
        m_undoState = MOVINGFILES;
    }
}

void FileUndoManagerPrivate::stepTrashingFiles(bool redo)
{
    if (!m_fileTrashStack.empty()) {
        m_currentJob = KIO::trash(m_fileTrashStack, KIO::HideProgressInfo);
        m_currentJob->setParentJob(m_undoJob);
        auto onFinished = [this](UndoCommand cmd) {
            m_cmdToBePushed = cmd;
        };
        new CommandRecorder(m_currentCmd.m_type, m_fileTrashStack, QUrl(QStringLiteral("trash:/")), onFinished, m_currentJob);
        connect(m_currentJob, &KJob::result, this, redo ? &FileUndoManagerPrivate::slotRedoResult : &FileUndoManagerPrivate::slotUndoResult);
        m_undoJob->emitTrashing();

        while (!m_fileTrashStack.empty()) {
            const QUrl url = m_fileTrashStack.pop().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
            addDirToUpdate(url);
        }
    } else {
        m_undoState = REMOVINGDIRS;
    }
}

void FileUndoManagerPrivate::stepRemovingDirectories()
{
    if (!m_dirCleanupStack.isEmpty()) {
        QUrl dir = m_dirCleanupStack.pop();
        // qDebug() << "rmdir" << dir;
        m_currentJob = KIO::rmdir(dir);
        m_currentJob->setParentJob(m_undoJob);
        m_undoJob->emitDeleting(dir);
        addDirToUpdate(dir);
    } else {
        m_currentCmd.m_valid = false;
        m_currentJob = nullptr;
        if (m_undoJob) {
            // qDebug() << "deleting undojob";
            m_undoJob->emitResult();
            m_undoJob = nullptr;
        }
#ifdef WITH_QTDBUS
        for (const QUrl &url : std::as_const(m_dirsToUpdate)) {
            // qDebug() << "Notifying FilesAdded for " << url;
            org::kde::KDirNotify::emitFilesAdded(url);
        }
#endif
        slotUnlock();
        Q_EMIT q->undoJobFinished();
    }
}

// Misnamed method: It moves files back, but it also
// renames directories back, recreates symlinks,
// deletes copied files, and restores trashed files.
void FileUndoManagerPrivate::undoStepMovingFiles()
{
    if (m_currentCmd.m_opQueue.isEmpty()) {
        m_undoState = TRASHINGFILES;
        return;
    }

    const BasicOperation op = m_currentCmd.m_opQueue.head();
    Q_ASSERT(op.m_valid);
    if (op.m_type == BasicOperation::Directory || op.m_type == BasicOperation::Item) {
        Q_ASSERT(op.m_renamed);
        // qDebug() << "rename" << op.m_dst << op.m_src;
        m_currentJob = KIO::rename(op.m_dst, op.m_src, KIO::HideProgressInfo);
        m_undoJob->emitMovingOrRenaming(op.m_dst, op.m_src, m_currentCmd.m_type);
    } else if (op.m_type == BasicOperation::Link) {
        if (m_currentCmd.isMoveOrRename() && m_undoState != MOVINGLINK) { // Moving or renaming a link is done in two steps
            m_currentJob = KIO::symlink(op.m_target, op.m_src, KIO::HideProgressInfo);
            m_undoState = MOVINGLINK; // temporarily
            return;
        } else {
            m_currentJob = KIO::file_delete(op.m_dst);
            m_undoState = MOVINGFILES;
        }
    } else if (m_currentCmd.m_type == FileUndoManager::Copy) {
        if (m_undoState == MOVINGFILES) { // dest not stat'ed yet
            // Before we delete op.m_dst, let's check if it was modified (#20532)
            // qDebug() << "stat" << op.m_dst;
            m_currentJob = KIO::stat(op.m_dst, KIO::HideProgressInfo);
            m_undoState = STATINGFILE; // temporarily
            return; // no pop() yet, we'll finish the work in slotResult
        } else { // dest was stat'ed, and the deletion was approved in slotResult
            m_currentJob = KIO::file_delete(op.m_dst, KIO::HideProgressInfo);
            m_undoJob->emitDeleting(op.m_dst);
            m_undoState = MOVINGFILES;
        }
    } else if (m_currentCmd.isMoveOrRename() || m_currentCmd.m_type == FileUndoManager::Trash) {
        m_currentJob = KIO::file_move(op.m_dst, op.m_src, -1, KIO::HideProgressInfo);
        m_currentJob->uiDelegateExtension()->createClipboardUpdater(m_currentJob, JobUiDelegateExtension::UpdateContent);
        m_undoJob->emitMovingOrRenaming(op.m_dst, op.m_src, m_currentCmd.m_type);
    }

    if (m_currentJob) {
        m_currentJob->setParentJob(m_undoJob);
    }

    m_currentCmd.m_opQueue.dequeue();
    // The above KIO jobs are lowlevel, they don't trigger KDirNotify notification
    // So we need to do it ourselves (but schedule it to the end of the undo, to compress them)
    QUrl url = op.m_dst.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    addDirToUpdate(url);

    url = op.m_src.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    addDirToUpdate(url);
}

void FileUndoManagerPrivate::redoStepMovingFiles()
{
    if (m_currentCmd.m_opQueue.isEmpty()) {
        m_undoState = TRASHINGFILES;
        return;
    }

    const BasicOperation op = m_currentCmd.m_opQueue.head();
    Q_ASSERT(op.m_valid);
    if (op.m_type == BasicOperation::Directory || op.m_type == BasicOperation::Item) {
        Q_ASSERT(op.m_renamed);
        // qDebug() << "rename" << op.m_dst << op.m_src;
        m_currentJob = KIO::rename(op.m_src, op.m_dst, KIO::HideProgressInfo);
        m_undoJob->emitMovingOrRenaming(op.m_src, op.m_dst, m_currentCmd.m_type);
    } else if (op.m_type == BasicOperation::Link) {
        if (m_currentCmd.isMoveOrRename() && m_undoState != MOVINGLINK) { // Moving or renaming a link is done in two steps
            m_currentJob = KIO::file_delete(op.m_src);
            m_undoState = MOVINGLINK; // temporarily
            return;
        } else {
            m_currentJob = KIO::symlink(op.m_target, op.m_dst);
            m_undoState = MOVINGFILES;
        }
    } else if (m_currentCmd.m_type == FileUndoManager::Copy) {
        m_currentJob = KIO::file_copy(op.m_src, op.m_dst, -1, KIO::HideProgressInfo);
        m_undoJob->emitCopying(op.m_src, op.m_dst);
    } else if (m_currentCmd.isMoveOrRename() || m_currentCmd.m_type == FileUndoManager::Put) {
        m_currentJob = KIO::file_move(op.m_src, op.m_dst, -1, KIO::HideProgressInfo);
        m_currentJob->uiDelegateExtension()->createClipboardUpdater(m_currentJob, JobUiDelegateExtension::UpdateContent);
        m_undoJob->emitMovingOrRenaming(op.m_src, op.m_dst, m_currentCmd.m_type);
    }

    if (m_currentJob) {
        m_currentJob->setParentJob(m_undoJob);
    }

    m_currentCmd.m_opQueue.dequeue();
    // The above KIO jobs are lowlevel, they don't trigger KDirNotify notification
    // So we need to do it ourselves (but schedule it to the end of the undo, to compress them)
    QUrl url = op.m_dst.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    addDirToUpdate(url);

    url = op.m_src.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    addDirToUpdate(url);
}

// const ref doesn't work due to QDataStream
void FileUndoManagerPrivate::slotPushUndoCommand(QByteArray data)
{
    QDataStream strm(&data, QIODevice::ReadOnly);
    UndoCommand cmd;
    strm >> cmd;
    clearRedoStack();
    pushUndoCommand(cmd);
}

void FileUndoManagerPrivate::pushUndoCommand(const UndoCommand &cmd)
{
    m_undoCommands.push(cmd);
    if (m_undoCommands.size() == 1 && !m_lock) {
        Q_EMIT q->undoAvailable(true);
    }
    Q_EMIT q->undoTextChanged(q->undoText());
}

void FileUndoManagerPrivate::slotPopUndoCommand()
{
    m_undoCommands.pop();
    if (m_undoCommands.empty() && !m_lock) {
        Q_EMIT q->undoAvailable(false);
    }
    Q_EMIT q->undoTextChanged(q->undoText());
}

void FileUndoManagerPrivate::slotLock()
{
    //  Q_ASSERT(!m_lock);
    if (q->isUndoAvailable()) {
        Q_EMIT q->undoAvailable(false);
    }
    if (q->isRedoAvailable()) {
        Q_EMIT q->redoAvailable(false);
    }
    m_lock = true;
}

void FileUndoManagerPrivate::slotUnlock()
{
    //  Q_ASSERT(m_lock);
    m_lock = false;
    if (q->isUndoAvailable()) {
        Q_EMIT q->undoAvailable(true);
    }
    if (q->isRedoAvailable()) {
        Q_EMIT q->redoAvailable(true);
    }
}

void FileUndoManagerPrivate::pushRedoCommand(const UndoCommand &cmd)
{
    m_redoCommands.push(cmd);
    if (m_redoCommands.size() == 1 && !m_lock) {
        Q_EMIT q->redoAvailable(true);
    }
    Q_EMIT q->redoTextChanged(q->redoText());
}

void FileUndoManagerPrivate::popRedoCommand()
{
    m_redoCommands.pop();
    if (m_redoCommands.empty() && !m_lock) {
        Q_EMIT q->redoAvailable(false);
    }
    Q_EMIT q->redoTextChanged(q->redoText());
}

void FileUndoManagerPrivate::clearRedoStack()
{
    bool wasEmpty = m_redoCommands.empty();
    m_redoCommands.clear();
    if (!wasEmpty && !m_lock) {
        Q_EMIT q->redoAvailable(false);
    }
    if (!wasEmpty) {
        Q_EMIT q->redoTextChanged(q->redoText());
    }
}

QByteArray FileUndoManagerPrivate::get() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << m_undoCommands;
    return data;
}

void FileUndoManager::setUiInterface(UiInterface *ui)
{
    d->m_uiInterface.reset(ui);
}

FileUndoManager::UiInterface *FileUndoManager::uiInterface() const
{
    return d->m_uiInterface.get();
}

////

class Q_DECL_HIDDEN FileUndoManager::UiInterface::UiInterfacePrivate
{
public:
    QPointer<QWidget> m_parentWidget;
    bool m_showProgressInfo = true;
};

FileUndoManager::UiInterface::UiInterface()
    : d(new UiInterfacePrivate)
{
}

FileUndoManager::UiInterface::~UiInterface() = default;

void FileUndoManager::UiInterface::jobError(KIO::Job *job)
{
    job->uiDelegate()->showErrorMessage();
}

bool FileUndoManager::UiInterface::copiedFileWasModified(const QUrl &src, const QUrl &dest, const QDateTime &srcTime, const QDateTime &destTime)
{
    Q_UNUSED(srcTime); // not sure it should appear in the msgbox
    // Possible improvement: only show the time if date is today
    const QString timeStr = QLocale().toString(destTime, QLocale::ShortFormat);
    const QString msg = i18n(
        "The file %1 was copied from %2, but since then it has apparently been modified at %3.\n"
        "Undoing the copy will delete the file, and all modifications will be lost.\n"
        "Are you sure you want to delete %4?",
        dest.toDisplayString(QUrl::PreferLocalFile),
        src.toDisplayString(QUrl::PreferLocalFile),
        timeStr,
        dest.toDisplayString(QUrl::PreferLocalFile));

    const auto result = KMessageBox::warningContinueCancel(d->m_parentWidget,
                                                           msg,
                                                           i18n("Undo File Copy Confirmation"),
                                                           KStandardGuiItem::cont(),
                                                           KStandardGuiItem::cancel(),
                                                           QString(),
                                                           KMessageBox::Options(KMessageBox::Notify) | KMessageBox::Dangerous);
    return result == KMessageBox::Continue;
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
        auto *p = static_cast<AskUserActionInterface **>(data);
        static KJobUiDelegate *delegate = KIO::createDefaultJobUiDelegate();
        static auto *askUserInterface = delegate ? delegate->findChild<AskUserActionInterface *>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
        *p = askUserInterface;
    }
}

#include "fileundomanager.moc"
#include "moc_fileundomanager.cpp"
#include "moc_fileundomanager_p.cpp"
