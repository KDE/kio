/* This file is part of the KDE project
   Copyright (c) 2004 Jan Schaefer <j_schaef@informatik.uni-kl.de>
   Copyright 2010 Rodrigo Belem <rclbelem@gmail.com>

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

#ifndef ksambashare_h
#define ksambashare_h

#include <QObject>
#include "kiocore_export.h"

class KSambaShareData;
class KSambaSharePrivate;

/**
 * @class KSambaShare ksambashare.h <KSambaShare>
 *
 * This class lists Samba user shares and monitors them for addition, update and removal.
 * Singleton class, call instance() to get an instance.
 */
class KIOCORE_EXPORT KSambaShare : public QObject
{
    Q_OBJECT

public:
    /**
     * @return the one and only instance of KSambaShare.
     */
    static KSambaShare *instance();

    /**
     * Whether or not the given path is shared by Samba.
     *
     * @param path the path to check if it is shared by Samba.
     *
     * @return whether the given path is shared by Samba.
     */
    bool isDirectoryShared(const QString &path) const;

    /**
     * Returns a list of all directories shared by local users in Samba.
     * The resulting list is not sorted.
     *
     * @return a list of all directories shared by Samba.
     */
    QStringList sharedDirectories() const;

    /**
     * Tests that a share name is valid and does not conflict with system users names or shares.
     *
     * @param name the share name.
     *
     * @return whether the given name is already being used or not.
     *
     * @since 4.7
     */
    bool isShareNameAvailable(const QString &name) const;

    /**
     * Returns the list of available shares.
     *
     * @return @c a QStringList containing the user shares names.
     * @return @c an empty list if there aren't user shared directories.
     *
     * @since 4.7
     */
    QStringList shareNames() const;

    /**
     * Returns the KSambaShareData object of the share name.
     *
     * @param name the share name.
     *
     * @return @c the KSambaShareData object that matches the name.
     * @return @c an empty KSambaShareData object if there isn't match for the name.
     *
     * @since 4.7
     */
    KSambaShareData getShareByName(const QString &name) const;

    /**
     * Returns a list of KSambaShareData matching the path.
     *
     * @param path the path that wants to get KSambaShareData object.
     *
     * @return @c the QList of KSambaShareData objects that matches the path.
     * @return @c an empty QList if there aren't matches for the given path.
     *
     * @since 4.7
     */
    QList<KSambaShareData> getSharesByPath(const QString &path) const;

    virtual ~KSambaShare();

#if KIOCORE_ENABLE_DEPRECATED_SINCE(4, 6)
    /**
     * Returns the path to the used smb.conf file
     * or empty string if no file was found
     *
     * @return @c the path to the smb.conf file
     *
     * @deprecated Since 4.6, the conf file is no longer used
     */
    KIOCORE_DEPRECATED_VERSION(4, 6, "Conf file no longer used")
    QString smbConfPath() const;
#endif

Q_SIGNALS:
    /**
     * Emitted when a share is updated, added or removed
     */
    void changed();

private:
    KSambaShare();

    KSambaSharePrivate *const d_ptr;
    Q_DECLARE_PRIVATE(KSambaShare)
    friend class KSambaShareData;
    friend class KSambaShareSingleton;

    Q_PRIVATE_SLOT(d_func(), void _k_slotFileChange(const QString &))
};

#endif
