/*
    This file is part of the KDE Cookie Jar

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/
//----------------------------------------------------------------------------
//
// KDE Password Server
// $Id$

#ifndef KPASSWDSERVER_H
#define KPASSWDSERVER_H

#include <qdict.h>
#include <qintdict.h>

#include <dcopclient.h>
#include <kio/authinfo.h>
#include <kded/kdedmodule.h>

class KPasswdServer : public KDEDModule
{
  Q_OBJECT
  K_DCOP
public:
  KPasswdServer(const QCString &);
  ~KPasswdServer();

k_dcop:
  KIO::AuthInfo checkAuthInfo(KIO::AuthInfo, long);
  KIO::AuthInfo queryAuthInfo(KIO::AuthInfo, QString, long, long);
  void addAuthInfo(KIO::AuthInfo, long);

public slots:
  void processRequest();
  // Remove all authentication info associated with windowId
  void removeAuthForWindowId(long windowId);

protected:
  struct AuthInfo;

  QString createCacheKey( const KIO::AuthInfo &info );
  const AuthInfo *findAuthInfoItem(const QString &key, const KIO::AuthInfo &info);
  void removeAuthInfoItem(const QString &key, const KIO::AuthInfo &info);
  void addAuthInfoItem(const QString &key, const KIO::AuthInfo &info, long windowId, long seqNr, bool canceled);
  KIO::AuthInfo copyAuthInfo(const AuthInfo *);
  
  struct AuthInfo {
    AuthInfo() { expire = expNever; isCanceled = false; seqNr = 0; }
  
    KURL url;
    QString directory;
    QString username;
    QString password;
    QString realmValue;
    QString digestInfo;
    
    enum { expNever, expWindowClose, expTime } expire;
    QValueList<long> windowList;
    unsigned long expireTime;
    long seqNr;
    
    bool isCanceled;
  };

  class AuthInfoList : public QPtrList<AuthInfo>
  {
    public: 
      AuthInfoList() { setAutoDelete(true); }
      int compareItems(QPtrCollection::Item n1, QPtrCollection::Item n2);
  };
  
  QDict< AuthInfoList > m_authDict;

  struct Request {
     DCOPClient *client;
     DCOPClientTransaction *transaction;
     QString key;
     KIO::AuthInfo info;
     QString errorMsg;
     long windowId;
     long seqNr;
  };

  QPtrList< Request > m_authPending;
  QPtrList< Request > m_authWait;
  QIntDict<QStringList> mWindowIdList;
  DCOPClient *m_dcopClient;
  long m_seqNr;
};

#endif
