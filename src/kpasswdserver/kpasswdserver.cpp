/*
    This file is part of the KDE Password Server

    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2005 David Faure (faure@kde.org)
    Copyright (C) 2012 Dawit Alemayehu (adawit@kde.org)

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

#include "kpasswdserveradaptor.h"

#include <kapplication.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <kpassworddialog.h>
#include <kwallet.h>
#include <kwindowsystem.h>

#include <kpluginfactory.h>
#include <kpluginloader.h>

#include <QtCore/QTimer>

#include <ctime>


K_PLUGIN_FACTORY(KPasswdServerFactory,
                 registerPlugin<KPasswdServer>();
    )
K_EXPORT_PLUGIN(KPasswdServerFactory("kpasswdserver"))

#define AUTHINFO_EXTRAFIELD_DOMAIN QLatin1String("domain")
#define AUTHINFO_EXTRAFIELD_ANONYMOUS QLatin1String("anonymous")
#define AUTHINFO_EXTRAFIELD_BYPASS_CACHE_AND_KWALLET QLatin1String("bypass-cache-and-kwallet")
#define AUTHINFO_EXTRAFIELD_SKIP_CACHING_ON_QUERY QLatin1String("skip-caching-on-query")
#define AUTHINFO_EXTRAFIELD_HIDE_USERNAME_INPUT QLatin1String("hide-username-line")

static int debugArea() { static int s_area = KDebug::registerArea("KPasswdServer"); return s_area; }

static qlonglong getRequestId()
{
    static qlonglong nextRequestId = 0;
    return nextRequestId++;
}

bool
KPasswdServer::AuthInfoContainer::Sorter::operator ()(AuthInfoContainer* n1, AuthInfoContainer* n2) const
{
   if (!n1 || !n2)
      return 0;

   const int l1 = n1->directory.length();
   const int l2 = n2->directory.length();
   return l1 < l2;
}


KPasswdServer::KPasswdServer(QObject* parent, const QList<QVariant>&)
 : KDEDModule(parent)
{
    KIO::AuthInfo::registerMetaTypes();

    m_seqNr = 0;
    m_wallet = 0;
    m_walletDisabled = false;

    KPasswdServerAdaptor *adaptor = new KPasswdServerAdaptor(this);
    // register separately from kded
    QDBusConnection::sessionBus().registerService("org.kde.kpasswdserver");
    // connect signals to the adaptor
    connect(this,
            SIGNAL(checkAuthInfoAsyncResult(qlonglong,qlonglong,KIO::AuthInfo)),
            adaptor,
            SIGNAL(checkAuthInfoAsyncResult(qlonglong,qlonglong,KIO::AuthInfo)));
    connect(this,
            SIGNAL(queryAuthInfoAsyncResult(qlonglong,qlonglong,KIO::AuthInfo)),
            adaptor,
            SIGNAL(queryAuthInfoAsyncResult(qlonglong,qlonglong,KIO::AuthInfo)));

    connect(this, SIGNAL(windowUnregistered(qlonglong)),
            this, SLOT(removeAuthForWindowId(qlonglong)));

    connect(KWindowSystem::self(), SIGNAL(windowRemoved(WId)),
            this, SLOT(windowRemoved(WId)));
}

KPasswdServer::~KPasswdServer()
{
    // TODO: what about clients waiting for requests? will they just
    //       notice kpasswdserver is gone from the dbus?
    qDeleteAll(m_authPending);
    qDeleteAll(m_authWait);
    qDeleteAll(m_authDict);
    qDeleteAll(m_authInProgress);
    qDeleteAll(m_authRetryInProgress);
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
    kDebug(debugArea()) << "walletKey =" << walletKey << "  reading existing map";
    if ( wallet->readMap( walletKey, map ) == 0 ) {
        Map::ConstIterator end = map.constEnd();
        Map::ConstIterator it = map.constFind( "login" );
        while ( it != end ) {
            if ( it.value() == info.username ) {
                break; // OK, overwrite this entry
            }
            it = map.constFind( QString( "login-" ) + QString::number( ++entryNumber ) );
        }
        // If no entry was found, create a new entry - entryNumber is set already.
    }
    const QString loginKey = makeMapKey( "login", entryNumber );
    const QString passwordKey = makeMapKey( "password", entryNumber );
    kDebug(debugArea()) << "writing to " << loginKey << "," << passwordKey;
    // note the overwrite=true by default
    map.insert( loginKey, info.username );
    map.insert( passwordKey, info.password );
    wallet->writeMap( walletKey, map );
    return true;
}


static bool readFromWallet( KWallet::Wallet* wallet, const QString& key, const QString& realm, QString& username, QString& password, bool userReadOnly, QMap<QString,QString>& knownLogins )
{
    //kDebug(debugArea()) << "key =" << key << " username =" << username << " password =" /*<< password*/ << " userReadOnly =" << userReadOnly << " realm =" << realm;
    if ( wallet->hasFolder( KWallet::Wallet::PasswordFolder() ) )
    {
        wallet->setFolder( KWallet::Wallet::PasswordFolder() );

        QMap<QString,QString> map;
        if ( wallet->readMap( makeWalletKey( key, realm ), map ) == 0 )
        {
            typedef QMap<QString,QString> Map;
            int entryNumber = 1;
            Map::ConstIterator end = map.constEnd();
            Map::ConstIterator it = map.constFind( "login" );
            while ( it != end ) {
                //kDebug(debugArea()) << "found " << it.key() << "=" << it.value();
                Map::ConstIterator pwdIter = map.constFind( makeMapKey( "password", entryNumber ) );
                if ( pwdIter != end ) {
                    if ( it.value() == username )
                        password = pwdIter.value();
                    knownLogins.insert( it.value(), pwdIter.value() );
                }

                it = map.constFind( QString( "login-" ) + QString::number( ++entryNumber ) );
            }
            //kDebug(debugArea()) << knownLogins.count() << " known logins";

            if ( !userReadOnly && !knownLogins.isEmpty() && username.isEmpty() ) {
                // Pick one, any one...
                username = knownLogins.begin().key();
                password = knownLogins.begin().value();
                //kDebug(debugArea()) << "picked the first one:" << username;
            }

            return true;
        }
    }
    return false;
}

bool KPasswdServer::hasPendingQuery(const QString &key, const KIO::AuthInfo &info)
{
    const QString path2 (info.url.directory(KUrl::AppendTrailingSlash | KUrl::ObeyTrailingSlash));
    Q_FOREACH(const Request *request, m_authPending) {
        if (request->key != key) {
            continue;
        }

        if (info.verifyPath) {
            const QString path1 (request->info.url.directory(KUrl::AppendTrailingSlash |
                                                             KUrl::ObeyTrailingSlash));
            if (!path2.startsWith(path1)) {
                continue;
            }
        }

        return true;
    }

    return false;
}

QByteArray
KPasswdServer::checkAuthInfo(const QByteArray &data, qlonglong windowId, qlonglong usertime)
{
    KIO::AuthInfo info;
    QDataStream stream(data);
    stream >> info;
    if (usertime != 0) {
        kapp->updateUserTimestamp(usertime);
    }

    // if the check depends on a pending query, delay it
    // until that query is finished.
    const QString key (createCacheKey(info));
    if (hasPendingQuery(key, info)) {
        setDelayedReply(true);
        Request *pendingCheck = new Request;
        pendingCheck->isAsync = false;
        if (calledFromDBus()) {
            pendingCheck->transaction = message();
        }
        pendingCheck->key = key;
        pendingCheck->info = info;
        m_authWait.append(pendingCheck);
        return data;             // return value will be ignored
    }

    // kDebug(debugArea()) << "key =" << key << "user =" << info.username << "windowId =" << windowId;
    const AuthInfoContainer *result = findAuthInfoItem(key, info);
    if (!result || result->isCanceled)
    {
        if (!result &&
            (info.username.isEmpty() || info.password.isEmpty()) &&
            !KWallet::Wallet::keyDoesNotExist(KWallet::Wallet::NetworkWallet(),
                                              KWallet::Wallet::PasswordFolder(),
                                              makeWalletKey(key, info.realmValue)))
        {
            QMap<QString, QString> knownLogins;
            if (openWallet(windowId)) {
                if (readFromWallet(m_wallet, key, info.realmValue, info.username,
                                   info.password, info.readOnly, knownLogins))
                {
                    info.setModified(true);
                            // fall through
                }
            }
        } else {
            info.setModified(false);
        }
    } else {
        kDebug(debugArea()) << "Found cached authentication for" << key;
        updateAuthExpire(key, result, windowId, false);
        copyAuthInfo(result, info);
    }

    QByteArray data2;
    QDataStream stream2(&data2, QIODevice::WriteOnly);
    stream2 << info;
    return data2;
}

qlonglong KPasswdServer::checkAuthInfoAsync(KIO::AuthInfo info, qlonglong windowId,
                                            qlonglong usertime)
{
    if (usertime != 0) {
        kapp->updateUserTimestamp(usertime);
    }

    // send the request id back to the client
    qlonglong requestId = getRequestId();
    kDebug(debugArea()) << "User =" << info.username << ", WindowId =" << windowId;
    if (calledFromDBus()) {
        QDBusMessage reply(message().createReply(requestId));
        QDBusConnection::sessionBus().send(reply);
    }

    // if the check depends on a pending query, delay it
    // until that query is finished.
    const QString key (createCacheKey(info));
    if (hasPendingQuery(key, info)) {
        Request *pendingCheck = new Request;
        pendingCheck->isAsync = true;
        pendingCheck->requestId = requestId;
        pendingCheck->key = key;
        pendingCheck->info = info;
        m_authWait.append(pendingCheck);
        return 0; // ignored as we already sent a reply
    }

    const AuthInfoContainer *result = findAuthInfoItem(key, info);
    if (!result || result->isCanceled)
    {
        if (!result &&
            (info.username.isEmpty() || info.password.isEmpty()) &&
            !KWallet::Wallet::keyDoesNotExist(KWallet::Wallet::NetworkWallet(),
                                              KWallet::Wallet::PasswordFolder(),
                                              makeWalletKey(key, info.realmValue)))
        {
            QMap<QString, QString> knownLogins;
            if (openWallet(windowId)) {
                if (readFromWallet(m_wallet, key, info.realmValue, info.username,
                                   info.password, info.readOnly, knownLogins))
                {
                    info.setModified(true);
                            // fall through
                }
            }
        } else {
            info.setModified(false);
        }
    } else {
        // kDebug(debugArea()) << "Found cached authentication for" << key;
        updateAuthExpire(key, result, windowId, false);
        copyAuthInfo(result, info);
    }

    emit checkAuthInfoAsyncResult(requestId, m_seqNr, info);
    return 0; // ignored
}

QByteArray
KPasswdServer::queryAuthInfo(const QByteArray &data, const QString &errorMsg,
                             qlonglong windowId, qlonglong seqNr, qlonglong usertime)
{
    KIO::AuthInfo info;
    QDataStream stream(data);
    stream >> info;

    kDebug(debugArea()) << "User =" << info.username << ", WindowId =" << windowId
                        << "seqNr =" << seqNr << ", errorMsg =" << errorMsg;

    if ( !info.password.isEmpty() ) { // should we really allow the caller to pre-fill the password?
        kDebug(debugArea()) << "password was set by caller";
    }
    if (usertime != 0) {
        kapp->updateUserTimestamp(usertime);
    }

    const QString key (createCacheKey(info));
    Request *request = new Request;
    setDelayedReply(true);
    request->isAsync = false;
    request->transaction = message();
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

qlonglong
KPasswdServer::queryAuthInfoAsync(const KIO::AuthInfo &info, const QString &errorMsg,
                                  qlonglong windowId, qlonglong seqNr, qlonglong usertime)
{
    kDebug(debugArea()) << "User =" << info.username << ", WindowId =" << windowId
                        << "seqNr =" << seqNr << ", errorMsg =" << errorMsg;

    if (!info.password.isEmpty()) {
        kDebug(debugArea()) << "password was set by caller";
    }
    if (usertime != 0) {
        kapp->updateUserTimestamp(usertime);
    }

    const QString key (createCacheKey(info));
    Request *request = new Request;
    request->isAsync = true;
    request->requestId = getRequestId();
    request->key = key;
    request->info = info;
    request->windowId = windowId;
    request->seqNr = seqNr;
    if (errorMsg == "<NoAuthPrompt>") {
        request->errorMsg.clear();
        request->prompt = false;
    } else {
        request->errorMsg = errorMsg;
        request->prompt = true;
    }
    m_authPending.append(request);

    if (m_authPending.count() == 1) {
        QTimer::singleShot(0, this, SLOT(processRequest()));
    }

    return request->requestId;
}

void
KPasswdServer::addAuthInfo(const KIO::AuthInfo &info, qlonglong windowId)
{
    kDebug(debugArea()) << "User =" << info.username << ", Realm =" << info.realmValue << ", WindowId =" << windowId;
    const QString key (createCacheKey(info));

    m_seqNr++;

    if (!m_walletDisabled && openWallet(windowId) && storeInWallet(m_wallet, key, info)) {
        // Since storing the password in the wallet succeeded, make sure the
        // password information is stored in memory only for the duration the
        // windows associated with it are still around.
        AuthInfo authToken (info);
        authToken.keepPassword = false;
        addAuthInfoItem(key, authToken, windowId, m_seqNr, false);
        return;
    }

    addAuthInfoItem(key, info, windowId, m_seqNr, false);
}

void
KPasswdServer::addAuthInfo(const QByteArray &data, qlonglong windowId)
{
    KIO::AuthInfo info;
    QDataStream stream(data);
    stream >> info;
    addAuthInfo(info, windowId);
}

void
KPasswdServer::removeAuthInfo(const QString& host, const QString& protocol, const QString& user)
{
    kDebug(debugArea()) << protocol << host << user;

    QHashIterator< QString, AuthInfoContainerList* > dictIterator(m_authDict);
    while (dictIterator.hasNext())
    {
        dictIterator.next();

        AuthInfoContainerList *authList = dictIterator.value();
        if (!authList)
            continue;

        Q_FOREACH(AuthInfoContainer *current, *authList)
        {
            kDebug(debugArea()) << "Evaluating: " << current->info.url.protocol()
                     << current->info.url.host()
                     << current->info.username;
            if (current->info.url.protocol() == protocol &&
               current->info.url.host() == host &&
               (current->info.username == user || user.isEmpty()))
            {
                kDebug(debugArea()) << "Removing this entry";
                removeAuthInfoItem(dictIterator.key(), current->info);
            }
        }
    }
}

bool
KPasswdServer::openWallet( qlonglong windowId )
{
    if ( m_wallet && !m_wallet->isOpen() ) { // forced closed
        delete m_wallet;
        m_wallet = 0;
    }
    if ( !m_wallet )
        m_wallet = KWallet::Wallet::openWallet(
            KWallet::Wallet::NetworkWallet(), (WId)(windowId));
    return m_wallet != 0;
}

void
KPasswdServer::processRequest()
{
    if (m_authPending.isEmpty()) {
        return;
    }

    QScopedPointer<Request> request (m_authPending.takeFirst());

    // Prevent multiple prompts originating from the same window or the same
    // key (server address).
    const QString windowIdStr = QString::number(request->windowId);
    if (m_authPrompted.contains(windowIdStr) || m_authPrompted.contains(request->key)) {
        m_authPending.prepend(request.take());  // put it back.
        return;
    }

    m_authPrompted.append(windowIdStr);
    m_authPrompted.append(request->key);

    KIO::AuthInfo &info = request->info;

    // NOTE: If info.username is empty and info.url.user() is not, set
    // info.username to info.url.user() to ensure proper caching. See
    // note passwordDialogDone.
    if (info.username.isEmpty() && !info.url.user().isEmpty()) {
        info.username = info.url.user();
    }
    const bool bypassCacheAndKWallet = info.getExtraField(AUTHINFO_EXTRAFIELD_BYPASS_CACHE_AND_KWALLET).toBool();

    const AuthInfoContainer *result = findAuthInfoItem(request->key, request->info);
    kDebug(debugArea()) << "key=" << request->key << ", user=" << info.username << "seqNr: request=" << request->seqNr << ", result=" << (result ? result->seqNr : -1);

    if (!bypassCacheAndKWallet && result && (request->seqNr < result->seqNr))
    {
        kDebug(debugArea()) << "auto retry!";
        if (result->isCanceled)
        {
           info.setModified(false);
        }
        else
        {
           updateAuthExpire(request->key, result, request->windowId, false);
           copyAuthInfo(result, info);
        }
    }
    else
    {
        m_seqNr++;
        if (result && !request->errorMsg.isEmpty())
        {
            QString prompt (request->errorMsg.trimmed());
            prompt += QLatin1Char('\n');
            prompt += i18n("Do you want to retry?");

            KDialog* dlg = new KDialog(0, Qt::Dialog);
            connect(dlg, SIGNAL(finished(int)), this, SLOT(retryDialogDone(int)));
            connect(this, SIGNAL(destroyed(QObject*)), dlg, SLOT(deleteLater()));
            dlg->setPlainCaption(i18n("Retry Authentication"));
            dlg->setWindowIcon(KIcon("dialog-password"));
            dlg->setButtons(KDialog::Yes | KDialog::No);
            dlg->setObjectName("warningOKCancel");
            KGuiItem buttonContinue (i18nc("@action:button filter-continue", "Retry"));
            dlg->setButtonGuiItem(KDialog::Yes, buttonContinue);
            dlg->setButtonGuiItem(KDialog::No, KStandardGuiItem::cancel());
            dlg->setDefaultButton(KDialog::Yes);
            dlg->setEscapeButton(KDialog::No);

            KMessageBox::createKMessageBox(dlg, QMessageBox::Warning, prompt,
                                           QStringList(), QString(), 0L,
                                           (KMessageBox::Notify | KMessageBox::NoExec));

        #ifndef Q_WS_WIN
            KWindowSystem::setMainWindow(dlg, request->windowId);
        #else
            KWindowSystem::setMainWindow(dlg, (HWND)(long)request->windowId);
        #endif

            kDebug(debugArea()) << "Calling open on retry dialog" << dlg;
            m_authRetryInProgress.insert(dlg, request.take());
            dlg->open();
            return;
        }

        if (request->prompt)
        {
            showPasswordDialog(request.take());
            return;
        }
        else
        {
            if (!bypassCacheAndKWallet && request->prompt)
            {
                addAuthInfoItem(request->key, info, 0, m_seqNr, true);
            }
            info.setModified( false );
        }
    }

    sendResponse(request.data());
}

QString KPasswdServer::createCacheKey( const KIO::AuthInfo &info )
{
    if( !info.url.isValid() ) {
        // Note that a null key will break findAuthInfoItem later on...
        kWarning(debugArea()) << "createCacheKey: invalid URL " << info.url ;
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

void KPasswdServer::copyAuthInfo(const AuthInfoContainer *i, KIO::AuthInfo& info)
{
    info = i->info;
    info.setModified(true);
}

const KPasswdServer::AuthInfoContainer *
KPasswdServer::findAuthInfoItem(const QString &key, const KIO::AuthInfo &info)
{
   // kDebug(debugArea()) << "key=" << key << ", user=" << info.username;

   AuthInfoContainerList *authList = m_authDict.value(key);
   if (authList)
   {
      QString path2 = info.url.directory(KUrl::AppendTrailingSlash|KUrl::ObeyTrailingSlash);
      Q_FOREACH(AuthInfoContainer *current, *authList)
      {
          if (current->expire == AuthInfoContainer::expTime &&
              static_cast<qulonglong>(time(0)) > current->expireTime)
          {
              authList->removeOne(current);
              delete current;
              continue;
          }

          if (info.verifyPath)
          {
              QString path1 = current->directory;
              if (path2.startsWith(path1) &&
                  (info.username.isEmpty() || info.username == current->info.username))
                return current;
          }
          else
          {
              if (current->info.realmValue == info.realmValue &&
                  (info.username.isEmpty() || info.username == current->info.username))
                return current; // TODO: Update directory info,
          }
      }
   }
   return 0;
}

void
KPasswdServer::removeAuthInfoItem(const QString &key, const KIO::AuthInfo &info)
{
   AuthInfoContainerList *authList = m_authDict.value(key);
   if (!authList)
      return;

   Q_FOREACH(AuthInfoContainer *current, *authList)
   {
       if (current->info.realmValue == info.realmValue)
       {
          authList->removeOne(current);
          delete current;
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
   kDebug(debugArea()) << "key=" << key
                       << "window-id=" << windowId
                       << "username=" << info.username
                       << "realm=" << info.realmValue
                       << "seqNr=" << seqNr
                       << "keepPassword?" << info.keepPassword
                       << "canceled?" << canceled;
   AuthInfoContainerList *authList = m_authDict.value(key);
   if (!authList)
   {
      authList = new AuthInfoContainerList;
      m_authDict.insert(key, authList);
   }
   AuthInfoContainer *authItem = 0;
   Q_FOREACH(AuthInfoContainer* current, *authList)
   {
       if (current->info.realmValue == info.realmValue)
       {
          authList->removeAll(current);
          authItem = current;
          break;
       }
   }

   if (!authItem)
   {
      kDebug(debugArea()) << "Creating AuthInfoContainer";
      authItem = new AuthInfoContainer;
      authItem->expire = AuthInfoContainer::expTime;
   }

   authItem->info = info;
   authItem->directory = info.url.directory(KUrl::AppendTrailingSlash|KUrl::ObeyTrailingSlash);
   authItem->seqNr = seqNr;
   authItem->isCanceled = canceled;

   updateAuthExpire(key, authItem, windowId, (info.keepPassword && !canceled));

   // Insert into list, keep the list sorted "longest path" first.
   authList->append(authItem);
   qSort(authList->begin(), authList->end(), AuthInfoContainer::Sorter());
}

void
KPasswdServer::updateAuthExpire(const QString &key, const AuthInfoContainer *auth, qlonglong windowId, bool keep)
{
   AuthInfoContainer *current = const_cast<AuthInfoContainer *>(auth);
   Q_ASSERT(current);

   kDebug(debugArea()) << "key=" << key << "expire=" << current->expire << "window-id=" << windowId << "keep=" << keep;

   if (keep && !windowId)
   {
      current->expire = AuthInfoContainer::expNever;
   }
   else if (windowId && (current->expire != AuthInfoContainer::expNever))
   {
      current->expire = AuthInfoContainer::expWindowClose;
      if (!current->windowList.contains(windowId))
         current->windowList.append(windowId);
   }
   else if (current->expire == AuthInfoContainer::expTime)
   {
      current->expireTime = time(0) + 10;
   }

   // Update mWindowIdList
   if (windowId)
   {
      QStringList& keysChanged = mWindowIdList[windowId]; // find or insert
      if (!keysChanged.contains(key))
         keysChanged.append(key);
   }
}

void
KPasswdServer::removeAuthForWindowId(qlonglong windowId)
{
   const QStringList keysChanged = mWindowIdList.value(windowId);
   foreach (const QString &key, keysChanged)
   {
      AuthInfoContainerList *authList = m_authDict.value(key);
      if (!authList)
         continue;

      QMutableListIterator<AuthInfoContainer*> it (*authList);
      while (it.hasNext())
      {
        AuthInfoContainer* current = it.next();
        if (current->expire == AuthInfoContainer::expWindowClose)
        {
           if (current->windowList.removeAll(windowId) && current->windowList.isEmpty())
           {
              delete current;
              it.remove();
           }
        }
      }
   }
}

void KPasswdServer::showPasswordDialog (KPasswdServer::Request* request)
{
    KIO::AuthInfo &info = request->info;
    const bool bypassCacheAndKWallet = info.getExtraField(AUTHINFO_EXTRAFIELD_BYPASS_CACHE_AND_KWALLET).toBool();

    QString username = info.username;
    QString password = info.password;
    bool hasWalletData = false;
    QMap<QString, QString> knownLogins;

    if ( !bypassCacheAndKWallet
        && ( username.isEmpty() || password.isEmpty() )
        && !KWallet::Wallet::keyDoesNotExist(KWallet::Wallet::NetworkWallet(), KWallet::Wallet::PasswordFolder(), makeWalletKey( request->key, info.realmValue )) )
    {
        // no login+pass provided, check if kwallet has one
        if ( openWallet( request->windowId ) )
            hasWalletData = readFromWallet( m_wallet, request->key, info.realmValue, username, password, info.readOnly, knownLogins );
    }

    // assemble dialog-flags
    KPasswordDialog::KPasswordDialogFlags dialogFlags;

    if (info.getExtraField(AUTHINFO_EXTRAFIELD_DOMAIN).isValid())
    {
        dialogFlags |= KPasswordDialog::ShowDomainLine;
        if (info.getExtraFieldFlags(AUTHINFO_EXTRAFIELD_DOMAIN) & KIO::AuthInfo::ExtraFieldReadOnly)
        {
            dialogFlags |= KPasswordDialog::DomainReadOnly;
        }
    }

    if (info.getExtraField(AUTHINFO_EXTRAFIELD_ANONYMOUS).isValid())
    {
        dialogFlags |= KPasswordDialog::ShowAnonymousLoginCheckBox;
    }

    if (!info.getExtraField(AUTHINFO_EXTRAFIELD_HIDE_USERNAME_INPUT).toBool())
    {
        dialogFlags |= KPasswordDialog::ShowUsernameLine;
    }

    // If wallet is not enabled and the caller explicitly requested for it,
    // do not show the keep password checkbox.
    if (info.keepPassword && KWallet::Wallet::isEnabled())
        dialogFlags |= KPasswordDialog::ShowKeepPassword;

    // instantiate dialog
#ifndef Q_WS_WIN
    kDebug(debugArea()) << "Widget for" << request->windowId << QWidget::find(request->windowId) << QApplication::activeWindow();
#else
    kDebug(debugArea()) << "Widget for" << request->windowId << QWidget::find((HWND)request->windowId) << QApplication::activeWindow();
#endif

    KPasswordDialog* dlg = new KPasswordDialog(0, dialogFlags);
    connect(dlg, SIGNAL(finished(int)), this, SLOT(passwordDialogDone(int)));
    connect(this, SIGNAL(destroyed(QObject*)), dlg, SLOT(deleteLater()));

    dlg->setPrompt(info.prompt);
    dlg->setUsername(username);
    if (info.caption.isEmpty())
        dlg->setPlainCaption( i18n("Authentication Dialog") );
    else
        dlg->setPlainCaption( info.caption );

    if ( !info.comment.isEmpty() )
        dlg->addCommentLine( info.commentLabel, info.comment );

    if ( !password.isEmpty() )
        dlg->setPassword( password );

    if (info.readOnly)
        dlg->setUsernameReadOnly( true );
    else
        dlg->setKnownLogins( knownLogins );

    if (hasWalletData)
        dlg->setKeepPassword( true );

    if (info.getExtraField(AUTHINFO_EXTRAFIELD_DOMAIN).isValid ())
        dlg->setDomain(info.getExtraField(AUTHINFO_EXTRAFIELD_DOMAIN).toString());

    if (info.getExtraField(AUTHINFO_EXTRAFIELD_ANONYMOUS).isValid () && password.isEmpty() && username.isEmpty())
        dlg->setAnonymousMode(info.getExtraField(AUTHINFO_EXTRAFIELD_ANONYMOUS).toBool());

#ifndef Q_WS_WIN
    KWindowSystem::setMainWindow(dlg, request->windowId);
#else
    KWindowSystem::setMainWindow(dlg, (HWND)request->windowId);
#endif

    kDebug(debugArea()) << "Showing password dialog" << dlg << ", window-id=" << request->windowId;
    m_authInProgress.insert(dlg, request);
    dlg->open();
}


void KPasswdServer::sendResponse (KPasswdServer::Request* request)
{
    Q_ASSERT(request);
    if (!request) {
        return;
    }

    kDebug(debugArea()) << "key=" << request->key;
    if (request->isAsync) {
        emit queryAuthInfoAsyncResult(request->requestId, m_seqNr, request->info);
    } else {
        QByteArray replyData;
        QDataStream stream2(&replyData, QIODevice::WriteOnly);
        stream2 << request->info;
        QDBusConnection::sessionBus().send(request->transaction.createReply(QVariantList() << replyData << m_seqNr));
    }

    // Check all requests in the wait queue.
    Request *waitRequest;
    QMutableListIterator<Request*> it(m_authWait);
    while (it.hasNext()) {
        waitRequest = it.next();

        if (!hasPendingQuery(waitRequest->key, waitRequest->info))
        {
            const AuthInfoContainer *result = findAuthInfoItem(waitRequest->key,
                                                               waitRequest->info);
            QByteArray replyData;

            QDataStream stream2(&replyData, QIODevice::WriteOnly);

            KIO::AuthInfo rcinfo;
            if (!result || result->isCanceled)
            {
                waitRequest->info.setModified(false);
                stream2 << waitRequest->info;
            }
            else
            {
                updateAuthExpire(waitRequest->key, result, waitRequest->windowId, false);
                copyAuthInfo(result, rcinfo);
                stream2 << rcinfo;
            }

            if (waitRequest->isAsync) {
                emit checkAuthInfoAsyncResult(waitRequest->requestId, m_seqNr, rcinfo);
            } else {
                QDBusConnection::sessionBus().send(waitRequest->transaction.createReply(QVariantList() << replyData << m_seqNr));
            }

            delete waitRequest;
            it.remove();
        }
    }

    // Re-enable password request processing for the current window id again.
    m_authPrompted.removeAll(QString::number(request->windowId));
    m_authPrompted.removeAll(request->key);

    if (m_authPending.count())
       QTimer::singleShot(0, this, SLOT(processRequest()));
}

void KPasswdServer::passwordDialogDone (int result)
{
    KPasswordDialog* dlg = qobject_cast<KPasswordDialog*>(sender());
    Q_ASSERT(dlg);

    QScopedPointer<Request> request (m_authInProgress.take(dlg));
    Q_ASSERT(request);  // request should never be NULL.

    if (request) {
        KIO::AuthInfo& info = request->info;
        const bool bypassCacheAndKWallet = info.getExtraField(AUTHINFO_EXTRAFIELD_BYPASS_CACHE_AND_KWALLET).toBool();

        kDebug(debugArea()) << "dialog result=" << result << ", bypassCacheAndKWallet?" << bypassCacheAndKWallet;
        if (dlg && result == KDialog::Accepted) {
            Q_ASSERT(dlg);
            const QString oldUsername (info.username);
            info.username = dlg->username();
            info.password = dlg->password();
            info.keepPassword = dlg->keepPassword();

            if (info.getExtraField(AUTHINFO_EXTRAFIELD_DOMAIN).isValid ())
                info.setExtraField(AUTHINFO_EXTRAFIELD_DOMAIN, dlg->domain());
            if (info.getExtraField(AUTHINFO_EXTRAFIELD_ANONYMOUS).isValid ())
                info.setExtraField(AUTHINFO_EXTRAFIELD_ANONYMOUS, dlg->anonymousMode());

            // When the user checks "keep password", that means:
            // * if the wallet is enabled, store it there for long-term, and in kpasswdserver
            // only for the duration of the window (#92928)
            // * otherwise store in kpasswdserver for the duration of the KDE session.
            if (!bypassCacheAndKWallet) {
                /*
                  NOTE: The following code changes the key under which the auth
                  info is stored in memory if the request url contains a username.
                  e.g. "ftp://user@localhost", but the user changes that username
                  in the password dialog.

                  Since the key generated to store the credential contains the
                  username from the request URL, the key must be updated on such
                  changes. Otherwise, the key will not be found on subsequent
                  requests and the user will be end up being prompted over and
                  over to re-enter the password unnecessarily.
                */
                if (!info.url.user().isEmpty() && info.username != info.url.user()) {
                    const QString oldKey(request->key);
                    removeAuthInfoItem(oldKey, info);
                    info.url.setUser(info.username);
                    request->key = createCacheKey(info);
                    updateCachedRequestKey(m_authPending, oldKey, request->key);
                    updateCachedRequestKey(m_authWait, oldKey, request->key);
                }

                const bool skipAutoCaching = info.getExtraField(AUTHINFO_EXTRAFIELD_SKIP_CACHING_ON_QUERY).toBool();
                if (!skipAutoCaching && info.keepPassword && openWallet(request->windowId)) {
                    if ( storeInWallet( m_wallet, request->key, info ) )
                        // password is in wallet, don't keep it in memory after window is closed
                        info.keepPassword = false;
                }
                addAuthInfoItem(request->key, info, request->windowId, m_seqNr, false);
            }
            info.setModified( true );
        } else {
            if (!bypassCacheAndKWallet && request->prompt) {
                addAuthInfoItem(request->key, info, 0, m_seqNr, true);
            }
            info.setModified( false );
        }

        sendResponse(request.data());
    }

    dlg->deleteLater();
}

void KPasswdServer::retryDialogDone (int result)
{
    KDialog* dlg = qobject_cast<KDialog*>(sender());
    Q_ASSERT(dlg);

    QScopedPointer<Request> request (m_authRetryInProgress.take(dlg));
    Q_ASSERT(request);

    if (request) {
        if (result == KDialog::Yes) {
            showPasswordDialog(request.take());
        } else {
            // NOTE: If the user simply cancels the retry dialog, we remove the
            // credential stored under this key because the original attempt to
            // use it has failed. Otherwise, the failed credential would be cached
            // and used subsequently.
            //
            // TODO: decide whether it should be removed from the wallet too.
            KIO::AuthInfo& info = request->info;
            removeAuthInfoItem(request->key, request->info);
            info.setModified(false);
            sendResponse(request.data());
        }
    }
}

void KPasswdServer::windowRemoved (WId id)
{
    bool foundMatch = false;
    if (!m_authInProgress.isEmpty()) {
        const qlonglong windowId = (qlonglong)(id);
        QMutableHashIterator<QObject*, Request*> it (m_authInProgress);
        while (it.hasNext()) {
            it.next();
            if (it.value()->windowId == windowId) {
                Request* request = it.value();
                QObject* obj = it.key();
                it.remove();
                m_authPrompted.removeAll(QString::number(request->windowId));
                m_authPrompted.removeAll(request->key);
                delete obj;
                delete request;
                foundMatch = true;
            }
        }
    }

    if (!foundMatch && !m_authRetryInProgress.isEmpty()) {
        const qlonglong windowId = (qlonglong)(id);
        QMutableHashIterator<QObject*, Request*> it (m_authRetryInProgress);
        while (it.hasNext()) {
            it.next();
            if (it.value()->windowId == windowId) {
                Request* request = it.value();
                QObject* obj = it.key();
                it.remove();
                delete obj;
                delete request;
            }
        }
    }
}

void KPasswdServer::updateCachedRequestKey (QList<KPasswdServer::Request*>& list, const QString& oldKey, const QString& newKey)
{
    QListIterator<Request*> it (list);
    while (it.hasNext()) {
        Request* r = it.next();
        if (r->key == oldKey) {
            r->key = newKey;
        }
    }
}


#include "kpasswdserver.moc"
