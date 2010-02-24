/*
    This file is part of the KDE Password Server

    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this library; see the file COPYING. If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
//----------------------------------------------------------------------------
//
// KDE Password Server

#ifndef KPASSWDSERVER_H
#define KPASSWDSERVER_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtDBus/QtDBus>

#include <kio/authinfo.h>
#include <kdedmodule.h>

namespace KWallet {
    class Wallet;
}

using namespace KIO;

class KPasswdServer : public KDEDModule, protected QDBusContext
{
  Q_OBJECT

public:
  KPasswdServer(QObject* parent, const QList<QVariant>& = QList<QVariant>());
  ~KPasswdServer();

public Q_SLOTS:
  qlonglong checkAuthInfoAsync(KIO::AuthInfo, qlonglong, qlonglong);
  qlonglong queryAuthInfoAsync(const KIO::AuthInfo &, const QString &, qlonglong, qlonglong, qlonglong);
  void addAuthInfo(const KIO::AuthInfo &, qlonglong);
  void removeAuthInfo(const QString& host, const QString& protocol, const QString& user);

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

protected:
  struct AuthInfoContainer;

  QString createCacheKey( const KIO::AuthInfo &info );
  const AuthInfoContainer *findAuthInfoItem(const QString &key, const KIO::AuthInfo &info);
  void removeAuthInfoItem(const QString &key, const KIO::AuthInfo &info);
  void addAuthInfoItem(const QString &key, const KIO::AuthInfo &info, qlonglong windowId, qlonglong seqNr, bool canceled);
  KIO::AuthInfo copyAuthInfo(const AuthInfoContainer *);
  void updateAuthExpire(const QString &key, const AuthInfoContainer *, qlonglong windowId, bool keep);
  int findWalletEntry( const QMap<QString,QString>& map, const QString& username );
  bool openWallet( int windowId );

  bool hasPendingQuery(const QString &key, const KIO::AuthInfo &info);

  struct AuthInfoContainer {
      AuthInfoContainer() : expire( expNever ), seqNr( 0 ), isCanceled( false ) {}

    KIO::AuthInfo info;
    QString directory;

    enum { expNever, expWindowClose, expTime } expire;
    QList<qlonglong> windowList;
    qulonglong expireTime;
    qlonglong seqNr;

    bool isCanceled;

    struct Sorter {
        bool operator() (AuthInfoContainer* n1, AuthInfoContainer* n2) const;
    };
  };

  typedef QList<AuthInfoContainer*> AuthInfoContainerList;
  QHash< QString, AuthInfoContainerList* > m_authDict;

  struct Request {
     bool isAsync; // true for async requests
     qlonglong requestId; // set for async requests only
     QDBusMessage transaction; // set for sync requests only
     QString key;
     KIO::AuthInfo info;
     QString errorMsg;
     qlonglong windowId;
     qlonglong seqNr;
     bool prompt;
  };

  QList<Request*> m_authPending;
  QList<Request*> m_authWait;
  QHash<int, QStringList> mWindowIdList;
  KWallet::Wallet* m_wallet;
  qlonglong m_seqNr;
};

#endif
