/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Smith <dsmith@algonet.se>
    SPDX-FileCopyrightText: 2004 Scott Wheeler <wheeler@kde.org>

    This class was inspired by a previous KUrlCompletion by
    SPDX-FileContributor: Henner Zeller <zeller@think.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurlcompletion.h"
#include "../pathhelpers_p.h" // isAbsoluteLocalPath()

#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include <QMutex>
#include <QRegularExpression>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QThread>
#include <QUrl>
#include <QDebug>
#include <QMimeDatabase>
#include <QProcessEnvironment>
#include <qplatformdefs.h> // QT_LSTAT, QT_STAT, QT_STATBUF

#include <kurlauthorized.h>
#include <kio/job.h>
#include <kprotocolmanager.h>
#include <KConfig>
#include <KSharedConfig>
#include <kioglobal_p.h>
#include <KUser>
#include <kio_widgets_debug.h>

#include <time.h>
#include <KConfigGroup>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#else
#include <sys/param.h>
#include <pwd.h>
#endif

static bool expandTilde(QString &);
static bool expandEnv(QString &);

static QString unescape(const QString &text);

// Permission mask for files that are executable by
// user, group or other
#define MODE_EXE (S_IXUSR | S_IXGRP | S_IXOTH)

// Constants for types of completion
enum ComplType {CTNone = 0, CTEnv, CTUser, CTMan, CTExe, CTFile, CTUrl, CTInfo};

class CompletionThread;

// Ensure that we don't end up with "//".
static QUrl addPathToUrl(const QUrl &url, const QString &relPath)
{
    QUrl u(url);
    u.setPath(concatPaths(url.path(), relPath));
    return u;
}


static QBasicAtomicInt s_waitDuration = Q_BASIC_ATOMIC_INITIALIZER(-1);

static int initialWaitDuration()
{
    if (s_waitDuration.loadRelaxed() == -1) {
        const QByteArray envVar = qgetenv("KURLCOMPLETION_WAIT");
        if (envVar.isEmpty()) {
            s_waitDuration = 200; // default: 200 ms
        } else {
            s_waitDuration = envVar.toInt();
        }
    }
    return s_waitDuration;
}

///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
// KUrlCompletionPrivate
//
class KUrlCompletionPrivate
{
public:
    explicit KUrlCompletionPrivate(KUrlCompletion *parent)
        : q(parent),
          url_auto_completion(true),
          userListThread(nullptr),
          dirListThread(nullptr)
    {
    }

    ~KUrlCompletionPrivate();

    void _k_slotEntries(KIO::Job *, const KIO::UDSEntryList &);
    void _k_slotIOFinished(KJob *);
    void slotCompletionThreadDone(QThread *thread, const QStringList &matches);

    class MyURL;
    bool userCompletion(const MyURL &url, QString *match);
    bool envCompletion(const MyURL &url, QString *match);
    bool exeCompletion(const MyURL &url, QString *match);
    bool fileCompletion(const MyURL &url, QString *match);
    bool urlCompletion(const MyURL &url, QString *match);

    bool isAutoCompletion();

    // List the next dir in m_dirs
    QString listDirectories(const QStringList &,
                            const QString &,
                            bool only_exe = false,
                            bool only_dir = false,
                            bool no_hidden = false,
                            bool stat_files = true);

    void listUrls(const QList<QUrl> &urls,
                  const QString &filter = QString(),
                  bool only_exe = false,
                  bool no_hidden = false);

    void addMatches(const QStringList &);
    QString finished();

    void init();

    void setListedUrl(ComplType compl_type,
                      const QString &dir = QString(),
                      const QString &filter = QString(),
                      bool no_hidden = false);

    bool isListedUrl(ComplType compl_type,
                     const QString &dir = QString(),
                     const QString &filter = QString(),
                     bool no_hidden = false);

    KUrlCompletion * const q;
    QList<QUrl> list_urls;

    bool onlyLocalProto;

    // urlCompletion() in Auto/Popup mode?
    bool url_auto_completion;

    // Append '/' to directories in Popup mode?
    // Doing that stat's all files and is slower
    bool popup_append_slash;

    // Keep track of currently listed files to avoid reading them again
    bool last_no_hidden;
    QString last_path_listed;
    QString last_file_listed;
    QString last_prepend;
    ComplType last_compl_type;

    QUrl cwd; // "current directory" = base dir for completion

    KUrlCompletion::Mode mode; // ExeCompletion, FileCompletion, DirCompletion
    bool replace_env;
    bool replace_home;
    bool complete_url; // if true completing a URL (i.e. 'prepend' is a URL), otherwise a path

    KIO::ListJob *list_job; // kio job to list directories

    QString prepend; // text to prepend to listed items
    QString compl_text; // text to pass on to KCompletion

    // Filters for files read with  kio
    bool list_urls_only_exe; // true = only list executables
    bool list_urls_no_hidden;
    QString list_urls_filter; // filter for listed files

    CompletionThread *userListThread;
    CompletionThread *dirListThread;

    QStringList mimeTypeFilters;
};

class CompletionThread : public QThread
{
    Q_OBJECT
protected:
    CompletionThread(KUrlCompletionPrivate *receiver) :
        QThread(),
        m_prepend(receiver->prepend),
        m_complete_url(receiver->complete_url),
        m_terminationRequested(false)
    {}

public:
    void requestTermination()
    {
        if (!isFinished()) {
            qCDebug(KIO_WIDGETS) << "stopping thread" << this;
        }
        m_terminationRequested.storeRelaxed(true);
        wait();
    }

    QStringList matches() const
    {
        QMutexLocker locker(&m_mutex);
        return m_matches;
    }

Q_SIGNALS:
    void completionThreadDone(QThread *thread, const QStringList &matches);

protected:
    void addMatch(const QString &match)
    {
        QMutexLocker locker(&m_mutex);
        m_matches.append(match);
    }
    bool terminationRequested() const
    {
        return m_terminationRequested.loadRelaxed();
    }
    void done()
    {
        if (!terminationRequested()) {
            qCDebug(KIO_WIDGETS) << "done, emitting signal with" << m_matches.count() << "matches";
            Q_EMIT completionThreadDone(this, m_matches);
        }
    }

    const QString m_prepend;
    const bool m_complete_url; // if true completing a URL (i.e. 'm_prepend' is a URL), otherwise a path

private:
    mutable QMutex m_mutex; // protects m_matches
    QStringList m_matches; // written by secondary thread, read by the matches() method
    QAtomicInt m_terminationRequested; // used as a bool
};

/**
 * A simple thread that fetches a list of tilde-completions and returns this
 * to the caller via the completionThreadDone signal.
 */

class UserListThread : public CompletionThread
{
    Q_OBJECT
public:
    UserListThread(KUrlCompletionPrivate *receiver) :
        CompletionThread(receiver)
    {}

protected:
    void run() override
    {
#ifndef Q_OS_ANDROID
        const QChar tilde = QLatin1Char('~');

        // we don't need to handle prepend here, right? ~user is always at pos 0
        assert(m_prepend.isEmpty());
#pragma message("TODO: add KUser::allUserNames() with a std::function<bool()> shouldTerminate parameter")
#ifndef Q_OS_WIN
        struct passwd *pw;
        ::setpwent();
        while ((pw = ::getpwent()) && !terminationRequested()) {
            addMatch(tilde + QString::fromLocal8Bit(pw->pw_name));
        }
        ::endpwent();
#else
        //currently terminationRequested is ignored on Windows
        const QStringList allUsers = KUser::allUserNames();
        for (const QString& s : allUsers) {
            addMatch(tilde + s);
        }
#endif
        addMatch(QString(tilde));
#endif
        done();
    }
};

class DirectoryListThread : public CompletionThread
{
    Q_OBJECT
public:
    DirectoryListThread(KUrlCompletionPrivate *receiver,
                        const QStringList &dirList,
                        const QString &filter,
                        const QStringList &mimeTypeFilters,
                        bool onlyExe,
                        bool onlyDir,
                        bool noHidden,
                        bool appendSlashToDir) :
        CompletionThread(receiver),
        m_dirList(dirList),
        m_filter(filter),
        m_mimeTypeFilters(mimeTypeFilters),
        m_onlyExe(onlyExe),
        m_onlyDir(onlyDir),
        m_noHidden(noHidden),
        m_appendSlashToDir(appendSlashToDir)
    {}

    void run() override;

private:
    QStringList m_dirList;
    QString m_filter;
    QStringList m_mimeTypeFilters;
    bool m_onlyExe;
    bool m_onlyDir;
    bool m_noHidden;
    bool m_appendSlashToDir;
};

void DirectoryListThread::run()
{
    //qDebug() << "Entered DirectoryListThread::run(), m_filter=" << m_filter << ", m_onlyExe=" << m_onlyExe << ", m_onlyDir=" << m_onlyDir << ", m_appendSlashToDir=" << m_appendSlashToDir << ", m_dirList.size()=" << m_dirList.size();

    QDir::Filters iterator_filter = (m_noHidden ? QDir::Filter(0) : QDir::Hidden) | QDir::Readable | QDir::NoDotAndDotDot;
    if (m_onlyExe) {
        iterator_filter |= (QDir::Dirs | QDir::Files | QDir::Executable);
    } else if (m_onlyDir) {
        iterator_filter |= QDir::Dirs;
    } else {
        iterator_filter |= (QDir::Dirs | QDir::Files);
    }

    QMimeDatabase mimeTypes;

    const QStringList::const_iterator end = m_dirList.constEnd();
    for (QStringList::const_iterator it = m_dirList.constBegin();
            it != end && !terminationRequested();
            ++it) {
        //qDebug() << "Scanning directory" << *it;

        QDirIterator current_dir_iterator(*it, iterator_filter);

        while (current_dir_iterator.hasNext() && !terminationRequested()) {
            current_dir_iterator.next();

            QFileInfo file_info = current_dir_iterator.fileInfo();
            const QString file_name = file_info.fileName();

            //qDebug() << "Found" << file_name;

            if (!m_filter.isEmpty() && !file_name.startsWith(m_filter)) {
                continue;
            }

            if (!m_mimeTypeFilters.isEmpty() && !file_info.isDir()) {
                auto mimeType = mimeTypes.mimeTypeForFile(file_info);
                if (!m_mimeTypeFilters.contains(mimeType.name())) {
                    continue;
                }
            }

            QString toAppend = file_name;
            // Add '/' to directories
            if (m_appendSlashToDir && file_info.isDir()) {
                toAppend.append(QLatin1Char('/'));
            }

            if (m_complete_url) {
                QUrl info(m_prepend);
                info = addPathToUrl(info, toAppend);
                addMatch(info.toDisplayString());
            } else {
                addMatch(m_prepend + toAppend);
            }
        }
    }

    done();
}

KUrlCompletionPrivate::~KUrlCompletionPrivate()
{
}

///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
// MyURL - wrapper for QUrl with some different functionality
//

class KUrlCompletionPrivate::MyURL
{
public:
    MyURL(const QString &url, const QUrl &cwd);
    MyURL(const MyURL &url);
    ~MyURL();

    QUrl kurl() const
    {
        return m_kurl;
    }

    bool isLocalFile() const
    {
        return m_kurl.isLocalFile();
    }
    QString scheme() const
    {
        return m_kurl.scheme();
    }
    // The directory with a trailing '/'
    QString dir() const
    {
        return m_kurl.adjusted(QUrl::RemoveFilename).path();
    }
    QString file() const
    {
        return m_kurl.fileName();
    }

    // The initial, unparsed, url, as a string.
    QString url() const
    {
        return m_url;
    }

    // Is the initial string a URL, or just a path (whether absolute or relative)
    bool isURL() const
    {
        return m_isURL;
    }

    void filter(bool replace_user_dir, bool replace_env);

private:
    void init(const QString &url, const QUrl &cwd);

    QUrl m_kurl;
    QString m_url;
    bool m_isURL;
};

KUrlCompletionPrivate::MyURL::MyURL(const QString &_url, const QUrl &cwd)
{
    init(_url, cwd);
}

KUrlCompletionPrivate::MyURL::MyURL(const MyURL &_url)
    : m_kurl(_url.m_kurl)
{
    m_url = _url.m_url;
    m_isURL = _url.m_isURL;
}

void KUrlCompletionPrivate::MyURL::init(const QString &_url, const QUrl &cwd)
{
    // Save the original text
    m_url = _url;

    // Non-const copy
    QString url_copy = _url;

    // Special shortcuts for "man:" and "info:"
    if (url_copy.startsWith(QLatin1Char('#'))) {
        if (url_copy.length() > 1 && url_copy.at(1) == QLatin1Char('#')) {
            url_copy.replace(0, 2, QStringLiteral("info:"));
        } else {
            url_copy.replace(0, 1, QStringLiteral("man:"));
        }
    }

    // Look for a protocol in 'url'
    const QRegularExpression protocol_regex(QStringLiteral("^(?![A-Za-z]:)[^/\\s\\\\]*:"));

    // Assume "file:" or whatever is given by 'cwd' if there is
    // no protocol.  (QUrl does this only for absolute paths)
    if (protocol_regex.match(url_copy).hasMatch()) {
        m_kurl = QUrl(url_copy);
        m_isURL = true;
    } else { // relative path or ~ or $something
        m_isURL = false;
        if (isAbsoluteLocalPath(url_copy) ||
                url_copy.startsWith(QLatin1Char('~')) ||
                url_copy.startsWith(QLatin1Char('$'))) {
            m_kurl = QUrl::fromLocalFile(url_copy);
        } else {
            // Relative path
            if (cwd.isEmpty()) {
                m_kurl = QUrl(url_copy);
            } else {
                m_kurl = cwd;
                m_kurl.setPath(concatPaths(m_kurl.path(), url_copy));
            }
        }
    }
}

KUrlCompletionPrivate::MyURL::~MyURL()
{
}

void KUrlCompletionPrivate::MyURL::filter(bool replace_user_dir, bool replace_env)
{
    QString d = dir() + file();
    if (replace_user_dir) {
        expandTilde(d);
    }
    if (replace_env) {
        expandEnv(d);
    }
    m_kurl.setPath(d);
}

///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
// KUrlCompletion
//

KUrlCompletion::KUrlCompletion() : KCompletion(), d(new KUrlCompletionPrivate(this))
{
    d->init();
}

KUrlCompletion::KUrlCompletion(Mode _mode)
    : KCompletion(),
      d(new KUrlCompletionPrivate(this))
{
    d->init();
    setMode(_mode);
}

KUrlCompletion::~KUrlCompletion()
{
    stop();
    delete d;
}

void KUrlCompletionPrivate::init()
{
    cwd = QUrl::fromLocalFile(QDir::homePath());

    replace_home = true;
    replace_env = true;
    last_no_hidden = false;
    last_compl_type = CTNone;
    list_job = nullptr;
    mode = KUrlCompletion::FileCompletion;

    // Read settings
    KConfigGroup cg(KSharedConfig::openConfig(), "URLCompletion");

    url_auto_completion = cg.readEntry("alwaysAutoComplete", true);
    popup_append_slash = cg.readEntry("popupAppendSlash", true);
    onlyLocalProto = cg.readEntry("LocalProtocolsOnly", false);

    q->setIgnoreCase(true);
}

void KUrlCompletion::setDir(const QUrl &dir)
{
    d->cwd = dir;
}

QUrl KUrlCompletion::dir() const
{
    return d->cwd;
}

KUrlCompletion::Mode KUrlCompletion::mode() const
{
    return d->mode;
}

void KUrlCompletion::setMode(Mode _mode)
{
    d->mode = _mode;
}

bool KUrlCompletion::replaceEnv() const
{
    return d->replace_env;
}

void KUrlCompletion::setReplaceEnv(bool replace)
{
    d->replace_env = replace;
}

bool KUrlCompletion::replaceHome() const
{
    return d->replace_home;
}

void KUrlCompletion::setReplaceHome(bool replace)
{
    d->replace_home = replace;
}

/*
 * makeCompletion()
 *
 * Entry point for file name completion
 */
QString KUrlCompletion::makeCompletion(const QString &text)
{
    qCDebug(KIO_WIDGETS) << text << "d->cwd=" << d->cwd;

    KUrlCompletionPrivate::MyURL url(text, d->cwd);

    d->compl_text = text;

    // Set d->prepend to the original URL, with the filename [and ref/query] stripped.
    // This is what gets prepended to the directory-listing matches.
    if (url.isURL()) {
        QUrl directoryUrl(url.kurl());
        directoryUrl.setQuery(QString());
        directoryUrl.setFragment(QString());
        directoryUrl.setPath(url.dir());
        d->prepend = directoryUrl.toString();
    } else {
        d->prepend = text.left(text.length() - url.file().length());
    }

    d->complete_url = url.isURL();

    QString aMatch;

    // Environment variables
    //
    if (d->replace_env && d->envCompletion(url, &aMatch)) {
        return aMatch;
    }

    // User directories
    //
    if (d->replace_home && d->userCompletion(url, &aMatch)) {
        return aMatch;
    }

    // Replace user directories and variables
    url.filter(d->replace_home, d->replace_env);

    //qDebug() << "Filtered: proto=" << url.scheme()
    //          << ", dir=" << url.dir()
    //          << ", file=" << url.file()
    //          << ", kurl url=" << *url.kurl();

    if (d->mode == ExeCompletion) {
        // Executables
        //
        if (d->exeCompletion(url, &aMatch)) {
            return aMatch;
        }

        // KRun can run "man:" and "info:" etc. so why not treat them
        // as executables...

        if (d->urlCompletion(url, &aMatch)) {
            return aMatch;
        }
    } else {
        // Local files, directories
        //
        if (d->fileCompletion(url, &aMatch)) {
            return aMatch;
        }

        // All other...
        //
        if (d->urlCompletion(url, &aMatch)) {
            return aMatch;
        }
    }

    d->setListedUrl(CTNone);
    stop();

    return QString();
}

/*
 * finished
 *
 * Go on and call KCompletion.
 * Called when all matches have been added
 */
QString KUrlCompletionPrivate::finished()
{
    if (last_compl_type == CTInfo) {
        return q->KCompletion::makeCompletion(compl_text.toLower());
    } else {
        return q->KCompletion::makeCompletion(compl_text);
    }
}

/*
 * isRunning
 *
 * Return true if either a KIO job or a thread is running
 */
bool KUrlCompletion::isRunning() const
{
    return d->list_job || (d->dirListThread && !d->dirListThread->isFinished()) || (d->userListThread && !d->userListThread->isFinished());
}

/*
 * stop
 *
 * Stop and delete a running KIO job or the DirLister
 */
void KUrlCompletion::stop()
{
    if (d->list_job) {
        d->list_job->kill();
        d->list_job = nullptr;
    }

    if (d->dirListThread) {
        d->dirListThread->requestTermination();
        delete d->dirListThread;
        d->dirListThread = nullptr;
    }

    if (d->userListThread) {
        d->userListThread->requestTermination();
        delete d->userListThread;
        d->userListThread = nullptr;
    }
}

/*
 * Keep track of the last listed directory
 */
void KUrlCompletionPrivate::setListedUrl(ComplType complType,
        const QString &directory,
        const QString &filter,
        bool no_hidden)
{
    last_compl_type = complType;
    last_path_listed = directory;
    last_file_listed = filter;
    last_no_hidden = no_hidden;
    last_prepend = prepend;
}

bool KUrlCompletionPrivate::isListedUrl(ComplType complType,
                                        const QString &directory,
                                        const QString &filter,
                                        bool no_hidden)
{
    return  last_compl_type == complType
            && (last_path_listed == directory
                || (directory.isEmpty() && last_path_listed.isEmpty()))
            && (filter.startsWith(last_file_listed)
                || (filter.isEmpty() && last_file_listed.isEmpty()))
            && last_no_hidden == no_hidden
            && last_prepend == prepend; // e.g. relative path vs absolute
}

/*
 * isAutoCompletion
 *
 * Returns true if completion mode is Auto or Popup
 */
bool KUrlCompletionPrivate::isAutoCompletion()
{
    return q->completionMode() == KCompletion::CompletionAuto
           || q->completionMode() == KCompletion::CompletionPopup
           || q->completionMode() == KCompletion::CompletionMan
           || q->completionMode() == KCompletion::CompletionPopupAuto;
}
//////////////////////////////////////////////////
//////////////////////////////////////////////////
// User directories
//

bool KUrlCompletionPrivate::userCompletion(const KUrlCompletionPrivate::MyURL &url, QString *pMatch)
{
    if (url.scheme() != QLatin1String("file")
            || !url.dir().isEmpty()
            || !url.file().startsWith(QLatin1Char('~'))
            || !prepend.isEmpty()) {
        return false;
    }

    if (!isListedUrl(CTUser)) {
        q->stop();
        q->clear();
        setListedUrl(CTUser);

        Q_ASSERT(!userListThread); // caller called stop()
        userListThread = new UserListThread(this);
        QObject::connect(userListThread, &CompletionThread::completionThreadDone,
                q, [this](QThread *thread, const QStringList &matches){ slotCompletionThreadDone(thread, matches); });
        userListThread->start();

        // If the thread finishes quickly make sure that the results
        // are added to the first matching case.

        userListThread->wait(initialWaitDuration());
        const QStringList l = userListThread->matches();
        addMatches(l);
    }
    *pMatch = finished();
    return true;
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// Environment variables
//

bool KUrlCompletionPrivate::envCompletion(const KUrlCompletionPrivate::MyURL &url, QString *pMatch)
{
    if (url.file().isEmpty() || url.file().at(0) != QLatin1Char('$')) {
        return false;
    }

    if (!isListedUrl(CTEnv)) {
        q->stop();
        q->clear();

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        const QStringList keys = env.keys();

        QStringList l;
        l.reserve(keys.size());
        for (const QString &key : keys) {
            l.append(prepend + QLatin1Char('$') + key);
        }

        addMatches(l);
    }

    setListedUrl(CTEnv);

    *pMatch = finished();
    return true;
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
// Executables
//

bool KUrlCompletionPrivate::exeCompletion(const KUrlCompletionPrivate::MyURL &url, QString *pMatch)
{
    if (!url.isLocalFile()) {
        return false;
    }

    QString directory = unescape(url.dir());  // remove escapes

    // Find directories to search for completions, either
    //
    // 1. complete path given in url
    // 2. current directory (d->cwd)
    // 3. $PATH
    // 4. no directory at all

    QStringList dirList;

    if (!url.file().isEmpty()) {
        // $PATH
        dirList = QString::fromLocal8Bit(qgetenv("PATH")).split(
                      QDir::listSeparator(), Qt::SkipEmptyParts);

        QStringList::Iterator it = dirList.begin();

        for (; it != dirList.end(); ++it) {
            it->append(QLatin1Char('/'));
        }
    } else if (isAbsoluteLocalPath(directory)) {
        // complete path in url
        dirList.append(directory);
    } else if (!directory.isEmpty() && !cwd.isEmpty()) {
        // current directory
        dirList.append(cwd.toLocalFile() + QLatin1Char('/') + directory);
    }

    // No hidden files unless the user types "."
    bool no_hidden_files = url.file().isEmpty() || url.file().at(0) != QLatin1Char('.');

    // List files if needed
    //
    if (!isListedUrl(CTExe, directory, url.file(), no_hidden_files)) {
        q->stop();
        q->clear();

        setListedUrl(CTExe, directory, url.file(), no_hidden_files);

        *pMatch = listDirectories(dirList, url.file(), true, false, no_hidden_files);
    } else {
        *pMatch = finished();
    }

    return true;
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
// Local files
//

bool KUrlCompletionPrivate::fileCompletion(const KUrlCompletionPrivate::MyURL &url, QString *pMatch)
{
    if (!url.isLocalFile()) {
        return false;
    }

    QString directory = unescape(url.dir());

    if (url.url() == QLatin1String("..")) {
        *pMatch = QStringLiteral("..");
        return true;
    }

    //qDebug() << "fileCompletion" << url << "dir=" << dir;

    // Find directories to search for completions, either
    //
    // 1. complete path given in url
    // 2. current directory (d->cwd)
    // 3. no directory at all

    QStringList dirList;

    if (isAbsoluteLocalPath(directory)) {
        // complete path in url
        dirList.append(directory);
    } else if (!cwd.isEmpty()) {
        // current directory
        QString dirToAdd = cwd.toLocalFile();
        if (!directory.isEmpty()) {
            if (!dirToAdd.endsWith(QLatin1Char('/'))) {
                dirToAdd.append(QLatin1Char('/'));
            }
            dirToAdd.append(directory);
        }
        dirList.append(dirToAdd);
    }

    // No hidden files unless the user types "."
    bool no_hidden_files = !url.file().startsWith(QLatin1Char('.'));

    // List files if needed
    //
    if (!isListedUrl(CTFile, directory, QString(), no_hidden_files)) {
        q->stop();
        q->clear();

        setListedUrl(CTFile, directory, QString(), no_hidden_files);

        // Append '/' to directories in Popup mode?
        bool append_slash = (popup_append_slash
                             && (q->completionMode() == KCompletion::CompletionPopup ||
                                 q->completionMode() == KCompletion::CompletionPopupAuto));

        bool only_dir = (mode == KUrlCompletion::DirCompletion);

        *pMatch = listDirectories(dirList, QString(), false, only_dir, no_hidden_files,
                                  append_slash);
    } else {
        *pMatch = finished();
    }

    return true;
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
// URLs not handled elsewhere...
//

static bool isLocalProtocol(const QString &protocol)
{
    return (KProtocolInfo::protocolClass(protocol) == QLatin1String(":local"));
}

bool KUrlCompletionPrivate::urlCompletion(const KUrlCompletionPrivate::MyURL &url, QString *pMatch)
{
    //qDebug() << *url.kurl();
    if (onlyLocalProto && isLocalProtocol(url.scheme())) {
        return false;
    }

    // Use d->cwd as base url in case url is not absolute
    QUrl url_dir = url.kurl();
    if (url_dir.isRelative() && !cwd.isEmpty()) {
        // Create an URL with the directory to be listed
        url_dir = cwd.resolved(url_dir);
    }

    // url is malformed
    if (!url_dir.isValid() || url.scheme().isEmpty()) {
        return false;
    }

    // non local urls
    if (!isLocalProtocol(url.scheme())) {
        // url does not specify host
        if (url_dir.host().isEmpty()) {
            return false;
        }

        // url does not specify a valid directory
        if (url_dir.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).path().isEmpty()) {
            return false;
        }

        // automatic completion is disabled
        if (isAutoCompletion() && !url_auto_completion) {
            return false;
        }
    }

    // url handler doesn't support listing
    if (!KProtocolManager::supportsListing(url_dir)) {
        return false;
    }

    // Remove escapes
    const QString directory = unescape(url_dir.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).path());
    url_dir.setPath(directory);

    // List files if needed
    //
    if (!isListedUrl(CTUrl, directory, url.file())) {
        q->stop();
        q->clear();

        setListedUrl(CTUrl, directory, QString());

        QList<QUrl> url_list;
        url_list.append(url_dir);

        listUrls(url_list, QString(), false);

        pMatch->clear();
    } else if (!q->isRunning()) {
        *pMatch = finished();
    } else {
        pMatch->clear();
    }

    return true;
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
// Directory and URL listing
//

/*
 * addMatches
 *
 * Called to add matches to KCompletion
 */
void KUrlCompletionPrivate::addMatches(const QStringList &matchList)
{
    q->insertItems(matchList);
}

/*
 * listDirectories
 *
 * List files starting with 'filter' in the given directories,
 * either using DirLister or listURLs()
 *
 * In either case, addMatches() is called with the listed
 * files, and eventually finished() when the listing is done
 *
 * Returns the match if available, or QString() if
 * DirLister timed out or using kio
 */
QString KUrlCompletionPrivate::listDirectories(
    const QStringList &dirList,
    const QString &filter,
    bool only_exe,
    bool only_dir,
    bool no_hidden,
    bool append_slash_to_dir)
{
    assert(!q->isRunning());

    if (qEnvironmentVariableIsEmpty("KURLCOMPLETION_LOCAL_KIO")) {

        qCDebug(KIO_WIDGETS) << "Listing directories:" << dirList << "with filter=" << filter << "using thread";

        // Don't use KIO

        QStringList dirs;

        QStringList::ConstIterator end = dirList.constEnd();
        for (QStringList::ConstIterator it = dirList.constBegin();
                it != end;
                ++it) {
            QUrl url = QUrl::fromLocalFile(*it);
            if (KUrlAuthorized::authorizeUrlAction(QStringLiteral("list"), QUrl(), url)) {
                dirs.append(*it);
            }
        }

        Q_ASSERT(!dirListThread); // caller called stop()
        dirListThread = new DirectoryListThread(this, dirs, filter, mimeTypeFilters,
                                                only_exe, only_dir, no_hidden,
                                                append_slash_to_dir);
        QObject::connect(dirListThread, &CompletionThread::completionThreadDone,
                q, [this](QThread *thread, const QStringList &matches){ slotCompletionThreadDone(thread, matches); });
        dirListThread->start();
        dirListThread->wait(initialWaitDuration());
        qCDebug(KIO_WIDGETS) << "Adding initial matches:" << dirListThread->matches();
        addMatches(dirListThread->matches());

        return finished();
    }

    // Use KIO
    //qDebug() << "Listing (listDirectories):" << dirList << "with KIO";

    QList<QUrl> url_list;

    QStringList::ConstIterator it = dirList.constBegin();
    QStringList::ConstIterator end = dirList.constEnd();

    url_list.reserve(dirList.size());
    for (; it != end; ++it) {
        url_list.append(QUrl(*it));
    }

    listUrls(url_list, filter, only_exe, no_hidden);
    // Will call addMatches() and finished()

    return QString();
}

/*
 * listURLs
 *
 * Use KIO to list the given urls
 *
 * addMatches() is called with the listed files
 * finished() is called when the listing is done
 */
void KUrlCompletionPrivate::listUrls(
    const QList<QUrl> &urls,
    const QString &filter,
    bool only_exe,
    bool no_hidden)
{
    assert(list_urls.isEmpty());
    assert(list_job == nullptr);

    list_urls = urls;
    list_urls_filter = filter;
    list_urls_only_exe = only_exe;
    list_urls_no_hidden = no_hidden;

    //qDebug() << "Listing URLs:" << *urls[0] << ",...";

    // Start it off by calling _k_slotIOFinished
    //
    // This will start a new list job as long as there
    // are urls in d->list_urls
    //
    _k_slotIOFinished(nullptr);
}

/*
 * _k_slotEntries
 *
 * Receive files listed by KIO and call addMatches()
 */
void KUrlCompletionPrivate::_k_slotEntries(KIO::Job *, const KIO::UDSEntryList &entries)
{
    QStringList matchList;

    KIO::UDSEntryList::ConstIterator it = entries.constBegin();
    const KIO::UDSEntryList::ConstIterator end = entries.constEnd();

    QString filter = list_urls_filter;

    int filter_len = filter.length();

    // Iterate over all files
    //
    for (; it != end; ++it) {
        const KIO::UDSEntry &entry = *it;
        const QString url = entry.stringValue(KIO::UDSEntry::UDS_URL);

        QString entry_name;
        if (!url.isEmpty()) {
            //qDebug() << "url:" << url;
            entry_name = QUrl(url).fileName();
        } else {
            entry_name = entry.stringValue(KIO::UDSEntry::UDS_NAME);
        }

        // This can happen with kdeconnect://deviceId as a completion for kdeconnect:/,
        // there's no fileName [and the UDS_NAME is unrelated, can't use that].
        // This code doesn't support completing hostnames anyway (see addPathToUrl below).
        if (entry_name.isEmpty()) {
            continue;
        }

        if (entry_name.at(0) == QLatin1Char('.') &&
                (list_urls_no_hidden ||
                 entry_name.length() == 1 ||
                 (entry_name.length() == 2 && entry_name.at(1) == QLatin1Char('.')))) {
            continue;
        }

        const bool isDir = entry.isDir();

        if (mode == KUrlCompletion::DirCompletion && !isDir) {
            continue;
        }

        if (filter_len != 0 && entry_name.leftRef(filter_len) != filter) {
            continue;
        }

        if (!mimeTypeFilters.isEmpty() && !isDir &&
            !mimeTypeFilters.contains(entry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE))) {
            continue;
        }

        QString toAppend = entry_name;

        if (isDir) {
            toAppend.append(QLatin1Char('/'));
        }

        if (!list_urls_only_exe ||
                (entry.numberValue(KIO::UDSEntry::UDS_ACCESS) & MODE_EXE)  // true if executable
            ) {
            if (complete_url) {
                QUrl url(prepend);
                url = addPathToUrl(url, toAppend);
                matchList.append(url.toDisplayString());
            } else {
                matchList.append(prepend + toAppend);
            }
        }

    }

    addMatches(matchList);
}

/*
 * _k_slotIOFinished
 *
 * Called when a KIO job is finished.
 *
 * Start a new list job if there are still urls in
 * list_urls, otherwise call finished()
 */
void KUrlCompletionPrivate::_k_slotIOFinished(KJob *job)
{
    assert(job == list_job); Q_UNUSED(job)

    if (list_urls.isEmpty()) {

        list_job = nullptr;

        finished(); // will call KCompletion::makeCompletion()

    } else {

        QUrl kurl(list_urls.takeFirst());

//      list_urls.removeAll( kurl );

//qDebug() << "Start KIO::listDir" << kurl;

        list_job = KIO::listDir(kurl, KIO::HideProgressInfo);
        list_job->addMetaData(QStringLiteral("no-auth-prompt"), QStringLiteral("true"));

        assert(list_job);

        q->connect(list_job, &KJob::result, q, [this](KJob *job) { _k_slotIOFinished(job); });

        q->connect(list_job, &KIO::ListJob::entries,
                   q, [this](KIO::Job *job, const KIO::UDSEntryList &list) { _k_slotEntries(job, list); });
    }
}

///////////////////////////////////////////////////
///////////////////////////////////////////////////

/*
 * postProcessMatch, postProcessMatches
 *
 * Called by KCompletion before emitting match() and matches()
 *
 * Append '/' to directories for file completion. This is
 * done here to avoid stat()'ing a lot of files
 */
void KUrlCompletion::postProcessMatch(QString *pMatch) const
{
//qDebug() << *pMatch;

    if (!pMatch->isEmpty() && pMatch->startsWith(QLatin1String("file:"))) {

        // Add '/' to directories in file completion mode
        // unless it has already been done
        if (d->last_compl_type == CTFile
                && pMatch->at(pMatch->length() - 1) != QLatin1Char('/')) {
            QString copy = QUrl(*pMatch).toLocalFile();
            expandTilde(copy);
            expandEnv(copy);
            if (!isAbsoluteLocalPath(copy)) {
                copy.prepend(d->cwd.toLocalFile() + QLatin1Char('/'));
            }

            //qDebug() << "stat'ing" << copy;

            QByteArray file = QFile::encodeName(copy);

            QT_STATBUF sbuff;
            if (QT_STAT(file.constData(), &sbuff) == 0) {
                if ((sbuff.st_mode & QT_STAT_MASK) == QT_STAT_DIR) {
                    pMatch->append(QLatin1Char('/'));
                }
            } else {
                //qDebug() << "Could not stat file" << copy;
            }
        }
    }
}

void KUrlCompletion::postProcessMatches(QStringList * /*matches*/) const
{
    // Maybe '/' should be added to directories here as in
    // postProcessMatch() but it would slow things down
    // when there are a lot of matches...
}

void KUrlCompletion::postProcessMatches(KCompletionMatches * /*matches*/) const
{
    // Maybe '/' should be added to directories here as in
    // postProcessMatch() but it would slow things down
    // when there are a lot of matches...
}

// no longer used, KF6 TODO: remove this method
void KUrlCompletion::customEvent(QEvent *e)
{
    KCompletion::customEvent(e);
}

void KUrlCompletionPrivate::slotCompletionThreadDone(QThread *thread, const QStringList &matches)
{
    if (thread != userListThread && thread != dirListThread) {
        qCDebug(KIO_WIDGETS) << "got" << matches.count() << "outdated matches";
        return;
    }

    qCDebug(KIO_WIDGETS) << "got" << matches.count() << "matches at end of thread";
    q->setItems(matches);

    if (userListThread == thread) {
        thread->wait();
        delete thread;
        userListThread = nullptr;
    }

    if (dirListThread == thread) {
        thread->wait();
        delete thread;
        dirListThread = nullptr;
    }
    finished(); // will call KCompletion::makeCompletion()
}

// static
QString KUrlCompletion::replacedPath(const QString &text, bool replaceHome, bool replaceEnv)
{
    if (text.isEmpty()) {
        return text;
    }

    KUrlCompletionPrivate::MyURL url(text, QUrl());  // no need to replace something of our current cwd
    if (!url.kurl().isLocalFile()) {
        return text;
    }

    url.filter(replaceHome, replaceEnv);
    return url.dir() + url.file();
}

QString KUrlCompletion::replacedPath(const QString &text) const
{
    return replacedPath(text, d->replace_home, d->replace_env);
}

void KUrlCompletion::setMimeTypeFilters(const QStringList &mimeTypeFilters)
{
    d->mimeTypeFilters = mimeTypeFilters;
}

QStringList KUrlCompletion::mimeTypeFilters() const
{
    return d->mimeTypeFilters;
}

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
// Static functions

/*
 * expandEnv
 *
 * Expand environment variables in text. Escaped '$' are ignored.
 * Return true if expansion was made.
 */
static bool expandEnv(QString &text)
{
    // Find all environment variables beginning with '$'
    //
    int pos = 0;

    bool expanded = false;

    while ((pos = text.indexOf(QLatin1Char('$'), pos)) != -1) {

        // Skip escaped '$'
        //
        if (pos > 0 && text.at(pos - 1) == QLatin1Char('\\')) {
            pos++;
        }
        // Variable found => expand
        //
        else {
            // Find the end of the variable = next '/' or ' '
            //
            int pos2 = text.indexOf(QLatin1Char(' '), pos + 1);
            int pos_tmp = text.indexOf(QLatin1Char('/'), pos + 1);

            if (pos2 == -1 || (pos_tmp != -1 && pos_tmp < pos2)) {
                pos2 = pos_tmp;
            }

            if (pos2 == -1) {
                pos2 = text.length();
            }

            // Replace if the variable is terminated by '/' or ' '
            // and defined
            //
            if (pos2 >= 0) {
                int len = pos2 - pos;
                const QStringRef key = text.midRef(pos + 1, len - 1);
                QString value =
                    QString::fromLocal8Bit(qgetenv(key.toLocal8Bit().constData()));

                if (!value.isEmpty()) {
                    expanded = true;
                    text.replace(pos, len, value);
                    pos = pos + value.length();
                } else {
                    pos = pos2;
                }
            }
        }
    }

    return expanded;
}

/*
 * expandTilde
 *
 * Replace "~user" with the users home directory
 * Return true if expansion was made.
 */
static bool expandTilde(QString &text)
{
    if (text.isEmpty() || (text.at(0) != QLatin1Char('~'))) {
        return false;
    }

    bool expanded = false;

    // Find the end of the user name = next '/' or ' '
    //
    int pos2 = text.indexOf(QLatin1Char(' '), 1);
    int pos_tmp = text.indexOf(QLatin1Char('/'), 1);

    if (pos2 == -1 || (pos_tmp != -1 && pos_tmp < pos2)) {
        pos2 = pos_tmp;
    }

    if (pos2 == -1) {
        pos2 = text.length();
    }

    // Replace ~user if the user name is terminated by '/' or ' '
    //
    if (pos2 >= 0) {

        QString userName = text.mid(1, pos2 - 1);
        QString dir;

        // A single ~ is replaced with $HOME
        //
        if (userName.isEmpty()) {
            dir = QDir::homePath();
        }
        // ~user is replaced with the dir from passwd
        //
        else {
            KUser user(userName);
            dir = user.homeDir();
        }

        if (!dir.isEmpty()) {
            expanded = true;
            text.replace(0, pos2, dir);
        }
    }

    return expanded;
}

/*
 * unescape
 *
 * Remove escapes and return the result in a new string
 *
 */
static QString unescape(const QString &text)
{
    QString result;
    result.reserve(text.size());

    for (const QChar ch : text) {
        if (ch != QLatin1Char('\\')) {
            result.append(ch);
        }
    }

    return result;
}

#include "moc_kurlcompletion.cpp"
#include "kurlcompletion.moc"
