/* This file is part of the KDE project
   Copyright (C) 2004 David Faure <faure@kde.org>

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

#include <kapplication.h>
#include <kio/netaccess.h>
#include <kio/job.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#include <kdirnotify_stub.h>
#include <kdebug.h>
//Added by qt3to4:
#include <QByteArray>

static KCmdLineOptions options[] =
{
    { "empty", I18N_NOOP( "Empty the contents of the trash" ), 0 },
    //{ "migrate", I18N_NOOP( "Migrate contents of old trash" ), 0 },
    { "restore <file>", I18N_NOOP( "Restore a trashed file to its original location" ), 0 },
    // This hack is for the servicemenu on trash.desktop which uses Exec=ktrash -empty. %f is implied...
    { "+[ignored]", I18N_NOOP( "Ignored" ), 0 },
    KCmdLineLastOption
};

int main(int argc, char *argv[])
{
    KApplication::disableAutoDcopRegistration();
    KCmdLineArgs::init( argc, argv, "ktrash",
                        I18N_NOOP( "ktrash" ),
                        I18N_NOOP( "Helper program to handle the KDE trash can\n"
				   "Note: to move files to the trash, do not use ktrash, but \"kfmclient move 'url' trash:/\"" ),
                        KDE_VERSION_STRING );
    KCmdLineArgs::addCmdLineOptions( options );
    KApplication app;

    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    if ( args->isSet( "empty" ) ) {
        // We use a kio job instead of linking to TrashImpl, for a smaller binary
        // (and the possibility of a central service at some point)
        QByteArray packedArgs;
        QDataStream stream( &packedArgs, QIODevice::WriteOnly );
        stream << (int)1;
        KIO::Job* job = KIO::special( "trash:/", packedArgs );
        (void)KIO::NetAccess::synchronousRun( job, 0 );

        // Update konq windows opened on trash:/
        KDirNotify_stub allDirNotify("*", "KDirNotify*");
        allDirNotify.FilesAdded( "trash:/" ); // yeah, files were removed, but we don't know which ones...
        return 0;
    }

#if 0
    // This is only for testing. KDesktop handles it automatically.
    if ( args->isSet( "migrate" ) ) {
        QByteArray packedArgs;
        QDataStream stream( packedArgs, QIODevice::WriteOnly );
        stream << (int)2;
        KIO::Job* job = KIO::special( "trash:/", packedArgs );
        (void)KIO::NetAccess::synchronousRun( job, 0 );
        return 0;
    }
#endif

    QByteArray restoreArg = args->getOption( "restore" );
    if ( !restoreArg.isEmpty() ) {

        if (restoreArg.find("system:/trash")==0) {
            restoreArg.remove(0, 13);
            restoreArg.prepend("trash:");
        }

        KUrl trashURL( restoreArg );
        if ( !trashURL.isValid() || trashURL.protocol() != "trash" ) {
            kdError() << "Invalid URL for restoring a trashed file:" << trashURL << endl;
            return 1;
        }

        QByteArray packedArgs;
        QDataStream stream( &packedArgs, QIODevice::WriteOnly );
        stream << (int)3 << trashURL;
        KIO::Job* job = KIO::special( trashURL, packedArgs );
        bool ok = KIO::NetAccess::synchronousRun( job, 0 );
        if ( !ok )
            kdError() << KIO::NetAccess::lastErrorString() << endl;
        return 0;
    }

    return 0;
}
