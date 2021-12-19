/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2006 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2021 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "copyjob.h"
#include "../pathhelpers_p.h"
#include "deletejob.h"
#include "filecopyjob.h"
#include "global.h"
#include "job.h" // buildErrorString
#include "kcoredirlister.h"
#include "kfileitem.h"
#include "kiocoredebug.h"
#include "kioglobal_p.h"
#include "listjob.h"
#include "mkdirjob.h"
#include "statjob.h"
#include <cerrno>

#include <KConfigGroup>
#include <KDesktopFile>
#include <KLocalizedString>

#include "kprotocolmanager.h"
#include "scheduler.h"
#include "slave.h"
#include <KDirWatch>

#include "askuseractioninterface.h"
#include <jobuidelegateextension.h>
#include <kio/jobuidelegatefactory.h>

#include <kdirnotify.h>

#ifdef Q_OS_UNIX
#include <utime.h>
#endif

#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QTemporaryFile>
#include <QTimer>

#include <sys/stat.h> // mode_t

#include "job_p.h"
#include <KFileSystemType>
#include <KFileUtils>
#include <KIO/FileSystemFreeSpaceJob>

#include <list>
#include <set>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(KIO_COPYJOB_DEBUG)
Q_LOGGING_CATEGORY(KIO_COPYJOB_DEBUG, "kf.kio.core.copyjob", QtWarningMsg)

using namespace KIO;

// this will update the report dialog with 5 Hz, I think this is fast enough, aleXXX
static constexpr int s_reportTimeout = 200;

#if !defined(NAME_MAX)
#if defined(_MAX_FNAME)
static constexpr int NAME_MAX = _MAX_FNAME; // For Windows
#else
static constexpr NAME_MAX = 0;
#endif
#endif

enum DestinationState {
    DEST_NOT_STATED,
    DEST_IS_DIR,
    DEST_IS_FILE,
    DEST_DOESNT_EXIST,
};

/**
 * States:
 *     STATE_INITIAL the constructor was called
 *     STATE_STATING for the dest
 *     statCurrentSrc then does, for each src url:
 *      STATE_RENAMING if direct rename looks possible
 *         (on already exists, and user chooses rename, TODO: go to STATE_RENAMING again)
 *      STATE_STATING
 *         and then, if dir -> STATE_LISTING (filling 'd->dirs' and 'd->files')
 *     STATE_CREATING_DIRS (createNextDir, iterating over 'd->dirs')
 *          if conflict: STATE_CONFLICT_CREATING_DIRS
 *     STATE_COPYING_FILES (copyNextFile, iterating over 'd->files')
 *          if conflict: STATE_CONFLICT_COPYING_FILES
 *     STATE_DELETING_DIRS (deleteNextDir) (if moving)
 *     STATE_SETTING_DIR_ATTRIBUTES (setNextDirAttribute, iterating over d->m_directoriesCopied)
 *     done.
 */
enum CopyJobState {
    STATE_INITIAL,
    STATE_STATING,
    STATE_RENAMING,
    STATE_LISTING,
    STATE_CREATING_DIRS,
    STATE_CONFLICT_CREATING_DIRS,
    STATE_COPYING_FILES,
    STATE_CONFLICT_COPYING_FILES,
    STATE_DELETING_DIRS,
    STATE_SETTING_DIR_ATTRIBUTES,
};

static QUrl addPathToUrl(const QUrl &url, const QString &relPath)
{
    QUrl u(url);
    u.setPath(concatPaths(url.path(), relPath));
    return u;
}

static bool compareUrls(const QUrl &srcUrl, const QUrl &destUrl)
{
    /* clang-format off */
    return srcUrl.scheme() == destUrl.scheme()
        && srcUrl.host() == destUrl.host()
        && srcUrl.port() == destUrl.port()
        && srcUrl.userName() == destUrl.userName()
        && srcUrl.password() == destUrl.password();
    /* clang-format on */
}

// https://docs.microsoft.com/en-us/windows/win32/fileio/naming-a-file#naming-conventions
static const char s_msdosInvalidChars[] = R"(<>:"/\|?*)";

static bool hasInvalidChars(const QString &dest)
{
    return std::any_of(std::begin(s_msdosInvalidChars), std::end(s_msdosInvalidChars), [=](const char c) {
        return dest.contains(QLatin1Char(c));
    });
}

static void cleanMsdosDestName(QString &name)
{
    for (const char c : s_msdosInvalidChars) {
        name.replace(QLatin1Char(c), QLatin1String("_"));
    }
}

static bool isFatFs(KFileSystemType::Type fsType)
{
    return fsType == KFileSystemType::Fat || fsType == KFileSystemType::Exfat;
}

static bool isFatOrNtfs(KFileSystemType::Type fsType)
{
    return fsType == KFileSystemType::Ntfs || isFatFs(fsType);
}

static QString symlinkSupportMsg(const QString &path, const QString &fsName)
{
    const QString msg = i18nc(
        "The first arg is the path to the symlink that couldn't be created, the second"
        "arg is the filesystem type (e.g. vfat, exfat)",
        "Could not create symlink \"%1\".\n"
        "The destination filesystem (%2) doesn't support symlinks.",
        path,
        fsName);
    return msg;
}

static QString invalidCharsSupportMsg(const QString &path, const QString &fsName, bool isDir = false)
{
    QString msg;
    if (isDir) {
        msg = i18n(
            "Could not create \"%1\".\n"
            "The destination filesystem (%2) disallows the following characters in folder names: %3\n"
            "Selecting Replace will replace any invalid characters (in the destination folder name) with an underscore \"_\".",
            path,
            fsName,
            QLatin1String(s_msdosInvalidChars));
    } else {
        msg = i18n(
            "Could not create \"%1\".\n"
            "The destination filesystem (%2) disallows the following characters in file names: %3\n"
            "Selecting Replace will replace any invalid characters (in the destination file name) with an underscore \"_\".",
            path,
            fsName,
            QLatin1String(s_msdosInvalidChars));
    }

    return msg;
}

/** @internal */
class KIO::CopyJobPrivate : public KIO::JobPrivate
{
public:
    CopyJobPrivate(const QList<QUrl> &src, const QUrl &dest, CopyJob::CopyMode mode, bool asMethod)
        : m_globalDest(dest)
        , m_globalDestinationState(DEST_NOT_STATED)
        , m_defaultPermissions(false)
        , m_bURLDirty(false)
        , m_mode(mode)
        , m_asMethod(asMethod)
        , destinationState(DEST_NOT_STATED)
        , state(STATE_INITIAL)
        , m_freeSpace(-1)
        , m_totalSize(0)
        , m_processedSize(0)
        , m_fileProcessedSize(0)
        , m_filesHandledByDirectRename(0)
        , m_processedFiles(0)
        , m_processedDirs(0)
        , m_srcList(src)
        , m_currentStatSrc(m_srcList.constBegin())
        , m_bCurrentOperationIsLink(false)
        , m_bSingleFileCopy(false)
        , m_bOnlyRenames(mode == CopyJob::Move)
        , m_dest(dest)
        , m_bAutoRenameFiles(false)
        , m_bAutoRenameDirs(false)
        , m_bAutoSkipFiles(false)
        , m_bAutoSkipDirs(false)
        , m_bOverwriteAllFiles(false)
        , m_bOverwriteAllDirs(false)
        , m_bOverwriteWhenOlder(false)
        , m_conflictError(0)
        , m_reportTimer(nullptr)
    {
    }

    // This is the dest URL that was initially given to CopyJob
    // It is copied into m_dest, which can be changed for a given src URL
    // (when using the RENAME dialog in slotResult),
    // and which will be reset for the next src URL.
    QUrl m_globalDest;
    // The state info about that global dest
    DestinationState m_globalDestinationState;
    // See setDefaultPermissions
    bool m_defaultPermissions;
    // Whether URLs changed (and need to be emitted by the next slotReport call)
    bool m_bURLDirty;
    // Used after copying all the files into the dirs, to set mtime (TODO: and permissions?)
    // after the copy is done
    std::list<CopyInfo> m_directoriesCopied;
    std::list<CopyInfo>::const_iterator m_directoriesCopiedIterator;

    CopyJob::CopyMode m_mode;
    bool m_asMethod; // See copyAs() method
    DestinationState destinationState;
    CopyJobState state;

    KIO::filesize_t m_freeSpace;

    KIO::filesize_t m_totalSize;
    KIO::filesize_t m_processedSize;
    KIO::filesize_t m_fileProcessedSize;
    int m_filesHandledByDirectRename;
    int m_processedFiles;
    int m_processedDirs;
    QList<CopyInfo> files;
    QList<CopyInfo> dirs;
    // List of dirs that will be copied then deleted when CopyMode is Move
    QList<QUrl> dirsToRemove;
    QList<QUrl> m_srcList;
    QList<QUrl> m_successSrcList; // Entries in m_srcList that have successfully been moved
    QList<QUrl>::const_iterator m_currentStatSrc;
    bool m_bCurrentSrcIsDir;
    bool m_bCurrentOperationIsLink;
    bool m_bSingleFileCopy;
    bool m_bOnlyRenames;
    QUrl m_dest;
    QUrl m_currentDest; // set during listing, used by slotEntries
    //
    QStringList m_skipList;
    QSet<QString> m_overwriteList;
    bool m_bAutoRenameFiles;
    bool m_bAutoRenameDirs;
    bool m_bAutoSkipFiles;
    bool m_bAutoSkipDirs;
    bool m_bOverwriteAllFiles;
    bool m_bOverwriteAllDirs;
    bool m_bOverwriteWhenOlder;

    bool m_autoSkipDirsWithInvalidChars = false;
    bool m_autoSkipFilesWithInvalidChars = false;
    bool m_autoReplaceInvalidChars = false;

    bool m_autoSkipFatSymlinks = false;

    enum SkipType {
        // No skip dialog is involved
        NoSkipType = 0,
        // SkipDialog is asking about invalid chars in destination file/dir names
        SkipInvalidChars,
        // SkipDialog is asking about how to handle symlinks why copying to a
        // filesystem that doesn't support symlinks
        SkipFatSymlinks,
    };

    int m_conflictError;

    QTimer *m_reportTimer;

    // The current src url being stat'ed or copied
    // During the stat phase, this is initially equal to *m_currentStatSrc but it can be resolved to a local file equivalent (#188903).
    QUrl m_currentSrcURL;
    QUrl m_currentDestURL;

    std::set<QString> m_parentDirs;

    void statCurrentSrc();
    void statNextSrc();

    // Those aren't slots but submethods for slotResult.
    void slotResultStating(KJob *job);
    void startListing(const QUrl &src);

    void slotResultCreatingDirs(KJob *job);
    void slotResultConflictCreatingDirs(KJob *job);
    void createNextDir();
    void processCreateNextDir(const QList<CopyInfo>::Iterator &it, int result);

    void slotResultCopyingFiles(KJob *job);
    void slotResultErrorCopyingFiles(KJob *job);
    void processFileRenameDialogResult(const QList<CopyInfo>::Iterator &it, RenameDialog_Result result, const QUrl &newUrl, const QDateTime &destmtime);

    //     KIO::Job* linkNextFile( const QUrl& uSource, const QUrl& uDest, bool overwrite );
    KIO::Job *linkNextFile(const QUrl &uSource, const QUrl &uDest, JobFlags flags);
    // MsDos filesystems don't allow certain characters in filenames, and VFAT and ExFAT
    // don't support symlinks, this method detects those conditions and tries to handle it
    bool handleMsdosFsQuirks(QList<CopyInfo>::Iterator it, KFileSystemType::Type fsType);
    void copyNextFile();
    void processCopyNextFile(const QList<CopyInfo>::Iterator &it, int result, SkipType skipType);

    void slotResultDeletingDirs(KJob *job);
    void deleteNextDir();
    void sourceStated(const UDSEntry &entry, const QUrl &sourceUrl);
    // Removes a dir from the "dirsToRemove" list
    void skip(const QUrl &sourceURL, bool isDir);

    void slotResultRenaming(KJob *job);
    void directRenamingFailed(const QUrl &dest);
    void processDirectRenamingConflictResult(RenameDialog_Result result,
                                             bool srcIsDir,
                                             bool destIsDir,
                                             const QDateTime &mtimeSrc,
                                             const QDateTime &mtimeDest,
                                             const QUrl &dest,
                                             const QUrl &newUrl);

    void slotResultSettingDirAttributes(KJob *job);
    void setNextDirAttribute();

    void startRenameJob(const QUrl &slave_url);
    bool shouldOverwriteDir(const QString &path) const;
    bool shouldOverwriteFile(const QString &path) const;
    bool shouldSkip(const QString &path) const;
    void skipSrc(bool isDir);
    void renameDirectory(const QList<CopyInfo>::iterator &it, const QUrl &newUrl);
    QUrl finalDestUrl(const QUrl &src, const QUrl &dest) const;

    void slotStart();
    void slotEntries(KIO::Job *, const KIO::UDSEntryList &list);
    void slotSubError(KIO::ListJob *job, KIO::ListJob *subJob);
    void addCopyInfoFromUDSEntry(const UDSEntry &entry, const QUrl &srcUrl, bool srcIsDir, const QUrl &currentDest);
    /**
     * Forward signal from subjob
     */
    void slotProcessedSize(KJob *, qulonglong data_size);
    /**
     * Forward signal from subjob
     * @param size the total size
     */
    void slotTotalSize(KJob *, qulonglong size);

    void slotReport();

    Q_DECLARE_PUBLIC(CopyJob)

    static inline CopyJob *newJob(const QList<QUrl> &src, const QUrl &dest, CopyJob::CopyMode mode, bool asMethod, JobFlags flags)
    {
        CopyJob *job = new CopyJob(*new CopyJobPrivate(src, dest, mode, asMethod));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        if (flags & KIO::Overwrite) {
            job->d_func()->m_bOverwriteAllDirs = true;
            job->d_func()->m_bOverwriteAllFiles = true;
        }
        if (!(flags & KIO::NoPrivilegeExecution)) {
            job->d_func()->m_privilegeExecutionEnabled = true;
            FileOperationType copyType;
            switch (mode) {
            case CopyJob::Copy:
                copyType = Copy;
                break;
            case CopyJob::Move:
                copyType = Move;
                break;
            case CopyJob::Link:
                copyType = Symlink;
                break;
            default:
                Q_UNREACHABLE();
            }
            job->d_func()->m_operationType = copyType;
        }
        return job;
    }
};

CopyJob::CopyJob(CopyJobPrivate &dd)
    : Job(dd)
{
    Q_D(CopyJob);
    setProperty("destUrl", d_func()->m_dest.toString());
    QTimer::singleShot(0, this, [d]() {
        d->slotStart();
    });
    qRegisterMetaType<KIO::UDSEntry>();
}

CopyJob::~CopyJob()
{
}

QList<QUrl> CopyJob::srcUrls() const
{
    return d_func()->m_srcList;
}

QUrl CopyJob::destUrl() const
{
    return d_func()->m_dest;
}

void CopyJobPrivate::slotStart()
{
    Q_Q(CopyJob);
    if (q->isSuspended()) {
        return;
    }

    if (m_mode == CopyJob::CopyMode::Move) {
        for (const QUrl &url : std::as_const(m_srcList)) {
            if (m_dest.scheme() == url.scheme() && m_dest.host() == url.host()) {
                QString srcPath = url.path();
                if (!srcPath.endsWith(QLatin1Char('/'))) {
                    srcPath += QLatin1Char('/');
                }
                if (m_dest.path().startsWith(srcPath)) {
                    q->setError(KIO::ERR_CANNOT_MOVE_INTO_ITSELF);
                    q->emitResult();
                    return;
                }
            }
        }
    }

    if (m_mode == CopyJob::CopyMode::Link && m_globalDest.isLocalFile()) {
        const QString destPath = m_globalDest.toLocalFile();
        const auto destFs = KFileSystemType::fileSystemType(destPath);
        if (isFatFs(destFs)) {
            q->setError(ERR_SYMLINKS_NOT_SUPPORTED);
            const QString errText = destPath + QLatin1String(" [") + KFileSystemType::fileSystemName(destFs) + QLatin1Char(']');
            q->setErrorText(errText);
            q->emitResult();
            return;
        }
    }

    /**
       We call the functions directly instead of using signals.
       Calling a function via a signal takes approx. 65 times the time
       compared to calling it directly (at least on my machine). aleXXX
    */
    m_reportTimer = new QTimer(q);

    q->connect(m_reportTimer, &QTimer::timeout, q, [this]() {
        slotReport();
    });
    m_reportTimer->start(s_reportTimeout);

    // Stat the dest
    state = STATE_STATING;
    const QUrl dest = m_asMethod ? m_dest.adjusted(QUrl::RemoveFilename) : m_dest;
    // We need isDir() and UDS_LOCAL_PATH (for slaves who set it). Let's assume the latter is part of StatBasic too.
    KIO::Job *job = KIO::statDetails(dest, StatJob::DestinationSide, KIO::StatBasic | KIO::StatResolveSymlink, KIO::HideProgressInfo);
    qCDebug(KIO_COPYJOB_DEBUG) << "CopyJob: stating the dest" << dest;
    q->addSubjob(job);
}

// For unit test purposes
KIOCORE_EXPORT bool kio_resolve_local_urls = true;

void CopyJobPrivate::slotResultStating(KJob *job)
{
    Q_Q(CopyJob);
    qCDebug(KIO_COPYJOB_DEBUG);
    // Was there an error while stating the src ?
    if (job->error() && destinationState != DEST_NOT_STATED) {
        const QUrl srcurl = static_cast<SimpleJob *>(job)->url();
        if (!srcurl.isLocalFile()) {
            // Probably : src doesn't exist. Well, over some protocols (e.g. FTP)
            // this info isn't really reliable (thanks to MS FTP servers).
            // We'll assume a file, and try to download anyway.
            qCDebug(KIO_COPYJOB_DEBUG) << "Error while stating source. Activating hack";
            q->removeSubjob(job);
            Q_ASSERT(!q->hasSubjobs()); // We should have only one job at a time ...
            struct CopyInfo info;
            info.permissions = (mode_t)-1;
            info.size = KIO::invalidFilesize;
            info.uSource = srcurl;
            info.uDest = m_dest;
            // Append filename or dirname to destination URL, if allowed
            if (destinationState == DEST_IS_DIR && !m_asMethod) {
                const QString fileName = srcurl.scheme() == QLatin1String("data") ? QStringLiteral("data") : srcurl.fileName(); // #379093
                info.uDest = addPathToUrl(info.uDest, fileName);
            }

            files.append(info);
            statNextSrc();
            return;
        }
        // Local file. If stat fails, the file definitely doesn't exist.
        // yes, q->Job::, because we don't want to call our override
        q->Job::slotResult(job); // will set the error and emit result(this)
        return;
    }

    // Keep copy of the stat result
    const UDSEntry entry = static_cast<StatJob *>(job)->statResult();

    if (destinationState == DEST_NOT_STATED) {
        const bool isGlobalDest = m_dest == m_globalDest;

        // we were stating the dest
        if (job->error()) {
            destinationState = DEST_DOESNT_EXIST;
            qCDebug(KIO_COPYJOB_DEBUG) << "dest does not exist";
        } else {
            const bool isDir = entry.isDir();

            // Check for writability, before spending time stat'ing everything (#141564).
            // This assumes all kioslaves set permissions correctly...
            const int permissions = entry.numberValue(KIO::UDSEntry::UDS_ACCESS, -1);
            const bool isWritable = (permissions != -1) && (permissions & S_IWUSR);
            if (!m_privilegeExecutionEnabled && !isWritable) {
                const QUrl dest = m_asMethod ? m_dest.adjusted(QUrl::RemoveFilename) : m_dest;
                q->setError(ERR_WRITE_ACCESS_DENIED);
                q->setErrorText(dest.toDisplayString(QUrl::PreferLocalFile));
                q->emitResult();
                return;
            }

            // Treat symlinks to dirs as dirs here, so no test on isLink
            destinationState = isDir ? DEST_IS_DIR : DEST_IS_FILE;
            qCDebug(KIO_COPYJOB_DEBUG) << "dest is dir:" << isDir;

            if (isGlobalDest) {
                m_globalDestinationState = destinationState;
            }

            const QString sLocalPath = entry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH);
            if (!sLocalPath.isEmpty() && kio_resolve_local_urls) {
                const QString fileName = m_dest.fileName();
                m_dest = QUrl::fromLocalFile(sLocalPath);
                if (m_asMethod) {
                    m_dest = addPathToUrl(m_dest, fileName);
                }
                qCDebug(KIO_COPYJOB_DEBUG) << "Setting m_dest to the local path:" << sLocalPath;
                if (isGlobalDest) {
                    m_globalDest = m_dest;
                }
            }
        }

        q->removeSubjob(job);
        Q_ASSERT(!q->hasSubjobs());

        // In copy-as mode, we want to check the directory to which we're
        // copying. The target file or directory does not exist yet, which
        // might confuse FileSystemFreeSpaceJob.
        const QUrl existingDest = m_asMethod ? m_dest.adjusted(QUrl::RemoveFilename) : m_dest;
        KIO::FileSystemFreeSpaceJob *spaceJob = KIO::fileSystemFreeSpace(existingDest);
        q->connect(spaceJob,
                   &KIO::FileSystemFreeSpaceJob::result,
                   q,
                   [this, existingDest](KIO::Job *spaceJob, KIO::filesize_t /*size*/, KIO::filesize_t available) {
                       if (!spaceJob->error()) {
                           m_freeSpace = available;
                       } else {
                           qCDebug(KIO_COPYJOB_DEBUG) << "Couldn't determine free space information for" << existingDest;
                       }
                       // After knowing what the dest is, we can start stat'ing the first src.
                       statCurrentSrc();
                   });
        return;
    } else {
        sourceStated(entry, static_cast<SimpleJob *>(job)->url());
        q->removeSubjob(job);
    }
}

void CopyJobPrivate::sourceStated(const UDSEntry &entry, const QUrl &sourceUrl)
{
    const QString sLocalPath = entry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH);
    const bool isDir = entry.isDir();

    // We were stating the current source URL
    // Is it a file or a dir ?

    // There 6 cases, and all end up calling addCopyInfoFromUDSEntry first :
    // 1 - src is a dir, destination is a directory,
    // slotEntries will append the source-dir-name to the destination
    // 2 - src is a dir, destination is a file -- will offer to overwrite, later on.
    // 3 - src is a dir, destination doesn't exist, then it's the destination dirname,
    // so slotEntries will use it as destination.

    // 4 - src is a file, destination is a directory,
    // slotEntries will append the filename to the destination.
    // 5 - src is a file, destination is a file, m_dest is the exact destination name
    // 6 - src is a file, destination doesn't exist, m_dest is the exact destination name

    QUrl srcurl;
    if (!sLocalPath.isEmpty() && destinationState != DEST_DOESNT_EXIST) {
        qCDebug(KIO_COPYJOB_DEBUG) << "Using sLocalPath. destinationState=" << destinationState;
        // Prefer the local path -- but only if we were able to stat() the dest.
        // Otherwise, renaming a desktop:/ url would copy from src=file to dest=desktop (#218719)
        srcurl = QUrl::fromLocalFile(sLocalPath);
    } else {
        srcurl = sourceUrl;
    }
    addCopyInfoFromUDSEntry(entry, srcurl, false, m_dest);

    m_currentDest = m_dest;
    m_bCurrentSrcIsDir = false;

    if (isDir //
        && !entry.isLink() // treat symlinks as files (no recursion)
        && m_mode != CopyJob::Link) { // No recursion in Link mode either.
        qCDebug(KIO_COPYJOB_DEBUG) << "Source is a directory";

        if (srcurl.isLocalFile()) {
            const QString parentDir = srcurl.adjusted(QUrl::StripTrailingSlash).toLocalFile();
            m_parentDirs.insert(parentDir);
        }

        m_bCurrentSrcIsDir = true; // used by slotEntries
        if (destinationState == DEST_IS_DIR) { // (case 1)
            if (!m_asMethod) {
                // Use <desturl>/<directory_copied> as destination, from now on
                QString directory = srcurl.fileName();
                const QString sName = entry.stringValue(KIO::UDSEntry::UDS_NAME);
                KProtocolInfo::FileNameUsedForCopying fnu = KProtocolManager::fileNameUsedForCopying(srcurl);
                if (fnu == KProtocolInfo::Name) {
                    if (!sName.isEmpty()) {
                        directory = sName;
                    }
                } else if (fnu == KProtocolInfo::DisplayName) {
                    const QString dispName = entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
                    if (!dispName.isEmpty()) {
                        directory = dispName;
                    } else if (!sName.isEmpty()) {
                        directory = sName;
                    }
                }
                m_currentDest = addPathToUrl(m_currentDest, directory);
            }
        } else { // (case 3)
            // otherwise dest is new name for toplevel dir
            // so the destination exists, in fact, from now on.
            // (This even works with other src urls in the list, since the
            //  dir has effectively been created)
            destinationState = DEST_IS_DIR;
            if (m_dest == m_globalDest) {
                m_globalDestinationState = destinationState;
            }
        }

        startListing(srcurl);
    } else {
        qCDebug(KIO_COPYJOB_DEBUG) << "Source is a file (or a symlink), or we are linking -> no recursive listing";

        if (srcurl.isLocalFile()) {
            const QString parentDir = srcurl.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).path();
            m_parentDirs.insert(parentDir);
        }

        statNextSrc();
    }
}

bool CopyJob::doSuspend()
{
    Q_D(CopyJob);
    d->slotReport();
    return Job::doSuspend();
}

bool CopyJob::doResume()
{
    Q_D(CopyJob);
    switch (d->state) {
    case STATE_INITIAL:
        QTimer::singleShot(0, this, [d]() {
            d->slotStart();
        });
        break;
    default:
        // not implemented
        break;
    }
    return Job::doResume();
}

void CopyJobPrivate::slotReport()
{
    Q_Q(CopyJob);
    if (q->isSuspended()) {
        return;
    }

    // If showProgressInfo was set, progressId() is > 0.
    switch (state) {
    case STATE_RENAMING:
        if (m_bURLDirty) {
            m_bURLDirty = false;
            Q_ASSERT(m_mode == CopyJob::Move);
            emitMoving(q, m_currentSrcURL, m_currentDestURL);
            Q_EMIT q->moving(q, m_currentSrcURL, m_currentDestURL);
        }
        // "N" files renamed shouldn't include skipped files
        q->setProcessedAmount(KJob::Files, m_processedFiles);
        // % value should include skipped files
        q->emitPercent(m_filesHandledByDirectRename, q->totalAmount(KJob::Files));
        break;

    case STATE_COPYING_FILES:
        q->setProcessedAmount(KJob::Files, m_processedFiles);
        q->setProcessedAmount(KJob::Bytes, m_processedSize + m_fileProcessedSize);
        if (m_bURLDirty) {
            // Only emit urls when they changed. This saves time, and fixes #66281
            m_bURLDirty = false;
            if (m_mode == CopyJob::Move) {
                emitMoving(q, m_currentSrcURL, m_currentDestURL);
                Q_EMIT q->moving(q, m_currentSrcURL, m_currentDestURL);
            } else if (m_mode == CopyJob::Link) {
                emitCopying(q, m_currentSrcURL, m_currentDestURL); // we don't have a delegate->linking
                Q_EMIT q->linking(q, m_currentSrcURL.path(), m_currentDestURL);
            } else {
                emitCopying(q, m_currentSrcURL, m_currentDestURL);
                Q_EMIT q->copying(q, m_currentSrcURL, m_currentDestURL);
            }
        }
        break;

    case STATE_CREATING_DIRS:
        q->setProcessedAmount(KJob::Directories, m_processedDirs);
        if (m_bURLDirty) {
            m_bURLDirty = false;
            Q_EMIT q->creatingDir(q, m_currentDestURL);
            emitCreatingDir(q, m_currentDestURL);
        }
        break;

    case STATE_STATING:
    case STATE_LISTING:
        if (m_bURLDirty) {
            m_bURLDirty = false;
            if (m_mode == CopyJob::Move) {
                emitMoving(q, m_currentSrcURL, m_currentDestURL);
            } else {
                emitCopying(q, m_currentSrcURL, m_currentDestURL);
            }
        }
        q->setProgressUnit(KJob::Bytes);
        q->setTotalAmount(KJob::Bytes, m_totalSize);
        q->setTotalAmount(KJob::Files, files.count() + m_filesHandledByDirectRename);
        q->setTotalAmount(KJob::Directories, dirs.count());
        break;

    default:
        break;
    }
}

void CopyJobPrivate::slotEntries(KIO::Job *job, const UDSEntryList &list)
{
    // Q_Q(CopyJob);
    UDSEntryList::ConstIterator it = list.constBegin();
    UDSEntryList::ConstIterator end = list.constEnd();
    for (; it != end; ++it) {
        const UDSEntry &entry = *it;
        addCopyInfoFromUDSEntry(entry, static_cast<SimpleJob *>(job)->url(), m_bCurrentSrcIsDir, m_currentDest);
    }
}

void CopyJobPrivate::slotSubError(ListJob *job, ListJob *subJob)
{
    const QUrl &url = subJob->url();
    qCWarning(KIO_CORE) << url << subJob->errorString();

    Q_Q(CopyJob);

    Q_EMIT q->warning(job, subJob->errorString(), QString());
    skip(url, true);
}

void CopyJobPrivate::addCopyInfoFromUDSEntry(const UDSEntry &entry, const QUrl &srcUrl, bool srcIsDir, const QUrl &currentDest)
{
    struct CopyInfo info;
    info.permissions = entry.numberValue(KIO::UDSEntry::UDS_ACCESS, -1);
    const auto timeVal = entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, -1);
    if (timeVal != -1) {
        info.mtime = QDateTime::fromMSecsSinceEpoch(1000 * timeVal, Qt::UTC);
    }
    info.ctime = QDateTime::fromMSecsSinceEpoch(1000 * entry.numberValue(KIO::UDSEntry::UDS_CREATION_TIME, -1), Qt::UTC);
    info.size = static_cast<KIO::filesize_t>(entry.numberValue(KIO::UDSEntry::UDS_SIZE, -1));
    const bool isDir = entry.isDir();

    if (!isDir && info.size != KIO::invalidFilesize) {
        m_totalSize += info.size;
    }

    // recursive listing, displayName can be a/b/c/d
    const QString fileName = entry.stringValue(KIO::UDSEntry::UDS_NAME);
    const QString urlStr = entry.stringValue(KIO::UDSEntry::UDS_URL);
    QUrl url;
    if (!urlStr.isEmpty()) {
        url = QUrl(urlStr);
    }
    QString localPath = entry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH);
    info.linkDest = entry.stringValue(KIO::UDSEntry::UDS_LINK_DEST);

    if (fileName != QLatin1String("..") && fileName != QLatin1String(".")) {
        const bool hasCustomURL = !url.isEmpty() || !localPath.isEmpty();
        if (!hasCustomURL) {
            // Make URL from displayName
            url = srcUrl;
            if (srcIsDir) { // Only if src is a directory. Otherwise uSource is fine as is
                qCDebug(KIO_COPYJOB_DEBUG) << "adding path" << fileName;
                url = addPathToUrl(url, fileName);
            }
        }
        qCDebug(KIO_COPYJOB_DEBUG) << "fileName=" << fileName << "url=" << url;
        if (!localPath.isEmpty() && kio_resolve_local_urls && destinationState != DEST_DOESNT_EXIST) {
            url = QUrl::fromLocalFile(localPath);
        }

        info.uSource = url;
        info.uDest = currentDest;
        qCDebug(KIO_COPYJOB_DEBUG) << "uSource=" << info.uSource << "uDest(1)=" << info.uDest;
        // Append filename or dirname to destination URL, if allowed
        if (destinationState == DEST_IS_DIR &&
            // "copy/move as <foo>" means 'foo' is the dest for the base srcurl
            // (passed here during stating) but not its children (during listing)
            (!(m_asMethod && state == STATE_STATING))) {
            QString destFileName;
            KProtocolInfo::FileNameUsedForCopying fnu = KProtocolManager::fileNameUsedForCopying(url);
            if (hasCustomURL && fnu == KProtocolInfo::FromUrl) {
                // destFileName = url.fileName(); // Doesn't work for recursive listing
                // Count the number of prefixes used by the recursive listjob
                int numberOfSlashes = fileName.count(QLatin1Char('/')); // don't make this a find()!
                QString path = url.path();
                int pos = 0;
                for (int n = 0; n < numberOfSlashes + 1; ++n) {
                    pos = path.lastIndexOf(QLatin1Char('/'), pos - 1);
                    if (pos == -1) { // error
                        qCWarning(KIO_CORE) << "kioslave bug: not enough slashes in UDS_URL" << path << "- looking for" << numberOfSlashes << "slashes";
                        break;
                    }
                }
                if (pos >= 0) {
                    destFileName = path.mid(pos + 1);
                }

            } else if (fnu == KProtocolInfo::Name) { // destination filename taken from UDS_NAME
                destFileName = fileName;
            } else { // from display name (with fallback to name)
                const QString displayName = entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
                destFileName = displayName.isEmpty() ? fileName : displayName;
            }

            // Here we _really_ have to add some filename to the dest.
            // Otherwise, we end up with e.g. dest=..../Desktop/ itself.
            // (This can happen when dropping a link to a webpage with no path)
            if (destFileName.isEmpty()) {
                destFileName = KIO::encodeFileName(info.uSource.toDisplayString());
            }

            qCDebug(KIO_COPYJOB_DEBUG) << " adding destFileName=" << destFileName;
            info.uDest = addPathToUrl(info.uDest, destFileName);
        }
        qCDebug(KIO_COPYJOB_DEBUG) << " uDest(2)=" << info.uDest;
        qCDebug(KIO_COPYJOB_DEBUG) << " " << info.uSource << "->" << info.uDest;
        if (info.linkDest.isEmpty() && isDir && m_mode != CopyJob::Link) { // Dir
            dirs.append(info); // Directories
            if (m_mode == CopyJob::Move) {
                dirsToRemove.append(info.uSource);
            }
        } else {
            files.append(info); // Files and any symlinks
        }
    }
}

// Adjust for kio_trash choosing its own dest url...
QUrl CopyJobPrivate::finalDestUrl(const QUrl &src, const QUrl &dest) const
{
    Q_Q(const CopyJob);
    if (dest.scheme() == QLatin1String("trash")) {
        const QMap<QString, QString> &metaData = q->metaData();
        QMap<QString, QString>::ConstIterator it = metaData.find(QLatin1String("trashURL-") + src.path());
        if (it != metaData.constEnd()) {
            qCDebug(KIO_COPYJOB_DEBUG) << "finalDestUrl=" << it.value();
            return QUrl(it.value());
        }
    }
    return dest;
}

void CopyJobPrivate::skipSrc(bool isDir)
{
    m_dest = m_globalDest;
    destinationState = m_globalDestinationState;
    skip(*m_currentStatSrc, isDir);
    ++m_currentStatSrc;
    statCurrentSrc();
}

void CopyJobPrivate::statNextSrc()
{
    /* Revert to the global destination, the one that applies to all source urls.
     * Imagine you copy the items a b and c into /d, but /d/b exists so the user uses "Rename" to put it in /foo/b instead.
     * d->m_dest is /foo/b for b, but we have to revert to /d for item c and following.
     */
    m_dest = m_globalDest;
    qCDebug(KIO_COPYJOB_DEBUG) << "Setting m_dest to" << m_dest;
    destinationState = m_globalDestinationState;
    ++m_currentStatSrc;
    statCurrentSrc();
}

void CopyJobPrivate::statCurrentSrc()
{
    Q_Q(CopyJob);
    if (m_currentStatSrc != m_srcList.constEnd()) {
        m_currentSrcURL = (*m_currentStatSrc);
        m_bURLDirty = true;
        if (m_mode == CopyJob::Link) {
            // Skip the "stating the source" stage, we don't need it for linking
            m_currentDest = m_dest;
            struct CopyInfo info;
            info.permissions = -1;
            info.size = KIO::invalidFilesize;
            info.uSource = m_currentSrcURL;
            info.uDest = m_currentDest;
            // Append filename or dirname to destination URL, if allowed
            if (destinationState == DEST_IS_DIR && !m_asMethod) {
                if (compareUrls(m_currentSrcURL, info.uDest)) {
                    // This is the case of creating a real symlink
                    info.uDest = addPathToUrl(info.uDest, m_currentSrcURL.fileName());
                } else {
                    // Different protocols, we'll create a .desktop file
                    // We have to change the extension anyway, so while we're at it,
                    // name the file like the URL
                    QByteArray encodedFilename = QFile::encodeName(m_currentSrcURL.toDisplayString());
                    const int truncatePos = NAME_MAX - (info.uDest.toDisplayString().length() + 8); // length(.desktop) = 8
                    if (truncatePos > 0) {
                        encodedFilename.truncate(truncatePos);
                    }
                    const QString decodedFilename = QFile::decodeName(encodedFilename);
                    info.uDest = addPathToUrl(info.uDest, KIO::encodeFileName(decodedFilename) + QLatin1String(".desktop"));
                }
            }
            files.append(info); // Files and any symlinks
            statNextSrc(); // we could use a loop instead of a recursive call :)
            return;
        }

        // Let's see if we can skip stat'ing, for the case where a directory view has the info already
        KIO::UDSEntry entry;
        const KFileItem cachedItem = KCoreDirLister::cachedItemForUrl(m_currentSrcURL);
        if (!cachedItem.isNull()) {
            entry = cachedItem.entry();
            if (destinationState != DEST_DOESNT_EXIST) { // only resolve src if we could resolve dest (#218719)
                bool dummyIsLocal;
                m_currentSrcURL = cachedItem.mostLocalUrl(&dummyIsLocal); // #183585
            }
        }

        // Don't go renaming right away if we need a stat() to find out the destination filename
        const bool needStat =
            KProtocolManager::fileNameUsedForCopying(m_currentSrcURL) == KProtocolInfo::FromUrl || destinationState != DEST_IS_DIR || m_asMethod;
        if (m_mode == CopyJob::Move && needStat) {
            // If moving, before going for the full stat+[list+]copy+del thing, try to rename
            // The logic is pretty similar to FileCopyJobPrivate::slotStart()
            if (compareUrls(m_currentSrcURL, m_dest)) {
                startRenameJob(m_currentSrcURL);
                return;
            } else if (m_currentSrcURL.isLocalFile() && KProtocolManager::canRenameFromFile(m_dest)) {
                startRenameJob(m_dest);
                return;
            } else if (m_dest.isLocalFile() && KProtocolManager::canRenameToFile(m_currentSrcURL)) {
                startRenameJob(m_currentSrcURL);
                return;
            }
        }

        // if the source file system doesn't support deleting, we do not even stat
        if (m_mode == CopyJob::Move && !KProtocolManager::supportsDeleting(m_currentSrcURL)) {
            QPointer<CopyJob> that = q;
            Q_EMIT q->warning(q, buildErrorString(ERR_CANNOT_DELETE, m_currentSrcURL.toDisplayString()));
            if (that) {
                statNextSrc(); // we could use a loop instead of a recursive call :)
            }
            return;
        }

        m_bOnlyRenames = false;

        // Testing for entry.count()>0 here is not good enough; KFileItem inserts
        // entries for UDS_USER and UDS_GROUP even on initially empty UDSEntries (#192185)
        if (entry.contains(KIO::UDSEntry::UDS_NAME)) {
            qCDebug(KIO_COPYJOB_DEBUG) << "fast path! found info about" << m_currentSrcURL << "in KCoreDirLister";
            // sourceStated(entry, m_currentSrcURL); // don't recurse, see #319747, use queued invokeMethod instead
            QMetaObject::invokeMethod(q, "sourceStated", Qt::QueuedConnection, Q_ARG(KIO::UDSEntry, entry), Q_ARG(QUrl, m_currentSrcURL));
            return;
        }

        // Stat the next src url
        Job *job = KIO::statDetails(m_currentSrcURL, StatJob::SourceSide, KIO::StatDefaultDetails, KIO::HideProgressInfo);
        qCDebug(KIO_COPYJOB_DEBUG) << "KIO::stat on" << m_currentSrcURL;
        state = STATE_STATING;
        q->addSubjob(job);
        m_currentDestURL = m_dest;
        m_bURLDirty = true;
    } else {
        // Finished the stat'ing phase
        // First make sure that the totals were correctly emitted
        m_bURLDirty = true;
        slotReport();

        qCDebug(KIO_COPYJOB_DEBUG) << "Stating finished. To copy:" << m_totalSize << ", available:" << m_freeSpace;

        if (m_totalSize > m_freeSpace && m_freeSpace != static_cast<KIO::filesize_t>(-1)) {
            q->setError(ERR_DISK_FULL);
            q->setErrorText(m_currentSrcURL.toDisplayString());
            q->emitResult();
            return;
        }

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 2)
        if (!dirs.isEmpty()) {
            Q_EMIT q->aboutToCreate(q, dirs);
        }
        if (!files.isEmpty()) {
            Q_EMIT q->aboutToCreate(q, files);
        }
#endif
        // Check if we are copying a single file
        m_bSingleFileCopy = (files.count() == 1 && dirs.isEmpty());
        // Then start copying things
        state = STATE_CREATING_DIRS;
        createNextDir();
    }
}

void CopyJobPrivate::startRenameJob(const QUrl &slave_url)
{
    Q_Q(CopyJob);

    // Silence KDirWatch notifications, otherwise performance is horrible
    if (m_currentSrcURL.isLocalFile()) {
        const QString parentDir = m_currentSrcURL.adjusted(QUrl::RemoveFilename).path();
        const auto [it, isInserted] = m_parentDirs.insert(parentDir);
        if (isInserted) {
            KDirWatch::self()->stopDirScan(parentDir);
        }
    }

    QUrl dest = m_dest;
    // Append filename or dirname to destination URL, if allowed
    if (destinationState == DEST_IS_DIR && !m_asMethod) {
        dest = addPathToUrl(dest, m_currentSrcURL.fileName());
    }
    m_currentDestURL = dest;
    qCDebug(KIO_COPYJOB_DEBUG) << m_currentSrcURL << "->" << dest << "trying direct rename first";
    if (state != STATE_RENAMING) {
        q->setTotalAmount(KJob::Files, m_srcList.count());
    }
    state = STATE_RENAMING;

    struct CopyInfo info;
    info.permissions = -1;
    info.size = KIO::invalidFilesize;
    info.uSource = m_currentSrcURL;
    info.uDest = dest;
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 2)
    QList<CopyInfo> files;
    files.append(info);
    Q_EMIT q->aboutToCreate(q, files);
#endif

    KIO_ARGS << m_currentSrcURL << dest << (qint8) false /*no overwrite*/;
    SimpleJob *newJob = SimpleJobPrivate::newJobNoUi(slave_url, CMD_RENAME, packedArgs);
    newJob->setParentJob(q);
    q->addSubjob(newJob);
    if (m_currentSrcURL.adjusted(QUrl::RemoveFilename) != dest.adjusted(QUrl::RemoveFilename)) { // For the user, moving isn't renaming. Only renaming is.
        m_bOnlyRenames = false;
    }
}

void CopyJobPrivate::startListing(const QUrl &src)
{
    Q_Q(CopyJob);
    state = STATE_LISTING;
    m_bURLDirty = true;
    ListJob *newjob = listRecursive(src, KIO::HideProgressInfo);
    newjob->setUnrestricted(true);
    q->connect(newjob, &ListJob::entries, q, [this](KIO::Job *job, const KIO::UDSEntryList &list) {
        slotEntries(job, list);
    });
    q->connect(newjob, &ListJob::subError, q, [this](KIO::ListJob *job, KIO::ListJob *subJob) {
        slotSubError(job, subJob);
    });
    q->addSubjob(newjob);
}

void CopyJobPrivate::skip(const QUrl &sourceUrl, bool isDir)
{
    QUrl dir(sourceUrl);
    if (!isDir) {
        // Skipping a file: make sure not to delete the parent dir (#208418)
        dir = dir.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    }
    while (dirsToRemove.removeAll(dir) > 0) {
        // Do not rely on rmdir() on the parent directories aborting.
        // Exclude the parent dirs explicitly.
        dir = dir.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    }
}

bool CopyJobPrivate::shouldOverwriteDir(const QString &path) const
{
    if (m_bOverwriteAllDirs) {
        return true;
    }
    return m_overwriteList.contains(path);
}

bool CopyJobPrivate::shouldOverwriteFile(const QString &path) const
{
    if (m_bOverwriteAllFiles) {
        return true;
    }
    return m_overwriteList.contains(path);
}

bool CopyJobPrivate::shouldSkip(const QString &path) const
{
    for (const QString &skipPath : std::as_const(m_skipList)) {
        if (path.startsWith(skipPath)) {
            return true;
        }
    }
    return false;
}

void CopyJobPrivate::renameDirectory(const QList<CopyInfo>::iterator &it, const QUrl &newUrl)
{
    Q_Q(CopyJob);
    Q_EMIT q->renamed(q, (*it).uDest, newUrl); // for e.g. KPropertiesDialog

    QString oldPath = (*it).uDest.path();
    if (!oldPath.endsWith(QLatin1Char('/'))) {
        oldPath += QLatin1Char('/');
    }

    // Change the current one and strip the trailing '/'
    (*it).uDest = newUrl.adjusted(QUrl::StripTrailingSlash);

    QString newPath = newUrl.path(); // With trailing slash
    if (!newPath.endsWith(QLatin1Char('/'))) {
        newPath += QLatin1Char('/');
    }
    QList<CopyInfo>::Iterator renamedirit = it;
    ++renamedirit;
    // Change the name of subdirectories inside the directory
    for (; renamedirit != dirs.end(); ++renamedirit) {
        QString path = (*renamedirit).uDest.path();
        if (path.startsWith(oldPath)) {
            QString n = path;
            n.replace(0, oldPath.length(), newPath);
            /*qDebug() << "dirs list:" << (*renamedirit).uSource.path()
                         << "was going to be" << path
                         << ", changed into" << n;*/
            (*renamedirit).uDest.setPath(n, QUrl::DecodedMode);
        }
    }
    // Change filenames inside the directory
    QList<CopyInfo>::Iterator renamefileit = files.begin();
    for (; renamefileit != files.end(); ++renamefileit) {
        QString path = (*renamefileit).uDest.path(QUrl::FullyDecoded);
        if (path.startsWith(oldPath)) {
            QString n = path;
            n.replace(0, oldPath.length(), newPath);
            /*qDebug() << "files list:" << (*renamefileit).uSource.path()
                         << "was going to be" << path
                         << ", changed into" << n;*/
            (*renamefileit).uDest.setPath(n, QUrl::DecodedMode);
        }
    }
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 2)
    if (!dirs.isEmpty()) {
        Q_EMIT q->aboutToCreate(q, dirs);
    }
    if (!files.isEmpty()) {
        Q_EMIT q->aboutToCreate(q, files);
    }
#endif
}

void CopyJobPrivate::slotResultCreatingDirs(KJob *job)
{
    Q_Q(CopyJob);
    // The dir we are trying to create:
    QList<CopyInfo>::Iterator it = dirs.begin();
    // Was there an error creating a dir ?
    if (job->error()) {
        m_conflictError = job->error();
        if (m_conflictError == ERR_DIR_ALREADY_EXIST //
            || m_conflictError == ERR_FILE_ALREADY_EXIST) { // can't happen?
            QUrl oldURL = ((SimpleJob *)job)->url();
            // Should we skip automatically ?
            if (m_bAutoSkipDirs) {
                // We don't want to copy files in this directory, so we put it on the skip list
                QString path = oldURL.path();
                if (!path.endsWith(QLatin1Char('/'))) {
                    path += QLatin1Char('/');
                }
                m_skipList.append(path);
                skip(oldURL, true);
                dirs.erase(it); // Move on to next dir
            } else {
                // Did the user choose to overwrite already?
                const QString destDir = (*it).uDest.path();
                if (shouldOverwriteDir(destDir)) { // overwrite => just skip
                    Q_EMIT q->copyingDone(q, (*it).uSource, finalDestUrl((*it).uSource, (*it).uDest), (*it).mtime, true /* directory */, false /* renamed */);
                    dirs.erase(it); // Move on to next dir
                    ++m_processedDirs;
                } else {
                    if (m_bAutoRenameDirs) {
                        const QUrl destDirectory = (*it).uDest.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
                        const QString newName = KFileUtils::suggestName(destDirectory, (*it).uDest.fileName());
                        QUrl newUrl(destDirectory);
                        newUrl.setPath(concatPaths(newUrl.path(), newName));
                        renameDirectory(it, newUrl);
                    } else {
                        if (!KIO::delegateExtension<AskUserActionInterface *>(q)) {
                            q->Job::slotResult(job); // will set the error and emit result(this)
                            return;
                        }

                        Q_ASSERT(((SimpleJob *)job)->url() == (*it).uDest);
                        q->removeSubjob(job);
                        Q_ASSERT(!q->hasSubjobs()); // We should have only one job at a time ...

                        // We need to stat the existing dir, to get its last-modification time
                        QUrl existingDest((*it).uDest);
                        SimpleJob *newJob = KIO::statDetails(existingDest, StatJob::DestinationSide, KIO::StatDefaultDetails, KIO::HideProgressInfo);
                        qCDebug(KIO_COPYJOB_DEBUG) << "KIO::stat for resolving conflict on" << existingDest;
                        state = STATE_CONFLICT_CREATING_DIRS;
                        q->addSubjob(newJob);
                        return; // Don't move to next dir yet !
                    }
                }
            }
        } else {
            // Severe error, abort
            q->Job::slotResult(job); // will set the error and emit result(this)
            return;
        }
    } else { // no error : remove from list, to move on to next dir
        // this is required for the undo feature
        Q_EMIT q->copyingDone(q, (*it).uSource, finalDestUrl((*it).uSource, (*it).uDest), (*it).mtime, true, false);
        m_directoriesCopied.push_back(*it);
        dirs.erase(it);
        ++m_processedDirs;
    }

    q->removeSubjob(job);
    Q_ASSERT(!q->hasSubjobs()); // We should have only one job at a time ...
    createNextDir();
}

void CopyJobPrivate::slotResultConflictCreatingDirs(KJob *job)
{
    Q_Q(CopyJob);
    // We come here after a conflict has been detected and we've stated the existing dir

    // The dir we were trying to create:
    QList<CopyInfo>::Iterator it = dirs.begin();

    const UDSEntry entry = ((KIO::StatJob *)job)->statResult();

    QDateTime destmtime;
    QDateTime destctime;
    const KIO::filesize_t destsize = entry.numberValue(KIO::UDSEntry::UDS_SIZE);
    const QString linkDest = entry.stringValue(KIO::UDSEntry::UDS_LINK_DEST);

    q->removeSubjob(job);
    Q_ASSERT(!q->hasSubjobs()); // We should have only one job at a time ...

    // Always multi and skip (since there are files after that)
    RenameDialog_Options options(RenameDialog_MultipleItems | RenameDialog_Skip | RenameDialog_DestIsDirectory);
    // Overwrite only if the existing thing is a dir (no chance with a file)
    if (m_conflictError == ERR_DIR_ALREADY_EXIST) {
        // We are in slotResultConflictCreatingDirs(), so the source is a dir
        options |= RenameDialog_SourceIsDirectory;

        if ((*it).uSource == (*it).uDest
            || ((*it).uSource.scheme() == (*it).uDest.scheme() && (*it).uSource.adjusted(QUrl::StripTrailingSlash).path() == linkDest)) {
            options |= RenameDialog_OverwriteItself;
        } else {
            options |= RenameDialog_Overwrite;
            destmtime = QDateTime::fromMSecsSinceEpoch(1000 * entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, -1), Qt::UTC);
            destctime = QDateTime::fromMSecsSinceEpoch(1000 * entry.numberValue(KIO::UDSEntry::UDS_CREATION_TIME, -1), Qt::UTC);
        }
    }

    if (m_reportTimer) {
        m_reportTimer->stop();
    }

    auto *askUserActionInterface = KIO::delegateExtension<KIO::AskUserActionInterface *>(q);

    auto renameSignal = &KIO::AskUserActionInterface::askUserRenameResult;
    QObject::connect(askUserActionInterface, renameSignal, q, [=](RenameDialog_Result result, const QUrl &newUrl, KJob *parentJob) {
        Q_ASSERT(parentJob == q);
        // Only receive askUserRenameResult once per rename dialog
        QObject::disconnect(askUserActionInterface, renameSignal, q, nullptr);

        if (m_reportTimer) {
            m_reportTimer->start(s_reportTimeout);
        }

        const QString existingDest = (*it).uDest.path();

        switch (result) {
        case Result_Cancel:
            q->setError(ERR_USER_CANCELED);
            q->emitResult();
            return;
        case Result_AutoRename:
            m_bAutoRenameDirs = true;
            // fall through
            Q_FALLTHROUGH();
        case Result_Rename:
            renameDirectory(it, newUrl);
            break;
        case Result_AutoSkip:
            m_bAutoSkipDirs = true;
            // fall through
            Q_FALLTHROUGH();
        case Result_Skip:
            m_skipList.append(existingDest);
            skip((*it).uSource, true);
            // Move on to next dir
            dirs.erase(it);
            ++m_processedDirs;
            break;
        case Result_Overwrite:
            m_overwriteList.insert(existingDest);
            Q_EMIT q->copyingDone(q, (*it).uSource, finalDestUrl((*it).uSource, (*it).uDest), (*it).mtime, true /* directory */, false /* renamed */);
            // Move on to next dir
            dirs.erase(it);
            ++m_processedDirs;
            break;
        case Result_OverwriteAll:
            m_bOverwriteAllDirs = true;
            Q_EMIT q->copyingDone(q, (*it).uSource, finalDestUrl((*it).uSource, (*it).uDest), (*it).mtime, true /* directory */, false /* renamed */);
            // Move on to next dir
            dirs.erase(it);
            ++m_processedDirs;
            break;
        default:
            Q_ASSERT(0);
        }
        state = STATE_CREATING_DIRS;
        createNextDir();
    });

    /* clang-format off */
    askUserActionInterface->askUserRename(q, i18n("Folder Already Exists"),
                                          (*it).uSource, (*it).uDest,
                                          options,
                                          (*it).size, destsize,
                                          (*it).ctime, destctime,
                                          (*it).mtime, destmtime);
    /* clang-format on */
}

void CopyJobPrivate::createNextDir()
{
    Q_Q(CopyJob);

    // Take first dir to create out of list
    QList<CopyInfo>::Iterator it = dirs.begin();
    // Is this URL on the skip list or the overwrite list ?
    while (it != dirs.end()) {
        const QString dir = it->uDest.path();
        if (shouldSkip(dir)) {
            it = dirs.erase(it);
        } else {
            break;
        }
    }

    if (it != dirs.end()) { // any dir to create, finally ?
        if (it->uDest.isLocalFile()) {
            // uDest doesn't exist yet, check the filesystem of the parent dir
            const auto destFileSystem = KFileSystemType::fileSystemType(it->uDest.adjusted(QUrl::StripTrailingSlash | QUrl::RemoveFilename).toLocalFile());
            if (isFatOrNtfs(destFileSystem)) {
                const QString dirName = it->uDest.adjusted(QUrl::StripTrailingSlash).fileName();
                if (hasInvalidChars(dirName)) {
                    // We already asked the user?
                    if (m_autoReplaceInvalidChars) {
                        processCreateNextDir(it, KIO::Result_ReplaceInvalidChars);
                        return;
                    } else if (m_autoSkipDirsWithInvalidChars) {
                        processCreateNextDir(it, KIO::Result_Skip);
                        return;
                    }

                    const QString msg = invalidCharsSupportMsg(it->uDest.toDisplayString(QUrl::PreferLocalFile),
                                                               KFileSystemType::fileSystemName(destFileSystem),
                                                               true /* isDir */);

                    if (auto *askUserActionInterface = KIO::delegateExtension<KIO::AskUserActionInterface *>(q)) {
                        SkipDialog_Options options = KIO::SkipDialog_Replace_Invalid_Chars;
                        if (dirs.size() > 1) {
                            options |= SkipDialog_MultipleItems;
                        }

                        auto skipSignal = &KIO::AskUserActionInterface::askUserSkipResult;
                        QObject::connect(askUserActionInterface, skipSignal, q, [=](SkipDialog_Result result, KJob *parentJob) {
                            Q_ASSERT(parentJob == q);

                            // Only receive askUserSkipResult once per skip dialog
                            QObject::disconnect(askUserActionInterface, skipSignal, q, nullptr);

                            processCreateNextDir(it, result);
                        });

                        askUserActionInterface->askUserSkip(q, options, msg);

                        return;
                    } else { // No Job Ui delegate
                        qCWarning(KIO_COPYJOB_DEBUG) << msg;
                        q->emitResult();
                        return;
                    }
                }
            }
        }

        processCreateNextDir(it, -1);
    } else { // we have finished creating dirs
        q->setProcessedAmount(KJob::Directories, m_processedDirs); // make sure final number appears

        if (m_mode == CopyJob::Move) {
            // Now we know which dirs hold the files we're going to delete.
            // To speed things up and prevent double-notification, we disable KDirWatch
            // on those dirs temporarily (using KDirWatch::self, that's the instance
            // used by e.g. kdirlister).
            for (const auto &dir : m_parentDirs) {
                KDirWatch::self()->stopDirScan(dir);
            }
        }

        state = STATE_COPYING_FILES;
        ++m_processedFiles; // Ralf wants it to start at 1, not 0
        copyNextFile();
    }
}

void CopyJobPrivate::processCreateNextDir(const QList<CopyInfo>::Iterator &it, int result)
{
    Q_Q(CopyJob);

    switch (result) {
    case Result_Cancel:
        q->setError(ERR_USER_CANCELED);
        q->emitResult();
        return;
    case KIO::Result_ReplaceAllInvalidChars:
        m_autoReplaceInvalidChars = true;
        Q_FALLTHROUGH();
    case KIO::Result_ReplaceInvalidChars: {
        it->uDest = it->uDest.adjusted(QUrl::StripTrailingSlash);
        QString dirName = it->uDest.fileName();
        const int len = dirName.size();
        cleanMsdosDestName(dirName);
        QString path = it->uDest.path();
        path.replace(path.size() - len, len, dirName);
        it->uDest.setPath(path);
        break;
    }
    case KIO::Result_AutoSkip:
        m_autoSkipDirsWithInvalidChars = true;
        Q_FALLTHROUGH();
    case KIO::Result_Skip:
        m_skipList.append(it->uDest.path());
        skip(it->uSource, true);
        dirs.erase(it); // Move on to next dir
        ++m_processedDirs;
        createNextDir();
        return;
    default:
        break;
    }

    // Create the directory - with default permissions so that we can put files into it
    // TODO : change permissions once all is finished; but for stuff coming from CDROM it sucks...
    KIO::SimpleJob *newjob = KIO::mkdir(it->uDest, -1);
    newjob->setParentJob(q);
    if (shouldOverwriteFile(it->uDest.path())) { // if we are overwriting an existing file or symlink
        newjob->addMetaData(QStringLiteral("overwrite"), QStringLiteral("true"));
    }

    m_currentDestURL = it->uDest;
    m_bURLDirty = true;

    q->addSubjob(newjob);
}

void CopyJobPrivate::slotResultCopyingFiles(KJob *job)
{
    Q_Q(CopyJob);
    // The file we were trying to copy:
    QList<CopyInfo>::Iterator it = files.begin();
    if (job->error()) {
        // Should we skip automatically ?
        if (m_bAutoSkipFiles) {
            skip((*it).uSource, false);
            m_fileProcessedSize = (*it).size;
            files.erase(it); // Move on to next file
        } else {
            m_conflictError = job->error(); // save for later
            // Existing dest ?
            if (m_conflictError == ERR_FILE_ALREADY_EXIST //
                || m_conflictError == ERR_DIR_ALREADY_EXIST //
                || m_conflictError == ERR_IDENTICAL_FILES) {
                if (m_bAutoRenameFiles) {
                    QUrl destDirectory = (*it).uDest.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
                    const QString newName = KFileUtils::suggestName(destDirectory, (*it).uDest.fileName());
                    QUrl newDest(destDirectory);
                    newDest.setPath(concatPaths(newDest.path(), newName));
                    Q_EMIT q->renamed(q, (*it).uDest, newDest); // for e.g. kpropsdlg
                    (*it).uDest = newDest;

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 2)
                    QList<CopyInfo> files;
                    files.append(*it);
                    Q_EMIT q->aboutToCreate(q, files);
#endif
                } else {
                    if (!KIO::delegateExtension<AskUserActionInterface *>(q)) {
                        q->Job::slotResult(job); // will set the error and emit result(this)
                        return;
                    }

                    q->removeSubjob(job);
                    Q_ASSERT(!q->hasSubjobs());
                    // We need to stat the existing file, to get its last-modification time
                    QUrl existingFile((*it).uDest);
                    SimpleJob *newJob =
                        KIO::statDetails(existingFile, StatJob::DestinationSide, KIO::StatDetail::StatBasic | KIO::StatDetail::StatTime, KIO::HideProgressInfo);
                    qCDebug(KIO_COPYJOB_DEBUG) << "KIO::stat for resolving conflict on" << existingFile;
                    state = STATE_CONFLICT_COPYING_FILES;
                    q->addSubjob(newJob);
                    return; // Don't move to next file yet !
                }
            } else {
                if (m_bCurrentOperationIsLink && qobject_cast<KIO::DeleteJob *>(job)) {
                    // Very special case, see a few lines below
                    // We are deleting the source of a symlink we successfully moved... ignore error
                    m_fileProcessedSize = (*it).size;
                    ++m_processedFiles;
                    files.erase(it);
                } else {
                    if (!KIO::delegateExtension<AskUserActionInterface *>(q)) {
                        q->Job::slotResult(job); // will set the error and emit result(this)
                        return;
                    }

                    // Go directly to the conflict resolution, there is nothing to stat
                    slotResultErrorCopyingFiles(job);
                    return;
                }
            }
        }
    } else { // no error
        // Special case for moving links. That operation needs two jobs, unlike others.
        if (m_bCurrentOperationIsLink //
            && m_mode == CopyJob::Move //
            && !qobject_cast<KIO::DeleteJob *>(job) // Deleting source not already done
        ) {
            q->removeSubjob(job);
            Q_ASSERT(!q->hasSubjobs());
            // The only problem with this trick is that the error handling for this del operation
            // is not going to be right... see 'Very special case' above.
            KIO::Job *newjob = KIO::del((*it).uSource, HideProgressInfo);
            newjob->setParentJob(q);
            q->addSubjob(newjob);
            return; // Don't move to next file yet !
        }

        const QUrl finalUrl = finalDestUrl((*it).uSource, (*it).uDest);

        if (m_bCurrentOperationIsLink) {
            QString target = (m_mode == CopyJob::Link ? (*it).uSource.path() : (*it).linkDest);
            // required for the undo feature
            Q_EMIT q->copyingLinkDone(q, (*it).uSource, target, finalUrl);
        } else {
            // required for the undo feature
            Q_EMIT q->copyingDone(q, (*it).uSource, finalUrl, (*it).mtime, false, false);
            if (m_mode == CopyJob::Move) {
#ifndef KIO_ANDROID_STUB
                org::kde::KDirNotify::emitFileMoved((*it).uSource, finalUrl);
#endif
            }
            m_successSrcList.append((*it).uSource);
            if (m_freeSpace != KIO::invalidFilesize && (*it).size != KIO::invalidFilesize) {
                m_freeSpace -= (*it).size;
            }
        }
        // remove from list, to move on to next file
        files.erase(it);
        ++m_processedFiles;
    }

    // clear processed size for last file and add it to overall processed size
    m_processedSize += m_fileProcessedSize;
    m_fileProcessedSize = 0;

    qCDebug(KIO_COPYJOB_DEBUG) << files.count() << "files remaining";

    // Merge metadata from subjob
    KIO::Job *kiojob = qobject_cast<KIO::Job *>(job);
    Q_ASSERT(kiojob);
    m_incomingMetaData += kiojob->metaData();
    q->removeSubjob(job);
    Q_ASSERT(!q->hasSubjobs()); // We should have only one job at a time ...
    copyNextFile();
}

void CopyJobPrivate::slotResultErrorCopyingFiles(KJob *job)
{
    Q_Q(CopyJob);
    // We come here after a conflict has been detected and we've stated the existing file
    // The file we were trying to create:
    QList<CopyInfo>::Iterator it = files.begin();

    RenameDialog_Result res = Result_Cancel;

    if (m_reportTimer) {
        m_reportTimer->stop();
    }

    q->removeSubjob(job);
    Q_ASSERT(!q->hasSubjobs());
    auto *askUserActionInterface = KIO::delegateExtension<KIO::AskUserActionInterface *>(q);

    if (m_conflictError == ERR_FILE_ALREADY_EXIST //
        || m_conflictError == ERR_DIR_ALREADY_EXIST //
        || m_conflictError == ERR_IDENTICAL_FILES) {
        // Its modification time:
        const UDSEntry entry = static_cast<KIO::StatJob *>(job)->statResult();

        QDateTime destmtime;
        QDateTime destctime;
        const KIO::filesize_t destsize = entry.numberValue(KIO::UDSEntry::UDS_SIZE);
        const QString linkDest = entry.stringValue(KIO::UDSEntry::UDS_LINK_DEST);

        // Offer overwrite only if the existing thing is a file
        // If src==dest, use "overwrite-itself"
        RenameDialog_Options options;
        bool isDir = true;

        if (m_conflictError == ERR_DIR_ALREADY_EXIST) {
            options = RenameDialog_DestIsDirectory;
        } else {
            if ((*it).uSource == (*it).uDest
                || ((*it).uSource.scheme() == (*it).uDest.scheme() && (*it).uSource.adjusted(QUrl::StripTrailingSlash).path() == linkDest)) {
                options = RenameDialog_OverwriteItself;
            } else {
                const qint64 destMTimeStamp = entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, -1);
                if (m_bOverwriteWhenOlder && (*it).mtime.isValid() && destMTimeStamp != -1) {
                    if ((*it).mtime.currentSecsSinceEpoch() > destMTimeStamp) {
                        qCDebug(KIO_COPYJOB_DEBUG) << "dest is older, overwriting" << (*it).uDest;
                        res = Result_Overwrite;
                    } else {
                        qCDebug(KIO_COPYJOB_DEBUG) << "dest is newer, skipping" << (*it).uDest;
                        res = Result_Skip;
                    }
                } else {
                    // These timestamps are used only when RenameDialog_Overwrite is set.
                    destmtime = QDateTime::fromMSecsSinceEpoch(1000 * destMTimeStamp, Qt::UTC);
                    destctime = QDateTime::fromMSecsSinceEpoch(1000 * entry.numberValue(KIO::UDSEntry::UDS_CREATION_TIME, -1), Qt::UTC);

                    options = RenameDialog_Overwrite;
                }
            }
            isDir = false;
        }

        // if no preset value was set
        if (res == Result_Cancel) {
            if (!m_bSingleFileCopy) {
                options = RenameDialog_Options(options | RenameDialog_MultipleItems | RenameDialog_Skip);
            }

            const QString caption = !isDir ? i18n("File Already Exists") : i18n("Already Exists as Folder");

            auto renameSignal = &KIO::AskUserActionInterface::askUserRenameResult;
            QObject::connect(askUserActionInterface, renameSignal, q, [=](RenameDialog_Result result, const QUrl &newUrl, KJob *parentJob) {
                Q_ASSERT(parentJob == q);
                // Only receive askUserRenameResult once per rename dialog
                QObject::disconnect(askUserActionInterface, renameSignal, q, nullptr);
                processFileRenameDialogResult(it, result, newUrl, destmtime);
            });

            /* clang-format off */
            askUserActionInterface->askUserRename(q, caption,
                                                  (*it).uSource, (*it).uDest,
                                                  options,
                                                  (*it).size, destsize,
                                                  (*it).ctime, destctime,
                                                  (*it).mtime, destmtime); /* clang-format on */
            return;
        }
    } else {
        if (job->error() == ERR_USER_CANCELED) {
            res = Result_Cancel;
        } else if (!askUserActionInterface) {
            q->Job::slotResult(job); // will set the error and emit result(this)
            return;
        } else {
            SkipDialog_Options options;
            if (files.count() > 1) {
                options |= SkipDialog_MultipleItems;
            }

            auto skipSignal = &KIO::AskUserActionInterface::askUserSkipResult;
            QObject::connect(askUserActionInterface, skipSignal, q, [=](SkipDialog_Result result, KJob *parentJob) {
                Q_ASSERT(parentJob == q);
                // Only receive askUserSkipResult once per skip dialog
                QObject::disconnect(askUserActionInterface, skipSignal, q, nullptr);
                processFileRenameDialogResult(it, result, QUrl() /* no new url in skip */, QDateTime{});
            });

            askUserActionInterface->askUserSkip(q, options, job->errorString());
            return;
        }
    }

    processFileRenameDialogResult(it, res, QUrl{}, QDateTime{});
}

void CopyJobPrivate::processFileRenameDialogResult(const QList<CopyInfo>::Iterator &it,
                                                   RenameDialog_Result result,
                                                   const QUrl &newUrl,
                                                   const QDateTime &destmtime)
{
    Q_Q(CopyJob);

    if (m_reportTimer) {
        m_reportTimer->start(s_reportTimeout);
    }

    if (result == Result_OverwriteWhenOlder) {
        m_bOverwriteWhenOlder = true;
        if ((*it).mtime > destmtime) {
            qCDebug(KIO_COPYJOB_DEBUG) << "dest is older, overwriting" << (*it).uDest;
            result = Result_Overwrite;
        } else {
            qCDebug(KIO_COPYJOB_DEBUG) << "dest is newer, skipping" << (*it).uDest;
            result = Result_Skip;
        }
    }

    switch (result) {
    case Result_Cancel:
        q->setError(ERR_USER_CANCELED);
        q->emitResult();
        return;
    case Result_AutoRename:
        m_bAutoRenameFiles = true;
        // fall through
        Q_FALLTHROUGH();
    case Result_Rename: {
        Q_EMIT q->renamed(q, (*it).uDest, newUrl); // for e.g. kpropsdlg
        (*it).uDest = newUrl;
        m_bURLDirty = true;

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 2)
        QList<CopyInfo> files;
        files.append(*it);
        Q_EMIT q->aboutToCreate(q, files);
#endif
        break;
    }
    case Result_AutoSkip:
        m_bAutoSkipFiles = true;
        // fall through
        Q_FALLTHROUGH();
    case Result_Skip:
        // Move on to next file
        skip((*it).uSource, false);
        m_processedSize += (*it).size;
        files.erase(it);
        break;
    case Result_OverwriteAll:
        m_bOverwriteAllFiles = true;
        break;
    case Result_Overwrite:
        // Add to overwrite list, so that copyNextFile knows to overwrite
        m_overwriteList.insert((*it).uDest.path());
        break;
    case Result_Retry:
        // Do nothing, copy file again
        break;
    default:
        Q_ASSERT(0);
    }
    state = STATE_COPYING_FILES;
    copyNextFile();
}

KIO::Job *CopyJobPrivate::linkNextFile(const QUrl &uSource, const QUrl &uDest, JobFlags flags)
{
    qCDebug(KIO_COPYJOB_DEBUG) << "Linking";
    if (compareUrls(uSource, uDest)) {
        // This is the case of creating a real symlink
        KIO::SimpleJob *newJob = KIO::symlink(uSource.path(), uDest, flags | HideProgressInfo /*no GUI*/);
        newJob->setParentJob(q_func());
        qCDebug(KIO_COPYJOB_DEBUG) << "Linking target=" << uSource.path() << "link=" << uDest;
        // emit linking( this, uSource.path(), uDest );
        m_bCurrentOperationIsLink = true;
        m_currentSrcURL = uSource;
        m_currentDestURL = uDest;
        m_bURLDirty = true;
        // Observer::self()->slotCopying( this, uSource, uDest ); // should be slotLinking perhaps
        return newJob;
    } else {
        Q_Q(CopyJob);
        qCDebug(KIO_COPYJOB_DEBUG) << "Linking URL=" << uSource << "link=" << uDest;
        if (uDest.isLocalFile()) {
            // if the source is a devices url, handle it a littlebit special

            QString path = uDest.toLocalFile();
            qCDebug(KIO_COPYJOB_DEBUG) << "path=" << path;
            QFile f(path);
            if (f.open(QIODevice::ReadWrite)) {
                f.close();
                KDesktopFile desktopFile(path);
                KConfigGroup config = desktopFile.desktopGroup();
                QUrl url = uSource;
                url.setPassword(QString());
                config.writePathEntry("URL", url.toString());
                config.writeEntry("Name", url.toString());
                config.writeEntry("Type", QStringLiteral("Link"));
                QString protocol = uSource.scheme();
                if (protocol == QLatin1String("ftp")) {
                    config.writeEntry("Icon", QStringLiteral("folder-remote"));
                } else if (protocol == QLatin1String("http") || protocol == QLatin1String("https")) {
                    config.writeEntry("Icon", QStringLiteral("text-html"));
                } else if (protocol == QLatin1String("info")) {
                    config.writeEntry("Icon", QStringLiteral("text-x-texinfo"));
                } else if (protocol == QLatin1String("mailto")) { // sven:
                    config.writeEntry("Icon", QStringLiteral("internet-mail")); // added mailto: support
                } else if (protocol == QLatin1String("trash") && url.path().length() <= 1) { // trash:/ link
                    config.writeEntry("Name", i18n("Trash"));
                    config.writeEntry("Icon", QStringLiteral("user-trash-full"));
                    config.writeEntry("EmptyIcon", QStringLiteral("user-trash"));
                } else {
                    config.writeEntry("Icon", QStringLiteral("unknown"));
                }
                config.sync();
                files.erase(files.begin()); // done with this one, move on
                ++m_processedFiles;
                copyNextFile();
                return nullptr;
            } else {
                qCDebug(KIO_COPYJOB_DEBUG) << "ERR_CANNOT_OPEN_FOR_WRITING";
                q->setError(ERR_CANNOT_OPEN_FOR_WRITING);
                q->setErrorText(uDest.toLocalFile());
                q->emitResult();
                return nullptr;
            }
        } else {
            // Todo: not show "link" on remote dirs if the src urls are not from the same protocol+host+...
            q->setError(ERR_CANNOT_SYMLINK);
            q->setErrorText(uDest.toDisplayString());
            q->emitResult();
            return nullptr;
        }
    }
}

bool CopyJobPrivate::handleMsdosFsQuirks(QList<CopyInfo>::Iterator it, KFileSystemType::Type fsType)
{
    Q_Q(CopyJob);

    QString msg;
    SkipDialog_Options options;
    SkipType skipType = NoSkipType;

    if (isFatFs(fsType) && !it->linkDest.isEmpty()) { // Copying a symlink
        skipType = SkipFatSymlinks;
        if (m_autoSkipFatSymlinks) { // Have we already asked the user?
            processCopyNextFile(it, KIO::Result_Skip, skipType);
            return true;
        }
        options = KIO::SkipDialog_Hide_Retry;
        msg = symlinkSupportMsg(it->uDest.toLocalFile(), KFileSystemType::fileSystemName(fsType));
    } else if (hasInvalidChars(it->uDest.fileName())) {
        skipType = SkipInvalidChars;
        if (m_autoReplaceInvalidChars) { // Have we already asked the user?
            processCopyNextFile(it, KIO::Result_ReplaceInvalidChars, skipType);
            return true;
        } else if (m_autoSkipFilesWithInvalidChars) { // Have we already asked the user?
            processCopyNextFile(it, KIO::Result_Skip, skipType);
            return true;
        }

        options = KIO::SkipDialog_Replace_Invalid_Chars;
        msg = invalidCharsSupportMsg(it->uDest.toDisplayString(QUrl::PreferLocalFile), KFileSystemType::fileSystemName(fsType));
    }

    if (!msg.isEmpty()) {
        if (auto *askUserActionInterface = KIO::delegateExtension<KIO::AskUserActionInterface *>(q)) {
            if (files.size() > 1) {
                options |= SkipDialog_MultipleItems;
            }

            auto skipSignal = &KIO::AskUserActionInterface::askUserSkipResult;
            QObject::connect(askUserActionInterface, skipSignal, q, [=](SkipDialog_Result result, KJob *parentJob) {
                Q_ASSERT(parentJob == q);
                // Only receive askUserSkipResult once per skip dialog
                QObject::disconnect(askUserActionInterface, skipSignal, q, nullptr);

                processCopyNextFile(it, result, skipType);
            });

            askUserActionInterface->askUserSkip(q, options, msg);

            return true;
        } else { // No Job Ui delegate
            qCWarning(KIO_COPYJOB_DEBUG) << msg;
            q->emitResult();
            return true;
        }
    }

    return false; // Not handled, move on
}

void CopyJobPrivate::copyNextFile()
{
    Q_Q(CopyJob);
    bool bCopyFile = false;
    qCDebug(KIO_COPYJOB_DEBUG);

    bool isDestLocal = m_globalDest.isLocalFile();

    // Take the first file in the list
    QList<CopyInfo>::Iterator it = files.begin();
    // Is this URL on the skip list ?
    while (it != files.end() && !bCopyFile) {
        const QString destFile = (*it).uDest.path();
        bCopyFile = !shouldSkip(destFile);
        if (!bCopyFile) {
            it = files.erase(it);
        }

        if (it != files.end() && isDestLocal && (*it).size > 0xFFFFFFFF) { // 4GB-1
            const auto destFileSystem = KFileSystemType::fileSystemType(m_globalDest.toLocalFile());
            if (destFileSystem == KFileSystemType::Fat) {
                q->setError(ERR_FILE_TOO_LARGE_FOR_FAT32);
                q->setErrorText((*it).uDest.toDisplayString());
                q->emitResult();
                return;
            }
        }
    }

    if (bCopyFile) { // any file to create, finally ?
        if (isDestLocal) {
            const auto destFileSystem = KFileSystemType::fileSystemType(m_globalDest.toLocalFile());
            if (isFatOrNtfs(destFileSystem)) {
                if (handleMsdosFsQuirks(it, destFileSystem)) {
                    return;
                }
            }
        }

        processCopyNextFile(it, -1, NoSkipType);
    } else {
        // We're done
        qCDebug(KIO_COPYJOB_DEBUG) << "copyNextFile finished";
        --m_processedFiles; // undo the "start at 1" hack
        slotReport(); // display final numbers, important if progress dialog stays up

        deleteNextDir();
    }
}

void CopyJobPrivate::processCopyNextFile(const QList<CopyInfo>::Iterator &it, int result, SkipType skipType)
{
    Q_Q(CopyJob);

    switch (result) {
    case Result_Cancel:
        q->setError(ERR_USER_CANCELED);
        q->emitResult();
        return;
    case KIO::Result_ReplaceAllInvalidChars:
        m_autoReplaceInvalidChars = true;
        Q_FALLTHROUGH();
    case KIO::Result_ReplaceInvalidChars: {
        QString fileName = it->uDest.fileName();
        const int len = fileName.size();
        cleanMsdosDestName(fileName);
        QString path = it->uDest.path();
        path.replace(path.size() - len, len, fileName);
        it->uDest.setPath(path);
        break;
    }
    case KIO::Result_AutoSkip:
        if (skipType == SkipInvalidChars) {
            m_autoSkipFilesWithInvalidChars = true;
        } else if (skipType == SkipFatSymlinks) {
            m_autoSkipFatSymlinks = true;
        }
        Q_FALLTHROUGH();
    case KIO::Result_Skip:
        // Move on the next file
        files.erase(it);
        copyNextFile();
        return;
    default:
        break;
    }

    qCDebug(KIO_COPYJOB_DEBUG) << "preparing to copy" << (*it).uSource << (*it).size << m_freeSpace;
    if (m_freeSpace != KIO::invalidFilesize && (*it).size != KIO::invalidFilesize) {
        if (m_freeSpace < (*it).size) {
            q->setError(ERR_DISK_FULL);
            q->emitResult();
            return;
        }
    }

    const QUrl &uSource = (*it).uSource;
    const QUrl &uDest = (*it).uDest;
    // Do we set overwrite ?
    bool bOverwrite;
    const QString destFile = uDest.path();
    qCDebug(KIO_COPYJOB_DEBUG) << "copying" << destFile;
    if (uDest == uSource) {
        bOverwrite = false;
    } else {
        bOverwrite = shouldOverwriteFile(destFile);
    }

    // If source isn't local and target is local, we ignore the original permissions
    // Otherwise, files downloaded from HTTP end up with -r--r--r--
    const bool remoteSource = !KProtocolManager::supportsListing(uSource) || uSource.scheme() == QLatin1String("trash");
    int permissions = (*it).permissions;
    if (m_defaultPermissions || (remoteSource && uDest.isLocalFile())) {
        permissions = -1;
    }
    const JobFlags flags = bOverwrite ? Overwrite : DefaultFlags;

    m_bCurrentOperationIsLink = false;
    KIO::Job *newjob = nullptr;
    if (m_mode == CopyJob::Link) {
        // User requested that a symlink be made
        newjob = linkNextFile(uSource, uDest, flags);
        if (!newjob) {
            return;
        }
    } else if (!(*it).linkDest.isEmpty() && compareUrls(uSource, uDest))
    // Copying a symlink - only on the same protocol/host/etc. (#5601, downloading an FTP file through its link),
    {
        KIO::SimpleJob *newJob = KIO::symlink((*it).linkDest, uDest, flags | HideProgressInfo /*no GUI*/);
        newJob->setParentJob(q);
        newjob = newJob;
        qCDebug(KIO_COPYJOB_DEBUG) << "Linking target=" << (*it).linkDest << "link=" << uDest;
        m_currentSrcURL = QUrl::fromUserInput((*it).linkDest);
        m_currentDestURL = uDest;
        m_bURLDirty = true;
        // emit linking( this, (*it).linkDest, uDest );
        // Observer::self()->slotCopying( this, m_currentSrcURL, uDest ); // should be slotLinking perhaps
        m_bCurrentOperationIsLink = true;
        // NOTE: if we are moving stuff, the deletion of the source will be done in slotResultCopyingFiles
    } else if (m_mode == CopyJob::Move) { // Moving a file
        KIO::FileCopyJob *moveJob = KIO::file_move(uSource, uDest, permissions, flags | HideProgressInfo /*no GUI*/);
        moveJob->setParentJob(q);
        moveJob->setSourceSize((*it).size);
        moveJob->setModificationTime((*it).mtime); // #55804
        newjob = moveJob;
        qCDebug(KIO_COPYJOB_DEBUG) << "Moving" << uSource << "to" << uDest;
        // emit moving( this, uSource, uDest );
        m_currentSrcURL = uSource;
        m_currentDestURL = uDest;
        m_bURLDirty = true;
        // Observer::self()->slotMoving( this, uSource, uDest );
    } else { // Copying a file
        KIO::FileCopyJob *copyJob = KIO::file_copy(uSource, uDest, permissions, flags | HideProgressInfo /*no GUI*/);
        copyJob->setParentJob(q); // in case of rename dialog
        copyJob->setSourceSize((*it).size);
        copyJob->setModificationTime((*it).mtime);
        newjob = copyJob;
        qCDebug(KIO_COPYJOB_DEBUG) << "Copying" << uSource << "to" << uDest;
        m_currentSrcURL = uSource;
        m_currentDestURL = uDest;
        m_bURLDirty = true;
    }
    q->addSubjob(newjob);
    q->connect(newjob, &Job::processedSize, q, [this](KJob *job, qulonglong processedSize) {
        slotProcessedSize(job, processedSize);
    });
    q->connect(newjob, &Job::totalSize, q, [this](KJob *job, qulonglong totalSize) {
        slotTotalSize(job, totalSize);
    });
}

void CopyJobPrivate::deleteNextDir()
{
    Q_Q(CopyJob);
    if (m_mode == CopyJob::Move && !dirsToRemove.isEmpty()) { // some dirs to delete ?
        state = STATE_DELETING_DIRS;
        m_bURLDirty = true;
        // Take first dir to delete out of list - last ones first !
        QList<QUrl>::Iterator it = --dirsToRemove.end();
        SimpleJob *job = KIO::rmdir(*it);
        job->setParentJob(q);
        dirsToRemove.erase(it);
        q->addSubjob(job);
    } else {
        // This step is done, move on
        state = STATE_SETTING_DIR_ATTRIBUTES;
        m_directoriesCopiedIterator = m_directoriesCopied.cbegin();
        setNextDirAttribute();
    }
}

void CopyJobPrivate::setNextDirAttribute()
{
    Q_Q(CopyJob);
    while (m_directoriesCopiedIterator != m_directoriesCopied.cend() && !(*m_directoriesCopiedIterator).mtime.isValid()) {
        ++m_directoriesCopiedIterator;
    }
    if (m_directoriesCopiedIterator != m_directoriesCopied.cend()) {
        const QUrl url = (*m_directoriesCopiedIterator).uDest;
        const QDateTime dt = (*m_directoriesCopiedIterator).mtime;
        ++m_directoriesCopiedIterator;

        KIO::SimpleJob *job = KIO::setModificationTime(url, dt);
        job->setParentJob(q);
        q->addSubjob(job);
    } else {
        if (m_reportTimer) {
            m_reportTimer->stop();
        }

        q->emitResult();
    }
}

void CopyJob::emitResult()
{
    Q_D(CopyJob);
    // Before we go, tell the world about the changes that were made.
    // Even if some error made us abort midway, we might still have done
    // part of the job so we better update the views! (#118583)
    if (!d->m_bOnlyRenames) {
        // If only renaming happened, KDirNotify::FileRenamed was emitted by the rename jobs
        QUrl url(d->m_globalDest);
        if (d->m_globalDestinationState != DEST_IS_DIR || d->m_asMethod) {
            url = url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
        }
        qCDebug(KIO_COPYJOB_DEBUG) << "KDirNotify'ing FilesAdded" << url;
#ifndef KIO_ANDROID_STUB
        org::kde::KDirNotify::emitFilesAdded(url);
#endif

        if (d->m_mode == CopyJob::Move && !d->m_successSrcList.isEmpty()) {
            qCDebug(KIO_COPYJOB_DEBUG) << "KDirNotify'ing FilesRemoved" << d->m_successSrcList;
#ifndef KIO_ANDROID_STUB
            org::kde::KDirNotify::emitFilesRemoved(d->m_successSrcList);
#endif
        }
    }

    // Re-enable watching on the dirs that held the deleted/moved files
    if (d->m_mode == CopyJob::Move) {
        for (const auto &dir : d->m_parentDirs) {
            KDirWatch::self()->restartDirScan(dir);
        }
    }
    Job::emitResult();
}

void CopyJobPrivate::slotProcessedSize(KJob *, qulonglong data_size)
{
    Q_Q(CopyJob);
    qCDebug(KIO_COPYJOB_DEBUG) << data_size;
    m_fileProcessedSize = data_size;

    if (m_processedSize + m_fileProcessedSize > m_totalSize) {
        // Example: download any attachment from bugs.kde.org
        m_totalSize = m_processedSize + m_fileProcessedSize;
        qCDebug(KIO_COPYJOB_DEBUG) << "Adjusting m_totalSize to" << m_totalSize;
        q->setTotalAmount(KJob::Bytes, m_totalSize); // safety
    }
    qCDebug(KIO_COPYJOB_DEBUG) << "emit processedSize" << (unsigned long)(m_processedSize + m_fileProcessedSize);
}

void CopyJobPrivate::slotTotalSize(KJob *, qulonglong size)
{
    Q_Q(CopyJob);
    qCDebug(KIO_COPYJOB_DEBUG) << size;
    // Special case for copying a single file
    // This is because some protocols don't implement stat properly
    // (e.g. HTTP), and don't give us a size in some cases (redirection)
    // so we'd rather rely on the size given for the transfer
    if (m_bSingleFileCopy && size != m_totalSize) {
        qCDebug(KIO_COPYJOB_DEBUG) << "slotTotalSize: updating totalsize to" << size;
        m_totalSize = size;
        q->setTotalAmount(KJob::Bytes, size);
    }
}

void CopyJobPrivate::slotResultDeletingDirs(KJob *job)
{
    Q_Q(CopyJob);
    if (job->error()) {
        // Couldn't remove directory. Well, perhaps it's not empty
        // because the user pressed Skip for a given file in it.
        // Let's not display "Could not remove dir ..." for each of those dir !
    } else {
        m_successSrcList.append(static_cast<KIO::SimpleJob *>(job)->url());
    }
    q->removeSubjob(job);
    Q_ASSERT(!q->hasSubjobs());
    deleteNextDir();
}

void CopyJobPrivate::slotResultSettingDirAttributes(KJob *job)
{
    Q_Q(CopyJob);
    if (job->error()) {
        // Couldn't set directory attributes. Ignore the error, it can happen
        // with inferior file systems like VFAT.
        // Let's not display warnings for each dir like "cp -a" does.
    }
    q->removeSubjob(job);
    Q_ASSERT(!q->hasSubjobs());
    setNextDirAttribute();
}

void CopyJobPrivate::directRenamingFailed(const QUrl &dest)
{
    Q_Q(CopyJob);

    qCDebug(KIO_COPYJOB_DEBUG) << "Couldn't rename" << m_currentSrcURL << "to" << dest << ", reverting to normal way, starting with stat";
    qCDebug(KIO_COPYJOB_DEBUG) << "KIO::stat on" << m_currentSrcURL;

    KIO::Job *job = KIO::statDetails(m_currentSrcURL, StatJob::SourceSide, KIO::StatDefaultDetails, KIO::HideProgressInfo);
    state = STATE_STATING;
    q->addSubjob(job);
    m_bOnlyRenames = false;
}

// We were trying to do a direct renaming, before even stat'ing
void CopyJobPrivate::slotResultRenaming(KJob *job)
{
    Q_Q(CopyJob);
    int err = job->error();
    const QString errText = job->errorText();
    // Merge metadata from subjob
    KIO::Job *kiojob = qobject_cast<KIO::Job *>(job);
    Q_ASSERT(kiojob);
    m_incomingMetaData += kiojob->metaData();
    q->removeSubjob(job);
    Q_ASSERT(!q->hasSubjobs());
    // Determine dest again
    QUrl dest = m_dest;
    if (destinationState == DEST_IS_DIR && !m_asMethod) {
        dest = addPathToUrl(dest, m_currentSrcURL.fileName());
    }
    auto *askUserActionInterface = KIO::delegateExtension<KIO::AskUserActionInterface *>(q);

    if (err) {
        // This code is similar to CopyJobPrivate::slotResultErrorCopyingFiles
        // but here it's about the base src url being moved/renamed
        // (m_currentSrcURL) and its dest (m_dest), not about a single file.
        // It also means we already stated the dest, here.
        // On the other hand we haven't stated the src yet (we skipped doing it
        // to save time, since it's not necessary to rename directly!)...

        // Existing dest?
        if (err == ERR_DIR_ALREADY_EXIST || err == ERR_FILE_ALREADY_EXIST || err == ERR_IDENTICAL_FILES) {
            // Should we skip automatically ?
            bool isDir = (err == ERR_DIR_ALREADY_EXIST); // ## technically, isDir means "source is dir", not "dest is dir" #######
            if ((isDir && m_bAutoSkipDirs) || (!isDir && m_bAutoSkipFiles)) {
                // Move on to next source url
                ++m_filesHandledByDirectRename;
                skipSrc(isDir);
                return;
            } else if ((isDir && m_bOverwriteAllDirs) || (!isDir && m_bOverwriteAllFiles)) {
                ; // nothing to do, stat+copy+del will overwrite
            } else if ((isDir && m_bAutoRenameDirs) || (!isDir && m_bAutoRenameFiles)) {
                QUrl destDirectory = m_currentDestURL.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash); // m_currendDestURL includes filename
                const QString newName = KFileUtils::suggestName(destDirectory, m_currentDestURL.fileName());

                m_dest = destDirectory;
                m_dest.setPath(concatPaths(m_dest.path(), newName));
                Q_EMIT q->renamed(q, dest, m_dest);
                KIO::Job *job = KIO::statDetails(m_dest, StatJob::DestinationSide, KIO::StatDefaultDetails, KIO::HideProgressInfo);
                state = STATE_STATING;
                destinationState = DEST_NOT_STATED;
                q->addSubjob(job);
                return;
            } else if (askUserActionInterface) {
                // we lack mtime info for both the src (not stated)
                // and the dest (stated but this info wasn't stored)
                // Let's do it for local files, at least
                KIO::filesize_t sizeSrc = KIO::invalidFilesize;
                KIO::filesize_t sizeDest = KIO::invalidFilesize;
                QDateTime ctimeSrc;
                QDateTime ctimeDest;
                QDateTime mtimeSrc;
                QDateTime mtimeDest;

                bool destIsDir = err == ERR_DIR_ALREADY_EXIST;

                // ## TODO we need to stat the source using KIO::stat
                // so that this code is properly network-transparent.

                if (m_currentSrcURL.isLocalFile()) {
                    QFileInfo info(m_currentSrcURL.toLocalFile());
                    if (info.exists()) {
                        sizeSrc = info.size();
                        ctimeSrc = info.birthTime();
                        mtimeSrc = info.lastModified();
                        isDir = info.isDir();
                    }
                }
                if (dest.isLocalFile()) {
                    QFileInfo destInfo(dest.toLocalFile());
                    if (destInfo.exists()) {
                        sizeDest = destInfo.size();
                        ctimeDest = destInfo.birthTime();
                        mtimeDest = destInfo.lastModified();
                        destIsDir = destInfo.isDir();
                    }
                }

                // If src==dest, use "overwrite-itself"
                RenameDialog_Options options = (m_currentSrcURL == dest) ? RenameDialog_OverwriteItself : RenameDialog_Overwrite;
                if (!isDir && destIsDir) {
                    // We can't overwrite a dir with a file.
                    options = RenameDialog_Options();
                }

                if (m_srcList.count() > 1) {
                    options |= RenameDialog_Options(RenameDialog_MultipleItems | RenameDialog_Skip);
                }

                if (destIsDir) {
                    options |= RenameDialog_DestIsDirectory;
                }

                if (m_reportTimer) {
                    m_reportTimer->stop();
                }

                RenameDialog_Result r;
                if (m_bOverwriteWhenOlder && mtimeSrc.isValid() && mtimeDest.isValid()) {
                    if (mtimeSrc > mtimeDest) {
                        qCDebug(KIO_COPYJOB_DEBUG) << "dest is older, overwriting" << dest;
                        r = Result_Overwrite;
                    } else {
                        qCDebug(KIO_COPYJOB_DEBUG) << "dest is newer, skipping" << dest;
                        r = Result_Skip;
                    }

                    processDirectRenamingConflictResult(r, isDir, destIsDir, mtimeSrc, mtimeDest, dest, QUrl{});
                    return;
                } else {
                    auto renameSignal = &KIO::AskUserActionInterface::askUserRenameResult;
                    QObject::connect(askUserActionInterface, renameSignal, q, [=](RenameDialog_Result result, const QUrl &newUrl, KJob *parentJob) {
                        Q_ASSERT(parentJob == q);
                        // Only receive askUserRenameResult once per rename dialog
                        QObject::disconnect(askUserActionInterface, renameSignal, q, nullptr);

                        processDirectRenamingConflictResult(result, isDir, destIsDir, mtimeSrc, mtimeDest, dest, newUrl);
                    });

                    const QString caption = err != ERR_DIR_ALREADY_EXIST ? i18n("File Already Exists") : i18n("Already Exists as Folder");

                    /* clang-format off */
                    askUserActionInterface->askUserRename(q, caption,
                                                          m_currentSrcURL, dest,
                                                          options,
                                                          sizeSrc, sizeDest,
                                                          ctimeSrc, ctimeDest,
                                                          mtimeSrc, mtimeDest);
                    /* clang-format on */

                    return;
                }
            } else if (err != KIO::ERR_UNSUPPORTED_ACTION) {
                // Dest already exists, and job is not interactive -> abort with error
                q->setError(err);
                q->setErrorText(errText);
                q->emitResult();
                return;
            }
        } else if (err != KIO::ERR_UNSUPPORTED_ACTION) {
            qCDebug(KIO_COPYJOB_DEBUG) << "Couldn't rename" << m_currentSrcURL << "to" << dest << ", aborting";
            q->setError(err);
            q->setErrorText(errText);
            q->emitResult();
            return;
        }

        directRenamingFailed(dest);
        return;
    }

    // No error
    qCDebug(KIO_COPYJOB_DEBUG) << "Renaming succeeded, move on";
    ++m_processedFiles;
    ++m_filesHandledByDirectRename;
    // Emit copyingDone for FileUndoManager to remember what we did.
    // Use resolved URL m_currentSrcURL since that's what we just used for renaming. See bug 391606 and kio_desktop's testTrashAndUndo().
    const bool srcIsDir = false; // # TODO: we just don't know, since we never stat'ed it
    Q_EMIT q->copyingDone(q, m_currentSrcURL, finalDestUrl(m_currentSrcURL, dest), QDateTime() /*mtime unknown, and not needed*/, srcIsDir, true);
    m_successSrcList.append(*m_currentStatSrc);
    statNextSrc();
}

void CopyJobPrivate::processDirectRenamingConflictResult(RenameDialog_Result result,
                                                         bool srcIsDir,
                                                         bool destIsDir,
                                                         const QDateTime &mtimeSrc,
                                                         const QDateTime &mtimeDest,
                                                         const QUrl &dest,
                                                         const QUrl &newUrl)
{
    Q_Q(CopyJob);

    if (m_reportTimer) {
        m_reportTimer->start(s_reportTimeout);
    }

    if (result == Result_OverwriteWhenOlder) {
        m_bOverwriteWhenOlder = true;
        if (mtimeSrc > mtimeDest) {
            qCDebug(KIO_COPYJOB_DEBUG) << "dest is older, overwriting" << dest;
            result = Result_Overwrite;
        } else {
            qCDebug(KIO_COPYJOB_DEBUG) << "dest is newer, skipping" << dest;
            result = Result_Skip;
        }
    }

    switch (result) {
    case Result_Cancel: {
        q->setError(ERR_USER_CANCELED);
        q->emitResult();
        return;
    }
    case Result_AutoRename:
        if (srcIsDir) {
            m_bAutoRenameDirs = true;
        } else {
            m_bAutoRenameFiles = true;
        }
        // fall through
        Q_FALLTHROUGH();
    case Result_Rename: {
        // Set m_dest to the chosen destination
        // This is only for this src url; the next one will revert to m_globalDest
        m_dest = newUrl;
        Q_EMIT q->renamed(q, dest, m_dest); // For e.g. KPropertiesDialog
        KIO::Job *job = KIO::statDetails(m_dest, StatJob::DestinationSide, KIO::StatDefaultDetails, KIO::HideProgressInfo);
        state = STATE_STATING;
        destinationState = DEST_NOT_STATED;
        q->addSubjob(job);
        return;
    }
    case Result_AutoSkip:
        if (srcIsDir) {
            m_bAutoSkipDirs = true;
        } else {
            m_bAutoSkipFiles = true;
        }
        // fall through
        Q_FALLTHROUGH();
    case Result_Skip:
        // Move on to next url
        ++m_filesHandledByDirectRename;
        skipSrc(srcIsDir);
        return;
    case Result_OverwriteAll:
        if (destIsDir) {
            m_bOverwriteAllDirs = true;
        } else {
            m_bOverwriteAllFiles = true;
        }
        break;
    case Result_Overwrite:
        // Add to overwrite list
        // Note that we add dest, not m_dest.
        // This ensures that when moving several urls into a dir (m_dest),
        // we only overwrite for the current one, not for all.
        // When renaming a single file (m_asMethod), it makes no difference.
        qCDebug(KIO_COPYJOB_DEBUG) << "adding to overwrite list: " << dest.path();
        m_overwriteList.insert(dest.path());
        break;
    default:
        // Q_ASSERT( 0 );
        break;
    }

    directRenamingFailed(dest);
}

void CopyJob::slotResult(KJob *job)
{
    Q_D(CopyJob);
    qCDebug(KIO_COPYJOB_DEBUG) << "d->state=" << (int)d->state;
    // In each case, what we have to do is :
    // 1 - check for errors and treat them
    // 2 - removeSubjob(job);
    // 3 - decide what to do next

    switch (d->state) {
    case STATE_STATING: // We were trying to stat a src url or the dest
        d->slotResultStating(job);
        break;
    case STATE_RENAMING: { // We were trying to do a direct renaming, before even stat'ing
        d->slotResultRenaming(job);
        break;
    }
    case STATE_LISTING: // recursive listing finished
        qCDebug(KIO_COPYJOB_DEBUG) << "totalSize:" << (unsigned int)d->m_totalSize << "files:" << d->files.count() << "d->dirs:" << d->dirs.count();
        // Was there an error ?
        if (job->error()) {
            Job::slotResult(job); // will set the error and emit result(this)
            return;
        }

        removeSubjob(job);
        Q_ASSERT(!hasSubjobs());

        d->statNextSrc();
        break;
    case STATE_CREATING_DIRS:
        d->slotResultCreatingDirs(job);
        break;
    case STATE_CONFLICT_CREATING_DIRS:
        d->slotResultConflictCreatingDirs(job);
        break;
    case STATE_COPYING_FILES:
        d->slotResultCopyingFiles(job);
        break;
    case STATE_CONFLICT_COPYING_FILES:
        d->slotResultErrorCopyingFiles(job);
        break;
    case STATE_DELETING_DIRS:
        d->slotResultDeletingDirs(job);
        break;
    case STATE_SETTING_DIR_ATTRIBUTES:
        d->slotResultSettingDirAttributes(job);
        break;
    default:
        Q_ASSERT(0);
    }
}

void KIO::CopyJob::setDefaultPermissions(bool b)
{
    d_func()->m_defaultPermissions = b;
}

KIO::CopyJob::CopyMode KIO::CopyJob::operationMode() const
{
    return d_func()->m_mode;
}

void KIO::CopyJob::setAutoSkip(bool autoSkip)
{
    d_func()->m_bAutoSkipFiles = autoSkip;
    d_func()->m_bAutoSkipDirs = autoSkip;
}

void KIO::CopyJob::setAutoRename(bool autoRename)
{
    d_func()->m_bAutoRenameFiles = autoRename;
    d_func()->m_bAutoRenameDirs = autoRename;
}

void KIO::CopyJob::setWriteIntoExistingDirectories(bool overwriteAll) // #65926
{
    d_func()->m_bOverwriteAllDirs = overwriteAll;
}

CopyJob *KIO::copy(const QUrl &src, const QUrl &dest, JobFlags flags)
{
    qCDebug(KIO_COPYJOB_DEBUG) << "src=" << src << "dest=" << dest;
    QList<QUrl> srcList;
    srcList.append(src);
    return CopyJobPrivate::newJob(srcList, dest, CopyJob::Copy, false, flags);
}

CopyJob *KIO::copyAs(const QUrl &src, const QUrl &dest, JobFlags flags)
{
    qCDebug(KIO_COPYJOB_DEBUG) << "src=" << src << "dest=" << dest;
    QList<QUrl> srcList;
    srcList.append(src);
    return CopyJobPrivate::newJob(srcList, dest, CopyJob::Copy, true, flags);
}

CopyJob *KIO::copy(const QList<QUrl> &src, const QUrl &dest, JobFlags flags)
{
    qCDebug(KIO_COPYJOB_DEBUG) << src << dest;
    return CopyJobPrivate::newJob(src, dest, CopyJob::Copy, false, flags);
}

CopyJob *KIO::move(const QUrl &src, const QUrl &dest, JobFlags flags)
{
    qCDebug(KIO_COPYJOB_DEBUG) << src << dest;
    QList<QUrl> srcList;
    srcList.append(src);
    CopyJob *job = CopyJobPrivate::newJob(srcList, dest, CopyJob::Move, false, flags);
    if (job->uiDelegateExtension()) {
        job->uiDelegateExtension()->createClipboardUpdater(job, JobUiDelegateExtension::UpdateContent);
    }
    return job;
}

CopyJob *KIO::moveAs(const QUrl &src, const QUrl &dest, JobFlags flags)
{
    qCDebug(KIO_COPYJOB_DEBUG) << src << dest;
    QList<QUrl> srcList;
    srcList.append(src);
    CopyJob *job = CopyJobPrivate::newJob(srcList, dest, CopyJob::Move, true, flags);
    if (job->uiDelegateExtension()) {
        job->uiDelegateExtension()->createClipboardUpdater(job, JobUiDelegateExtension::UpdateContent);
    }
    return job;
}

CopyJob *KIO::move(const QList<QUrl> &src, const QUrl &dest, JobFlags flags)
{
    qCDebug(KIO_COPYJOB_DEBUG) << src << dest;
    CopyJob *job = CopyJobPrivate::newJob(src, dest, CopyJob::Move, false, flags);
    if (job->uiDelegateExtension()) {
        job->uiDelegateExtension()->createClipboardUpdater(job, JobUiDelegateExtension::UpdateContent);
    }
    return job;
}

CopyJob *KIO::link(const QUrl &src, const QUrl &destDir, JobFlags flags)
{
    QList<QUrl> srcList;
    srcList.append(src);
    return CopyJobPrivate::newJob(srcList, destDir, CopyJob::Link, false, flags);
}

CopyJob *KIO::link(const QList<QUrl> &srcList, const QUrl &destDir, JobFlags flags)
{
    return CopyJobPrivate::newJob(srcList, destDir, CopyJob::Link, false, flags);
}

CopyJob *KIO::linkAs(const QUrl &src, const QUrl &destDir, JobFlags flags)
{
    QList<QUrl> srcList;
    srcList.append(src);
    return CopyJobPrivate::newJob(srcList, destDir, CopyJob::Link, true, flags);
}

CopyJob *KIO::trash(const QUrl &src, JobFlags flags)
{
    QList<QUrl> srcList;
    srcList.append(src);
    return CopyJobPrivate::newJob(srcList, QUrl(QStringLiteral("trash:/")), CopyJob::Move, false, flags);
}

CopyJob *KIO::trash(const QList<QUrl> &srcList, JobFlags flags)
{
    return CopyJobPrivate::newJob(srcList, QUrl(QStringLiteral("trash:/")), CopyJob::Move, false, flags);
}

#include "moc_copyjob.cpp"
