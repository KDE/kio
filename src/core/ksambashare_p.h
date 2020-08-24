/*
    SPDX-FileCopyrightText: 2010 Rodrigo Belem <rclbelem@gmail.com>
    SPDX-FileCopyrightText: 2020 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef ksambashare_p_h
#define ksambashare_p_h

#include <QMap>

#include "ksambasharedata.h"

class QString;
class KSambaShare;

class KSambaSharePrivate
{

public:
    explicit KSambaSharePrivate(KSambaShare *parent);
    ~KSambaSharePrivate();

    static bool isSambaInstalled();
#if KIOCORE_BUILD_DEPRECATED_SINCE(4, 6)
    bool findSmbConf();
#endif
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
    bool areGuestsAllowed() const;
    KSambaShareData::UserShareError isPathValid(const QString &path) const;
    KSambaShareData::UserShareError isAclValid(const QString &acl) const;
    KSambaShareData::UserShareError guestsAllowed(const KSambaShareData::GuestPermission &guestok) const;

    KSambaShareData::UserShareError add(const KSambaShareData &shareData);
    KSambaShareData::UserShareError remove(const KSambaShareData &shareName);
    static QMap<QString, KSambaShareData> parse(const QByteArray &usershareData);

    void _k_slotFileChange(const QString &path);

private:
    KSambaShare *const q_ptr;
    Q_DECLARE_PUBLIC(KSambaShare)

    QMap<QString, KSambaShareData> data;
    QString smbConf;
    QString userSharePath;
    bool skipUserShare;
    QByteArray m_stdErr;
};

#endif
