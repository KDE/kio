/*
    This file is part of the KDE Password Server

    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2005 David Faure (faure@kde.org)

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

#include "kpasswdserver.h"

#include <time.h>

#include <QtCore/QTimer>

#include <kapplication.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <kpassworddialog.h>
#include <kwallet.h>

#include "config.h"
#ifdef Q_WS_X11
#include <qx11info_x11.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#endif

extern "C" {
    KDE_EXPORT KDEDModule *create_kpasswdserver()
    {
       return new KPasswdServer();
    }
}

int
KPasswdServer::AuthInfoList::compareItems(Q3PtrCollection::Item n1, Q3PtrCollection::Item n2)
{
   if (!n1 || !n2)
      return 0;

   AuthInfo *i1 = (AuthInfo *) n1;
   AuthInfo *i2 = (AuthInfo *) n2;

   int l1 = i1->directory.length();
   int l2 = i2->directory.length();

   if (l1 > l2)
      return -1;
   if (l1 < l2)
      return 1;
   return 0;
}


KPasswdServer::KPasswdServer()
 : KDEDModule()
{
    m_authPending.setAutoDelete(true);
    m_seqNr = 0;
    m_wallet = 0;
    connect(this, SIGNAL(windowUnregistered(qlonglong)),
            this, SLOT(removeAuthForWindowId(qlonglong)));
}

KPasswdServer::~KPasswdServer()
{
    qDeleteAll(m_authDict);
    delete m_wallet;
}

// Helper - returns the wallet key to use for read/store/checking for existence.
static QString makeWalletKey( const QString& key, const QString& realm )
{
    return realm.isEmpty() ? key : key + '-' + realm;
}

// Helper for storeInWallet/readFromWallet
static QString makeMapKey( const char* key, int entryNumber )
{
    QString str = QLatin1String( key );
    if ( entryNumber > 1 )
        str += '-' + QString::number( entryNumber );
    return str;
}

static bool storeInWallet( KWallet::Wallet* wallet, const QString& key, const KIO::AuthInfo &info )
{
    if ( !wallet->hasFolder( KWallet::Wallet::PasswordFolder() ) )
        if ( !wallet->createFolder( KWallet::Wallet::PasswordFolder() ) )
            return false;
    wallet->setFolder( KWallet::Wallet::PasswordFolder() );
    // Before saving, check if there's already an entry with this login.
    // If so, replace it (with the new password). Otherwise, add a new entry.
    typedef QMap<QString,QString> Map;
    int entryNumber = 1;
    Map map;
    QString walletKey = makeWalletKey( key, info.realmValue );
    kDebug(130) << "storeInWallet: walletKey=" << walletKey << "  reading existing map" << endl;
    if ( wallet->readMap( walletKey, map ) == 0 ) {
        Map::ConstIterator end = map.end();
        Map::ConstIterator it = map.find( "login" );
        while ( it != end ) {
            if ( it.value() == info.username ) {
                break; // OK, overwrite this entry
            }
            it = map.find( QString( "login-" ) + QString::number( ++entryNumber ) );
        }
        // If no entry was found, create a new entry - entryNumber is set already.
    }
    const QString loginKey = makeMapKey( "login", entryNumber );
    const QString passwordKey = makeMapKey( "password", entryNumber );
    kDebug(130) << "storeInWallet: writing to " << loginKey << "," << passwordKey << endl;
    // note the overwrite=true by default
    map.insert( loginKey, info.username );
    map.insert( passwordKey, info.password );
    wallet->writeMap( walletKey, map );
    return true;
}


static bool readFromWallet( KWallet::Wallet* wallet, const QString& key, const QString& realm, QString& username, QString& password, bool userReadOnly, QMap<QString,QString>& knownLogins )
{
    //kDebug(130) << "readFromWallet: key=" << key << " username=" << username << " password=" /*<< password*/ << " userReadOnly=" << userReadOnly << " realm=" << realm << endl;
    if ( wallet->hasFolder( KWallet::Wallet::PasswordFolder() ) )
    {
        wallet->setFolder( KWallet::Wallet::PasswordFolder() );

        QMap<QString,QString> map;
        if ( wallet->readMap( makeWalletKey( key, realm ), map ) == 0 )
        {
            typedef QMap<QString,QString> Map;
            int entryNumber = 1;
            Map::ConstIterator end = map.end();
            Map::ConstIterator it = map.find( "login" );
            while ( it != end ) {
                //kDebug(130) << "readFromWallet: found " << it.key() << "=" << it.value() << endl;
                Map::ConstIterator pwdIter = map.find( makeMapKey( "password", entryNumber ) );
                if ( pwdIter != end ) {
                    if ( it.value() == username )
                        password = pwdIter.value();
                    knownLogins.insert( it.value(), pwdIter.value() );
                }

                it = map.find( QString( "login-" ) + QString::number( ++entryNumber ) );
            }
            //kDebug(130) << knownLogins.count() << " known logins" << endl;

            if ( !userReadOnly && !knownLogins.isEmpty() && username.isEmpty() ) {
                // Pick one, any one...
                username = knownLogins.begin().key();
                password = knownLogins.begin().value();
                //kDebug(130) << "readFromWallet: picked the first one : " << username << endl;
            }

            return true;
        }
    }
    return false;
}

QByteArray
KPasswdServer::checkAuthInfo(const QByteArray &data, qlonglong windowId, qlonglong usertime, const QDBusMessage &msg)
{
    KIO::AuthInfo info;
    {
        QByteArray data2(data);
        QDataStream stream(&data2, QIODevice::ReadOnly);
        stream >> info;
    }
    kDebug(130) << "KPasswdServer::checkAuthInfo: User= " << info.username
              << ", WindowId = " << windowId << endl;
    if( usertime != 0 )
        kapp->updateUserTimestamp( usertime );

    QString key = createCacheKey(info);

    Request *request = m_authPending.first();
    QString path2 = info.url.directory(KUrl::AppendTrailingSlash|KUrl::ObeyTrailingSlash);
    for(; request; request = m_authPending.next())
    {
       if (request->key != key)
           continue;

       if (info.verifyPath)
       {
         QString path1 = request->info.url.directory(KUrl::AppendTrailingSlash|KUrl::ObeyTrailingSlash);
          if (!path2.startsWith(path1))
             continue;
       }

       msg.setDelayedReply(true);
       request = new Request;
       request->transaction = msg;
       request->key = key;
       request->info = info;
       m_authWait.append(request);
       return data;             // return value will be ignored
    }

    const AuthInfo *result = findAuthInfoItem(key, info);
    if (!result || result->isCanceled)
    {
       if (!result &&
           (info.username.isEmpty() || info.password.isEmpty()) &&
           !KWallet::Wallet::keyDoesNotExist(KWallet::Wallet::NetworkWallet(),
                                             KWallet::Wallet::PasswordFolder(), makeWalletKey(key, info.realmValue)))
       {
          QMap<QString, QString> knownLogins;
          if (openWallet(windowId)) {
              if (readFromWallet(m_wallet, key, info.realmValue, info.username, info.password,
                             info.readOnly, knownLogins))
	      {
		      info.setModified(true);
                      // fall through
	      }
	  }
       } else {
           info.setModified(false);
       }

       QByteArray data2;
       QDataStream stream(&data2, QIODevice::WriteOnly);
       stream << info;
       return data2;
    }

    updateAuthExpire(key, result, windowId, false);

    info = copyAuthInfo(result);
    QByteArray data2;
    QDataStream stream(&data2, QIODevice::WriteOnly);
    stream << info;
    return data2;
}

QByteArray
KPasswdServer::queryAuthInfo(const QByteArray &data, const QString &errorMsg, qlonglong windowId,
                             qlonglong seqNr, qlonglong usertime, const QDBusMessage &msg)
{
    KIO::AuthInfo info;
    {
        QByteArray data2(data);
        QDataStream stream(&data2, QIODevice::ReadOnly);
        stream >> info;
    }
    kDebug(130) << "KPasswdServer::queryAuthInfo: User= " << info.username
              << ", Message= " << info.prompt << ", WindowId = " << windowId << endl;
    if ( !info.password.isEmpty() ) // should we really allow the caller to pre-fill the password?
        kDebug(130) <<  "password was set by caller" << endl;
    if( usertime != 0 )
        kapp->updateUserTimestamp( usertime );

    QString key = createCacheKey(info);
    Request *request = new Request;
    msg.setDelayedReply(true);
    request->transaction = msg;
    request->key = key;
    request->info = info;
    request->windowId = windowId;
    request->seqNr = seqNr;
    if (errorMsg == "<NoAuthPrompt>")
    {
       request->errorMsg.clear();
       request->prompt = false;
    }
    else
    {
       request->errorMsg = errorMsg;
       request->prompt = true;
    }
    m_authPending.append(request);

    if (m_authPending.count() == 1)
       QTimer::singleShot(0, this, SLOT(processRequest()));

    return QByteArray();        // return value is going to be ignored
}

void
KPasswdServer::addAuthInfo(const QByteArray &data, qlonglong windowId)
{
    KIO::AuthInfo info;
    {
        QByteArray data2(data);
        QDataStream stream(&data2, QIODevice::ReadOnly);
        stream >> info;
    }
    kDebug(130) << "KPasswdServer::addAuthInfo: User= " << info.username
              << ", RealmValue= " << info.realmValue << ", WindowId = " << windowId << endl;
    QString key = createCacheKey(info);

    m_seqNr++;

    addAuthInfoItem(key, info, windowId, m_seqNr, false);
}

bool
KPasswdServer::openWallet( int windowId )
{
    if ( m_wallet && !m_wallet->isOpen() ) { // forced closed
        delete m_wallet;
        m_wallet = 0;
    }
    if ( !m_wallet )
        m_wallet = KWallet::Wallet::openWallet(
            KWallet::Wallet::NetworkWallet(), (WId) windowId );
    return m_wallet != 0;
}

void
KPasswdServer::processRequest()
{
    Request *request = m_authPending.first();
    if (!request)
       return;

    KIO::AuthInfo &info = request->info;

    kDebug(130) << "KPasswdServer::processRequest: User= " << info.username
              << ", Message= " << info.prompt << endl;
    const AuthInfo *result = findAuthInfoItem(request->key, request->info);

    if (result && (request->seqNr < result->seqNr))
    {
        kDebug(130) << "KPasswdServer::processRequest: auto retry!" << endl;
        if (result->isCanceled)
        {
           info.setModified(false);
        }
        else
        {
           updateAuthExpire(request->key, result, request->windowId, false);
           info = copyAuthInfo(result);
        }
    }
    else
    {
        m_seqNr++;
        bool askPw = request->prompt;
        if (result && !info.username.isEmpty() &&
            !request->errorMsg.isEmpty())
        {
           QString prompt = request->errorMsg + "  ";
           prompt += i18n("Do you want to retry?");
           int dlgResult = KMessageBox::warningContinueCancel(0, prompt,
                           i18n("Authentication"), KGuiItem(i18n("Retry")));
           if (dlgResult != KMessageBox::Continue)
              askPw = false;
        }

        int dlgResult = QDialog::Rejected;
        if (askPw)
        {
            QString username = info.username;
            QString password = info.password;
            bool hasWalletData = false;
            QMap<QString, QString> knownLogins;

            if ( ( username.isEmpty() || password.isEmpty() )
                && !KWallet::Wallet::keyDoesNotExist(KWallet::Wallet::NetworkWallet(), KWallet::Wallet::PasswordFolder(), makeWalletKey( request->key, info.realmValue )) )
            {
                // no login+pass provided, check if kwallet has one
                if ( openWallet( request->windowId ) )
                    hasWalletData = readFromWallet( m_wallet, request->key, info.realmValue, username, password, info.readOnly, knownLogins );
            }

            KPasswordDialog dlg( 0l,  info.keepPassword ? ( KPasswordDialog::ShowUsernameLine |  KPasswordDialog::ShowKeepPassword) : KPasswordDialog::ShowUsernameLine ) ;
            dlg.setPrompt(info.prompt);
            dlg.setUsername(username);
            if (info.caption.isEmpty())
               dlg.setPlainCaption( i18n("Authorization Dialog") );
            else
               dlg.setPlainCaption( info.caption );

            if ( !info.comment.isEmpty() )
               dlg.addCommentLine( info.commentLabel, info.comment );

            if ( !password.isEmpty() )
               dlg.setPassword( password );

            if (info.readOnly)
               dlg.setUsernameReadOnly( true );
            else
               dlg.setKnownLogins( knownLogins );

            if (hasWalletData)
                dlg.setKeepPassword( true );

#ifdef Q_WS_X11
            XSetTransientForHint( QX11Info::display(), dlg.winId(), request->windowId);
#endif

            dlgResult = dlg.exec();

            if (dlgResult == QDialog::Accepted)
            {
               info.username = dlg.username();
               info.password = dlg.password();
               info.keepPassword = dlg.keepPassword();

               // When the user checks "keep password", that means:
               // * if the wallet is enabled, store it there for long-term, and in kpasswdserver
               // only for the duration of the window (#92928)
               // * otherwise store in kpasswdserver for the duration of the KDE session.
               if ( info.keepPassword ) {
                   if ( openWallet( request->windowId ) ) {
                       if ( storeInWallet( m_wallet, request->key, info ) )
                           // password is in wallet, don't keep it in memory after window is closed
                           info.keepPassword = false;
                   }
               }
            }
        }
        if ( dlgResult != QDialog::Accepted )
        {
            addAuthInfoItem(request->key, info, 0, m_seqNr, true);
            info.setModified( false );
        }
        else
        {
            addAuthInfoItem(request->key, info, request->windowId, m_seqNr, false);
            info.setModified( true );
        }
    }

    QByteArray replyData;

    QDataStream stream2(&replyData, QIODevice::WriteOnly);
    stream2 << info;
    QDBusConnection::sessionBus().send(request->transaction.createReply(QVariantList() << replyData << m_seqNr));

    m_authPending.remove((unsigned int) 0);

    // Check all requests in the wait queue.
    for(Request *waitRequest = m_authWait.first();
        waitRequest; )
    {
       bool keepQueued = false;
       QString key = waitRequest->key;

       request = m_authPending.first();
       QString path2 = waitRequest->info.url.directory(KUrl::AppendTrailingSlash|KUrl::ObeyTrailingSlash);
       for(; request; request = m_authPending.next())
       {
           if (request->key != key)
               continue;

           if (info.verifyPath)
           {
             QString path1 = request->info.url.directory(KUrl::AppendTrailingSlash|KUrl::ObeyTrailingSlash);
               if (!path2.startsWith(path1))
                   continue;
           }

           keepQueued = true;
           break;
       }
       if (keepQueued)
       {
           waitRequest = m_authWait.next();
       }
       else
       {
           const AuthInfo *result = findAuthInfoItem(waitRequest->key, waitRequest->info);
           QByteArray replyData;

           QDataStream stream2(&replyData, QIODevice::WriteOnly);

           if (!result || result->isCanceled)
           {
               waitRequest->info.setModified(false);
               stream2 << waitRequest->info;
           }
           else
           {
               updateAuthExpire(waitRequest->key, result, waitRequest->windowId, false);
               KIO::AuthInfo info = copyAuthInfo(result);
               stream2 << info;
           }

           QDBusConnection::sessionBus().send(waitRequest->transaction.createReply());

           m_authWait.remove();
           waitRequest = m_authWait.current();
       }
    }

    if (m_authPending.count())
       QTimer::singleShot(0, this, SLOT(processRequest()));

}

QString KPasswdServer::createCacheKey( const KIO::AuthInfo &info )
{
    if( !info.url.isValid() ) {
        // Note that a null key will break findAuthInfoItem later on...
        kWarning(130) << "createCacheKey: invalid URL " << info.url << endl;
        return QString();
    }

    // Generate the basic key sequence.
    QString key = info.url.protocol();
    key += '-';
    if (!info.url.user().isEmpty())
    {
       key += info.url.user();
       key += '@';
    }
    key += info.url.host();
    int port = info.url.port();
    if( port )
    {
      key += ':';
      key += QString::number(port);
    }

    return key;
}

KIO::AuthInfo
KPasswdServer::copyAuthInfo(const AuthInfo *i)
{
    KIO::AuthInfo result;
    result.url = i->url;
    result.username = i->username;
    result.password = i->password;
    result.realmValue = i->realmValue;
    result.digestInfo = i->digestInfo;
    result.setModified(true);

    return result;
}

const KPasswdServer::AuthInfo *
KPasswdServer::findAuthInfoItem(const QString &key, const KIO::AuthInfo &info)
{
   AuthInfoList *authList = m_authDict.value(key);
   if (!authList)
      return 0;

   QString path2 = info.url.directory(KUrl::AppendTrailingSlash|KUrl::ObeyTrailingSlash);
   for(AuthInfo *current = authList->first();
       current; )
   {
       if ((current->expire == AuthInfo::expTime) &&
          (difftime(time(0), current->expireTime) > 0))
       {
          authList->remove();
          current = authList->current();
          continue;
       }

       if (info.verifyPath)
       {
          QString path1 = current->directory;
          if (path2.startsWith(path1) &&
              (info.username.isEmpty() || info.username == current->username))
             return current;
       }
       else
       {
          if (current->realmValue == info.realmValue &&
              (info.username.isEmpty() || info.username == current->username))
             return current; // TODO: Update directory info,
       }

       current = authList->next();
   }
   return 0;
}

void
KPasswdServer::removeAuthInfoItem(const QString &key, const KIO::AuthInfo &info)
{
   AuthInfoList *authList = m_authDict.value(key);
   if (!authList)
      return;

   for(AuthInfo *current = authList->first();
       current; )
   {
       if (current->realmValue == info.realmValue)
       {
          authList->remove();
          current = authList->current();
       }
       else
       {
          current = authList->next();
       }
   }
   if (authList->isEmpty())
   {
       delete m_authDict.take(key);
   }
}


void
KPasswdServer::addAuthInfoItem(const QString &key, const KIO::AuthInfo &info, qlonglong windowId, qlonglong seqNr, bool canceled)
{
   AuthInfoList *authList = m_authDict.value(key);
   if (!authList)
   {
      authList = new AuthInfoList;
      m_authDict.insert(key, authList);
   }
   AuthInfo *current = authList->first();
   for(; current; current = authList->next())
   {
       if (current->realmValue == info.realmValue)
       {
          authList->take();
          break;
       }
   }

   if (!current)
   {
      current = new AuthInfo;
      current->expire = AuthInfo::expTime;
      kDebug(130) << "Creating AuthInfo" << endl;
   }
   else
   {
      kDebug(130) << "Updating AuthInfo" << endl;
   }

   current->url = info.url;
   current->directory = info.url.directory(KUrl::AppendTrailingSlash|KUrl::ObeyTrailingSlash);
   current->username = info.username;
   current->password = info.password;
   current->realmValue = info.realmValue;
   current->digestInfo = info.digestInfo;
   current->seqNr = seqNr;
   current->isCanceled = canceled;

   updateAuthExpire(key, current, windowId, info.keepPassword && !canceled);

   // Insert into list, keep the list sorted "longest path" first.
   authList->inSort(current);
}

void
KPasswdServer::updateAuthExpire(const QString &key, const AuthInfo *auth, qlonglong windowId, bool keep)
{
   AuthInfo *current = const_cast<AuthInfo *>(auth);
   if (keep)
   {
      current->expire = AuthInfo::expNever;
   }
   else if (windowId && (current->expire != AuthInfo::expNever))
   {
      current->expire = AuthInfo::expWindowClose;
      if (!current->windowList.contains(windowId))
         current->windowList.append(windowId);
   }
   else if (current->expire == AuthInfo::expTime)
   {
      current->expireTime = time(0)+10;
   }

   // Update mWindowIdList
   if (windowId)
   {
      QStringList *keysChanged = mWindowIdList.value(windowId);
      if (!keysChanged)
      {
         keysChanged = new QStringList;
         mWindowIdList.insert(windowId, keysChanged);
      }
      if (!keysChanged->contains(key))
         keysChanged->append(key);
   }
}

void
KPasswdServer::removeAuthForWindowId(qlonglong windowId)
{
   QStringList *keysChanged = mWindowIdList.value(windowId);
   if (!keysChanged) return;

   for(QStringList::ConstIterator it = keysChanged->begin();
       it != keysChanged->end(); ++it)
   {
      QString key = *it;
      AuthInfoList *authList = m_authDict.value(key);
      if (!authList)
         continue;

      AuthInfo *current = authList->first();
      for(; current; )
      {
        if (current->expire == AuthInfo::expWindowClose)
        {
           if (current->windowList.removeAll(windowId) && current->windowList.isEmpty())
           {
              authList->remove();
              current = authList->current();
              continue;
           }
        }
        current = authList->next();
      }
   }
}

#include "kpasswdserver.moc"
