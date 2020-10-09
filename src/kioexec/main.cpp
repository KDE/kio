/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998, 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2000-2005 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "main.h"
#include "kio_version.h"
#include "kioexecdinterface.h"

#include <QFile>
#include <QDir>

#include <job.h>
#include <copyjob.h>
#include <desktopexecparser.h>
#include <QApplication>
#include <QDebug>
#include <KMessageBox>
#include <KAboutData>
#include <KService>
#include <KLocalizedString>
#include <KDBusService>

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QStandardPaths>
#include <QThread>
#include <QFileInfo>

#include <KStartupInfo>
#include <config-kioexec.h>
#if HAVE_X11
#include <QX11Info>
#endif

static const char description[] =
    I18N_NOOP("KIO Exec - Opens remote files, watches modifications, asks for upload");


KIOExec::KIOExec(const QStringList &args, bool tempFiles, const QString &suggestedFileName)
    : mExited(false)
    , mTempFiles(tempFiles)
    , mUseDaemon(false)
    , mSuggestedFileName(suggestedFileName)
    , expectedCounter(0)
    , command(args.first())
    , jobCounter(0)
{
    qDebug() << "command=" << command << "args=" << args;

    for (int i = 1; i < args.count(); i++) {
        const QUrl urlArg = QUrl::fromUserInput(args.value(i));
        if (!urlArg.isValid()) {
            KMessageBox::error(nullptr, i18n("Invalid URL: %1", args.value(i)));
            exit(1);
        }
        KIO::StatJob* mostlocal = KIO::mostLocalUrl(urlArg);
        bool b = mostlocal->exec();
        if (!b) {
            KMessageBox::error(nullptr, i18n("File not found: %1", urlArg.toDisplayString()));
            exit(1);
        }
        Q_ASSERT(b);
        const QUrl url = mostlocal->mostLocalUrl();

        //kDebug() << "url=" << url.url() << " filename=" << url.fileName();
        // A local file, not an URL ?
        // => It is not encoded and not shell escaped, too.
        if (url.isLocalFile()) {
            FileInfo file;
            file.path = url.toLocalFile();
            file.url = url;
            fileList.append(file);
        } else {
            // It is an URL
            if (!url.isValid()) {
                KMessageBox::error(nullptr, i18n("The URL %1\nis malformed" ,  url.url()));
            } else if (mTempFiles) {
                KMessageBox::error(nullptr, i18n("Remote URL %1\nnot allowed with --tempfiles switch" ,  url.toDisplayString()));
            } else {
                // We must fetch the file
                QString fileName = KIO::encodeFileName(url.fileName());
                if (!suggestedFileName.isEmpty())
                    fileName = suggestedFileName;
                if (fileName.isEmpty())
                    fileName = QStringLiteral("unnamed");
                // Build the destination filename, in ~/.cache/kioexec/krun/
                // Unlike KDE-1.1, we put the filename at the end so that the extension is kept
                // (Some programs rely on it)
                QString krun_writable = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/krun/%1_%2/").arg(QCoreApplication::applicationPid()).arg(jobCounter++);
                QDir().mkpath(krun_writable); // error handling will be done by the job
                QString tmp = krun_writable + fileName;
                FileInfo file;
                file.path = tmp;
                file.url = url;
                fileList.append(file);

                expectedCounter++;
                const QUrl dest = QUrl::fromLocalFile(tmp);
                qDebug() << "Copying" << url << " to" << dest;
                KIO::Job *job = KIO::file_copy(url, dest);
                jobList.append(job);

                connect(job, &KJob::result, this, &KIOExec::slotResult);
            }
        }
    }

    if (mTempFiles) {
        // delay call so QApplication::exit passes the exit code to exec()
        QTimer::singleShot(0, this, &KIOExec::slotRunApp);
        return;
    }

    counter = 0;
    if (counter == expectedCounter) {
        slotResult(nullptr);
    }
}

void KIOExec::slotResult(KJob *job)
{
    if (job) {
        KIO::FileCopyJob *copyJob = static_cast<KIO::FileCopyJob *>(job);
        const QString path = copyJob->destUrl().path();

        if (job->error()) {
            // That error dialog would be queued, i.e. not immediate...
            //job->showErrorDialog();
            if (job->error() != KIO::ERR_USER_CANCELED) {
                KMessageBox::error(nullptr, job->errorString());
            }

            auto it = std::find_if(fileList.begin(), fileList.end(),
                                   [&path](const FileInfo &i) { return i.path == path; });
            if (it != fileList.end()) {
                fileList.erase(it);
            } else {
                qDebug() <<  path << " not found in list";
            }
        }
        else
        {
            // Tell kioexecd to watch the file for changes.
            const QString dest = copyJob->srcUrl().toString();
            qDebug() << "Telling kioexecd to watch path" << path << "dest" << dest;
            OrgKdeKIOExecdInterface kioexecd(QStringLiteral("org.kde.kioexecd"), QStringLiteral("/modules/kioexecd"), QDBusConnection::sessionBus());
            kioexecd.watch(path, dest);
            mUseDaemon = !kioexecd.lastError().isValid();
            if (!mUseDaemon) {
                qDebug() << "Not using kioexecd";
            }
        }
    }

    counter++;

    if (counter < expectedCounter) {
        return;
    }

    qDebug() << "All files downloaded, will call slotRunApp shortly";
    // We know we can run the app now - but let's finish the job properly first.
    QTimer::singleShot(0, this, &KIOExec::slotRunApp);

    jobList.clear();
}

void KIOExec::slotRunApp()
{
    if (fileList.isEmpty()) {
        qDebug() << "No files downloaded -> exiting";
        mExited = true;
        QApplication::exit(1);
        return;
    }

    KService service(QStringLiteral("dummy"), command, QString());

    QList<QUrl> list;
    list.reserve(fileList.size());
    // Store modification times
    QList<FileInfo>::Iterator it = fileList.begin();
    for (; it != fileList.end() ; ++it) {
        QFileInfo info(it->path);
        it->time = info.lastModified();
        QUrl url = QUrl::fromLocalFile(it->path);
        list << url;
    }

    KIO::DesktopExecParser execParser(service, list);
    QStringList params = execParser.resultingArguments();
    if (params.isEmpty()) {
        qWarning() << execParser.errorMessage();
        QApplication::exit(-1);
        return;
    }

    qDebug() << "EXEC" << params.join(QLatin1Char(' '));

    // propagate the startup identification to the started process
    KStartupInfoId id;
    QByteArray startupId;
#if HAVE_X11
    if (QX11Info::isPlatformX11()) {
        startupId = QX11Info::nextStartupId();
    }
#endif
    id.initId(startupId);
    id.setupStartupEnv();

    QString exe(params.takeFirst());
    const int exit_code = QProcess::execute(exe, params);

    KStartupInfo::resetStartupEnv();

    qDebug() << "EXEC done";

    // Test whether one of the files changed
    for (it = fileList.begin(); it != fileList.end(); ++it) {
        QString src = it->path;
        const QUrl dest = it->url;
        QFileInfo info(src);
        const bool uploadChanges = !mUseDaemon && !dest.isLocalFile();
        if (info.exists() && (it->time != info.lastModified())) {
            if (mTempFiles) {
                if (KMessageBox::questionYesNo(nullptr,
                                               i18n("The supposedly temporary file\n%1\nhas been modified.\nDo you still want to delete it?", dest.toDisplayString(QUrl::PreferLocalFile)),
                                               i18n("File Changed"), KStandardGuiItem::del(), KGuiItem(i18n("Do Not Delete"))) != KMessageBox::Yes)
                    continue; // don't delete the temp file
            } else if (uploadChanges) { // no upload when it's already a local file or kioexecd already did it.
                if (KMessageBox::questionYesNo(nullptr,
                                               i18n("The file\n%1\nhas been modified.\nDo you want to upload the changes?" , dest.toDisplayString()),
                                               i18n("File Changed"), KGuiItem(i18n("Upload")), KGuiItem(i18n("Do Not Upload"))) == KMessageBox::Yes) {
                    qDebug() << "src='" << src << "'  dest='" << dest << "'";
                    // Do it the synchronous way.
                    KIO::CopyJob* job = KIO::copy(QUrl::fromLocalFile(src), dest);
                    if (!job->exec()) {
                        KMessageBox::error(nullptr, job->errorText());
                        continue; // don't delete the temp file
                    }
                }
            }
        }

        if ((uploadChanges || mTempFiles) && exit_code == 0) {
            // Wait for a reasonable time so that even if the application forks on startup (like OOo or amarok)
            // it will have time to start up and read the file before it gets deleted. #130709.
            const int sleepSecs = 180;
            qDebug() << "sleeping for" << sleepSecs << "seconds before deleting file...";
            QThread::sleep(sleepSecs);
            const QString parentDir = info.path();
            qDebug() << sleepSecs << "seconds have passed, deleting" << info.filePath();
            QFile(src).remove();
            // NOTE: this is not necessarily a temporary directory.
            if (QDir().rmdir(parentDir)) {
                qDebug() << "Removed empty parent directory" << parentDir;
            }
        }
    }

    mExited = true;
    QApplication::exit(exit_code);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    KAboutData aboutData(QStringLiteral("kioexec"), i18n("KIOExec"), QStringLiteral(KIO_VERSION_STRING),
                         i18n(description), KAboutLicense::GPL,
                         i18n("(c) 1998-2000,2003 The KFM/Konqueror Developers"));
    aboutData.addAuthor(i18n("David Faure"), QString(), QStringLiteral("faure@kde.org"));
    aboutData.addAuthor(i18n("Stephan Kulow"), QString(), QStringLiteral("coolo@kde.org"));
    aboutData.addAuthor(i18n("Bernhard Rosenkraenzer"), QString(), QStringLiteral("bero@arklinux.org"));
    aboutData.addAuthor(i18n("Waldo Bastian"), QString(), QStringLiteral("bastian@kde.org"));
    aboutData.addAuthor(i18n("Oswald Buddenhagen"), QString(), QStringLiteral("ossi@kde.org"));
    KAboutData::setApplicationData(aboutData);
    KDBusService service(KDBusService::Multiple);

    QCommandLineParser parser;
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("tempfiles")},
                                        i18n("Treat URLs as local files and delete them afterwards")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("suggestedfilename")},
                                        i18n("Suggested file name for the downloaded file"),
                                        QStringLiteral("filename")));
    parser.addPositionalArgument(QStringLiteral("command"), i18n("Command to execute"));
    parser.addPositionalArgument(QStringLiteral("urls"), i18n("URL(s) or local file(s) used for 'command'"));

    app.setQuitOnLastWindowClosed(false);

    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    if (parser.positionalArguments().count() < 1) {
        parser.showHelp(-1);
        return -1;
    }

    const bool tempfiles = parser.isSet(QStringLiteral("tempfiles"));
    const QString suggestedfilename = parser.value(QStringLiteral("suggestedfilename"));
    KIOExec exec(parser.positionalArguments(), tempfiles, suggestedfilename);

    // Don't go into the event loop if we already want to exit (#172197)
    if (exec.exited()) {
        return 0;
    }

    return app.exec();
}
