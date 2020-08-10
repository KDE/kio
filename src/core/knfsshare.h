/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 Jan Schaefer <j_schaef@informatik.uni-kl.de>

    SPDX-License-Identifier: LGPL-2.0-only
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
