/* This file is part of the KDE libraries
    Copyright (C) 2000 Torben Weis <weis@kde.org>
    Copyright (C) 2006-2013 David Faure <faure@kde.org>
    Copyright (C) 2009 Michael Pyne <michael.pyne@kdemail.net>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "desktopexecparser.h"

#include <kmacroexpander.h>
#include <kshell.h>
#include <kdesktopfile.h>
#include <kservice.h>
#include <kconfiggroup.h>
#include <kprotocolinfo.h>
#include <kmimetypetrader.h>

#include <QFile>
#include <QDir>
#include <QDebug>
#include <QUrl>
#include <QStandardPaths>

#include <config-kiocore.h> // CMAKE_INSTALL_PREFIX

class KRunMX1 : public KMacroExpanderBase
{
public:
    KRunMX1(const KService &_service) :
            KMacroExpanderBase('%'), hasUrls(false), hasSpec(false), service(_service) {}

    bool hasUrls;
    bool hasSpec;

protected:
    int expandEscapedMacro(const QString &str, int pos, QStringList &ret) Q_DECL_OVERRIDE;

private:
    const KService &service;
};

int KRunMX1::expandEscapedMacro(const QString &str, int pos, QStringList &ret)
{
    uint option = str[pos + 1].unicode();
    switch (option) {
    case 'c':
        ret << service.name().replace('%', "%%");
        break;
    case 'k':
        ret << service.entryPath().replace('%', "%%");
        break;
    case 'i':
        ret << "--icon" << service.icon().replace('%', "%%");
        break;
    case 'm':
//       ret << "-miniicon" << service.icon().replace( '%', "%%" );
        qWarning() << "-miniicon isn't supported anymore (service"
        << service.name() << ')';
        break;
    case 'u':
    case 'U':
        hasUrls = true;
        /* fallthrough */
    case 'f':
    case 'F':
    case 'n':
    case 'N':
    case 'd':
    case 'D':
    case 'v':
        hasSpec = true;
        /* fallthrough */
    default:
        return -2; // subst with same and skip
    }
    return 2;
}

class KRunMX2 : public KMacroExpanderBase
{
public:
    KRunMX2(const QList<QUrl> &_urls) :
            KMacroExpanderBase('%'), ignFile(false), urls(_urls) {}

    bool ignFile;

protected:
    int expandEscapedMacro(const QString &str, int pos, QStringList &ret) Q_DECL_OVERRIDE;

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
                //qDebug() << "No URLs supplied to single-URL service" << str;
            }
        }
        else if (urls.count() > 1) {
            qWarning() << urls.count() << "URLs supplied to single-URL service" << str;
        }
        else {
            subst(option, urls.first(), ret);
        }
        break;
    case 'F':
    case 'U':
    case 'N':
    case 'D':
        option += 'a' - 'A';
        for (QList<QUrl>::ConstIterator it = urls.begin(); it != urls.end(); ++it)
            subst(option, *it, ret);
        break;
    case '%':
        ret = QStringList(QLatin1String("%"));
        break;
    default:
        return -2; // subst with same and skip
    }
    return 2;
}

QStringList KIO::DesktopExecParser::supportedProtocols(const KService& service)
{
    QStringList supportedProtocols = service.property("X-KDE-Protocols").toStringList();
    KRunMX1 mx1(service);
    QString exec = service.exec();
    if (mx1.expandMacrosShellQuote(exec) && !mx1.hasUrls) {
        Q_ASSERT(supportedProtocols.isEmpty());   // huh? If you support protocols you need %u or %U...
    }
    else {
        if (supportedProtocols.isEmpty()) {
            // compat mode: assume KIO if not set and it's a KDE app (or a KDE service)
            const QStringList categories = service.property("Categories").toStringList();
            if (categories.contains("KDE")
                    || !service.isApplication()
                    || service.entryPath().isEmpty() /*temp service*/) {
                supportedProtocols.append("KIO");
            }
            else { // if no KDE app, be a bit over-generic
                supportedProtocols.append("http");
                supportedProtocols.append("https"); // #253294
                supportedProtocols.append("ftp");
            }
        }
    }
    //qDebug() << "supportedProtocols:" << supportedProtocols;
    return supportedProtocols;
}

bool KIO::DesktopExecParser::isProtocolInSupportedList(const QUrl &url, const QStringList &supportedProtocols)
{
    if (supportedProtocols.contains("KIO"))
        return true;
    return url.isLocalFile() || supportedProtocols.contains(url.scheme().toLower());
}

bool KIO::DesktopExecParser::hasSchemeHandler(const QUrl& url)
{
    if (KProtocolInfo::isHelperProtocol(url)) {
        return true;
    }
    if (KProtocolInfo::isKnownProtocol(url)) {
        return false; // see schemeHandler()... this is case B, we prefer kioslaves over the competition
    }
    const KService::Ptr service = KMimeTypeTrader::self()->preferredService(QString::fromLatin1("x-scheme-handler/") + url.scheme());
    if (service)
        qDebug() << "preferred service for x-scheme-handler/" + url.scheme() << service->desktopEntryName();
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


QStringList KIO::DesktopExecParser::resultingArguments() const
{
    QString exec = d->service.exec();
    if (exec.isEmpty()) {
        qWarning() << "KRun: no Exec field in `" << d->service.entryPath() << "' !";
        return QStringList();
    }

    QStringList result;
    bool appHasTempFileOption;

    KRunMX1 mx1(d->service);
    KRunMX2 mx2(d->urls);

    if (!mx1.expandMacrosShellQuote(exec)) {    // Error in shell syntax
        qWarning() << "KRun: syntax error in command" << d->service.exec() << ", service" << d->service.name();
        return QStringList();
    }

    // FIXME: the current way of invoking kioexec disables term and su use

    // Check if we need "tempexec" (kioexec in fact)
    appHasTempFileOption = d->tempFiles && d->service.property("X-KDE-HasTempFileOption").toBool();
    if (d->tempFiles && !appHasTempFileOption && d->urls.size()) {
        const QString kioexec = QFile::decodeName(CMAKE_INSTALL_PREFIX "/" LIBEXEC_INSTALL_DIR "/kioexec");
        Q_ASSERT(QFile::exists(kioexec));
        result << kioexec << "--tempfiles" << exec;
        if (!d->suggestedFileName.isEmpty()) {
            result << "--suggestedfilename";
            result << d->suggestedFileName;
        }
        result += QUrl::toStringList(d->urls);
        return result;
    }

    // Check if we need kioexec
    bool useKioexec = false;
    if (!mx1.hasUrls) {
        Q_FOREACH (const QUrl& url, d->urls)
            if (!url.isLocalFile() && !hasSchemeHandler(url)) {
                useKioexec = true;
                //qDebug() << "non-local files, application does not support urls, using kioexec";
                break;
            }
    } else { // app claims to support %u/%U, check which protocols
        QStringList appSupportedProtocols = supportedProtocols(d->service);
        Q_FOREACH (const QUrl& url, d->urls) {
            if (!isProtocolInSupportedList(url, appSupportedProtocols) && !hasSchemeHandler(url)) {
                useKioexec = true;
                //qDebug() << "application does not support url, using kioexec:" << url;
                break;
            }
        }
    }
    if (useKioexec) {
        // We need to run the app through kioexec
        const QString kioexec = CMAKE_INSTALL_PREFIX "/" LIBEXEC_INSTALL_DIR "/kioexec";
        Q_ASSERT(QFile::exists(kioexec));
        result << kioexec;
        if (d->tempFiles) {
            result << "--tempfiles";
        }
        if (!d->suggestedFileName.isEmpty()) {
            result << "--suggestedfilename";
            result << d->suggestedFileName;
        }
        result << exec;
        result += QUrl::toStringList(d->urls);
        return result;
    }

    if (appHasTempFileOption) {
        exec += " --tempfile";
    }

    // Did the user forget to append something like '%f'?
    // If so, then assume that '%f' is the right choice => the application
    // accepts only local files.
    if (!mx1.hasSpec) {
        exec += " %f";
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
        QString terminal = cg.readPathEntry("TerminalApplication", "konsole");
        if (terminal == "konsole") {
            if (!d->service.path().isEmpty()) {
                terminal += " --workdir " + KShell::quoteArg(d->service.path());
            }
            terminal += " -caption=%c %i %m";
        }
        terminal += ' ';
        terminal += d->service.terminalOptions();
        if (!mx1.expandMacrosShellQuote(terminal)) {
            qWarning() << "KRun: syntax error in command" << terminal << ", service" << d->service.name();
            return QStringList();
        }
        mx2.expandMacrosShellQuote(terminal);
        result = KShell::splitArgs(terminal);   // assuming that the term spec never needs a shell!
        result << "-e";
    }

    KShell::Errors err;
    QStringList execlist = KShell::splitArgs(exec, KShell::AbortOnMeta | KShell::TildeExpand, &err);
    if (err == KShell::NoError && !execlist.isEmpty()) { // mx1 checked for syntax errors already
        // Resolve the executable to ensure that helpers in libexec are found.
        // Too bad for commands that need a shell - they must reside in $PATH.
        QString exePath = QStandardPaths::findExecutable(execlist.first());
        if (exePath.isEmpty()) {
            exePath = QFile::decodeName(CMAKE_INSTALL_PREFIX "/" LIBEXEC_INSTALL_DIR "/") + execlist.first();
        }
        if (QFile::exists(exePath)) {
            execlist[0] = exePath;
        }
    }
    if (d->service.substituteUid()) {
        if (d->service.terminal()) {
            result << "su";
        }
        else {
            result << QStandardPaths::findExecutable("kdesu") << "-u";
        }

        result << d->service.username() << "-c";
        if (err == KShell::FoundMeta) {
            exec = "/bin/sh -c " + KShell::quoteArg(exec);
        }
        else {
            exec = KShell::joinArgs(execlist);
        }
        result << exec;
    }
    else {
        if (err == KShell::FoundMeta) {
            result << "/bin/sh" << "-c" << exec;
        }
        else {
            result += execlist;
        }
    }

    return result;
}

//static
QString KIO::DesktopExecParser::executableName(const QString &execLine)
{
    const QString bin = executablePath(execLine);
    return bin.mid(bin.lastIndexOf('/') + 1);
}

//static
QString KIO::DesktopExecParser::executablePath(const QString &execLine)
{
    // Remove parameters and/or trailing spaces.
    const QStringList args = KShell::splitArgs(execLine);
    for (QStringList::ConstIterator it = args.begin(); it != args.end(); ++it) {
        if (!(*it).contains('=')) {
            return *it;
        }
    }
    return QString();
}

