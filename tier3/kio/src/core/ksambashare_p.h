/*
 *   Copyright 2010 Rodrigo Belem <rclbelem@gmail.com>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) version 3, or any
 *   later version accepted by the membership of KDE e.V. (or its
 *   successor approved by the membership of KDE e.V.), which shall
 *   act as a proxy defined in Section 6 of version 3 of the license.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library.  If not, see <http://www.gnu.org/licenses/>
 */

#ifndef ksambashare_p_h
#define ksambashare_p_h

#include <QtCore/QMap>

#include "ksambasharedata.h"

class QString;
class KSambaShare;

class KSambaSharePrivate
{

public:
    KSambaSharePrivate(KSambaShare *parent);
    ~KSambaSharePrivate();

    static bool isSambaInstalled();
    bool findSmbConf();
    void setUserSharePath();

    static int runProcess(const QString &progName, const QStringList &args,
                          QByteArray &stdOut, QByteArray &stdErr);
    static QString testparmParamValue(const QString &parameterName);

    QByteArray getNetUserShareInfo();
    QStringList shareNames() const;
    QStringList sharedDirs() const;
    KSambaShareData getShareByName(const QString &shareName) const;
    QList<KSambaShareData> getSharesByPath(const QString &path) const;

    bool isShareNameValid(const QString &name) const;
    bool isDirectoryShared(const QString &path) const;
    bool isShareNameAvailable(const QString &name) const;
    KSambaShareData::UserShareError isPathValid(const QString &path) const;
    KSambaShareData::UserShareError isAclValid(const QString &acl) const;
    KSambaShareData::UserShareError guestsAllowed(const KSambaShareData::GuestPermission &guestok) const;

    KSambaShareData::UserShareError add(const KSambaShareData &shareData);
    KSambaShareData::UserShareError remove(const KSambaShareData &shareName) const;
    bool sync();

    void _k_slotFileChange(const QString &path);

private:
    KSambaShare * const q_ptr;
    Q_DECLARE_PUBLIC(KSambaShare)

    QMap<QString, KSambaShareData> data;
    QString smbConf;
    QString userSharePath;
    bool skipUserShare;
};

#endif
