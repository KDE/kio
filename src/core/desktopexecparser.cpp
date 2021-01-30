/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2006-2013 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2009 Michael Pyne <michael.pyne@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "desktopexecparser.h"
#include "kiofuse_interface.h"

#include <KMacroExpander>
#include <KShell>
#include <KSharedConfig>
#include <KDesktopFile>
#include <KService>
#include <KConfigGroup>
#include <kprotocolinfo.h>
#include <KApplicationTrader>
#include <KLocalizedString>

#include <QFile>
#include <QDir>
#include <QUrl>
#include <QStandardPaths>
#include <QDBusConnection>
#include <QDBusReply>

#include <config-kiocore.h> // KDE_INSTALL_FULL_LIBEXECDIR_KF5

#include "kiocoredebug.h"

class KRunMX1 : public KMacroExpanderBase
{
public:
    explicit KRunMX1(const KService &_service)
        : KMacroExpanderBase(QLatin1Char('%'))
        , hasUrls(false)
        , hasSpec(false)
        , service(_service)
    {}

    bool hasUrls;
    bool hasSpec;

protected:
    int expandEscapedMacro(const QString &str, int pos, QStringList &ret) override;

private:
    const KService &service;
};

int KRunMX1::expandEscapedMacro(const QString &str, int pos, QStringList &ret)
{
    uint option = str[pos + 1].unicode();
    switch (option) {
    case 'c':
        ret << service.name().replace(QLatin1Char('%'), QLatin1String("%%"));
        break;
    case 'k':
        ret << service.entryPath().replace(QLatin1Char('%'), QLatin1String("%%"));
        break;
    case 'i':
        ret << QStringLiteral("--icon") << service.icon().replace(QLatin1Char('%'), QLatin1String("%%"));
        break;
    case 'm':
//       ret << "-miniicon" << service.icon().replace( '%', "%%" );
        qCWarning(KIO_CORE) << "-miniicon isn't supported anymore (service"
                   << service.name() << ')';
        break;
    case 'u':
    case 'U':
        hasUrls = true;
        Q_FALLTHROUGH();
    /* fallthrough */
    case 'f':
    case 'F':
    case 'n':
    case 'N':
    case 'd':
    case 'D':
    case 'v':
        hasSpec = true;
        Q_FALLTHROUGH();
    /* fallthrough */
    default:
        return -2; // subst with same and skip
    }
    return 2;
}

class KRunMX2 : public KMacroExpanderBase
{
public:
    explicit KRunMX2(const QList<QUrl> &_urls)
        : KMacroExpanderBase(QLatin1Char('%'))
        , ignFile(false),
        urls(_urls)
    {}

    bool ignFile;

protected:
    int expandEscapedMacro(const QString &str, int pos, QStringList &ret) override;

private:
    void subst(int option, const QUrl &url, QStringList &ret);

    const QList<QUrl> &urls;
};

void KRunMX2::subst(int option, const QUrl &url, QStringList &ret)
{
    switch (option) {
    case 'u':
        ret << ((url.isLocalFile() && url.fragment().isNull() && url.query().isNull()) ?
                QDir::toNativeSeparators(url.toLocalFile())  : url.toString());
        break;
    case 'd':
        ret << url.adjusted(QUrl::RemoveFilename).path();
        break;
    case 'f':
        ret << QDir::toNativeSeparators(url.toLocalFile());
        break;
    case 'n':
        ret << url.fileName();
        break;
    case 'v':
        if (url.isLocalFile() && QFile::exists(url.toLocalFile())) {
            ret << KDesktopFile(url.toLocalFile()).desktopGroup().readEntry("Dev");
        }
        break;
    }
    return;
}

int KRunMX2::expandEscapedMacro(const QString &str, int pos, QStringList &ret)
{
    uint option = str[pos + 1].unicode();
    switch (option) {
    case 'f':
    case 'u':
    case 'n':
    case 'd':
    case 'v':
        if (urls.isEmpty()) {
            if (!ignFile) {
                //qCDebug(KIO_CORE) << "No URLs supplied to single-URL service" << str;
            }
        } else if (urls.count() > 1) {
            qCWarning(KIO_CORE) << urls.count() << "URLs supplied to single-URL service" << str;
        } else {
            subst(option, urls.first(), ret);
        }
        break;
    case 'F':
    case 'U':
    case 'N':
    case 'D':
        option += 'a' - 'A';
        for (const QUrl &url : urls) {
            subst(option, url, ret);
        }
        break;
    case '%':
        ret = QStringList(QStringLiteral("%"));
        break;
    default:
        return -2; // subst with same and skip
    }
    return 2;
}

QStringList KIO::DesktopExecParser::supportedProtocols(const KService &service)
{
    QStringList supportedProtocols = service.property(QStringLiteral("X-KDE-Protocols")).toStringList();
    KRunMX1 mx1(service);
    QString exec = service.exec();
    if (mx1.expandMacrosShellQuote(exec) && !mx1.hasUrls) {
        if (!supportedProtocols.isEmpty()) {
            qCWarning(KIO_CORE) << service.entryPath() << "contains a X-KDE-Protocols line but doesn't use %u or %U in its Exec line! This is inconsistent.";
        }
        return QStringList();
    } else {
        if (supportedProtocols.isEmpty()) {
            // compat mode: assume KIO if not set and it's a KDE app (or a KDE service)
            const QStringList categories = service.property(QStringLiteral("Categories")).toStringList();
            if (categories.contains(QLatin1String("KDE"))
                    || !service.isApplication()
                    || service.entryPath().isEmpty() /*temp service*/) {
                supportedProtocols.append(QStringLiteral("KIO"));
            } else { // if no KDE app, be a bit over-generic
                supportedProtocols.append(QStringLiteral("http"));
                supportedProtocols.append(QStringLiteral("https")); // #253294
                supportedProtocols.append(QStringLiteral("ftp"));
            }
        }
    }

    // add x-scheme-handler/<protocol>
    const auto servicesTypes = service.serviceTypes();
    for (const auto &mimeType : servicesTypes) {
        if (mimeType.startsWith(QLatin1String("x-scheme-handler/"))) {
            supportedProtocols << mimeType.mid(17);
        }
    }

    //qCDebug(KIO_CORE) << "supportedProtocols:" << supportedProtocols;
    return supportedProtocols;
}

bool KIO::DesktopExecParser::isProtocolInSupportedList(const QUrl &url, const QStringList &supportedProtocols)
{
    if (supportedProtocols.contains(QLatin1String("KIO"))) {
        return true;
    }
    return url.isLocalFile() || supportedProtocols.contains(url.scheme().toLower());
}

// We have up to two sources of data, for protocols not handled by kioslaves (so called "helper") :
// 1) the exec line of the .protocol file, if there's one
// 2) the application associated with x-scheme-handler/<protocol> if there's one

// If both exist, then:
//  A) if the .protocol file says "launch an application", then the new-style handler-app has priority
//  B) but if the .protocol file is for a kioslave (e.g. kio_http) then this has priority over
//     firefox or chromium saying x-scheme-handler/http. Gnome people want to send all HTTP urls
//     to a webbrowser, but we want MIME-type-determination-in-calling-application by default
//     (the user can configure a BrowserApplication though)
bool KIO::DesktopExecParser::hasSchemeHandler(const QUrl &url)
{
    if (KProtocolInfo::isHelperProtocol(url)) {
        return true;
    }
    if (KProtocolInfo::isKnownProtocol(url)) {
        return false; // this is case B, we prefer kioslaves over the competition
    }
    const KService::Ptr service = KApplicationTrader::preferredService(QLatin1String("x-scheme-handler/") + url.scheme());
    if (service) {
        qCDebug(KIO_CORE) << QLatin1String("preferred service for x-scheme-handler/") + url.scheme() << service->desktopEntryName();
    }
    return service;
}

class KIO::DesktopExecParserPrivate
{
public:
    DesktopExecParserPrivate(const KService &_service, const QList<QUrl> &_urls)
        : service(_service), urls(_urls), tempFiles(false) {}

    const KService &service;
    QList<QUrl> urls;
    bool tempFiles;
    QString suggestedFileName;
    QString m_errorString;
};

KIO::DesktopExecParser::DesktopExecParser(const KService &service, const QList<QUrl> &urls)
    : d(new DesktopExecParserPrivate(service, urls))
{
}

KIO::DesktopExecParser::~DesktopExecParser()
{
}

void KIO::DesktopExecParser::setUrlsAreTempFiles(bool tempFiles)
{
    d->tempFiles = tempFiles;
}

void KIO::DesktopExecParser::setSuggestedFileName(const QString &suggestedFileName)
{
    d->suggestedFileName = suggestedFileName;
}

static const QString kioexecPath()
{
    QString kioexec = QCoreApplication::applicationDirPath() + QLatin1String("/kioexec");
    if (!QFileInfo::exists(kioexec))
        kioexec = QStringLiteral(KDE_INSTALL_FULL_LIBEXECDIR_KF5 "/kioexec");
    Q_ASSERT(QFileInfo::exists(kioexec));
    return kioexec;
}

static QString findNonExecutableProgram(const QString &executable)
{
    // Relative to current dir, or absolute path
    const QFileInfo fi(executable);
    if (fi.exists() && !fi.isExecutable()) {
        return executable;
    }

#ifdef Q_OS_UNIX
    // This is a *very* simplified version of QStandardPaths::findExecutable
    const QStringList searchPaths = QString::fromLocal8Bit(qgetenv("PATH")).split(QDir::listSeparator(), Qt::SkipEmptyParts);
    for (const QString &searchPath : searchPaths) {
        const QString candidate = searchPath + QLatin1Char('/') + executable;
        const QFileInfo fileInfo(candidate);
        if (fileInfo.exists()) {
            if (fileInfo.isExecutable()) {
                qWarning() << "Internal program error. QStandardPaths::findExecutable couldn't find" << executable << "but our own logic found it at" << candidate << ". Please report a bug at https://bugs.kde.org";
            } else {
                return candidate;
            }
        }
    }
#endif
    return QString();
}

QStringList KIO::DesktopExecParser::resultingArguments() const
{
    QString exec = d->service.exec();
    if (exec.isEmpty()) {
        d->m_errorString = i18n("No Exec field in %1", d->service.entryPath());
        qCWarning(KIO_CORE) << "No Exec field in" << d->service.entryPath();
        return QStringList();
    }

    // Extract the name of the binary to execute from the full Exec line, to see if it exists
    const QString binary = executablePath(exec);
    QString executableFullPath;
    if (!binary.isEmpty()) { // skip all this if the Exec line is a complex shell command
        if (QDir::isRelativePath(binary)) {
            // Resolve the executable to ensure that helpers in libexec are found.
            // Too bad for commands that need a shell - they must reside in $PATH.
            executableFullPath = QStandardPaths::findExecutable(binary);
            if (executableFullPath.isEmpty()) {
                executableFullPath = QFile::decodeName(KDE_INSTALL_FULL_LIBEXECDIR_KF5 "/") + binary;
            }
        } else {
            executableFullPath = binary;
        }

        // Now check that the binary exists and has the executable flag
        if (!QFileInfo(executableFullPath).isExecutable()) {
            // Does it really not exist, or is it non-executable (on Unix)? (bug #415567)
            const QString nonExecutable = findNonExecutableProgram(binary);
            if (nonExecutable.isEmpty()) {
                d->m_errorString = i18n("Could not find the program '%1'", binary);
            } else {
                if (QDir::isRelativePath(binary)) {
                    d->m_errorString = i18n("The program '%1' was found at '%2' but it is missing executable permissions.", binary, nonExecutable);
                } else {
                    d->m_errorString = i18n("The program '%1' is missing executable permissions.", nonExecutable);
                }
            }
            return QStringList();
        }
    }

    QStringList result;
    bool appHasTempFileOption;

    KRunMX1 mx1(d->service);
    KRunMX2 mx2(d->urls);

    if (!mx1.expandMacrosShellQuote(exec)) {    // Error in shell syntax
        d->m_errorString = i18n("Syntax error in command %1 coming from %2", exec, d->service.entryPath());
        qCWarning(KIO_CORE) << "Syntax error in command" << d->service.exec() << ", service" << d->service.name();
        return QStringList();
    }

    // FIXME: the current way of invoking kioexec disables term and su use

    // Check if we need "tempexec" (kioexec in fact)
    appHasTempFileOption = d->tempFiles && d->service.property(QStringLiteral("X-KDE-HasTempFileOption")).toBool();
    if (d->tempFiles && !appHasTempFileOption && d->urls.size()) {
        result << kioexecPath() << QStringLiteral("--tempfiles") << exec;
        if (!d->suggestedFileName.isEmpty()) {
            result << QStringLiteral("--suggestedfilename");
            result << d->suggestedFileName;
        }
        result += QUrl::toStringList(d->urls);
        return result;
    }

    // Return true for non-KIO desktop files with explicit X-KDE-Protocols list, like vlc, for the special case below
    auto isNonKIO = [this]() {
        const QStringList protocols = d->service.property(QStringLiteral("X-KDE-Protocols")).toStringList();
        return !protocols.isEmpty() && !protocols.contains(QLatin1String("KIO"));
    };

    // Check if we need kioexec, or KIOFuse
    bool useKioexec = false;
    org::kde::KIOFuse::VFS kiofuse_iface(QStringLiteral("org.kde.KIOFuse"),
                                         QStringLiteral("/org/kde/KIOFuse"),
                                         QDBusConnection::sessionBus());
    struct MountRequest { QDBusPendingReply<QString> reply; int urlIndex; };
    QVector<MountRequest> requests;
    requests.reserve(d->urls.count());
    const QStringList appSupportedProtocols = supportedProtocols(d->service);
    for (int i = 0; i < d->urls.count(); ++i)  {
        const QUrl url = d->urls.at(i);
        const bool supported = mx1.hasUrls ? isProtocolInSupportedList(url, appSupportedProtocols) : url.isLocalFile();
        if (!supported) {
            // if FUSE fails, we'll have to fallback to kioexec
            useKioexec = true;
        }
        // NOTE: Some non-KIO apps may support the URLs (e.g. VLC supports smb://)
        // but will not have the password if they are not in the URL itself.
        // Hence convert URL to KIOFuse equivalent in case there is a password.
        // @see https://pointieststick.com/2018/01/17/videos-on-samba-shares/
        // @see https://bugs.kde.org/show_bug.cgi?id=330192
        if (!supported || (!url.userName().isEmpty() && url.password().isEmpty() && isNonKIO())) {
            requests.push_back({ kiofuse_iface.mountUrl(url.toString()), i });
        }
    }

    for (auto &request : requests) {
        request.reply.waitForFinished();
    }
    const bool fuseError = std::any_of(requests.cbegin(), requests.cend(), [](const MountRequest &request) { return request.reply.isError(); });

    if (fuseError && useKioexec) {
        // We need to run the app through kioexec
        result << kioexecPath();
        if (d->tempFiles) {
            result << QStringLiteral("--tempfiles");
        }
        if (!d->suggestedFileName.isEmpty()) {
            result << QStringLiteral("--suggestedfilename");
            result << d->suggestedFileName;
        }
        result << exec;
        result += QUrl::toStringList(d->urls);
        return result;
    }

    // At this point we know we're not using kioexec, so feel free to replace
    // KIO URLs with their KIOFuse local path.
    for (const auto &request : qAsConst(requests)) {
        if (!request.reply.isError()) {
            d->urls[request.urlIndex] = QUrl::fromLocalFile(request.reply.value());
        }
    }

    if (appHasTempFileOption) {
        exec += QLatin1String(" --tempfile");
    }

    // Did the user forget to append something like '%f'?
    // If so, then assume that '%f' is the right choice => the application
    // accepts only local files.
    if (!mx1.hasSpec) {
        exec += QLatin1String(" %f");
        mx2.ignFile = true;
    }

    mx2.expandMacrosShellQuote(exec);   // syntax was already checked, so don't check return value

    /*
     1 = need_shell, 2 = terminal, 4 = su

     0                                                           << split(cmd)
     1                                                           << "sh" << "-c" << cmd
     2 << split(term) << "-e"                                    << split(cmd)
     3 << split(term) << "-e"                                    << "sh" << "-c" << cmd

     4                        << "kdesu" << "-u" << user << "-c" << cmd
     5                        << "kdesu" << "-u" << user << "-c" << ("sh -c " + quote(cmd))
     6 << split(term) << "-e" << "su"            << user << "-c" << cmd
     7 << split(term) << "-e" << "su"            << user << "-c" << ("sh -c " + quote(cmd))

     "sh -c" is needed in the "su" case, too, as su uses the user's login shell, not sh.
     this could be optimized with the -s switch of some su versions (e.g., debian linux).
    */

    if (d->service.terminal()) {
        KConfigGroup cg(KSharedConfig::openConfig(), "General");
        QString terminal = cg.readPathEntry("TerminalApplication", QStringLiteral("konsole"));
        const bool isKonsole = (terminal == QLatin1String("konsole"));

        QString terminalPath = QStandardPaths::findExecutable(terminal);
        if (terminalPath.isEmpty()) {
            d->m_errorString = i18n("Terminal %1 not found while trying to run %2", terminal, d->service.entryPath());
            qCWarning(KIO_CORE) << "Terminal" << terminal << "not found, service" << d->service.name();
            return QStringList();
        }
        terminal = terminalPath;
        if (isKonsole) {
            if (!d->service.workingDirectory().isEmpty()) {
                terminal += QLatin1String(" --workdir ") + KShell::quoteArg(d->service.workingDirectory());
            }
            terminal += QLatin1String(" -qwindowtitle '%c'");
            if(!d->service.icon().isEmpty()) {
                terminal += QLatin1String(" -qwindowicon ") + KShell::quoteArg(d->service.icon().replace(QLatin1Char('%'), QLatin1String("%%")));
            }
        }
        terminal += QLatin1Char(' ') + d->service.terminalOptions();
        if (!mx1.expandMacrosShellQuote(terminal)) {
            d->m_errorString = i18n("Syntax error in command %1 while trying to run %2", terminal, d->service.entryPath());
            qCWarning(KIO_CORE) << "Syntax error in command" << terminal << ", service" << d->service.name();
            return QStringList();
        }
        mx2.expandMacrosShellQuote(terminal);
        result = KShell::splitArgs(terminal);   // assuming that the term spec never needs a shell!
        result << QStringLiteral("-e");
    }

    KShell::Errors err;
    QStringList execlist = KShell::splitArgs(exec, KShell::AbortOnMeta | KShell::TildeExpand, &err);
    if (!executableFullPath.isEmpty()) {
        execlist[0] = executableFullPath;
    }

    if (d->service.substituteUid()) {
        if (d->service.terminal()) {
            result << QStringLiteral("su");
        } else {
            QString kdesu = QFile::decodeName(KDE_INSTALL_FULL_LIBEXECDIR_KF5 "/kdesu");
            if (!QFile::exists(kdesu)) {
                kdesu = QStandardPaths::findExecutable(QStringLiteral("kdesu"));
            }
            if (!QFile::exists(kdesu)) {
                // Insert kdesu as string so we show a nice warning: 'Could not launch kdesu'
                result << QStringLiteral("kdesu");
                return result;
            } else {
                result << kdesu << QStringLiteral("-u");
            }
        }

        result << d->service.username() << QStringLiteral("-c");
        if (err == KShell::FoundMeta) {
            exec = QLatin1String("/bin/sh -c ") + KShell::quoteArg(exec);
        } else {
            exec = KShell::joinArgs(execlist);
        }
        result << exec;
    } else {
        if (err == KShell::FoundMeta) {
            result << QStringLiteral("/bin/sh") << QStringLiteral("-c") << exec;
        } else {
            result += execlist;
        }
    }

    return result;
}

QString KIO::DesktopExecParser::errorMessage() const
{
    return d->m_errorString;
}

//static
QString KIO::DesktopExecParser::executableName(const QString &execLine)
{
    const QString bin = executablePath(execLine);
    return bin.mid(bin.lastIndexOf(QLatin1Char('/')) + 1);
}

//static
QString KIO::DesktopExecParser::executablePath(const QString &execLine)
{
    // Remove parameters and/or trailing spaces.
    const QStringList args = KShell::splitArgs(execLine, KShell::AbortOnMeta | KShell::TildeExpand);
    for (const QString &arg : args) {
        if (!arg.contains(QLatin1Char('='))) {
            return arg;
        }
    }
    return QString();
}

