/* This file is part of the KDE project
   Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
   Copyright (C)  2000-2005 David Faure <faure@kde.org>
   Copyright (C)       2001 Waldo Bastian <bastian@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "main.h"

#include <QtCore/QFile>
#include <QtCore/Q_PID>

#include <kapplication.h>
#include <kdeversion.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kio/job.h>
#include <krun.h>
#include <kio/netaccess.h>
#include <kservice.h>
#include <klocale.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <kstartupinfo.h>
#include <kshell.h>
#include <kde_file.h>

static const char description[] =
        I18N_NOOP("KIO Exec - Opens remote files, watches modifications, asks for upload");


KIOExec::KIOExec()
    : mExited(false)
{
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    if (args->count() < 1)
        KCmdLineArgs::usageError(i18n("'command' expected.\n"));

    tempfiles = args->isSet("tempfiles");
    if ( args->isSet( "suggestedfilename" ) )
        suggestedFileName = args->getOption( "suggestedfilename" );
    expectedCounter = 0;
    jobCounter = 0;
    command = args->arg(0);
    kDebug() << "command=" << command;

    for ( int i = 1; i < args->count(); i++ )
    {
        KUrl url = args->url(i);
	url = KIO::NetAccess::mostLocalUrl( url, 0 );

        //kDebug() << "url=" << url.url() << " filename=" << url.fileName();
        // A local file, not an URL ?
        // => It is not encoded and not shell escaped, too.
        if ( url.isLocalFile() )
        {
            FileInfo file;
            file.path = url.toLocalFile();
            file.url = url;
            fileList.append(file);
        }
        // It is an URL
        else
        {
            if ( !url.isValid() )
                KMessageBox::error( 0L, i18n( "The URL %1\nis malformed" ,  url.url() ) );
            else if ( tempfiles )
                KMessageBox::error( 0L, i18n( "Remote URL %1\nnot allowed with --tempfiles switch" ,  url.url() ) );
            else
            // We must fetch the file
            {
                QString fileName = KIO::encodeFileName( url.fileName() );
                if ( !suggestedFileName.isEmpty() )
                    fileName = suggestedFileName;
                // Build the destination filename, in ~/.kde/cache-*/krun/
                // Unlike KDE-1.1, we put the filename at the end so that the extension is kept
                // (Some programs rely on it)
                QString tmp = KGlobal::dirs()->saveLocation( "cache", "krun/" ) +
                              QString("%1_%2_%3").arg(getpid()).arg(jobCounter++).arg(fileName);
                FileInfo file;
                file.path = tmp;
                file.url = url;
                fileList.append(file);

                expectedCounter++;
                KUrl dest;
                dest.setPath( tmp );
                kDebug() << "Copying " << url.prettyUrl() << " to " << dest;
                KIO::Job *job = KIO::file_copy( url, dest );
                jobList.append( job );

                connect( job, SIGNAL( result( KJob * ) ), SLOT( slotResult( KJob * ) ) );
            }
        }
    }
    args->clear();

    if ( tempfiles )
    {
        slotRunApp();
        return;
    }

    counter = 0;
    if ( counter == expectedCounter )
        slotResult( 0L );
}

void KIOExec::slotResult( KJob * job )
{
    if (job && job->error())
    {
        // That error dialog would be queued, i.e. not immediate...
        //job->showErrorDialog();
        if ( (job->error() != KIO::ERR_USER_CANCELED) )
            KMessageBox::error( 0L, job->errorString() );

        QString path = static_cast<KIO::FileCopyJob*>(job)->destUrl().path();

        QList<FileInfo>::Iterator it = fileList.begin();
        for(;it != fileList.end(); ++it)
        {
           if ((*it).path == path)
              break;
        }

        if ( it != fileList.end() )
           fileList.erase( it );
        else
           kDebug() <<  path << " not found in list";
    }

    counter++;

    if ( counter < expectedCounter )
        return;

    kDebug() << "All files downloaded, will call slotRunApp shortly";
    // We know we can run the app now - but let's finish the job properly first.
    QTimer::singleShot( 0, this, SLOT( slotRunApp() ) );

    jobList.clear();
}

void KIOExec::slotRunApp()
{
    if ( fileList.isEmpty() ) {
        kDebug() << "No files downloaded -> exiting";
        mExited = true;
        QApplication::exit(1);
        return;
    }

    KService service("dummy", command, QString());

    KUrl::List list;
    // Store modification times
    QList<FileInfo>::Iterator it = fileList.begin();
    for ( ; it != fileList.end() ; ++it )
    {
        KDE_struct_stat buff;
        (*it).time = KDE_stat( QFile::encodeName((*it).path), &buff ) ? 0 : buff.st_mtime;
        KUrl url;
        url.setPath((*it).path);
        list << url;
    }

    QStringList params = KRun::processDesktopExec(service, list);

    kDebug() << "EXEC " << KShell::joinArgs( params );

#ifdef Q_WS_X11
    // propagate the startup identification to the started process
    KStartupInfoId id;
    id.initId( kapp->startupId());
    id.setupStartupEnv();
#endif

    QString exe( params.takeFirst() );
    const int exit_code = QProcess::execute( exe, params );

#ifdef Q_WS_X11
    KStartupInfo::resetStartupEnv();
#endif

    kDebug() << "EXEC done";

    // Test whether one of the files changed
    it = fileList.begin();
    for( ;it != fileList.end(); ++it )
    {
        KDE_struct_stat buff;
        QString src = (*it).path;
        KUrl dest = (*it).url;
        if ( (KDE::stat( src, &buff ) == 0) &&
             ((*it).time != buff.st_mtime) )
        {
            if ( tempfiles )
            {
                if ( KMessageBox::questionYesNo( 0L,
                                                 i18n( "The supposedly temporary file\n%1\nhas been modified.\nDo you still want to delete it?" , dest.prettyUrl()),
                                                 i18n( "File Changed" ), KStandardGuiItem::del(), KGuiItem(i18n("Do Not Delete")) ) != KMessageBox::Yes )
                    continue; // don't delete the temp file
            }
            else if ( ! dest.isLocalFile() )  // no upload when it's already a local file
            {
                if ( KMessageBox::questionYesNo( 0L,
                                                 i18n( "The file\n%1\nhas been modified.\nDo you want to upload the changes?" , dest.prettyUrl()),
                                                 i18n( "File Changed" ), KGuiItem(i18n("Upload")), KGuiItem(i18n("Do Not Upload")) ) == KMessageBox::Yes )
                {
                    kDebug() << "src='" << src << "'  dest='" << dest << "'";
                    // Do it the synchronous way.
                    if ( !KIO::NetAccess::upload( src, dest, 0 ) )
                    {
                        KMessageBox::error( 0L, KIO::NetAccess::lastErrorString() );
                        continue; // don't delete the temp file
                    }
                }
            }
        }

        if ((!dest.isLocalFile() || tempfiles) && exit_code == 0) {
            // Wait for a reasonable time so that even if the application forks on startup (like OOo or amarok)
            // it will have time to start up and read the file before it gets deleted. #130709.
            kDebug() << "sleeping...";
            sleep(180); // 3 mn
            kDebug() << "about to delete " << src;
            unlink( QFile::encodeName(src) );
        }
    }

    mExited = true;
    QApplication::exit(exit_code);
}

int main( int argc, char **argv )
{
    KAboutData aboutData( "kioexec", "kioexec", ki18n("KIOExec"),
        KDE_VERSION_STRING, ki18n(description), KAboutData::License_GPL,
        ki18n("(c) 1998-2000,2003 The KFM/Konqueror Developers"));
    aboutData.addAuthor(ki18n("David Faure"),KLocalizedString(), "faure@kde.org");
    aboutData.addAuthor(ki18n("Stephan Kulow"),KLocalizedString(), "coolo@kde.org");
    aboutData.addAuthor(ki18n("Bernhard Rosenkraenzer"),KLocalizedString(), "bero@arklinux.org");
    aboutData.addAuthor(ki18n("Waldo Bastian"),KLocalizedString(), "bastian@kde.org");
    aboutData.addAuthor(ki18n("Oswald Buddenhagen"),KLocalizedString(), "ossi@kde.org");

    KCmdLineArgs::init( argc, argv, &aboutData );

    KCmdLineOptions options;
    options.add("tempfiles", ki18n("Treat URLs as local files and delete them afterwards"));
    options.add("suggestedfilename <file name>", ki18n("Suggested file name for the downloaded file"));
    options.add("+command", ki18n("Command to execute"));
    options.add("+[URLs]", ki18n("URL(s) or local file(s) used for 'command'"));
    KCmdLineArgs::addCmdLineOptions( options );

    KApplication app;
    app.setQuitOnLastWindowClosed(false);

    KIOExec exec;

    // Don't go into the event loop if we already want to exit (#172197)
    if (exec.exited())
        return 0;

    return app.exec();
}

#include "main.moc"
