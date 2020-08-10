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

#include <QDateTime>
#include <QDBusConnection>
#include <QFileInfo>
#include <QLocale>

#include <assert.h>

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
    stream << cmd.m_valid << (qint8)cmd.m_type << cmd.m_opStack << cmd.m_src << cmd.m_dst;
    return stream;
}

static QDataStream &operator>>(QDataStream &stream, UndoCommand &cmd)
{
    qint8 type;
    stream >> cmd.m_valid >> type >> cmd.m_opStack >> cmd.m_src >> cmd.m_dst;
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

    virtual void kill(bool)
    {
        FileUndoManager::self()->d->stopUndo(true);
        KIO::Job::doKill();
    }

    void emitCreatingDir(const QUrl &dir)
    {
        emit description(this, i18n("Creating directory"),
                         qMakePair(i18n("Directory"), dir.toDisplayString()));
    }
    void emitMoving(const QUrl &src, const QUrl &dest)
    {
        emit description(this, i18n("Moving"),
                         qMakePair(i18nc("The source of a file operation", "Source"), src.toDisplayString()),
                         qMakePair(i18nc("The destination of a file operation", "Destination"), dest.toDisplayString()));
    }
    void emitDeleting(const QUrl &url)
    {
        emit description(this, i18n("Deleting"),
                         qMakePair(i18n("File"), url.toDisplayString()));
    }
    void emitResult()
    {
        KIO::Job::emitResult();
    }
};

CommandRecorder::CommandRecorder(FileUndoManager::CommandType op, const QList<QUrl> &src, const QUrl &dst, KIO::Job *job)
    : QObject(job)
{
    m_cmd.m_type = op;
    m_cmd.m_valid = true;
    m_cmd.m_serialNumber = FileUndoManager::self()->newCommandSerialNumber();
    m_cmd.m_src = src;
    m_cmd.m_dst = dst;
    connect(job, &KJob::result,
            this, &CommandRecorder::slotResult);
    if (auto *copyJob = qobject_cast<KIO::CopyJob*>(job)) {
        connect(copyJob, &KIO::CopyJob::copyingDone,
                this, &CommandRecorder::slotCopyingDone);
        connect(copyJob, &KIO::CopyJob::copyingLinkDone,
                this, &CommandRecorder::slotCopyingLinkDone);
    } else if (KIO::MkpathJob *mkpathJob = qobject_cast<KIO::MkpathJob *>(job)) {
        connect(mkpathJob, &KIO::MkpathJob::directoryCreated,
                this, &CommandRecorder::slotDirectoryCreated);
    } else if (KIO::BatchRenameJob *batchRenameJob = qobject_cast<KIO::BatchRenameJob *>(job)) {
        connect(batchRenameJob, &KIO::BatchRenameJob::fileRenamed,
                this, &CommandRecorder::slotBatchRenamingDone);
    }
}

CommandRecorder::~CommandRecorder()
{
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
    BasicOperation op;
    op.m_valid = true;
    op.m_type = directory ? BasicOperation::Directory : BasicOperation::File;
    op.m_renamed = renamed;
    op.m_src = from;
    op.m_dst = to;
    op.m_mtime = mtime;
    //qDebug() << op;
    m_cmd.m_opStack.prepend(op);
}

// TODO merge the signals?
void CommandRecorder::slotCopyingLinkDone(KIO::Job *, const QUrl &from, const QString &target, const QUrl &to)
{
    BasicOperation op;
    op.m_valid = true;
    op.m_type = BasicOperation::Link;
    op.m_renamed = false;
    op.m_src = from;
    op.m_target = target;
    op.m_dst = to;
    op.m_mtime = QDateTime();
    m_cmd.m_opStack.prepend(op);
}

void CommandRecorder::slotDirectoryCreated(const QUrl &dir)
{
    BasicOperation op;
    op.m_valid = true;
    op.m_type = BasicOperation::Directory;
    op.m_renamed = false;
    op.m_src = QUrl();
    op.m_dst = dir;
    op.m_mtime = QDateTime();
    m_cmd.m_opStack.prepend(op);
}

void CommandRecorder::slotBatchRenamingDone(const QUrl &from, const QUrl &to)
{
    BasicOperation op;
    op.m_valid = true;
    op.m_type = BasicOperation::Directory;
    op.m_renamed = true;
    op.m_src = from;
    op.m_dst = to;
    op.m_mtime = QDateTime();
    m_cmd.m_opStack.prepend(op);
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
      m_undoJob(nullptr), m_nextCommandIndex(1000), q(qq)
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
{
    d = new FileUndoManagerPrivate(this);
    d->m_lock = false;
    d->m_currentJob = nullptr;
}

FileUndoManager::~FileUndoManager()
{
    delete d;
}

void FileUndoManager::recordJob(CommandType op, const QList<QUrl> &src, const QUrl &dst, KIO::Job *job)
{
    // This records what the job does and calls addCommand when done
    (void) new CommandRecorder(op, src, dst, job);
    emit jobRecordingStarted(op);
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
    default: // prevent "wrong" compiler warning because of possibly uninitialized variable
        commandType = Link;
        break;
    }
    recordJob(commandType, copyJob->srcUrls(), copyJob->destUrl(), copyJob);
}

void FileUndoManagerPrivate::addCommand(const UndoCommand &cmd)
{
    pushCommand(cmd);
    emit q->jobRecordingFinished(cmd.m_type);
}

bool FileUndoManager::undoAvailable() const
{
    return (d->m_commands.count() > 0) && !d->m_lock;
}

QString FileUndoManager::undoText() const
{
    if (d->m_commands.isEmpty()) {
        return i18n("Und&o");
    }

    FileUndoManager::CommandType t = d->m_commands.last().m_type;
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
        const UndoCommand &cmd = d->m_commands.last();
        assert(cmd.m_valid);
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
    assert(cmd.m_valid);
    d->m_current = cmd;
    const CommandType commandType = cmd.m_type;

    BasicOperation::Stack &opStack = d->m_current.m_opStack;
    // Note that opStack is empty for simple operations like Mkdir.

    // Let's first ask for confirmation if we need to delete any file (#99898)
    QList<QUrl> itemsToDelete;
    BasicOperation::Stack::Iterator it = opStack.begin();
    for (; it != opStack.end(); ++it) {
        BasicOperation::Type type = (*it).m_type;
        const auto destination = (*it).m_dst;
        if (type == BasicOperation::File && commandType == FileUndoManager::Copy) {
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
        if (!d->m_uiInterface->confirmDeletion(itemsToDelete)) {
            return;
        }
    } else if (commandType == FileUndoManager::Copy) {
        d->slotPop();
        return;
    }

    d->slotPop();
    d->slotLock();

    d->m_dirCleanupStack.clear();
    d->m_dirStack.clear();
    d->m_dirsToUpdate.clear();

    d->m_undoState = MOVINGFILES;

    // Let's have a look at the basic operations we need to undo.
    // While we're at it, collect all links that should be deleted.

    it = opStack.begin();
    while (it != opStack.end()) { // don't cache end() here, erase modifies it
        bool removeBasicOperation = false;
        BasicOperation::Type type = (*it).m_type;
        if (type == BasicOperation::Directory && !(*it).m_renamed) {
            // If any directory has to be created/deleted, we'll start with that
            d->m_undoState = MAKINGDIRS;
            // Collect all the dirs that have to be created in case of a move undo.
            if (d->m_current.isMoveCommand()) {
                d->m_dirStack.push((*it).m_src);
            }
            // Collect all dirs that have to be deleted
            // from the destination in both cases (copy and move).
            d->m_dirCleanupStack.prepend((*it).m_dst);
            removeBasicOperation = true;
        } else if (type == BasicOperation::Link) {
            d->m_fileCleanupStack.prepend((*it).m_dst);

            removeBasicOperation = !d->m_current.isMoveCommand();
        }

        if (removeBasicOperation) {
            it = opStack.erase(it);
        } else {
            ++it;
        }
    }

    if (commandType == FileUndoManager::Put) {
        d->m_fileCleanupStack.append(d->m_current.m_dst);
    }

    qCDebug(KIO_WIDGETS) << "starting with" << undoStateToString(d->m_undoState);
    d->m_undoJob = new UndoJob(d->m_uiInterface->showProgressInfo());
    QMetaObject::invokeMethod(d, "undoStep", Qt::QueuedConnection);
}

void FileUndoManagerPrivate::stopUndo(bool step)
{
    m_current.m_opStack.clear();
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
        BasicOperation op = m_current.m_opStack.last();
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
    if (!m_current.m_opStack.isEmpty()) {
        BasicOperation op = m_current.m_opStack.last();
        BasicOperation::Type type = op.m_type;

        assert(op.m_valid);
        if (type == BasicOperation::Directory) {
            if (op.m_renamed) {
                //qDebug() << "rename" << op.m_dst << op.m_src;
                m_currentJob = KIO::rename(op.m_dst, op.m_src, KIO::HideProgressInfo);
                m_undoJob->emitMoving(op.m_dst, op.m_src);
            } else {
                assert(0);    // this should not happen!
            }
        } else if (type == BasicOperation::Link) {
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
            m_currentJob = KIO::file_move(op.m_dst, op.m_src, -1, KIO::Overwrite | KIO::HideProgressInfo);
            m_currentJob->uiDelegateExtension()->createClipboardUpdater(m_currentJob, JobUiDelegateExtension::UpdateContent);
            m_undoJob->emitMoving(op.m_dst, op.m_src);
        }

        if (m_currentJob) {
            m_currentJob->setParentJob(m_undoJob);
        }

        m_current.m_opStack.removeLast();
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
        QUrl file = m_fileCleanupStack.pop();
        //qDebug() << "file_delete" << file;
        m_currentJob = KIO::file_delete(file, KIO::HideProgressInfo);
        m_currentJob->setParentJob(m_undoJob);
        m_undoJob->emitDeleting(file);

        QUrl url = file.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
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
        QList<QUrl>::ConstIterator it = m_dirsToUpdate.constBegin();
        for (; it != m_dirsToUpdate.constEnd(); ++it) {
            //qDebug() << "Notifying FilesAdded for " << *it;
            org::kde::KDirNotify::emitFilesAdded((*it));
        }
        emit q->undoJobFinished();
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
    m_commands.append(cmd);
    emit q->undoAvailable(true);
    emit q->undoTextChanged(q->undoText());
}

void FileUndoManagerPrivate::slotPop()
{
    m_commands.removeLast();
    emit q->undoAvailable(q->undoAvailable());
    emit q->undoTextChanged(q->undoText());
}

void FileUndoManagerPrivate::slotLock()
{
//  assert(!m_lock);
    m_lock = true;
    emit q->undoAvailable(q->undoAvailable());
}

void FileUndoManagerPrivate::slotUnlock()
{
//  assert(m_lock);
    m_lock = false;
    emit q->undoAvailable(q->undoAvailable());
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

void FileUndoManager::UiInterface::virtual_hook(int, void *)
{
}

#include "moc_fileundomanager_p.cpp"
#include "moc_fileundomanager.cpp"
#include "fileundomanager.moc"
