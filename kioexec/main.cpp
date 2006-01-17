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

#include <config.h>

#include <qfile.h>

#include <kapplication.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kio/job.h>
#include <krun.h>
#include <kio/netaccess.h>
#include <kprocess.h>
#include <kservice.h>
#include <klocale.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <kstartupinfo.h>
#include <kshell.h>
#include <kde_file.h>


#include "main.h"


static const char description[] =
        I18N_NOOP("KIO Exec - Opens remote files, watches modifications, asks for upload");

static KCmdLineOptions options[] =
{
   { "tempfiles", I18N_NOOP("Treat URLs as local files and delete them afterwards"), 0 },
   { "+command", I18N_NOOP("Command to execute"), 0 },
   { "+[URLs]", I18N_NOOP("URL(s) or local file(s) used for 'command'"), 0 },
   KCmdLineLastOption
};


KIOExec::KIOExec()
{
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    if (args->count() < 1)
        KCmdLineArgs::usage(i18n("'command' expected.\n"));

    tempfiles = args->isSet("tempfiles");

    expectedCounter = 0;
    jobCounter = 0;
    command = args->arg(0);
    kdDebug() << "command=" << command << endl;

    for ( int i = 1; i < args->count(); i++ )
    {
        KUrl url = args->url(i);
        //kdDebug() << "url=" << url.url() << " filename=" << url.fileName() << endl;
        // A local file, not an URL ?
        // => It is not encoded and not shell escaped, too.
        if ( url.isLocalFile() )
        {
            FileInfo file;
            file.path = url.path();
            file.url = url;
            fileList.append(file);
        }
        // It is an URL
        else
        {
            if ( !url.isValid() )
                KMessageBox::error( 0L, i18n( "The URL %1\nis malformed" ).arg( url.url() ) );
            else if ( tempfiles )
                KMessageBox::error( 0L, i18n( "Remote URL %1\nnot allowed with --tempfiles switch" ).arg( url.url() ) );
            else
            // We must fetch the file
            {
                QString fileName = KIO::encodeFileName( url.fileName() );
                // Build the destination filename, in ~/.kde/cache-*/krun/
                // Unlike KDE-1.1, we put the filename at the end so that the extension is kept
                // (Some programs rely on it)
                QString tmp = KGlobal::dirs()->saveLocation( "cache", "krun/" ) +
                              QString("%1.%2.%3").arg(getpid()).arg(jobCounter++).arg(fileName);
                FileInfo file;
                file.path = tmp;
                file.url = url;
                fileList.append(file);

                expectedCounter++;
                KUrl dest;
                dest.setPath( tmp );
                kdDebug() << "Copying " << url.prettyURL() << " to " << dest << endl;
                KIO::Job *job = KIO::file_copy( url, dest );
                jobList.append( job );

                connect( job, SIGNAL( result( KIO::Job * ) ), SLOT( slotResult( KIO::Job * ) ) );
            }
        }
    }
    args->clear();

    if ( tempfiles )
        slotRunApp(); // does not return

    counter = 0;
    if ( counter == expectedCounter )
        slotResult( 0L );
}

void KIOExec::slotResult( KIO::Job * job )
{
    if (job && job->error())
    {
        // That error dialog would be queued, i.e. not immediate...
        //job->showErrorDialog();
        if ( (job->error() != KIO::ERR_USER_CANCELED) )
            KMessageBox::error( 0L, job->errorString() );

        QString path = static_cast<KIO::FileCopyJob*>(job)->destURL().path();

        QList<FileInfo>::Iterator it = fileList.begin();
        for(;it != fileList.end(); ++it)
        {
           if ((*it).path == path)
              break;
        }

        if ( it != fileList.end() )
           fileList.erase( it );
        else
           kdDebug() <<  path << " not found in list" << endl;
    }

    counter++;

    if ( counter < expectedCounter )
        return;

    kdDebug() << "All files downloaded, will call slotRunApp shortly" << endl;
    // We know we can run the app now - but let's finish the job properly first.
    QTimer::singleShot( 0, this, SLOT( slotRunApp() ) );

    jobList.clear();
}

void KIOExec::slotRunApp()
{
    if ( fileList.isEmpty() ) {
        kdDebug() << k_funcinfo << "No files downloaded -> exiting" << endl;
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

    const QStringList params = KRun::processDesktopExec(service, list, false /*no shell*/);

    kdDebug() << "EXEC " << KShell::joinArgs( params ) << endl;

#ifdef Q_WS_X11
    // propagate the startup indentification to the started process
    KStartupInfoId id;
    id.initId( kapp->startupId());
    id.setupStartupEnv();
#endif

    KProcess proc;
    proc << params;
    proc.start( KProcess::Block );

#ifdef Q_WS_X11
    KStartupInfo::resetStartupEnv();
#endif

    kdDebug() << "EXEC done" << endl;

    // Test whether one of the files changed
    it = fileList.begin();
    for( ;it != fileList.end(); ++it )
    {
        KDE_struct_stat buff;
        QString src = (*it).path;
        KUrl dest = (*it).url;
        if ( (KDE_stat( QFile::encodeName(src), &buff ) == 0) &&
             ((*it).time != buff.st_mtime) )
        {
            if ( tempfiles )
            {
                if ( KMessageBox::questionYesNo( 0L,
                                                 i18n( "The supposedly temporary file\n%1\nhas been modified.\nDo you still want to delete it?" ).arg(dest.prettyURL()),
                                                 i18n( "File Changed" ), KStdGuiItem::del(), i18n("Do Not Delete") ) != KMessageBox::Yes )
                    continue; // don't delete the temp file
            }
            else
            {
                if ( KMessageBox::questionYesNo( 0L,
                                                 i18n( "The file\n%1\nhas been modified.\nDo you want to upload the changes?" ).arg(dest.prettyURL()),
                                                 i18n( "File Changed" ), i18n("Upload"), i18n("Do Not Upload") ) == KMessageBox::Yes )
                {
                    kdDebug() << "src='" << src << "'  dest='" << dest << "'" << endl;
                    // Do it the synchronous way.
                    if ( !KIO::NetAccess::upload( src, dest, 0 ) )
                    {
                        KMessageBox::error( 0L, KIO::NetAccess::lastErrorString() );
                        continue; // don't delete the temp file
                    }
                }
            }
        }
        else
        {
            // don't upload (and delete!) local files
            if (!tempfiles && dest.isLocalFile())
                continue;
        }
        unlink( QFile::encodeName(src) );
    }

    QApplication::exit(0);
}

int main( int argc, char **argv )
{
    KAboutData aboutData( "kioexec", I18N_NOOP("KIOExec"),
        VERSION, description, KAboutData::License_GPL,
        "(c) 1998-2000,2003 The KFM/Konqueror Developers");
    aboutData.addAuthor("David Faure",0, "faure@kde.org");
    aboutData.addAuthor("Stephan Kulow",0, "coolo@kde.org");
    aboutData.addAuthor("Bernhard Rosenkraenzer",0, "bero@arklinux.org");
    aboutData.addAuthor("Waldo Bastian",0, "bastian@kde.org");
    aboutData.addAuthor("Oswald Buddenhagen",0, "ossi@kde.org");

    KCmdLineArgs::init( argc, argv, &aboutData );
    KCmdLineArgs::addCmdLineOptions( options );

    KApplication app;

    KIOExec exec;

    kdDebug() << "Constructor returned..." << endl;
    return app.exec();
}

#include "main.moc"
