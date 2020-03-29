/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "openurljob.h"
#include "openwithhandlerinterface.h"
#include "openorexecutefileinterface.h"
#include "global.h"
#include "job.h" // for buildErrorString
#include "commandlauncherjob.h"
#include "desktopexecparser.h"
#include "untrustedprogramhandlerinterface.h"
#include "jobuidelegatefactory.h"
#include "kiogui_debug.h"

#include <KApplicationTrader>
#include <KAuthorized>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KLocalizedString>
#include <KUrlAuthorized>
#include <QFileInfo>

#include <KProtocolManager>
#include <KSharedConfig>
#include <QDesktopServices>
#include <QHostInfo>
#include <QMimeDatabase>
#include <QOperatingSystemVersion>
#include <QTimer>

#include <kio/scheduler.h>

class KIO::OpenUrlJobPrivate
{
public:
    explicit OpenUrlJobPrivate(const QUrl &url, OpenUrlJob *qq)
        : m_url(url), q(qq)
    {
        q->setCapabilities(KJob::Killable);
    }

    void emitAccessDenied();
    void runUrlWithMimeType();
    QString externalBrowser() const;
    bool runExternalBrowser(const QString &exe);
    void useSchemeHandler();
    void determineLocalMimeType();
    void statFile();
    void scanFileWithGet();

    QUrl m_url;
    KIO::OpenUrlJob * const q;
    QString m_suggestedFileName;
    QByteArray m_startupId;
    QString m_mimeTypeName;
    KService::Ptr m_preferredService;
    bool m_deleteTemporaryFile = false;
    bool m_runExecutables = false;
    bool m_showOpenOrExecuteDialog = false;
    bool m_externalBrowserEnabled = true;
    bool m_followRedirections = true;

private:
    void executeCommand();
    void handleBinaries(const QMimeType &mimeType);
    void handleBinariesHelper(const QString &localPath, bool isNativeBinary);
    void handleDesktopFiles();
    void handleScripts();
    void openInPreferredApp();
    void runLink(const QString &filePath, const QString &urlStr, const QString &optionalServiceName);

    void showOpenWithDialog();
    void showOpenOrExecuteFileDialog(std::function<void(bool)> dialogFinished);
    void showUntrustedProgramWarningDialog(const QString &filePath);

    void startService(const KService::Ptr &service, const QList<QUrl> &urls);
    void startService(const KService::Ptr &service)
    {
        startService(service, {m_url});
    }
};

KIO::OpenUrlJob::OpenUrlJob(const QUrl &url, QObject *parent)
    : KCompositeJob(parent), d(new OpenUrlJobPrivate(url, this))
{
}

KIO::OpenUrlJob::OpenUrlJob(const QUrl &url, const QString &mimeType, QObject *parent)
    : KCompositeJob(parent), d(new OpenUrlJobPrivate(url, this))
{
    d->m_mimeTypeName = mimeType;
}

KIO::OpenUrlJob::~OpenUrlJob()
{
}

void KIO::OpenUrlJob::setDeleteTemporaryFile(bool b)
{
    d->m_deleteTemporaryFile = b;
}

void KIO::OpenUrlJob::setSuggestedFileName(const QString &suggestedFileName)
{
    d->m_suggestedFileName = suggestedFileName;
}

void KIO::OpenUrlJob::setStartupId(const QByteArray &startupId)
{
    d->m_startupId = startupId;
}

void KIO::OpenUrlJob::setRunExecutables(bool allow)
{
    d->m_runExecutables = allow;
}

void KIO::OpenUrlJob::setShowOpenOrExecuteDialog(bool b)
{
    d->m_showOpenOrExecuteDialog = b;
}

void KIO::OpenUrlJob::setEnableExternalBrowser(bool b)
{
    d->m_externalBrowserEnabled = b;
}

void KIO::OpenUrlJob::setFollowRedirections(bool b)
{
    d->m_followRedirections = b;
}

static bool checkNeedPortalSupport()
{
    return !(QStandardPaths::locate(QStandardPaths::RuntimeLocation, QLatin1String("flatpak-info")).isEmpty()
            || qEnvironmentVariableIsSet("SNAP"));
}

void KIO::OpenUrlJob::start()
{
    if (!d->m_url.isValid() || d->m_url.scheme().isEmpty()) {
        const QString error = !d->m_url.isValid() ? d->m_url.errorString() : d->m_url.toDisplayString();
        setError(KIO::ERR_MALFORMED_URL);
        setErrorText(i18n("Malformed URL\n%1", error));
        emitResult();
        return;
    }
    if (!KUrlAuthorized::authorizeUrlAction(QStringLiteral("open"), QUrl(), d->m_url)) {
        d->emitAccessDenied();
        return;
    }
    if (d->m_externalBrowserEnabled && checkNeedPortalSupport()) {
        // Use the function from QDesktopServices as it handles portals correctly
        // Note that it falls back to "normal way" if the portal service isn't running.
        if (!QDesktopServices::openUrl(d->m_url)) {
            // Is this an actual error, or USER_CANCELED?
            setError(KJob::UserDefinedError);
            setErrorText(i18n("Failed to open %1", d->m_url.toDisplayString()));
        }
        emitResult();
        return;
    }

    // If we know the MIME type, proceed
    if (!d->m_mimeTypeName.isEmpty()) {
        d->runUrlWithMimeType();
        return;
    }

    if (d->m_externalBrowserEnabled && d->m_url.scheme().startsWith(QLatin1String("http"))) {
        const QString externalBrowser = d->externalBrowser();
        if (!externalBrowser.isEmpty() && d->runExternalBrowser(externalBrowser)) {
            return;
        }
    }
    if (KIO::DesktopExecParser::hasSchemeHandler(d->m_url)) {
        d->useSchemeHandler();
        return;
    }

    if (!KProtocolManager::supportsListing(d->m_url)) {
        // No support for listing => it can't be a directory (example: http)
        d->scanFileWithGet();
        return;
    }

    // It may be a directory or a file, let's use stat to find out
    d->statFile();
}

bool KIO::OpenUrlJob::doKill()
{
    return true;
}

QString KIO::OpenUrlJobPrivate::externalBrowser() const
{
    if (!m_externalBrowserEnabled) {
        return QString();
    }

    const QString browserApp = KConfigGroup(KSharedConfig::openConfig(), "General").readEntry("BrowserApplication");
    if (!browserApp.isEmpty()) {
        return browserApp;
    }

    // If a default browser isn't set in kdeglobals, fall back to mimeapps.list
    KSharedConfig::Ptr profile = KSharedConfig::openConfig(QStringLiteral("mimeapps.list"), KConfig::NoGlobals, QStandardPaths::GenericConfigLocation);
    const KConfigGroup defaultApps(profile, "Default Applications");

    QString externalBrowser = defaultApps.readEntry("x-scheme-handler/https");
    if (externalBrowser.isEmpty()) {
        externalBrowser = defaultApps.readEntry("x-scheme-handler/http");
    }
    return externalBrowser;
}

bool KIO::OpenUrlJobPrivate::runExternalBrowser(const QString &exec)
{
    if (exec.startsWith(QLatin1Char('!'))) {
        // Literal command
        const QString command = exec.midRef(1) + QLatin1String(" %u");
        KService::Ptr service(new KService(QString(), command, QString()));
        startService(service);
        return true;
    } else {
        // Name of desktop file
        KService::Ptr service = KService::serviceByStorageId(exec);
        if (service) {
            startService(service);
            return true;
        }
    }
    return false;
}

void KIO::OpenUrlJobPrivate::useSchemeHandler()
{
    // look for an application associated with x-scheme-handler/<protocol>
   const KService::Ptr service = KApplicationTrader::preferredService(QLatin1String("x-scheme-handler/") + m_url.scheme());
    if (service) {
        startService(service);
        return;
    }
    // fallback, look for associated helper protocol
    Q_ASSERT(KProtocolInfo::isHelperProtocol(m_url.scheme()));
    const auto exec = KProtocolInfo::exec(m_url.scheme());
    if (exec.isEmpty()) {
        // use default MIME type opener for file
        m_mimeTypeName = KProtocolManager::defaultMimetype(m_url);
        runUrlWithMimeType();
    } else {
        KService::Ptr service(new KService(QString(), exec, QString()));
        startService(service);
    }
}

void KIO::OpenUrlJobPrivate::statFile()
{
    Q_ASSERT(m_mimeTypeName.isEmpty());

    KIO::StatJob *job = KIO::statDetails(m_url, KIO::StatJob::SourceSide, KIO::StatBasic, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QObject::connect(job, &KJob::result,
                     q, [=]() {
        const int errCode = job->error();
        if (errCode) {
            // ERR_NO_CONTENT is not an error, but an indication no further
            // actions needs to be taken.
            if (errCode != KIO::ERR_NO_CONTENT) {
                q->setError(errCode);
                q->setErrorText(KIO::buildErrorString(errCode, job->errorText()));
            }
            q->emitResult();
            return;
        }
        if (m_followRedirections) { // Update our URL in case of a redirection
            m_url = job->url();
        }

        const KIO::UDSEntry entry = job->statResult();

        const QString localPath = entry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH);
        if (!localPath.isEmpty()) {
            m_url = QUrl::fromLocalFile(localPath);
        }

        // MIME type already known? (e.g. print:/manager)
        m_mimeTypeName = entry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE);
        if (!m_mimeTypeName.isEmpty()) {
            runUrlWithMimeType();
            return;
        }

        if (entry.isDir()) {
            m_mimeTypeName = QStringLiteral("inode/directory");
            runUrlWithMimeType();
        } else { // it's a file
            // Start the timer. Once we get the timer event this
            // protocol server is back in the pool and we can reuse it.
            // This gives better performance than starting a new slave
            QTimer::singleShot(0, q, [this] { scanFileWithGet(); });
        }
    });
}

void KIO::OpenUrlJobPrivate::startService(const KService::Ptr &service, const QList<QUrl> &urls)
{
    KIO::ApplicationLauncherJob *job = new KIO::ApplicationLauncherJob(service, q);
    job->setUrls(urls);
    job->setRunFlags(m_deleteTemporaryFile ? KIO::ApplicationLauncherJob::DeleteTemporaryFiles : KIO::ApplicationLauncherJob::RunFlags{});
    job->setSuggestedFileName(m_suggestedFileName);
    job->setStartupId(m_startupId);
    job->setUiDelegate(q->uiDelegate());
    q->addSubjob(job);
    job->start();
}

static QMimeType fixupMimeType(const QString &mimeType, const QString &fileName)
{
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForName(mimeType);
    if ((!mime.isValid() || mime.isDefault()) && !fileName.isEmpty()) {
        mime = db.mimeTypeForFile(fileName, QMimeDatabase::MatchExtension);
    }
    return mime;
}

void KIO::OpenUrlJobPrivate::scanFileWithGet()
{
    Q_ASSERT(m_mimeTypeName.isEmpty());

    // First, let's check for well-known extensions
    // Not over HTTP and not when there is a query in the URL, in any case.
    if (!m_url.hasQuery() && !m_url.scheme().startsWith(QLatin1String("http"))) {
        QMimeDatabase db;
        QMimeType mime = db.mimeTypeForUrl(m_url);
        if (!mime.isDefault()) {
            //qDebug() << "Scanfile: MIME TYPE is " << mime.name();
            m_mimeTypeName = mime.name();
            runUrlWithMimeType();
            return;
        }
    }

    // No MIME type found, and the URL is not local  (or fast mode not allowed).
    // We need to apply the 'KIO' method, i.e. either asking the server or
    // getting some data out of the file, to know what MIME type it is.

    if (!KProtocolManager::supportsReading(m_url)) {
        qCWarning(KIO_GUI) << "#### NO SUPPORT FOR READING!";
        q->setError(KIO::ERR_CANNOT_READ);
        q->setErrorText(m_url.toDisplayString());
        q->emitResult();
        return;
    }
    //qDebug() << this << "Scanning file" << url;

    KIO::TransferJob *job = KIO::get(m_url, KIO::NoReload /*reload*/, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QObject::connect(job, &KJob::result, q, [=]() {
        const int errCode = job->error();
        if (errCode) {
             // ERR_NO_CONTENT is not an error, but an indication no further
             // actions needs to be taken.
             if (errCode != KIO::ERR_NO_CONTENT) {
                 q->setError(errCode);
                 q->setErrorText(job->errorText());
             }
             q->emitResult();
        }
        // if the job succeeded, we certainly hope it emitted mimetype()...
    });
    QObject::connect(job, &KIO::TransferJob::mimeTypeFound,
                     q, [=](KIO::Job *, const QString &mimetype) {
        if (m_followRedirections) { // Update our URL in case of a redirection
            m_url = job->url();
        }
        if (mimetype.isEmpty()) {
            qCWarning(KIO_GUI) << "get() didn't emit a MIME type! Probably a kioslave bug, please check the implementation of" << m_url.scheme();
        }
        m_mimeTypeName = mimetype;

        // If the current MIME type is the default MIME type, then attempt to
        // determine the "real" MIME type from the file name (bug #279675)
        const QMimeType mime = fixupMimeType(m_mimeTypeName, m_suggestedFileName.isEmpty() ? m_url.fileName() : m_suggestedFileName);
        const QString mimeName = mime.name();
        if (mime.isValid() && mimeName != m_mimeTypeName) {
            m_mimeTypeName = mimeName;
        }

        if (m_suggestedFileName.isEmpty()) {
            m_suggestedFileName = job->queryMetaData(QStringLiteral("content-disposition-filename"));
        }

        job->putOnHold();
        KIO::Scheduler::publishSlaveOnHold();
        runUrlWithMimeType();
    });
}

void KIO::OpenUrlJobPrivate::runLink(const QString &filePath, const QString &urlStr, const QString &optionalServiceName)
{
    if (urlStr.isEmpty()) {
        q->setError(KJob::UserDefinedError);
        q->setErrorText(i18n("The desktop entry file\n%1\nis of type Link but has no URL=... entry.", filePath));
        q->emitResult();
        return;
    }

    m_url = QUrl::fromUserInput(urlStr);
    m_mimeTypeName.clear();

    // X-KDE-LastOpenedWith holds the service desktop entry name that
    // should be preferred for opening this URL if possible.
    // This is used by the Recent Documents menu for instance.
    if (!optionalServiceName.isEmpty()) {
        m_preferredService = KService::serviceByDesktopName(optionalServiceName);
    }

    // Restart from scratch with the target of the link
    q->start();
}

void KIO::OpenUrlJobPrivate::emitAccessDenied()
{
    q->setError(KIO::ERR_ACCESS_DENIED);
    q->setErrorText(KIO::buildErrorString(KIO::ERR_ACCESS_DENIED, m_url.toDisplayString()));
    q->emitResult();
}

// was: KRun::isExecutable (minus application/x-desktop MIME type).
// Feel free to make public if needed.
static bool isBinary(const QMimeType &mimeType)
{
    // - Binaries could be e.g.:
    //   - application/x-executable
    //   - application/x-sharedlib e.g. /usr/bin/ls, see
    //     https://gitlab.freedesktop.org/xdg/shared-mime-info/-/issues/11
    //
    // - MIME types that inherit application/x-executable _and_ text/plain are scripts, these are
    //   handled by handleScripts()

    return (mimeType.inherits(QStringLiteral("application/x-executable"))
            || mimeType.inherits(QStringLiteral("application/x-sharedlib"))
            || mimeType.inherits(QStringLiteral("application/x-ms-dos-executable")));
}

// Helper function that returns whether a file is a text-based script
// e.g. ".sh", ".csh", ".py", ".js"
static bool isTextScript(const QMimeType &mimeType)
{
    return (mimeType.inherits(QStringLiteral("application/x-executable"))
            && mimeType.inherits(QStringLiteral("text/plain")));
}

// Helper function that returns whether a file has the execute bit set or not.
static bool hasExecuteBit(const QString &fileName)
{
    return QFileInfo(fileName).isExecutable();
}

// Handle native binaries (.e.g. /usr/bin/*); and .exe files
void KIO::OpenUrlJobPrivate::handleBinaries(const QMimeType &mimeType)
{
    if (!KAuthorized::authorize(QStringLiteral("shell_access"))) {
        emitAccessDenied();
        return;
    }

    const bool isLocal = m_url.isLocalFile();
    // Don't run remote executables
    if (!isLocal) {
        q->setError(KJob::UserDefinedError);
        q->setErrorText(i18n("The executable file \"%1\" is located on a remote filesystem. "
                             "For safety reasons it will not be started.", m_url.toDisplayString()));
        q->emitResult();
        return;
    }

    const QString localPath = m_url.toLocalFile();

    bool isNativeBinary = true;
#ifndef Q_OS_WIN
    isNativeBinary = !mimeType.inherits(QStringLiteral("application/x-ms-dos-executable"));
#endif

    if (m_showOpenOrExecuteDialog) {
        auto dialogFinished = [this, localPath, isNativeBinary](bool shouldExecute) {
            // shouldExecute is always true if we get here, because for binaries the
            // dialog only offers Execute/Cancel
            Q_UNUSED(shouldExecute)

            handleBinariesHelper(localPath, isNativeBinary);
        };

        // Ask the user for confirmation before executing this binary (for binaries
        // the dialog will only show Execute/Cancel)
        showOpenOrExecuteFileDialog(dialogFinished);
        return;
    }

    handleBinariesHelper(localPath, isNativeBinary);
}

void KIO::OpenUrlJobPrivate::handleBinariesHelper(const QString &localPath, bool isNativeBinary)
{
    // For local .exe files, open in the default app (e.g. WINE)
    if (!isNativeBinary) {
        openInPreferredApp();
        return;
    }

    // Native binaries

    if (!m_runExecutables) {
        q->setError(KJob::UserDefinedError);
        q->setErrorText(i18n("For security reasons, launching executables is not allowed in this context."));
        q->emitResult();
        return;
    }

    if (!hasExecuteBit(localPath)) {
        // Show untrustedProgram dialog for local, native executables without the execute bit
        showUntrustedProgramWarningDialog(localPath);
        return;
    }

    // Local executable with execute bit, proceed
    executeCommand();
}

// For local, native executables (i.e. not shell scripts) without execute bit,
// show a prompt asking the user if he wants to run the program.
void KIO::OpenUrlJobPrivate::showUntrustedProgramWarningDialog(const QString &filePath)
{
    auto *untrustedProgramHandler = KIO::delegateExtension<KIO::UntrustedProgramHandlerInterface *>(q);
    if (!untrustedProgramHandler) {
        // No way to ask the user to make it executable
        q->setError(KJob::UserDefinedError);
        q->setErrorText(i18n("The program \"%1\" needs to have executable permission before it can be launched.", filePath));
        q->emitResult();
        return;
    }
    QObject::connect(untrustedProgramHandler, &KIO::UntrustedProgramHandlerInterface::result, q, [=](bool result) {
        if (result) {
            QString errorString;
            if (untrustedProgramHandler->setExecuteBit(filePath, errorString)) {
                executeCommand();
            } else {
                q->setError(KJob::UserDefinedError);
                q->setErrorText(i18n("Unable to make file \"%1\" executable.\n%2.", filePath, errorString));
                q->emitResult();
            }
        } else {
            q->setError(KIO::ERR_USER_CANCELED);
            q->emitResult();
        }
    });
    untrustedProgramHandler->showUntrustedProgramWarning(q, m_url.fileName());
}

void KIO::OpenUrlJobPrivate::executeCommand()
{
    // Execute the URL as a command. This is how we start scripts and executables
    KIO::CommandLauncherJob *job = new KIO::CommandLauncherJob(m_url.toLocalFile(), QStringList());
    job->setUiDelegate(q->uiDelegate());
    job->setStartupId(m_startupId);
    job->setWorkingDirectory(m_url.adjusted(QUrl::RemoveFilename).toLocalFile());
    q->addSubjob(job);
    job->start();

    // TODO implement deleting the file if tempFile==true
    // CommandLauncherJob doesn't support that, unlike ApplicationLauncherJob
    // We'd have to do it in KProcessRunner.
}

void KIO::OpenUrlJobPrivate::runUrlWithMimeType()
{
    // Tell the app, in case it wants us to stop here
    Q_EMIT q->mimeTypeFound(m_mimeTypeName);
    if (q->error() == KJob::KilledJobError) {
        q->emitResult();
        return;
    }

    // Support for preferred service setting, see setPreferredService
    if (m_preferredService && m_preferredService->hasMimeType(m_mimeTypeName)) {
        startService(m_preferredService);
        return;
    }


    // Scripts and executables
    QMimeDatabase db;
    const QMimeType mimeType = db.mimeTypeForName(m_mimeTypeName);

    // .desktop files
    if (mimeType.inherits(QStringLiteral("application/x-desktop"))) {
        handleDesktopFiles();
        return;
    }

    // Scripts (e.g. .sh, .csh, .py, .js)
    if (isTextScript(mimeType)) {
        handleScripts();
        return;
    }

    // Binaries (e.g. /usr/bin/{konsole,ls}) and .exe files
    if (isBinary(mimeType)) {
        handleBinaries(mimeType);
        return;
    }

    // General case: look up associated application
    openInPreferredApp();
}

void KIO::OpenUrlJobPrivate::handleDesktopFiles()
{
    // Open remote .desktop files in the default (text editor) app
    if (!m_url.isLocalFile()) {
        openInPreferredApp();
        return;
    }

    if (m_url.fileName() == QLatin1String(".directory")) {
        // We cannot execute a .directory file, open in the default app
        m_mimeTypeName = QStringLiteral("text/plain");
        openInPreferredApp();
        return;
    }

    const QString filePath = m_url.toLocalFile();
    KDesktopFile cfg(filePath);
    KConfigGroup cfgGroup = cfg.desktopGroup();
    if (!cfgGroup.hasKey("Type")) {
        q->setError(KJob::UserDefinedError);
        q->setErrorText(i18n("The desktop entry file %1 has no Type=... entry.", filePath));
        q->emitResult();
        openInPreferredApp();
        return;
    }

    if (cfg.hasLinkType()) {
        runLink(filePath, cfg.readUrl(), cfg.desktopGroup().readEntry("X-KDE-LastOpenedWith"));
        return;
    }

    if ((cfg.hasApplicationType() || cfg.readType() == QLatin1String("Service"))) { // kio_settings lets users run Type=Service desktop files
        KService::Ptr service(new KService(filePath));
        if (!service->exec().isEmpty()) {
            if (m_showOpenOrExecuteDialog) { // Show the openOrExecute dialog
                auto dialogFinished = [this, filePath, service](bool shouldExecute) {
                    if (shouldExecute) { // Run the file
                        startService(service, {});
                        return;
                    }
                    // The user selected "open"
                    openInPreferredApp();
                };

                showOpenOrExecuteFileDialog(dialogFinished);
                return;
            }

            if (m_runExecutables) {
                startService(service, {});
                return;
            }
        } // exec is not empty
    } // type Application or Service

    // Fallback to opening in the default app
    openInPreferredApp();
}

void KIO::OpenUrlJobPrivate::handleScripts()
{
    // Executable scripts of any type can run arbitrary shell commands
    if (!KAuthorized::authorize(QStringLiteral("shell_access"))) {
        emitAccessDenied();
        return;
    }

    const bool isLocal = m_url.isLocalFile();
    const QString localPath = m_url.toLocalFile();
    if (!isLocal || !hasExecuteBit(localPath)) {
        // Open remote scripts or ones without the execute bit, with the default application
        openInPreferredApp();
        return;
    }

    if (m_showOpenOrExecuteDialog) {
        auto dialogFinished = [this](bool shouldExecute) {
            if (shouldExecute) {
                executeCommand();
            } else {
                openInPreferredApp();
            }
        };

        showOpenOrExecuteFileDialog(dialogFinished);
        return;
    }

    if (m_runExecutables) { // Local executable script, proceed
        executeCommand();
    } else { // Open in the default (text editor) app
        openInPreferredApp();
    }
}

void KIO::OpenUrlJobPrivate::openInPreferredApp()
{
    KService::Ptr service = KApplicationTrader::preferredService(m_mimeTypeName);
    if (service) {
        startService(service);
    } else {
        showOpenWithDialog();
    }
}

void KIO::OpenUrlJobPrivate::showOpenWithDialog()
{
    if (!KAuthorized::authorizeAction(QStringLiteral("openwith"))) {
        q->setError(KJob::UserDefinedError);
        q->setErrorText(i18n("You are not authorized to select an application to open this file."));
        q->emitResult();
        return;
    }

    auto *openWithHandler = KIO::delegateExtension<KIO::OpenWithHandlerInterface *>(q);
    if (!openWithHandler || QOperatingSystemVersion::currentType() == QOperatingSystemVersion::Windows) {
        // As KDE on windows doesn't know about the windows default applications, offers will be empty in nearly all cases.
        // So we use QDesktopServices::openUrl to let windows decide how to open the file.
        // It's also our fallback if there's no handler to show an open-with dialog.
        if (!QDesktopServices::openUrl(m_url)) {
            q->setError(KJob::UserDefinedError);
            q->setErrorText(i18n("Failed to open the file."));
        }
        q->emitResult();
        return;
    }

    QObject::connect(openWithHandler, &KIO::OpenWithHandlerInterface::canceled, q, [this]() {
        q->setError(KIO::ERR_USER_CANCELED);
        q->emitResult();
    });

    QObject::connect(openWithHandler, &KIO::OpenWithHandlerInterface::serviceSelected, q, [this](const KService::Ptr &service) {
        startService(service);
    });

    QObject::connect(openWithHandler, &KIO::OpenWithHandlerInterface::handled, q, [this]() {
        q->emitResult();
    });

    openWithHandler->promptUserForApplication(q, {m_url}, m_mimeTypeName);
}

void KIO::OpenUrlJobPrivate::showOpenOrExecuteFileDialog(std::function<void(bool)> dialogFinished)
{
    QMimeDatabase db;
    QMimeType mimeType = db.mimeTypeForName(m_mimeTypeName);

    auto *openOrExecuteFileHandler = KIO::delegateExtension<KIO::OpenOrExecuteFileInterface *>(q);
    if (!openOrExecuteFileHandler) {
        // No way to ask the user whether to execute or open
        if (isTextScript(mimeType)
            || mimeType.inherits(QStringLiteral("application/x-desktop"))) { // Open text-based ones in the default app
            openInPreferredApp();
        } else {
            q->setError(KJob::UserDefinedError);
            q->setErrorText(i18n("The program \"%1\" could not be launched.", m_url.toDisplayString(QUrl::PreferLocalFile)));
            q->emitResult();
        }
        return;
    }

    QObject::connect(openOrExecuteFileHandler, &KIO::OpenOrExecuteFileInterface::canceled, q, [this]() {
        q->setError(KIO::ERR_USER_CANCELED);
        q->emitResult();
    });

    QObject::connect(openOrExecuteFileHandler, &KIO::OpenOrExecuteFileInterface::executeFile,
                     q, [this, dialogFinished](bool shouldExecute) {
        m_runExecutables = shouldExecute;
        dialogFinished(shouldExecute);
    });

    openOrExecuteFileHandler->promptUserOpenOrExecute(q, m_mimeTypeName);
}

void KIO::OpenUrlJob::slotResult(KJob *job)
{
    // This is only used for the final application/launcher job, so we're done when it's done
    const int errCode = job->error();
    if (errCode) {
        setError(errCode);
        setErrorText(KIO::buildErrorString(errCode, job->errorText()));
    }
    emitResult();
}
