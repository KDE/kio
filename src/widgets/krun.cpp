/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2006 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2009 Michael Pyne <michael.pyne@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "krun.h"

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
#include "krun_p.h"
#include "kio_widgets_debug.h"

#include <assert.h>
#include <string.h>
#include <typeinfo>
#include <qplatformdefs.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QWidget>
#include <QApplication>
#include <QDesktopWidget>
#include <QMimeDatabase>
#include <QDebug>
#include <QHostInfo>
#include <QDesktopServices>

#include <KIconLoader>
#include <KJobUiDelegate>
#include <KApplicationTrader>
#include "kio/job.h"
#include "kio/global.h"
#include "kio/scheduler.h"
#include "kopenwithdialog.h"
#include "krecentdocument.h"
#include "kdesktopfileactions.h"
#include <kio/desktopexecparser.h>
#include "kprocessrunner_p.h" // for KIOGuiPrivate::checkStartupNotify
#include "applicationlauncherjob.h"
#include "jobuidelegate.h"
#include "widgetsuntrustedprogramhandler.h"

#include <kurlauthorized.h>
#include <KMessageBox>
#include <KLocalizedString>
#include <kprotocolmanager.h>
#include <KProcess>
#include <KJobWidgets>
#include <KSharedConfig>
#include <commandlauncherjob.h>

#include <QFile>
#include <QFileInfo>
#include <KDesktopFile>
#include <KShell>
#include <KConfigGroup>
#include <KStandardGuiItem>
#include <KGuiItem>

#include <KIO/OpenUrlJob>
#include <QStandardPaths>
#include <KIO/JobUiDelegate>

#ifdef Q_OS_WIN
#include "widgetsopenwithhandler_win.cpp" // displayNativeOpenWithDialog
#endif

KRunPrivate::KRunPrivate(KRun *parent)
    : q(parent),
      m_showingDialog(false)
{
}

void KRunPrivate::startTimer()
{
    m_timer->start(0);
}

// ---------------------------------------------------------------------------


static KService::Ptr schemeService(const QString &protocol)
{
    return KApplicationTrader::preferredService(QLatin1String("x-scheme-handler/") + protocol);
}

static bool checkNeedPortalSupport()
{
    return !QStandardPaths::locate(QStandardPaths::RuntimeLocation,
                                   QLatin1String("flatpak-info")).isEmpty() ||
            qEnvironmentVariableIsSet("SNAP");
}

qint64 KRunPrivate::runCommandLauncherJob(KIO::CommandLauncherJob *job, QWidget *widget)
{
    QObject *receiver = widget ? static_cast<QObject *>(widget) : static_cast<QObject *>(qApp);
    QObject::connect(job, &KJob::result, receiver, [widget](KJob *job) {
        if (job->error()) {
            QEventLoopLocker locker;
            KMessageBox::sorry(widget, job->errorString());
        }
    });
    job->start();
    job->waitForStarted();
    return job->error() ? 0 : job->pid();
}

// ---------------------------------------------------------------------------

// Helper function that returns whether a file has the execute bit set or not.
static bool hasExecuteBit(const QString &fileName)
{
    QFileInfo file(fileName);
    return file.isExecutable();
}

bool KRun::isExecutableFile(const QUrl &url, const QString &mimetype)
{
    if (!url.isLocalFile()) {
        return false;
    }

    // While isExecutable performs similar check to this one, some users depend on
    // this method not returning true for application/x-desktop
    QMimeDatabase db;
    QMimeType mimeType = db.mimeTypeForName(mimetype);
    if (!mimeType.inherits(QStringLiteral("application/x-executable"))
            && !mimeType.inherits(QStringLiteral("application/x-ms-dos-executable"))
            && !mimeType.inherits(QStringLiteral("application/x-executable-script"))
            && !mimeType.inherits(QStringLiteral("application/x-sharedlib"))) {
        return false;
    }

    if (!hasExecuteBit(url.toLocalFile()) && !mimeType.inherits(QStringLiteral("application/x-ms-dos-executable"))) {
        return false;
    }

    return true;
}

void KRun::handleInitError(int kioErrorCode, const QString &errorMsg)
{
    Q_UNUSED(kioErrorCode);
    d->m_showingDialog = true;
    KMessageBox::error(d->m_window, errorMsg);
    d->m_showingDialog = false;
}

void KRun::handleError(KJob *job)
{
    Q_ASSERT(job);
    if (job) {
        d->m_showingDialog = true;
        job->uiDelegate()->showErrorMessage();
        d->m_showingDialog = false;
    }
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 31)
bool KRun::runUrl(const QUrl &url, const QString &mimetype, QWidget *window, bool tempFile, bool runExecutables, const QString &suggestedFileName, const QByteArray &asn)
{
    RunFlags flags = tempFile ? KRun::DeleteTemporaryFiles : RunFlags();
    if (runExecutables) {
        flags |= KRun::RunExecutables;
    }

    return runUrl(url, mimetype, window, flags, suggestedFileName, asn);
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
// This is called by foundMimeType, since it knows the MIME type of the URL
bool KRun::runUrl(const QUrl &u, const QString &_mimetype, QWidget *window, RunFlags flags, const QString &suggestedFileName, const QByteArray &asn)
{
    const bool runExecutables = flags.testFlag(KRun::RunExecutables);
    const bool tempFile = flags.testFlag(KRun::DeleteTemporaryFiles);

    KIO::OpenUrlJob *job = new KIO::OpenUrlJob(u, _mimetype);
    job->setSuggestedFileName(suggestedFileName);
    job->setStartupId(asn);
    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
    job->setDeleteTemporaryFile(tempFile);
    job->setRunExecutables(runExecutables);
    job->start();
    return true;
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
bool KRun::displayOpenWithDialog(const QList<QUrl> &lst, QWidget *window, bool tempFiles,
                                 const QString &suggestedFileName, const QByteArray &asn)
{
    if (!KAuthorized::authorizeAction(QStringLiteral("openwith"))) {
        KMessageBox::sorry(window,
                           i18n("You are not authorized to select an application to open this file."));
        return false;
    }

#ifdef Q_OS_WIN
    KConfigGroup cfgGroup(KSharedConfig::openConfig(), QStringLiteral("KOpenWithDialog Settings"));
    if (cfgGroup.readEntry("Native", true)) {
        return displayNativeOpenWithDialog(lst, window);
    }
#endif

    // TODO : pass the MIME type as a parameter, to show it (comment field) in the dialog !
    // Note KOpenWithDialog::setMimeTypeFromUrls already guesses the MIME type if lst.size() == 1
    KOpenWithDialog dialog(lst, QString(), QString(), window);
    dialog.setWindowModality(Qt::WindowModal);
    if (dialog.exec()) {
        KService::Ptr service = dialog.service();
        if (!service) {
            //qDebug() << "No service set, running " << dialog.text();
            service = KService::Ptr(new KService(QString() /*name*/, dialog.text(), QString() /*icon*/));
        }
        const RunFlags flags = tempFiles ? KRun::DeleteTemporaryFiles : RunFlags();
        return KRun::runApplication(*service, lst, window, flags, suggestedFileName, asn);
    }
    return false;
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(4, 0)
void KRun::shellQuote(QString &_str)
{
    // Credits to Walter, says Bernd G. :)
    if (_str.isEmpty()) { // Don't create an explicit empty parameter
        return;
    }
    const QChar q = QLatin1Char('\'');
    _str.replace(q, QLatin1String("'\\''")).prepend(q).append(q);
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 0)
QStringList KRun::processDesktopExec(const KService &_service, const QList<QUrl> &_urls, bool tempFiles, const QString &suggestedFileName)
{
    KIO::DesktopExecParser parser(_service, _urls);
    parser.setUrlsAreTempFiles(tempFiles);
    parser.setSuggestedFileName(suggestedFileName);
    return parser.resultingArguments();
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 0)
QString KRun::binaryName(const QString &execLine, bool removePath)
{
    return removePath ? KIO::DesktopExecParser::executableName(execLine) : KIO::DesktopExecParser::executablePath(execLine);
}
#endif

// This code is also used in klauncher.
// TODO: port klauncher to KIOGuiPrivate::checkStartupNotify once this lands
// TODO: then deprecate this method, and remove in KF6
bool KRun::checkStartupNotify(const QString & /*binName*/, const KService *service, bool *silent_arg, QByteArray *wmclass_arg)
{
    return KIOGuiPrivate::checkStartupNotify(service, silent_arg, wmclass_arg);
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 6)
bool KRun::run(const KService &_service, const QList<QUrl> &_urls, QWidget *window,
               bool tempFiles, const QString &suggestedFileName, const QByteArray &asn)
{
    const RunFlags flags = tempFiles ? KRun::DeleteTemporaryFiles : RunFlags();
    return runApplication(_service, _urls, window, flags, suggestedFileName, asn) != 0;
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
qint64 KRun::runApplication(const KService &service, const QList<QUrl> &urls, QWidget *window,
                            RunFlags flags, const QString &suggestedFileName,
                            const QByteArray &asn)
{
    KService::Ptr servicePtr(new KService(service)); // clone
    // QTBUG-59017 Calling winId() on an embedded widget will break interaction
    // with it on high-dpi multi-screen setups (cf. also Bug 363548), hence using
    // its parent window instead
    if (window) {
        window = window->window();
    }

    KIO::ApplicationLauncherJob *job = new KIO::ApplicationLauncherJob(servicePtr);
    job->setUrls(urls);
    if (flags & DeleteTemporaryFiles) {
        job->setRunFlags(KIO::ApplicationLauncherJob::DeleteTemporaryFiles);
    }
    job->setSuggestedFileName(suggestedFileName);
    job->setStartupId(asn);
    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
    job->start();
    job->waitForStarted();
    return job->error() ? 0 : job->pid();
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
qint64 KRun::runService(const KService &_service, const QList<QUrl> &_urls, QWidget *window,
                      bool tempFiles, const QString &suggestedFileName, const QByteArray &asn)
{
    return runApplication(_service,
                   _urls,
                   window,
                   tempFiles ? RunFlags(DeleteTemporaryFiles) : RunFlags(),
                   suggestedFileName,
                   asn);
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
bool KRun::run(const QString &_exec, const QList<QUrl> &_urls, QWidget *window, const QString &_name,
               const QString &_icon, const QByteArray &asn)
{
    KService::Ptr service(new KService(_name, _exec, _icon));

    return runApplication(*service, _urls, window, RunFlags{}, QString(), asn);
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
bool KRun::runCommand(const QString &cmd, QWidget *window, const QString &workingDirectory)
{
    if (cmd.isEmpty()) {
        qCWarning(KIO_WIDGETS) << "Command was empty, nothing to run";
        return false;
    }

    const QStringList args = KShell::splitArgs(cmd);
    if (args.isEmpty()) {
        qCWarning(KIO_WIDGETS) << "Command could not be parsed.";
        return false;
    }

    const QString &bin = args.first();
    return KRun::runCommand(cmd, bin, bin /*iconName*/, window, QByteArray(), workingDirectory);
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
bool KRun::runCommand(const QString &cmd, const QString &execName, const QString &iconName, QWidget *window, const QByteArray &asn)
{
    return runCommand(cmd, execName, iconName, window, asn, QString());
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
bool KRun::runCommand(const QString &cmd, const QString &execName, const QString &iconName,
                      QWidget *window, const QByteArray &asn, const QString &workingDirectory)
{
    auto *job = new KIO::CommandLauncherJob(cmd);
    job->setExecutable(execName);
    job->setIcon(iconName);
    job->setStartupId(asn);
    job->setWorkingDirectory(workingDirectory);

    if (window) {
        window = window->window();
    }
    return KRunPrivate::runCommandLauncherJob(job, window);
}
#endif

KRun::KRun(const QUrl &url, QWidget *window,
           bool showProgressInfo, const QByteArray &asn)
    : d(new KRunPrivate(this))
{
    d->m_timer = new QTimer(this);
    d->m_timer->setObjectName(QStringLiteral("KRun::timer"));
    d->m_timer->setSingleShot(true);
    d->init(url, window, showProgressInfo, asn);
}

void KRunPrivate::init(const QUrl &url, QWidget *window,
                             bool showProgressInfo, const QByteArray &asn)
{
    m_bFault = false;
    m_bAutoDelete = true;
    m_bProgressInfo = showProgressInfo;
    m_bFinished = false;
    m_job = nullptr;
    m_strURL = url;
    m_bScanFile = false;
    m_bIsDirectory = false;
    m_runExecutables = true;
    m_followRedirections = true;
    m_window = window;
    m_asn = asn;
    q->setEnableExternalBrowser(true);

    // Start the timer. This means we will return to the event
    // loop and do initialization afterwards.
    // Reason: We must complete the constructor before we do anything else.
    m_bCheckPrompt = false;
    m_bInit = true;
    q->connect(m_timer, &QTimer::timeout, q, &KRun::slotTimeout);
    startTimer();
    //qDebug() << "new KRun" << q << url << "timer=" << m_timer;
}

void KRun::init()
{
    //qDebug() << "INIT called";
    if (!d->m_strURL.isValid() || d->m_strURL.scheme().isEmpty()) {
        const QString error = !d->m_strURL.isValid() ? d->m_strURL.errorString() : d->m_strURL.toString();
        handleInitError(KIO::ERR_MALFORMED_URL, i18n("Malformed URL\n%1", error));
        qCWarning(KIO_WIDGETS) << "Malformed URL:" << error;
        d->m_bFault = true;
        d->m_bFinished = true;
        d->startTimer();
        return;
    }
    if (!KUrlAuthorized::authorizeUrlAction(QStringLiteral("open"), QUrl(), d->m_strURL)) {
        QString msg = KIO::buildErrorString(KIO::ERR_ACCESS_DENIED, d->m_strURL.toDisplayString());
        handleInitError(KIO::ERR_ACCESS_DENIED, msg);
        d->m_bFault = true;
        d->m_bFinished = true;
        d->startTimer();
        return;
    }

    if (d->m_externalBrowserEnabled && checkNeedPortalSupport()) {
        // use the function from QDesktopServices as it handles portals correctly
        d->m_bFault = !QDesktopServices::openUrl(d->m_strURL);
        d->m_bFinished = true;
        d->startTimer();
        return;
    }

    if (!d->m_externalBrowser.isEmpty() && d->m_strURL.scheme().startsWith(QLatin1String("http"))) {
        if (d->runExternalBrowser(d->m_externalBrowser)) {
            return;
        }
    } else if (d->m_strURL.isLocalFile() &&
               (d->m_strURL.host().isEmpty() ||
                (d->m_strURL.host() == QLatin1String("localhost")) ||
                (d->m_strURL.host().compare(QHostInfo::localHostName(), Qt::CaseInsensitive) == 0))) {
        const QString localPath = d->m_strURL.toLocalFile();
        if (!QFile::exists(localPath)) {
            handleInitError(KIO::ERR_DOES_NOT_EXIST,
                            i18n("<qt>Unable to run the command specified. "
                                 "The file or folder <b>%1</b> does not exist.</qt>",
                                 localPath.toHtmlEscaped()));
            d->m_bFault = true;
            d->m_bFinished = true;
            d->startTimer();
            return;
        }

        QMimeDatabase db;
        QMimeType mime = db.mimeTypeForUrl(d->m_strURL);
        //qDebug() << "MIME TYPE is " << mime.name();
        if (mime.isDefault() && !QFileInfo(localPath).isReadable()) {
            // Unknown MIME type because the file is unreadable, no point in showing an open-with dialog (#261002)
            const QString msg = KIO::buildErrorString(KIO::ERR_ACCESS_DENIED, localPath);
            handleInitError(KIO::ERR_ACCESS_DENIED, msg);
            d->m_bFault = true;
            d->m_bFinished = true;
            d->startTimer();
            return;
        } else {
            mimeTypeDetermined(mime.name());
            return;
        }
    } else if (KIO::DesktopExecParser::hasSchemeHandler(d->m_strURL)) {
        // looks for an application associated with x-scheme-handler/<protocol>
        const KService::Ptr service = schemeService(d->m_strURL.scheme());
        if (service) {
            //  if there's one...
            if (runApplication(*service, QList<QUrl>() << d->m_strURL, d->m_window, RunFlags{}, QString(), d->m_asn)) {
                d->m_bFinished = true;
                d->startTimer();
                return;
            }
        } else {
            // fallback, look for associated helper protocol
            Q_ASSERT(KProtocolInfo::isHelperProtocol(d->m_strURL.scheme()));
            const auto exec = KProtocolInfo::exec(d->m_strURL.scheme());
            if (exec.isEmpty()) {
                // use default MIME type opener for file
                mimeTypeDetermined(KProtocolManager::defaultMimetype(d->m_strURL));
                return;
            } else {
                if (run(exec, QList<QUrl>() << d->m_strURL, d->m_window, QString(), QString(), d->m_asn)) {
                    d->m_bFinished = true;
                    d->startTimer();
                    return;
                }
            }
        }

    }

    // Let's see whether it is a directory

    if (!KProtocolManager::supportsListing(d->m_strURL)) {
        // No support for listing => it can't be a directory (example: http)

        if (!KProtocolManager::supportsReading(d->m_strURL)) {
            // No support for reading files either => we can't do anything (example: mailto URL, with no associated app)
            handleInitError(KIO::ERR_UNSUPPORTED_ACTION, i18n("Could not find any application or handler for %1", d->m_strURL.toDisplayString()));
            d->m_bFault = true;
            d->m_bFinished = true;
            d->startTimer();
            return;
        }
        scanFile();
        return;
    }

    //qDebug() << "Testing directory (stating)";

    // It may be a directory or a file, let's stat
    KIO::JobFlags flags = d->m_bProgressInfo ? KIO::DefaultFlags : KIO::HideProgressInfo;
    KIO::StatJob *job = KIO::statDetails(d->m_strURL, KIO::StatJob::SourceSide, KIO::StatBasic, flags);
    KJobWidgets::setWindow(job, d->m_window);
    connect(job, &KJob::result,
            this, &KRun::slotStatResult);
    d->m_job = job;
    //qDebug() << "Job" << job << "is about stating" << d->m_strURL;
}

KRun::~KRun()
{
    //qDebug() << this;
    d->m_timer->stop();
    killJob();
    //qDebug() << this << "done";
    delete d;
}

bool KRunPrivate::runExternalBrowser(const QString &_exec)
{
    QList<QUrl> urls;
    urls.append(m_strURL);
    if (_exec.startsWith(QLatin1Char('!'))) {
        // Literal command
        const QString exec = _exec.midRef(1) + QLatin1String(" %u");
        if (KRun::run(exec, urls, m_window, QString(), QString(), m_asn)) {
            m_bFinished = true;
            startTimer();
            return true;
        }
    } else {
        KService::Ptr service = KService::serviceByStorageId(_exec);
        if (service && KRun::runApplication(*service, urls, m_window, KRun::RunFlags{}, QString(), m_asn)) {
            m_bFinished = true;
            startTimer();
            return true;
        }
    }
    return false;
}

void KRunPrivate::showPrompt()
{
    ExecutableFileOpenDialog *dialog = new ExecutableFileOpenDialog(promptMode(), q->window());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(dialog, &ExecutableFileOpenDialog::finished, q, [this, dialog](int result){
                         onDialogFinished(result, dialog->isDontAskAgainChecked());
    });
    dialog->show();
}

bool KRunPrivate::isPromptNeeded()
{
    if (m_strURL == QUrl(QStringLiteral("remote:/x-wizard_service.desktop"))) {
        return false;
    }
    const QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForUrl(m_strURL);

    const bool isFileExecutable = (KRun::isExecutableFile(m_strURL, mime.name()) ||
                                   mime.inherits(QStringLiteral("application/x-desktop")));

    if (isFileExecutable) {
        KConfigGroup cfgGroup(KSharedConfig::openConfig(QStringLiteral("kiorc")), "Executable scripts");
        const QString value = cfgGroup.readEntry("behaviourOnLaunch", "alwaysAsk");

        if (value == QLatin1String("alwaysAsk")) {
            return true;
        } else {
            q->setRunExecutables(value == QLatin1String("execute"));
        }
    }

    return false;
}

ExecutableFileOpenDialog::Mode KRunPrivate::promptMode()
{
    const QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForUrl(m_strURL);

    if (mime.inherits(QStringLiteral("text/plain"))) {
        return ExecutableFileOpenDialog::OpenOrExecute;
    }
#ifndef Q_OS_WIN
    if (mime.inherits(QStringLiteral("application/x-ms-dos-executable"))) {
        return ExecutableFileOpenDialog::OpenAsExecute;
    }
#endif
    return ExecutableFileOpenDialog::OnlyExecute;
}

void KRunPrivate::onDialogFinished(int result, bool isDontAskAgainSet)
{
    if (result == ExecutableFileOpenDialog::Rejected) {
        m_bFinished = true;
        m_bInit = false;
        startTimer();
        return;
    }
    q->setRunExecutables(result == ExecutableFileOpenDialog::ExecuteFile);

    if (isDontAskAgainSet) {
        QString output = result == ExecutableFileOpenDialog::OpenFile ? QStringLiteral("open") : QStringLiteral("execute");
        KConfigGroup cfgGroup(KSharedConfig::openConfig(QStringLiteral("kiorc")), "Executable scripts");
        cfgGroup.writeEntry("behaviourOnLaunch", output);
    }
    startTimer();
}

void KRun::scanFile()
{
    //qDebug() << d->m_strURL;
    // First, let's check for well-known extensions
    // Not when there is a query in the URL, in any case.
    if (!d->m_strURL.hasQuery()) {
        QMimeDatabase db;
        QMimeType mime = db.mimeTypeForUrl(d->m_strURL);
        if (!mime.isDefault() || d->m_strURL.isLocalFile()) {
            //qDebug() << "Scanfile: MIME TYPE is " << mime.name();
            mimeTypeDetermined(mime.name());
            return;
        }
    }

    // No MIME type found, and the URL is not local  (or fast mode not allowed).
    // We need to apply the 'KIO' method, i.e. either asking the server or
    // getting some data out of the file, to know what MIME type it is.

    if (!KProtocolManager::supportsReading(d->m_strURL)) {
        qCWarning(KIO_WIDGETS) << "#### NO SUPPORT FOR READING!";
        d->m_bFault = true;
        d->m_bFinished = true;
        d->startTimer();
        return;
    }
    //qDebug() << this << "Scanning file" << d->m_strURL;

    KIO::JobFlags flags = d->m_bProgressInfo ? KIO::DefaultFlags : KIO::HideProgressInfo;
    KIO::TransferJob *job = KIO::get(d->m_strURL, KIO::NoReload /*reload*/, flags);
    KJobWidgets::setWindow(job, d->m_window);
    connect(job, &KJob::result,
            this, &KRun::slotScanFinished);
    connect(job, &KIO::TransferJob::mimeTypeFound,
            this, &KRun::slotScanMimeType);
    d->m_job = job;
    //qDebug() << "Job" << job << "is about getting from" << d->m_strURL;
}

// When arriving in that method there are 6 possible states:
// must_show_prompt, must_init, must_scan_file, found_dir, done+error or done+success.
void KRun::slotTimeout()
{
    if (d->m_bCheckPrompt) {
        d->m_bCheckPrompt = false;
        if (d->isPromptNeeded()) {
            d->showPrompt();
            return;
        }
    }
    if (d->m_bInit) {
        d->m_bInit = false;
        init();
        return;
    }

    if (d->m_bFault) {
        Q_EMIT error();
    }
    if (d->m_bFinished) {
        Q_EMIT finished();
    } else {
        if (d->m_bScanFile) {
            d->m_bScanFile = false;
            scanFile();
            return;
        } else if (d->m_bIsDirectory) {
            d->m_bIsDirectory = false;
            mimeTypeDetermined(QStringLiteral("inode/directory"));
            return;
        }
    }

    if (d->m_bAutoDelete) {
        deleteLater();
        return;
    }
}

void KRun::slotStatResult(KJob *job)
{
    d->m_job = nullptr;
    const int errCode = job->error();
    if (errCode) {
        // ERR_NO_CONTENT is not an error, but an indication no further
        // actions needs to be taken.
        if (errCode != KIO::ERR_NO_CONTENT) {
            qCWarning(KIO_WIDGETS) << this << "ERROR" << job->error() << job->errorString();
            handleError(job);
            //qDebug() << this << " KRun returning from showErrorDialog, starting timer to delete us";
            d->m_bFault = true;
        }

        d->m_bFinished = true;

        // will emit the error and autodelete this
        d->startTimer();
    } else {
        //qDebug() << "Finished";

        KIO::StatJob *statJob = qobject_cast<KIO::StatJob *>(job);
        if (!statJob) {
            qFatal("Fatal Error: job is a %s, should be a StatJob", typeid(*job).name());
        }

        // Update our URL in case of a redirection
        setUrl(statJob->url());

        const KIO::UDSEntry entry = statJob->statResult();
        const mode_t mode = entry.numberValue(KIO::UDSEntry::UDS_FILE_TYPE);
        if ((mode & QT_STAT_MASK) == QT_STAT_DIR) {
            d->m_bIsDirectory = true; // it's a dir
        } else {
            d->m_bScanFile = true; // it's a file
        }

        d->m_localPath = entry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH);

        // MIME type already known? (e.g. print:/manager)
        const QString knownMimeType = entry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE);

        if (!knownMimeType.isEmpty()) {
            mimeTypeDetermined(knownMimeType);
            d->m_bFinished = true;
        }

        // We should have found something
        assert(d->m_bScanFile || d->m_bIsDirectory);

        // Start the timer. Once we get the timer event this
        // protocol server is back in the pool and we can reuse it.
        // This gives better performance than starting a new slave
        d->startTimer();
    }
}

void KRun::slotScanMimeType(KIO::Job *, const QString &mimetype)
{
    if (mimetype.isEmpty()) {
        qCWarning(KIO_WIDGETS) << "get() didn't emit a MIME type! Probably a kioslave bug, please check the implementation of" << url().scheme();
    }
    mimeTypeDetermined(mimetype);
    d->m_job = nullptr;
}

void KRun::slotScanFinished(KJob *job)
{
    d->m_job = nullptr;
    const int errCode = job->error();
    if (errCode) {
        // ERR_NO_CONTENT is not an error, but an indication no further
        // actions needs to be taken.
        if (errCode != KIO::ERR_NO_CONTENT) {
            qCWarning(KIO_WIDGETS) << this << "ERROR (stat):" << job->error() << ' ' << job->errorString();
            handleError(job);

            d->m_bFault = true;
        }

        d->m_bFinished = true;
        // will emit the error and autodelete this
        d->startTimer();
    }
}

void KRun::mimeTypeDetermined(const QString &mimeType)
{
    // foundMimeType reimplementations might show a dialog box;
    // make sure some timer doesn't kill us meanwhile (#137678, #156447)
    Q_ASSERT(!d->m_showingDialog);
    d->m_showingDialog = true;

    foundMimeType(mimeType);

    d->m_showingDialog = false;

    // We cannot assume that we're finished here. Some reimplementations
    // start a KIO job and call setFinished only later.
}

void KRun::foundMimeType(const QString &type)
{
    //qDebug() << "Resulting MIME type is " << type;

    QMimeDatabase db;

    KIO::TransferJob *job = qobject_cast<KIO::TransferJob *>(d->m_job);
    if (job) {
        // Update our URL in case of a redirection
        if (d->m_followRedirections) {
            setUrl(job->url());
        }

        job->putOnHold();
        KIO::Scheduler::publishSlaveOnHold();
        d->m_job = nullptr;
    }

    Q_ASSERT(!d->m_bFinished);

    // Support for preferred service setting, see setPreferredService
    if (!d->m_preferredService.isEmpty()) {
        //qDebug() << "Attempting to open with preferred service: " << d->m_preferredService;
        KService::Ptr serv = KService::serviceByDesktopName(d->m_preferredService);
        if (serv && serv->hasMimeType(type)) {
            QList<QUrl> lst;
            lst.append(d->m_strURL);
            if (KRun::runApplication(*serv, lst, d->m_window, RunFlags{}, QString(), d->m_asn)) {
                setFinished(true);
                return;
            }
            /// Note: if that service failed, we'll go to runUrl below to
            /// maybe find another service, even though an error dialog box was
            /// already displayed. That's good if runUrl tries another service,
            /// but it's not good if it tries the same one :}
        }
    }

    // Resolve .desktop files from media:/, remote:/, applications:/ etc.
    QMimeType mime = db.mimeTypeForName(type);
    if (!mime.isValid()) {
        qCWarning(KIO_WIDGETS) << "Unknown MIME type" << type;
    } else if (mime.inherits(QStringLiteral("application/x-desktop")) && !d->m_localPath.isEmpty()) {
        d->m_strURL = QUrl::fromLocalFile(d->m_localPath);
    }

    KRun::RunFlags runFlags;
    if (d->m_runExecutables) {
        runFlags |= KRun::RunExecutables;
    }
    if (!KRun::runUrl(d->m_strURL, type, d->m_window, runFlags, d->m_suggestedFileName, d->m_asn)) {
        d->m_bFault = true;
    }
    setFinished(true);
}

void KRun::killJob()
{
    if (d->m_job) {
        //qDebug() << this << "m_job=" << d->m_job;
        d->m_job->kill();
        d->m_job = nullptr;
    }
}

void KRun::abort()
{
    if (d->m_bFinished) {
        return;
    }
    //qDebug() << this << "m_showingDialog=" << d->m_showingDialog;
    killJob();
    // If we're showing an error message box, the rest will be done
    // after closing the msgbox -> don't autodelete nor emit signals now.
    if (d->m_showingDialog) {
        return;
    }
    d->m_bFault = true;
    d->m_bFinished = true;
    d->m_bInit = false;
    d->m_bScanFile = false;

    // will emit the error and autodelete this
    d->startTimer();
}

QWidget *KRun::window() const
{
    return d->m_window;
}

bool KRun::hasError() const
{
    return d->m_bFault;
}

bool KRun::hasFinished() const
{
    return d->m_bFinished;
}

bool KRun::autoDelete() const
{
    return d->m_bAutoDelete;
}

void KRun::setAutoDelete(bool b)
{
    d->m_bAutoDelete = b;
}

void KRun::setEnableExternalBrowser(bool b)
{
    d->m_externalBrowserEnabled = b;
    if (d->m_externalBrowserEnabled) {
        d->m_externalBrowser = KConfigGroup(KSharedConfig::openConfig(), "General").readEntry("BrowserApplication");

        // If a default browser isn't set in kdeglobals, fall back to mimeapps.list
        if (!d->m_externalBrowser.isEmpty()) {
            return;
        }

        KSharedConfig::Ptr profile = KSharedConfig::openConfig(QStringLiteral("mimeapps.list"), KConfig::NoGlobals, QStandardPaths::GenericConfigLocation);
        KConfigGroup defaultApps(profile, "Default Applications");

        d->m_externalBrowser = defaultApps.readEntry("x-scheme-handler/https");
        if (d->m_externalBrowser.isEmpty()) {
            d->m_externalBrowser = defaultApps.readEntry("x-scheme-handler/http");
        }
    } else {
        d->m_externalBrowser.clear();
    }
}

void KRun::setPreferredService(const QString &desktopEntryName)
{
    d->m_preferredService = desktopEntryName;
}

void KRun::setRunExecutables(bool b)
{
    d->m_runExecutables = b;
}

void KRun::setSuggestedFileName(const QString &fileName)
{
    d->m_suggestedFileName = fileName;
}

void KRun::setShowScriptExecutionPrompt(bool showPrompt)
{
    d->m_bCheckPrompt = showPrompt;
}

void KRun::setFollowRedirections(bool followRedirections)
{
    d->m_followRedirections = followRedirections;
}

QString KRun::suggestedFileName() const
{
    return d->m_suggestedFileName;
}

bool KRun::isExecutable(const QString &mimeTypeName)
{
    QMimeDatabase db;
    QMimeType mimeType = db.mimeTypeForName(mimeTypeName);
    return (mimeType.inherits(QStringLiteral("application/x-desktop")) ||
            mimeType.inherits(QStringLiteral("application/x-executable")) ||
            /* See https://bugs.freedesktop.org/show_bug.cgi?id=97226 */
            mimeType.inherits(QStringLiteral("application/x-sharedlib")) ||
            mimeType.inherits(QStringLiteral("application/x-ms-dos-executable")) ||
            mimeType.inherits(QStringLiteral("application/x-shellscript")));
}

void KRun::setUrl(const QUrl &url)
{
    d->m_strURL = url;
}

QUrl KRun::url() const
{
    return d->m_strURL;
}

void KRun::setError(bool error)
{
    d->m_bFault = error;
}

void KRun::setProgressInfo(bool progressInfo)
{
    d->m_bProgressInfo = progressInfo;
}

bool KRun::progressInfo() const
{
    return d->m_bProgressInfo;
}

void KRun::setFinished(bool finished)
{
    d->m_bFinished = finished;
    if (finished) {
        d->startTimer();
    }
}

void KRun::setJob(KIO::Job *job)
{
    d->m_job = job;
}

KIO::Job *KRun::job()
{
    return d->m_job;
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(4, 4)
QTimer &KRun::timer()
{
    return *d->m_timer;
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(4, 1)
void KRun::setDoScanFile(bool scanFile)
{
    d->m_bScanFile = scanFile;
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(4, 1)
bool KRun::doScanFile() const
{
    return d->m_bScanFile;
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(4, 1)
void KRun::setIsDirecory(bool isDirectory)
{
    d->m_bIsDirectory = isDirectory;
}
#endif

bool KRun::isDirectory() const
{
    return d->m_bIsDirectory;
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(4, 1)
void KRun::setInitializeNextAction(bool initialize)
{
    d->m_bInit = initialize;
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(4, 1)
bool KRun::initializeNextAction() const
{
    return d->m_bInit;
}
#endif

bool KRun::isLocalFile() const
{
    return d->m_strURL.isLocalFile();
}

#include "moc_krun.cpp"
#endif
