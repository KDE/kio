/* This file is part of the KDE project
   Copyright (c) 2004 Jan Schaefer <j_schaef@informatik.uni-kl.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef knfsshare_h
#define knfsshare_h

#include <QObject>

#include "kiocore_export.h"

/**
 * @class KNFSShare knfsshare.h <KNFSShare>
 *
 * Similar functionality like KFileShare,
 * but works only for NFS and do not need
 * any suid script.
 * It parses the /etc/exports file to get its information.
 * Singleton class, call instance() to get an instance.
 */
class KIOCORE_EXPORT KNFSShare : public QObject
{
    Q_OBJECT
public:
    /**
     * Returns the one and only instance of KNFSShare
     */
    static KNFSShare *instance();

    /**
     * Whether or not the given path is shared by NFS.
     * @param path the path to check if it is shared by NFS.
     * @return whether the given path is shared by NFS.
     */
    bool isDirectoryShared(const QString &path) const;

    /**
     * Returns a list of all directories shared by NFS.
     * The resulting list is not sorted.
     * @return a list of all directories shared by NFS.
     */
    QStringList sharedDirectories() const;

    /**
     * KNFSShare destructor.
     * Do not call!
     * The instance is destroyed automatically!
     */
    virtual ~KNFSShare();

    /**
     * Returns the path to the used exports file,
     * or null if no exports file was found
     */
    QString exportsPath() const;

Q_SIGNALS:
    /**
     * Emitted when the /etc/exports file has changed
     */
    void changed();

private:
    KNFSShare();
    class KNFSSharePrivate;
    KNFSSharePrivate *const d;

    Q_PRIVATE_SLOT(d, void _k_slotFileChange(const QString &))

    friend class KNFSShareSingleton;
};

#endif
