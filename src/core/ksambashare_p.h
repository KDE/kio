/*
    SPDX-FileCopyrightText: 2010 Rodrigo Belem <rclbelem@gmail.com>
    SPDX-FileCopyrightText: 2020 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef ksambashare_p_h
#define ksambashare_p_h

#include <QFuture>
#include <QFutureWatcher>
#include <QMap>
#include <QString>

#include "ksambasharedata.h"

class KSambaShare;

class KSambaSharePrivate
{
public:
    // The output of one testparm and "net usershare info" run. Produced in a
    // worker thread, applied in the thread that owns KSambaShare.
    struct LoadedShares {
        QString userSharePath;
        QMap<QString, KSambaShareData> data;
        bool skipUserShare = false;
    };

    explicit KSambaSharePrivate(KSambaShare *parent);
    ~KSambaSharePrivate();

    // Runs load() in a worker thread. The result is applied in the thread that
    // owns KSambaShare, which then emits KSambaShare::changed().
    void startLoad();
    // Applies a load that has already finished. Does not block.
    void pollLoad() const;
    // Blocks until the running load has been applied.
    void ensureLoaded() const;

    static LoadedShares load(bool skipUserShare);
    static QString userSharePathFromTestparm();
    static QByteArray getNetUserShareInfo(bool &skipUserShare);

    static int runProcess(const QString &fullExecutablePath, const QStringList &args, QByteArray &stdOut, QByteArray &stdErr);
    static QString testparmParamValue(const QString &parameterName);

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

    void slotFileChange(const QString &path);

private:
    void applyLoaded(const LoadedShares &loaded);
    void updateWatch();

    KSambaShare *const q_ptr;
    Q_DECLARE_PUBLIC(KSambaShare)

    QMap<QString, KSambaShareData> data;
    QString smbConf;
    QString userSharePath;
    bool skipUserShare;
    QByteArray m_stdErr;

    QFuture<LoadedShares> m_future;
    QFutureWatcher<LoadedShares> *m_watcher = nullptr;
    bool m_loadPending = false;
    bool m_watchConnected = false;
    QString m_watchedPath;
};

#endif
