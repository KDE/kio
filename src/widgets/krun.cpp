/* This file is part of the KDE libraries
    Copyright (C) 2000 Torben Weis <weis@kde.org>
    Copyright (C) 2006 David Faure <faure@kde.org>
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

#include "krun.h"
#include "krun_p.h"
#include <config-kiowidgets.h> // HAVE_X11

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <typeinfo>
#include <sys/stat.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QApplication>
#include <QDesktopWidget>
#include <qmimedatabase.h>
#include <QDebug>

#include <kiconloader.h>
#include <kjobuidelegate.h>
#include <kmimetypetrader.h>
#include "kio/jobclasses.h" // for KIO::JobFlags
#include "kio/job.h"
#include "kio/global.h"
#include "kio/scheduler.h"
#include "kopenwithdialog.h"
#include "krecentdocument.h"
#include "kdesktopfileactions.h"
#include <kio/desktopexecparser.h>

#include <kurlauthorized.h>
#include <kmessagebox.h>
#include <ktoolinvocation.h>
#include <klocalizedstring.h>
#include <kprotocolmanager.h>
#include <kprocess.h>
#include <kiconloader.h>
#include <kjobwidgets.h>
#include <ksharedconfig.h>

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <QtCore/QDate>
#include <QtCore/QRegExp>
#include <QDir>
#include <kdesktopfile.h>
#include <kshell.h>
#include <kconfiggroup.h>
#include <kstandardguiitem.h>
#include <kguiitem.h>
#include <qsavefile.h>

#if HAVE_X11
#include <kwindowsystem.h>
#elif defined(Q_OS_WIN)
#include <QDesktopServices>
#endif
#include <qplatformdefs.h>
#include <qstandardpaths.h>

KRun::KRunPrivate::KRunPrivate(KRun *parent)
        : q(parent),
        m_showingDialog(false)
{
}

void KRun::KRunPrivate::startTimer()
{
    m_timer.start(0);
}

// ---------------------------------------------------------------------------

static QString schemeHandler(const QString& protocol)
{
    // We have up to two sources of data, for protocols not handled by kioslaves (so called "helper") :
    // 1) the exec line of the .protocol file, if there's one
    // 2) the application associated with x-scheme-handler/<protocol> if there's one

    // If both exist, then:
    //  A) if the .protocol file says "launch an application", then the new-style handler-app has priority
    //  B) but if the .protocol file is for a kioslave (e.g. kio_http) then this has priority over
    //     firefox or chromium saying x-scheme-handler/http. Gnome people want to send all HTTP urls
    //     to a webbrowser, but we want mimetype-determination-in-calling-application by default
    //     (the user can configure a BrowserApplication though)

    const KService::Ptr service = KMimeTypeTrader::self()->preferredService(QString::fromLatin1("x-scheme-handler/") + protocol);
    if (service) {
        return service->exec(); // for helper protocols, the handler app has priority over the hardcoded one (see A above)
    }
    Q_ASSERT(KProtocolInfo::isHelperProtocol(protocol));
    return KProtocolInfo::exec(protocol);
}

// ---------------------------------------------------------------------------

bool KRun::isExecutableFile(const QUrl& url, const QString &mimetype)
{
    if (!url.isLocalFile()) {
        return false;
    }
    QFileInfo file(url.toLocalFile());
    if (file.isExecutable()) {    // Got a prospective file to run
        QMimeDatabase db;
        QMimeType mimeType = db.mimeTypeForName(mimetype);
        if (mimeType.inherits(QLatin1String("application/x-executable")) ||
#ifdef Q_OS_WIN
            mimeType.inherits(QLatin1String("application/x-ms-dos-executable")) ||
#endif
            mimeType.inherits(QLatin1String("application/x-executable-script"))
           ) {
            return true;
        }
    }
    return false;
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

// This is called by foundMimeType, since it knows the mimetype of the URL
bool KRun::runUrl(const QUrl& u, const QString& _mimetype, QWidget* window, bool tempFile, bool runExecutables, const QString& suggestedFileName, const QByteArray& asn)
{
    bool noRun = false;
    bool noAuth = false;
    if (_mimetype == QLatin1String("inode/directory-locked")) {
        KMessageBox::error(window,
                                  i18n("<qt>Unable to enter <b>%1</b>.\nYou do not have access rights to this location.</qt>", u.toDisplayString().toHtmlEscaped()));
        return false;
    }
    else if (_mimetype == QLatin1String("application/x-desktop")) {
        if (u.isLocalFile() && runExecutables) {
            return KDesktopFileActions::run(u, true);
        }
    }
    else if (isExecutableFile(u, _mimetype)) {
        if (u.isLocalFile() && runExecutables) {
            if (KAuthorized::authorize("shell_access")) {
                return (KRun::runCommand(KShell::quoteArg(u.toLocalFile()), QString(), QString(), window, asn, u.adjusted(QUrl::RemoveFilename).path())); // just execute the url as a command
                // ## TODO implement deleting the file if tempFile==true
            }
            else {
                noAuth = true;
            }
        }
        else if (_mimetype == QLatin1String("application/x-executable")) {
            noRun = true;
        }
    }
    else if (isExecutable(_mimetype)) {
        if (!runExecutables) {
            noRun = true;
        }

        if (!KAuthorized::authorize("shell_access")) {
            noAuth = true;
        }
    }

    if (noRun) {
        KMessageBox::sorry(window,
                           i18n("<qt>The file <b>%1</b> is an executable program. "
                                "For safety it will not be started.</qt>", u.toDisplayString().toHtmlEscaped()));
        return false;
    }
    if (noAuth) {
        KMessageBox::error(window,
                                  i18n("<qt>You do not have permission to run <b>%1</b>.</qt>", u.toDisplayString().toHtmlEscaped()));
        return false;
    }

    QList<QUrl> lst;
    lst.append(u);

    KService::Ptr offer = KMimeTypeTrader::self()->preferredService(_mimetype);

    if (!offer) {
#ifdef Q_OS_WIN
        // As KDE on windows doesnt know about the windows default applications offers will be empty in nearly all cases.
        // So we use QDesktopServices::openUrl to let windows decide how to open the file
        return QDesktopServices::openUrl(u);
#else
        // Open-with dialog
        // TODO : pass the mimetype as a parameter, to show it (comment field) in the dialog !
        // Hmm, in fact KOpenWithDialog::setServiceType already guesses the mimetype from the first URL of the list...
        return displayOpenWithDialog(lst, window, tempFile, suggestedFileName, asn);
#endif
    }

    return KRun::run(*offer, lst, window, tempFile, suggestedFileName, asn);
}

bool KRun::displayOpenWithDialog(const QList<QUrl>& lst, QWidget* window, bool tempFiles,
                                 const QString& suggestedFileName, const QByteArray& asn)
{
    if (!KAuthorized::authorizeKAction("openwith")) {
        KMessageBox::sorry(window,
                           i18n("You are not authorized to select an application to open this file."));
        return false;
    }

#ifdef Q_OS_WIN
    KConfigGroup cfgGroup(KSharedConfig::openConfig(), "KOpenWithDialog Settings");
    if (cfgGroup.readEntry("Native", true)) {
        return KRun::KRunPrivate::displayNativeOpenWithDialog(lst, window, tempFiles,
                suggestedFileName, asn);
    }
#endif
    KOpenWithDialog l(lst, i18n("Open with:"), QString(), window);
    if (l.exec()) {
        KService::Ptr service = l.service();
        if (!service) {
            //qDebug() << "No service set, running " << l.text();
            service = KService::Ptr(new KService(QString() /*name*/, l.text(), QString() /*icon*/));
        }
        return KRun::run(*service, lst, window, tempFiles, suggestedFileName, asn);
    }
    return false;
}

#ifndef KDE_NO_DEPRECATED
void KRun::shellQuote(QString &_str)
{
    // Credits to Walter, says Bernd G. :)
    if (_str.isEmpty()) { // Don't create an explicit empty parameter
        return;
    }
    QChar q('\'');
    _str.replace(q, "'\\''").prepend(q).append(q);
}
#endif



QStringList KRun::processDesktopExec(const KService &_service, const QList<QUrl>& _urls, bool tempFiles, const QString& suggestedFileName)
{
    KIO::DesktopExecParser parser(_service, _urls);
    parser.setUrlsAreTempFiles(tempFiles);
    parser.setSuggestedFileName(suggestedFileName);
    return parser.resultingArguments();
}

#ifndef KDE_NO_DEPRECATED
QString KRun::binaryName(const QString &execLine, bool removePath)
{
    return removePath ? KIO::DesktopExecParser::executableName(execLine) : KIO::DesktopExecParser::executablePath(execLine);
}
#endif

static bool runCommandInternal(const QString &command, const KService* service, const QString& executable,
                               const QString &userVisibleName, const QString & iconName, QWidget* window,
                               const QByteArray& asn, QString workingDirectory = QString())
{
    if (window != NULL) {
        window = window->topLevelWidget();
    }
    if (service && !service->entryPath().isEmpty()
            && !KDesktopFile::isAuthorizedDesktopFile(service->entryPath()))
    {
        qWarning() << "No authorization to execute " << service->entryPath();
        KMessageBox::sorry(window, i18n("You are not authorized to execute this file."));
        return false;
    }

    QString bin = KIO::DesktopExecParser::executableName(executable);
#if HAVE_X11 // Startup notification doesn't work with QT/E, service isn't needed without Startup notification
    bool silent;
    QByteArray wmclass;
    KStartupInfoId id;
    bool startup_notify = (asn != "0" && KRun::checkStartupNotify(QString() /*unused*/, service, &silent, &wmclass));
    if (startup_notify) {
        id.initId(asn);
        id.setupStartupEnv();
        KStartupInfoData data;
        data.setHostname();
        data.setBin(bin);
        if (!userVisibleName.isEmpty()) {
            data.setName(userVisibleName);
        }
        else if (service && !service->name().isEmpty()) {
            data.setName(service->name());
        }
        data.setDescription(i18n("Launching %1" ,  data.name()));
        if (!iconName.isEmpty()) {
            data.setIcon(iconName);
        }
        else if (service && !service->icon().isEmpty()) {
            data.setIcon(service->icon());
        }
        if (!wmclass.isEmpty()) {
            data.setWMClass(wmclass);
        }
        if (silent) {
            data.setSilent(KStartupInfoData::Yes);
        }
        data.setDesktop(KWindowSystem::currentDesktop());
        if (window) {
            data.setLaunchedBy(window->winId());
        }
        if(service && !service->entryPath().isEmpty())
            data.setApplicationId(service->entryPath());
        KStartupInfo::sendStartup(id, data);
    }
    int pid = KProcessRunner::run(command, executable, id, workingDirectory);
    if (startup_notify && pid) {
        KStartupInfoData data;
        data.addPid(pid);
        KStartupInfo::sendChange(id, data);
        KStartupInfo::resetStartupEnv();
    }
    return pid != 0;
#else
    Q_UNUSED(userVisibleName);
    Q_UNUSED(iconName);
    return KProcessRunner::run(command, bin, workingDirectory) != 0;
#endif
}

// This code is also used in klauncher.
bool KRun::checkStartupNotify(const QString& /*binName*/, const KService* service, bool* silent_arg, QByteArray* wmclass_arg)
{
    bool silent = false;
    QByteArray wmclass;
    if (service && service->property("StartupNotify").isValid()) {
        silent = !service->property("StartupNotify").toBool();
        wmclass = service->property("StartupWMClass").toString().toLatin1();
    }
    else if (service && service->property("X-KDE-StartupNotify").isValid()) {
        silent = !service->property("X-KDE-StartupNotify").toBool();
        wmclass = service->property("X-KDE-WMClass").toString().toLatin1();
    }
    else { // non-compliant app
        if (service) {
            if (service->isApplication()) { // doesn't have .desktop entries needed, start as non-compliant
                wmclass = "0"; // krazy:exclude=doublequote_chars
            }
            else {
                return false; // no startup notification at all
            }
        }
        else {
#if 0
            // Create startup notification even for apps for which there shouldn't be any,
            // just without any visual feedback. This will ensure they'll be positioned on the proper
            // virtual desktop, and will get user timestamp from the ASN ID.
            wmclass = '0';
            silent = true;
#else   // That unfortunately doesn't work, when the launched non-compliant application
            // launches another one that is compliant and there is any delay inbetween (bnc:#343359)
            return false;
#endif
        }
    }
    if (silent_arg != NULL) {
        *silent_arg = silent;
    }
    if (wmclass_arg != NULL) {
        *wmclass_arg = wmclass;
    }
    return true;
}

static bool runTempService(const KService& _service, const QList<QUrl>& _urls, QWidget* window,
                           bool tempFiles, const QString& suggestedFileName, const QByteArray& asn)
{
    if (!_urls.isEmpty()) {
        //qDebug() << "runTempService: first url " << _urls.first();
    }

    QList<QUrl> urlsToRun = _urls;
    if ((_urls.count() > 1) && !_service.allowMultipleFiles()) {
        // We need to launch the application N times. That sucks.
        // We ignore the result for application 2 to N.
        // For the first file we launch the application in the
        // usual way. The reported result is based on this
        // application.
        QList<QUrl>::ConstIterator it = _urls.begin();
        while (++it != _urls.end()) {
            QList<QUrl> singleUrl;
            singleUrl.append(*it);
            runTempService(_service, singleUrl, window, tempFiles, suggestedFileName, QByteArray());
        }
        urlsToRun.clear();
        urlsToRun.append(_urls.first());
    }
    KIO::DesktopExecParser execParser(_service, urlsToRun);
    execParser.setUrlsAreTempFiles(tempFiles);
    execParser.setSuggestedFileName(suggestedFileName);
    const QStringList args = execParser.resultingArguments();
    if (args.isEmpty()) {
        KMessageBox::sorry(window, i18n("Error processing Exec field in %1", _service.entryPath()));
        return false;
    }
    //qDebug() << "runTempService: KProcess args=" << args;

    return runCommandInternal(args.join(" "), &_service, KIO::DesktopExecParser::executablePath(_service.exec()),
                              _service.name(), _service.icon(), window, asn, _service.path());
}

// WARNING: don't call this from DesktopExecParser, since klauncher uses that too...
// TODO: make this async, see the job->exec() in there...
static QList<QUrl> resolveURLs(const QList<QUrl>& _urls, const KService& _service)
{
    // Check which protocols the application supports.
    // This can be a list of actual protocol names, or just KIO for KDE apps.
    QStringList appSupportedProtocols = KIO::DesktopExecParser::supportedProtocols(_service);
    QList<QUrl> urls(_urls);
    if (!appSupportedProtocols.contains("KIO")) {
        for (QList<QUrl>::Iterator it = urls.begin(); it != urls.end(); ++it) {
            const QUrl url = *it;
            bool supported = KIO::DesktopExecParser::isProtocolInSupportedList(url, appSupportedProtocols);
            //qDebug() << "Looking at url=" << url << " supported=" << supported;
            if (!supported && KProtocolInfo::protocolClass(url.scheme()) == ":local") {
                // Maybe we can resolve to a local URL?
                KIO::StatJob *job = KIO::mostLocalUrl(url);
                if (job->exec()) { // ## nasty nested event loop!
                    const QUrl localURL = job->mostLocalUrl();
                    if (localURL != url) {
                        *it = localURL;
                        //qDebug() << "Changed to" << localURL;
                    }
                }
            }
        }
    }
    return urls;
}

// Simple QDialog that resizes the given text edit after being shown to more
// or less fit the enclosed text.
class SecureMessageDialog : public QDialog
{
    public:
    SecureMessageDialog(QWidget *parent) : QDialog(parent), m_textEdit(0)
    {
    }

    void setTextEdit(QPlainTextEdit *textEdit)
    {
        m_textEdit = textEdit;
    }

    protected:
    virtual void showEvent(QShowEvent* e)
    {
        // Now that we're shown, use our width to calculate a good
        // bounding box for the text, and resize m_textEdit appropriately.
        QDialog::showEvent(e);

        if(!m_textEdit)
            return;

        QSize fudge(20, 24); // About what it sounds like :-/

        // Form rect with a lot of height for bounding.  Use no more than
        // 5 lines.
        QRect curRect(m_textEdit->rect());
        QFontMetrics metrics(fontMetrics());
        curRect.setHeight(5 * metrics.lineSpacing());
        curRect.setWidth(qMax(curRect.width(), 300)); // At least 300 pixels ok?

        QString text(m_textEdit->toPlainText());
        curRect = metrics.boundingRect(curRect, Qt::TextWordWrap | Qt::TextSingleLine, text);

        // Scroll bars interfere.  If we don't think there's enough room, enable
        // the vertical scrollbar however.
        m_textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        if(curRect.height() < m_textEdit->height()) { // then we've got room
            m_textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_textEdit->setMaximumHeight(curRect.height() + fudge.height());
        }

        m_textEdit->setMinimumSize(curRect.size() + fudge);
        m_textEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        updateGeometry();
    }

    private:
    QPlainTextEdit *m_textEdit;
};

// Helper function to make the given .desktop file executable by ensuring
// that a #!/usr/bin/env xdg-open line is added if necessary and the file has
// the +x bit set for the user.  Returns false if either fails.
static bool makeFileExecutable(const QString &fileName)
{
    // Open the file and read the first two characters, check if it's
    // #!.  If not, create a new file, prepend appropriate lines, and copy
    // over.
    QFile desktopFile(fileName);
    if (!desktopFile.open(QFile::ReadOnly)) {
        qWarning() << "Error opening service" << fileName << desktopFile.errorString();
        return false;
    }

    QByteArray header = desktopFile.peek(2);   // First two chars of file
    if (header.size() == 0) {
        qWarning() << "Error inspecting service" << fileName << desktopFile.errorString();
        return false; // Some kind of error
    }

    if (header != "#!") {
        // Add header
        QSaveFile saveFile;
        saveFile.setFileName(fileName);
        if (!saveFile.open(QIODevice::WriteOnly)) {
            qWarning() << "Unable to open replacement file for" << fileName << saveFile.errorString();
            return false;
        }

        QByteArray shebang("#!/usr/bin/env xdg-open\n");
        if (saveFile.write(shebang) != shebang.size()) {
            qWarning() << "Error occurred adding header for" << fileName << saveFile.errorString();
            saveFile.cancelWriting();
            return false;
        }

        // Now copy the one into the other and then close and reopen desktopFile
        QByteArray desktopData(desktopFile.readAll());
        if (desktopData.isEmpty()) {
            qWarning() << "Unable to read service" << fileName << desktopFile.errorString();
            saveFile.cancelWriting();
            return false;
        }

        if (saveFile.write(desktopData) != desktopData.size()) {
            qWarning() << "Error copying service" << fileName << saveFile.errorString();
            saveFile.cancelWriting();
            return false;
        }

        desktopFile.close();
        if (!saveFile.commit()) { // Figures....
            qWarning() << "Error committing changes to service" << fileName << saveFile.errorString();
            return false;
        }

        if (!desktopFile.open(QFile::ReadOnly)) {
            qWarning() << "Error re-opening service" << fileName << desktopFile.errorString();
            return false;
        }
    } // Add header

    // corresponds to owner on unix, which will have to do since if the user
    // isn't the owner we can't change perms anyways.
    if (!desktopFile.setPermissions(QFile::ExeUser | desktopFile.permissions())) {
        qWarning() << "Unable to change permissions for" << fileName << desktopFile.errorString();
        return false;
    }

    // whew
    return true;
}

// Helper function to make a .desktop file executable if prompted by the user.
// returns true if KRun::run() should continue with execution, false if user declined
// to make the file executable or we failed to make it executable.
static bool makeServiceExecutable(const KService& service, QWidget* window)
{
    if (!KAuthorized::authorize("run_desktop_files")) {
        qWarning() << "No authorization to execute " << service.entryPath();
        KMessageBox::sorry(window, i18n("You are not authorized to execute this service."));
        return false; // Don't circumvent the Kiosk
    }

    SecureMessageDialog *baseDialog = new SecureMessageDialog(window);
    baseDialog->setWindowTitle(i18nc("Warning about executing unknown .desktop file", "Warning"));

    QVBoxLayout *topLayout = new QVBoxLayout;
    baseDialog->setLayout(topLayout);

    // Dialog will have explanatory text with a disabled lineedit with the
    // Exec= to make it visually distinct.
    QWidget *baseWidget = new QWidget(baseDialog);
    QHBoxLayout *mainLayout = new QHBoxLayout(baseWidget);

    QLabel *iconLabel = new QLabel(baseWidget);
    QPixmap warningIcon(KIconLoader::global()->loadIcon("dialog-warning", KIconLoader::NoGroup, KIconLoader::SizeHuge));
    mainLayout->addWidget(iconLabel);
    iconLabel->setPixmap(warningIcon);

    QVBoxLayout *contentLayout = new QVBoxLayout;
    QString warningMessage = i18nc("program name follows in a line edit below",
                                   "This will start the program:");

    QLabel *message = new QLabel(warningMessage, baseWidget);
    contentLayout->addWidget(message);

    // We can use KStandardDirs::findExe to resolve relative pathnames
    // but that gets rid of the command line arguments.
    QString program = QFileInfo(service.exec()).canonicalFilePath();
    if (program.isEmpty()) // e.g. due to command line arguments
        program = service.exec();

    QPlainTextEdit *textEdit = new QPlainTextEdit(baseWidget);
    textEdit->setPlainText(program);
    textEdit->setReadOnly(true);
    contentLayout->addWidget(textEdit);

    QLabel *footerLabel = new QLabel(i18n("If you do not trust this program, click Cancel"));
    contentLayout->addWidget(footerLabel);
    contentLayout->addStretch(0); // Don't allow the text edit to expand

    mainLayout->addLayout(contentLayout);

    topLayout->addWidget(baseWidget);
    baseDialog->setTextEdit(textEdit);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(baseDialog);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    KGuiItem::assign(buttonBox->button(QDialogButtonBox::Ok), KStandardGuiItem::cont());
    buttonBox->button(QDialogButtonBox::Cancel)->setDefault(true);
    buttonBox->button(QDialogButtonBox::Cancel)->setFocus();
    QObject::connect(buttonBox, SIGNAL(accepted()), baseDialog, SLOT(accept()));
    QObject::connect(buttonBox, SIGNAL(rejected()), baseDialog, SLOT(reject()));
    topLayout->addWidget(buttonBox);

    // Constrain maximum size.  Minimum size set in
    // the dialog's show event.
    QSize screenSize = QApplication::desktop()->screen()->size();
    baseDialog->resize(screenSize.width() / 4, 50);
    baseDialog->setMaximumHeight(screenSize.height() / 3);
    baseDialog->setMaximumWidth(screenSize.width() / 10 * 8);

    int result = baseDialog->exec();
    if (result != QDialog::Accepted) {
        return false;
    }

    // Assume that service is an absolute path since we're being called (relative paths
    // would have been allowed unless Kiosk said no, therefore we already know where the
    // .desktop file is.  Now add a header to it if it doesn't already have one
    // and add the +x bit.

    if (!::makeFileExecutable(service.entryPath())) {
        QString serviceName = service.name();
        if(serviceName.isEmpty())
            serviceName = service.genericName();

        KMessageBox::sorry(
            window,
            i18n("Unable to make the service %1 executable, aborting execution", serviceName)
        );

        return false;
    }

    return true;
}

bool KRun::run(const KService& _service, const QList<QUrl>& _urls, QWidget* window,
               bool tempFiles, const QString& suggestedFileName, const QByteArray& asn)
{
    if (!_service.entryPath().isEmpty() &&
            !KDesktopFile::isAuthorizedDesktopFile(_service.entryPath()) &&
            !::makeServiceExecutable(_service, window))
    {
        return false;
    }

    if (!tempFiles) {
        // Remember we opened those urls, for the "recent documents" menu in kicker
        QList<QUrl>::ConstIterator it = _urls.begin();
        for (; it != _urls.end(); ++it) {
            //qDebug() << "KRecentDocument::adding " << (*it).url();
            KRecentDocument::add(*it, _service.desktopEntryName());
        }
    }

    if (tempFiles || _service.entryPath().isEmpty() || !suggestedFileName.isEmpty()) {
        return runTempService(_service, _urls, window, tempFiles, suggestedFileName, asn);
    }

    //qDebug() << "KRun::run " << _service.entryPath();

    if (!_urls.isEmpty()) {
        //qDebug() << "First url" << _urls.first();
    }

    // Resolve urls if needed, depending on what the app supports
    const QList<QUrl> urls = resolveURLs(_urls, _service);

    QString error;
    int pid = 0;

    QByteArray myasn = asn;
    // startServiceByDesktopPath() doesn't take QWidget*, add it to the startup info now
    if (window != NULL) {
        if (myasn.isEmpty()) {
            myasn = KStartupInfo::createNewStartupId();
        }
        if (myasn != "0") {
            KStartupInfoId id;
            id.initId(myasn);
            KStartupInfoData data;
            data.setLaunchedBy(window->winId());
            KStartupInfo::sendChange(id, data);
        }
    }

    int i = KToolInvocation::startServiceByDesktopPath(
        _service.entryPath(), QUrl::toStringList(urls), &error, 0L, &pid, myasn
        );

    if (i != 0) {
        //qDebug() << error;
        KMessageBox::sorry(window, error);
        return false;
    }

    //qDebug() << "startServiceByDesktopPath worked fine";
    return true;
}


bool KRun::run(const QString& _exec, const QList<QUrl>& _urls, QWidget* window, const QString& _name,
               const QString& _icon, const QByteArray& asn)
{
    KService::Ptr service(new KService(_name, _exec, _icon));

    return run(*service, _urls, window, false, QString(), asn);
}

bool KRun::runCommand(const QString& cmd, QWidget* window, const QString& workingDirectory)
{
    if (cmd.isEmpty()) {
        qWarning() << "Command was empty, nothing to run";
        return false;
    }

    const QStringList args = KShell::splitArgs(cmd);
    if (args.isEmpty()) {
        qWarning() << "Command could not be parsed.";
        return false;
    }

    const QString bin = args.first();
    return KRun::runCommand(cmd, bin, bin /*iconName*/, window, QByteArray(), workingDirectory);
}

bool KRun::runCommand(const QString& cmd, const QString &execName, const QString & iconName, QWidget* window, const QByteArray& asn)
{
    return runCommand(cmd, execName, iconName, window, asn, QString());
}

bool KRun::runCommand(const QString& cmd, const QString &execName, const QString & iconName,
                      QWidget* window, const QByteArray& asn, const QString& workingDirectory)
{
    //qDebug() << "runCommand " << cmd << "," << execName;
    KProcess * proc = new KProcess;
    proc->setShellCommand(cmd);
    if (!workingDirectory.isEmpty()) {
        proc->setWorkingDirectory(workingDirectory);
    }
    QString bin = KIO::DesktopExecParser::executableName(execName);
    KService::Ptr service = KService::serviceByDesktopName(bin);
    return runCommandInternal(cmd, service.data(),
                              execName /*executable to check for in slotProcessExited*/,
                              execName /*user-visible name*/,
                              iconName, window, asn, workingDirectory);
}

KRun::KRun(const QUrl& url, QWidget* window,
           bool showProgressInfo, const QByteArray& asn)
        : d(new KRunPrivate(this))
{
    d->m_timer.setObjectName("KRun::timer");
    d->m_timer.setSingleShot(true);
    d->init(url, window, showProgressInfo, asn);
}

void KRun::KRunPrivate::init(const QUrl& url, QWidget* window,
                             bool showProgressInfo, const QByteArray& asn)
{
    m_bFault = false;
    m_bAutoDelete = true;
    m_bProgressInfo = showProgressInfo;
    m_bFinished = false;
    m_job = 0L;
    m_strURL = url;
    m_bScanFile = false;
    m_bIsDirectory = false;
    m_runExecutables = true;
    m_window = window;
    m_asn = asn;
    q->setEnableExternalBrowser(true);

    // Start the timer. This means we will return to the event
    // loop and do initialization afterwards.
    // Reason: We must complete the constructor before we do anything else.
    m_bInit = true;
    q->connect(&m_timer, SIGNAL(timeout()), q, SLOT(slotTimeout()));
    startTimer();
    //qDebug() << "new KRun" << q << url << "timer=" << &m_timer;
}

void KRun::init()
{
    //qDebug() << "INIT called";
    if (!d->m_strURL.isValid()) {
        handleInitError(KIO::ERR_MALFORMED_URL, i18n("Malformed URL\n%1", d->m_strURL.errorString()));
        qWarning() << d->m_strURL.errorString();
        d->m_bFault = true;
        d->m_bFinished = true;
        d->startTimer();
        return;
    }
    if (!KUrlAuthorized::authorizeUrlAction("open", QUrl(), d->m_strURL)) {
        QString msg = KIO::buildErrorString(KIO::ERR_ACCESS_DENIED, d->m_strURL.toDisplayString());
        handleInitError(KIO::ERR_ACCESS_DENIED, msg);
        d->m_bFault = true;
        d->m_bFinished = true;
        d->startTimer();
        return;
    }

    if (!d->m_externalBrowser.isEmpty() && d->m_strURL.scheme().startsWith(QLatin1String("http"))) {
        if (d->runExecutable(d->m_externalBrowser)) {
            return;
        }
    } else if (d->m_strURL.isLocalFile()) {
        const QString localPath = d->m_strURL.toLocalFile();
        if (!QFile::exists(localPath)) {
            handleInitError(KIO::ERR_DOES_NOT_EXIST,
                                      i18n("<qt>Unable to run the command specified. "
                                      "The file or folder <b>%1</b> does not exist.</qt>" ,
                                      localPath.toHtmlEscaped()));
            d->m_bFault = true;
            d->m_bFinished = true;
            d->startTimer();
            return;
        }

        QMimeDatabase db;
        QMimeType mime = db.mimeTypeForUrl(d->m_strURL);
        //qDebug() << "MIME TYPE is " << mime.name();
        if (!d->m_externalBrowser.isEmpty() && (
               mime.inherits(QLatin1String("text/html")) ||
               mime.inherits(QLatin1String("application/xhtml+xml")))) {
            if (d->runExecutable(d->m_externalBrowser)) {
                return;
            }
        } else if (mime.isDefault() && !QFileInfo(localPath).isReadable()) {
            // Unknown mimetype because the file is unreadable, no point in showing an open-with dialog (#261002)
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
    }
    else if (KIO::DesktopExecParser::hasSchemeHandler(d->m_strURL)) {
        //qDebug() << "Using scheme handler";
        const QString exec = schemeHandler(d->m_strURL.scheme());
        if (exec.isEmpty()) {
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

#if 0 // removed for KF5 (for portability). Reintroduce a bool or flag if useful.
    // Did we already get the information that it is a directory ?
    if ((d->m_mode & QT_STAT_MASK) == QT_STAT_DIR) {
        mimeTypeDetermined("inode/directory");
        return;
    }
#endif

    // Let's see whether it is a directory

    if (!KProtocolManager::supportsListing(d->m_strURL)) {
        //qDebug() << "Protocol has no support for listing";
        // No support for listing => it can't be a directory (example: http)
        scanFile();
        return;
    }

    //qDebug() << "Testing directory (stating)";

    // It may be a directory or a file, let's stat
    KIO::JobFlags flags = d->m_bProgressInfo ? KIO::DefaultFlags : KIO::HideProgressInfo;
    KIO::StatJob *job = KIO::stat(d->m_strURL, KIO::StatJob::SourceSide, 0 /* no details */, flags);
    KJobWidgets::setWindow(job, d->m_window);
    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(slotStatResult(KJob*)));
    d->m_job = job;
    //qDebug() << "Job" << job << "is about stating" << d->m_strURL;
}

KRun::~KRun()
{
    //qDebug() << this;
    d->m_timer.stop();
    killJob();
    //qDebug() << this << "done";
    delete d;
}

bool KRun::KRunPrivate::runExecutable(const QString& _exec)
{
    QList<QUrl> urls;
    urls.append(m_strURL);
    if (_exec.startsWith('!')) {
        QString exec = _exec.mid(1); // Literal command
        exec += " %u";
        if (q->run(exec, urls, m_window, QString(), QString(), m_asn)) {
            m_bFinished = true;
            startTimer();
            return true;
        }
    }
    else {
        KService::Ptr service = KService::serviceByStorageId(_exec);
        if (service && q->run(*service, urls, m_window, false, QString(), m_asn)) {
            m_bFinished = true;
            startTimer();
            return true;
        }
    }
    return false;
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

    // No mimetype found, and the URL is not local  (or fast mode not allowed).
    // We need to apply the 'KIO' method, i.e. either asking the server or
    // getting some data out of the file, to know what mimetype it is.

    if (!KProtocolManager::supportsReading(d->m_strURL)) {
        qWarning() << "#### NO SUPPORT FOR READING!";
        d->m_bFault = true;
        d->m_bFinished = true;
        d->startTimer();
        return;
    }
    //qDebug() << this << "Scanning file" << d->m_strURL;

    KIO::JobFlags flags = d->m_bProgressInfo ? KIO::DefaultFlags : KIO::HideProgressInfo;
    KIO::TransferJob *job = KIO::get(d->m_strURL, KIO::NoReload /*reload*/, flags);
    KJobWidgets::setWindow(job, d->m_window);
    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(slotScanFinished(KJob*)));
    connect(job, SIGNAL(mimetype(KIO::Job*,QString)),
            this, SLOT(slotScanMimeType(KIO::Job*,QString)));
    d->m_job = job;
    //qDebug() << "Job" << job << "is about getting from" << d->m_strURL;
}

// When arriving in that method there are 5 possible states:
// must_init, must_scan_file, found_dir, done+error or done+success.
void KRun::slotTimeout()
{
    //qDebug() << this << " slotTimeout called";
    if (d->m_bInit) {
        d->m_bInit = false;
        init();
        return;
    }

    if (d->m_bFault) {
        emit error();
    }
    if (d->m_bFinished) {
        emit finished();
    }
    else {
        if (d->m_bScanFile) {
            d->m_bScanFile = false;
            scanFile();
            return;
        }
        else if (d->m_bIsDirectory) {
            d->m_bIsDirectory = false;
            mimeTypeDetermined("inode/directory");
            return;
        }
    }

    if (d->m_bAutoDelete) {
        deleteLater();
        return;
    }
}

void KRun::slotStatResult(KJob * job)
{
    d->m_job = 0L;
    const int errCode = job->error();
    if (errCode) {
        // ERR_NO_CONTENT is not an error, but an indication no further
        // actions needs to be taken.
        if (errCode != KIO::ERR_NO_CONTENT) {
            qWarning() << this << "ERROR" << job->error() << job->errorString();
            handleError(job);
            //qDebug() << this << " KRun returning from showErrorDialog, starting timer to delete us";
            d->m_bFault = true;
        }

        d->m_bFinished = true;

        // will emit the error and autodelete this
        d->startTimer();
    }
    else {
        //qDebug() << "Finished";

        KIO::StatJob* statJob = qobject_cast<KIO::StatJob*>(job);
        if (!statJob) {
            qFatal("Fatal Error: job is a %s, should be a StatJob", typeid(*job).name());
        }

        // Update our URL in case of a redirection
        setUrl(statJob->url());

        const KIO::UDSEntry entry = statJob->statResult();
        const mode_t mode = entry.numberValue(KIO::UDSEntry::UDS_FILE_TYPE);
        if ((mode & QT_STAT_MASK) == QT_STAT_DIR) {
            d->m_bIsDirectory = true; // it's a dir
        }
        else {
            d->m_bScanFile = true; // it's a file
        }

        d->m_localPath = entry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH);

        // mimetype already known? (e.g. print:/manager)
        const QString knownMimeType = entry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE) ;

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
        qWarning() << "get() didn't emit a mimetype! Probably a kioslave bug, please check the implementation of" << url().scheme();
    }
    mimeTypeDetermined(mimetype);
    d->m_job = 0;
}

void KRun::slotScanFinished(KJob *job)
{
    d->m_job = 0;
    const int errCode = job->error();
    if (errCode) {
        // ERR_NO_CONTENT is not an error, but an indication no further
        // actions needs to be taken.
        if (errCode != KIO::ERR_NO_CONTENT) {
            qWarning() << this << "ERROR (stat):" << job->error() << ' ' << job->errorString();
            handleError(job);

            d->m_bFault = true;
        }

        d->m_bFinished = true;
        // will emit the error and autodelete this
        d->startTimer();
    }
}

void KRun::mimeTypeDetermined(const QString& mimeType)
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

void KRun::foundMimeType(const QString& type)
{
    //qDebug() << "Resulting mime type is " << type;

    QMimeDatabase db;

    KIO::TransferJob *job = qobject_cast<KIO::TransferJob *>(d->m_job);
    if (job) {
        // Update our URL in case of a redirection
        setUrl( job->url() );

        job->putOnHold();
        KIO::Scheduler::publishSlaveOnHold();
        d->m_job = 0;
    }

    Q_ASSERT(!d->m_bFinished);

    // Support for preferred service setting, see setPreferredService
    if (!d->m_preferredService.isEmpty()) {
        //qDebug() << "Attempting to open with preferred service: " << d->m_preferredService;
        KService::Ptr serv = KService::serviceByDesktopName(d->m_preferredService);
        if (serv && serv->hasMimeType(type)) {
            QList<QUrl> lst;
            lst.append(d->m_strURL);
            if (KRun::run(*serv, lst, d->m_window, false, QString(), d->m_asn)) {
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
        qWarning() << "Unknown mimetype " << type;
    } else if (mime.inherits("application/x-desktop") && !d->m_localPath.isEmpty()) {
        d->m_strURL = QUrl::fromLocalFile(d->m_localPath);
    }

    if (!KRun::runUrl(d->m_strURL, type, d->m_window, false /*tempfile*/, d->m_runExecutables, d->m_suggestedFileName, d->m_asn)) {
        d->m_bFault = true;
    }
    setFinished(true);
}

void KRun::killJob()
{
    if (d->m_job) {
        //qDebug() << this << "m_job=" << d->m_job;
        d->m_job->kill();
        d->m_job = 0L;
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

QWidget* KRun::window() const
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
    if (b) {
        d->m_externalBrowser = KConfigGroup(KSharedConfig::openConfig(), "General").readEntry("BrowserApplication");
    }
    else {
        d->m_externalBrowser.clear();
    }
}

void KRun::setPreferredService(const QString& desktopEntryName)
{
    d->m_preferredService = desktopEntryName;
}

void KRun::setRunExecutables(bool b)
{
    d->m_runExecutables = b;
}

void KRun::setSuggestedFileName(const QString& fileName)
{
    d->m_suggestedFileName = fileName;
}

QString KRun::suggestedFileName() const
{
    return d->m_suggestedFileName;
}

bool KRun::isExecutable(const QString& serviceType)
{
    return (serviceType == "application/x-desktop" ||
            serviceType == "application/x-executable" ||
            serviceType == "application/x-ms-dos-executable" ||
            serviceType == "application/x-shellscript");
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
    if (finished)
        d->startTimer();
}

void KRun::setJob(KIO::Job *job)
{
    d->m_job = job;
}

KIO::Job* KRun::job()
{
    return d->m_job;
}

#ifndef KDE_NO_DEPRECATED
QTimer& KRun::timer()
{
    return d->m_timer;
}
#endif

#ifndef KDE_NO_DEPRECATED
void KRun::setDoScanFile(bool scanFile)
{
    d->m_bScanFile = scanFile;
}
#endif

#ifndef KDE_NO_DEPRECATED
bool KRun::doScanFile() const
{
    return d->m_bScanFile;
}
#endif

#ifndef KDE_NO_DEPRECATED
void KRun::setIsDirecory(bool isDirectory)
{
    d->m_bIsDirectory = isDirectory;
}
#endif

bool KRun::isDirectory() const
{
    return d->m_bIsDirectory;
}

#ifndef KDE_NO_DEPRECATED
void KRun::setInitializeNextAction(bool initialize)
{
    d->m_bInit = initialize;
}
#endif

#ifndef KDE_NO_DEPRECATED
bool KRun::initializeNextAction() const
{
    return d->m_bInit;
}
#endif

bool KRun::isLocalFile() const
{
    return d->m_strURL.isLocalFile();
}

/****************/

#if !HAVE_X11
int KProcessRunner::run(const QString &command, const QString& executable, const QString &workingDirectory)
{
    return (new KProcessRunner(command, executable, workingDirectory))->pid();
}
#else
int KProcessRunner::run(const QString& command, const QString& executable, const KStartupInfoId& id, const QString& workingDirectory)
{
    return (new KProcessRunner(command, executable, id, workingDirectory))->pid();
}
#endif

#if !HAVE_X11
KProcessRunner::KProcessRunner(const QString &command, const QString & executable, const QString &workingDirectory)
#else
KProcessRunner::KProcessRunner(const QString& command, const QString& executable, const KStartupInfoId& id, const QString& workingDirectory) :
        id(id)
#endif
{
    m_pid = 0;
    process = new QProcess;
    m_executable = executable;
    connect(process, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(slotProcessExited(int,QProcess::ExitStatus)));

    if (!workingDirectory.isEmpty())
        process->setWorkingDirectory(workingDirectory);

    process->start(command);
    if (!process->waitForStarted()) {
        //qDebug() << "wait for started failed, exitCode=" << process->exitCode()
        //         << "exitStatus=" << process->exitStatus();
        // Note that exitCode is 255 here (the first time), and 0 later on (bug?).
        slotProcessExited(255, process->exitStatus());
    }
    else {
#if HAVE_X11
        m_pid = process->pid();
#endif
    }
}

KProcessRunner::~KProcessRunner()
{
    delete process;
}

int KProcessRunner::pid() const
{
    return m_pid;
}

void KProcessRunner::terminateStartupNotification()
{
#if HAVE_X11
    if (!id.none()) {
        KStartupInfoData data;
        data.addPid(m_pid); // announce this pid for the startup notification has finished
        data.setHostname();
        KStartupInfo::sendFinish(id, data);
    }
#endif

}

void
KProcessRunner::slotProcessExited(int exitCode, QProcess::ExitStatus exitStatus)
{
    //qDebug() << m_executable << "exitCode=" << exitCode << "exitStatus=" << exitStatus;
    Q_UNUSED(exitStatus);

    terminateStartupNotification(); // do this before the messagebox
    if (exitCode != 0 && !m_executable.isEmpty()) {
        // Let's see if the error is because the exe doesn't exist.
        // When this happens, waitForStarted returns false, but not if kioexec
        // was involved, then we come here, that's why the code is here.
        //
        // We'll try to find the executable relatively to current directory,
        // (or with a full path, if m_executable is absolute), and then in the PATH.
        if (!QFile(m_executable).exists() && QStandardPaths::findExecutable(m_executable).isEmpty()) {
            QEventLoopLocker locker;
            KMessageBox::sorry(0L, i18n("Could not find the program '%1'", m_executable));
        }
        else {
            //qDebug() << process->readAllStandardError();
        }
    }
    deleteLater();
}

#include "moc_krun.cpp"
#include "moc_krun_p.cpp"
