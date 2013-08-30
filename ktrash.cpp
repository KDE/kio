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

#include <qapplication.h>
#include <kio/netaccess.h>
#include <kio/job.h>
 #include <KLocalizedString>
#include <klocale.h>
#include <kdirnotify.h>
#include <kdebug.h>
#include <kdeversion.h>
#include <qcommandlineparser.h>
#include <qcommandlineoption.h>

int main(int argc, char *argv[])
{
    //KApplication::disableAutoDcopRegistration();
    QApplication app( argc, argv);
    QCommandLineParser *parser = new QCommandLineParser;
    // app.setApplicationVersion(version);
    //  parser->addVersionOption();
    //  parser->addHelpOption(description);
    parser->addOption(QCommandLineOption(QStringList() << "empty", i18n("Empty the contents of the trash")));
    parser->addOption(QCommandLineOption(QStringList() << "restore <file>", i18n( "Restore a trashed file to its original location" )));
    parser->addOption(QCommandLineOption(QStringList() <<"+[ignored]", i18n( "Ignored" )));
    parser->process(app);
    if ( parser->isSet( "empty" ) ) {
        // We use a kio job instead of linking to TrashImpl, for a smaller binary
        // (and the possibility of a central service at some point)
        QByteArray packedArgs;
        QDataStream stream( &packedArgs, QIODevice::WriteOnly );
        stream << (int)1;
        KIO::Job* job = KIO::special( QUrl("trash:/"), packedArgs );
        (void)KIO::NetAccess::synchronousRun( job, 0 );

        // Update konq windows opened on trash:/
        QUrl drl;
        drl.setPath("trash:/");
        org::kde::KDirNotify::emitFilesAdded(drl); // yeah, files were removed, but we don't know which ones...
        return 0;
    }

#if 0
    // This is only for testing. KDesktop handles it automatically.
    if ( parser->isSet( "migrate" ) ) {
        QByteArray packedArgs;
        QDataStream stream( packedArgs, QIODevice::WriteOnly );
        stream << (int)2;
        KIO::Job* job = KIO::special( "trash:/", packedArgs );
        (void)KIO::NetAccess::synchronousRun( job, 0 );
        return 0;
    }
#endif

    QString restoreArg = parser->value( "restore" );
    if ( !restoreArg.isEmpty() ) {

        if (restoreArg.indexOf(QLatin1String("system:/trash"))==0) {
            restoreArg.remove(0, 13);
            restoreArg.prepend(QString::fromLatin1("trash:"));
        }

        QUrl trashURL( restoreArg );
        if ( !trashURL.isValid() || trashURL.scheme() != QLatin1String("trash") ) {
          //  qDebug() << "Invalid URL for restoring a trashed file:" << trashURL << endl;
            return 1;
        }

        QByteArray packedArgs;
        QDataStream stream( &packedArgs, QIODevice::WriteOnly );
        stream << (int)3 << trashURL;
            KIO::Job* job = KIO::special( trashURL, packedArgs );
           bool ok = KIO::NetAccess::synchronousRun( job, 0 );
           if ( !ok )
                qDebug() << KIO::NetAccess::lastErrorString() << endl;
        return 0;
    }

    return 0;
}
