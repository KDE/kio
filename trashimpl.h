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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef TRASHIMPL_H
#define TRASHIMPL_H

#include <qstring.h>
#include <qdatetime.h>
#include <qmap.h>

/**
 * Implementation of all low-level operations done by kio_trash
 * The structure of the trash directory follows the freedesktop.org standard <TODO URL>
 */
class TrashImpl
{
public:
    TrashImpl();

    /// Check .Trash directory in $HOME
    /// This MUST be called before doing anything else
    bool init();

    /// Trash this file
    bool add( const QString& origPath );

    /// Get rid of a trashed file
    bool del( int trashId, int fileId );

    /// Restore a trashed file
    bool restore( int trashId, int fileId );

    /// Empty trash, i.e. delete all trashed files
    bool emptyTrash();

    struct TrashedFileInfo {
        int trashId; // for the url
        QString fileId; // for the url
        QString physicalPath; // for stat'ing etc.
        QString origPath; // from info file
        QDateTime deletionDate; // from info file
    };
    /// List trashed files
    QValueList<TrashedFileInfo> list();

    /// Return the info for a given trashed file
    bool infoForFile( TrashedFileInfo& info );

    /// KIO error code
    int lastErrorCode() const { return m_lastErrorCode; }
    QString lastErrorMessage() const { return m_lastErrorMessage; }

    /// Helper method. Tries to call ::rename(src,dest) and does error handling.
    bool tryRename( const char* src, const char* dest );

private:
    bool testDir( const QString &_name );
    void error( int e, QString s );

    /// Find the trash dir to use for a given file to delete, based on original path
    int findTrashDirectory( const QString& origPath );
    /// Check .Trash directory in another partition
    bool initTrashDirectory( const QString& origPath );
    QString trashDirectoryPath( int trashId ) const {
        return m_trashDirectories[trashId];
    }

private:
    int m_lastErrorCode;
    QString m_lastErrorMessage;

    enum { InitToBeDone, InitOK, InitError } m_initStatus;

    // A "trash directory" is a physical directory on disk,
    // e.g. $HOME/.Trash/$uid or /mnt/foo/.Trash/$uid
    // It has an id (number) and a path.
    // The home trash has id 0.
    QMap<int, QString> m_trashDirectories; // id -> path
    int m_lastId;

    // We don't cache any data related to the trashed files.
    // Another kioslave could change that behind our feet.
    // If we want to start caching data - and avoiding some race conditions -,
    // we should turn this class into a kded module and use DCOP to talk to it
    // from the kioslave.
};

#endif
