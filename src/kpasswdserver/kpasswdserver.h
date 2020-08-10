/*
    This file is part of the KDE Password Server
    SPDX-FileCopyrightText: 2002 Waldo Bastian (bastian@kde.org)
    SPDX-FileCopyrightText: 2012 Dawit Alemayehu (adawit@kde.org)

    SPDX-License-Identifier: GPL-2.0-only
*/

// KDE Password Server

#ifndef KPASSWDSERVER_H
#define KPASSWDSERVER_H

#include <QDBusContext>
#include <QDBusMessage>
#include <QHash>
#include <QList>
#include <QWidget>

#include <KDEDModule>
#include <kio/authinfo.h>

namespace KWallet
{
class Wallet;
}

class KPasswdServer : public KDEDModule, protected QDBusContext
{
    Q_OBJECT

public:
    explicit KPasswdServer(QObject *parent, const QList<QVariant> & = QList<QVariant>());
    ~KPasswdServer();

    // Called by the unit test
    void setWalletDisabled(bool d)
    {
        m_walletDisabled = d;
    }

public Q_SLOTS:
    qlonglong checkAuthInfoAsync(KIO::AuthInfo, qlonglong, qlonglong);
    qlonglong queryAuthInfoAsync(const KIO::AuthInfo &, const QString &, qlonglong, qlonglong, qlonglong);
    void addAuthInfo(const KIO::AuthInfo &, qlonglong);
    void removeAuthInfo(const QString &host, const QString &protocol, const QString &user);

    // legacy methods provided for compatibility with old clients
    QByteArray checkAuthInfo(const QByteArray &, qlonglong, qlonglong);
    QByteArray queryAuthInfo(const QByteArray &, const QString &, qlonglong, qlonglong, qlonglong);
    void addAuthInfo(const QByteArray &, qlonglong);

    void processRequest();
    // Remove all authentication info associated with windowId
    void removeAuthForWindowId(qlonglong windowId);

Q_SIGNALS:
    void checkAuthInfoAsyncResult(qlonglong requestId, qlonglong seqNr, const KIO::AuthInfo &);
    void queryAuthInfoAsyncResult(qlonglong requestId, qlonglong seqNr, const KIO::AuthInfo &);

private Q_SLOTS:
    void passwordDialogDone(int);
    void retryDialogDone(int);
    void windowRemoved(WId);

private:
    struct AuthInfoContainer {
        AuthInfoContainer()
            : expire(expNever)
            , seqNr(0)
            , isCanceled(false)
        {
        }

        KIO::AuthInfo info;
        QString directory;

        enum { expNever, expWindowClose, expTime } expire;
        QList<qlonglong> windowList;
        qulonglong expireTime;
        qlonglong seqNr;

        bool isCanceled;

        struct Sorter {
            bool operator()(AuthInfoContainer *n1, AuthInfoContainer *n2) const;
        };
    };

    struct Request {
        bool isAsync;             // true for async requests
        qlonglong requestId;      // set for async requests only
        QDBusMessage transaction; // set for sync requests only
        QString key;
        KIO::AuthInfo info;
        QString errorMsg;
        qlonglong windowId;
        qlonglong seqNr;
        bool prompt;
    };

    QString createCacheKey(const KIO::AuthInfo &info);
    const AuthInfoContainer *findAuthInfoItem(const QString &key, const KIO::AuthInfo &info);
    void removeAuthInfoItem(const QString &key, const KIO::AuthInfo &info);
    void addAuthInfoItem(const QString &key, const KIO::AuthInfo &info, qlonglong windowId, qlonglong seqNr, bool canceled);
    void copyAuthInfo(const AuthInfoContainer *, KIO::AuthInfo &);
    void updateAuthExpire(const QString &key, const AuthInfoContainer *, qlonglong windowId, bool keep);

#ifdef HAVE_KF5WALLET
    bool openWallet(qlonglong windowId);
#endif

    bool hasPendingQuery(const QString &key, const KIO::AuthInfo &info);
    void sendResponse(Request *request);
    void showPasswordDialog(Request *request);
    void updateCachedRequestKey(QList<Request *> &, const QString &oldKey, const QString &newKey);

    typedef QList<AuthInfoContainer *> AuthInfoContainerList;
    QHash<QString, AuthInfoContainerList *> m_authDict;

    QList<Request *> m_authPending;
    QList<Request *> m_authWait;
    QHash<int, QStringList> mWindowIdList;
    QHash<QObject *, Request *> m_authInProgress;
    QHash<QObject *, Request *> m_authRetryInProgress;
    QStringList m_authPrompted;
    KWallet::Wallet *m_wallet;
    bool m_walletDisabled;
    qlonglong m_seqNr;
};

#endif
