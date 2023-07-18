/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 Jan Schaefer <j_schaef@informatik.uni-kl.de>
    SPDX-FileCopyrightText: 2010 Rodrigo Belem <rclbelem@gmail.com>
    SPDX-FileCopyrightText: 2020 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef ksambashare_h
#define ksambashare_h

#include "kiocore_export.h"
#include <QObject>

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
     */
    bool isShareNameAvailable(const QString &name) const;

    /**
     * Returns the list of available shares.
     *
     * @return @c a QStringList containing the user shares names.
     * @return @c an empty list if there aren't user shared directories.
     *
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
     */
    QList<KSambaShareData> getSharesByPath(const QString &path) const;

    ~KSambaShare() override;

    /**
     * Used to obtain UserShareSystemError error strings. This is usually the
     * verbatim stderr of internal helper commands and may contain newlines.
     * Do not use this to obtain error strings for other error types!
     *
     * @return QString containing the most relevant last stderr
     * @since 5.74
     */
    QString lastSystemErrorString() const;

    /**
     * Check whether usershares may enable guests. System-level configuration
     * may disable usershare guests and prevent saving KSambaShareData with
     * UserGuestPermission set.
     *
     * @return @c true when usershares may allow guest access
     * @since 5.74
     */
    bool areGuestsAllowed() const;

Q_SIGNALS:
    /**
     * Emitted when a share is updated, added or removed
     */
    void changed();

private:
    KIOCORE_NO_EXPORT KSambaShare();

    KSambaSharePrivate *const d_ptr;
    Q_DECLARE_PRIVATE(KSambaShare)
    friend class KSambaShareData;
    friend class KSambaShareSingleton;
};

#endif
