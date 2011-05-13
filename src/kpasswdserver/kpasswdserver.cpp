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
#include <kwindowsystem.h>

#include <kpluginfactory.h>
#include <kpluginloader.h>

#include "kpasswdserveradaptor.h"

K_PLUGIN_FACTORY(KPasswdServerFactory,
                 registerPlugin<KPasswdServer>();
    )
K_EXPORT_PLUGIN(KPasswdServerFactory("kpasswdserver"))

#define AUTHINFO_EXTRAFIELD_DOMAIN "domain"
#define AUTHINFO_EXTRAFIELD_ANONYMOUS "anonymous"
#define AUTHINFO_EXTRAFIELD_BYPASS_CACHE_AND_KWALLET "bypass-cache-and-kwallet"
#define AUTHINFO_EXTRAFIELD_SKIP_CACHING_ON_QUERY QLatin1String("skip-caching-on-query")

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

    KPasswdServerAdaptor *adaptor = new KPasswdServerAdaptor(this);
    // register separately from kded
    QDBusConnection::sessionBus().registerService("org.kde.kpasswdserver");
    // connect signals to the adaptor
    connect(this,
            SIGNAL(checkAuthInfoAsyncResult(qlonglong, qlonglong, const KIO::AuthInfo &)),
            adaptor,
            SIGNAL(checkAuthInfoAsyncResult(qlonglong, qlonglong, const KIO::AuthInfo &)));
    connect(this,
            SIGNAL(queryAuthInfoAsyncResult(qlonglong, qlonglong, const KIO::AuthInfo &)),
            adaptor,
            SIGNAL(queryAuthInfoAsyncResult(qlonglong, qlonglong, const KIO::AuthInfo &)));

    connect(this, SIGNAL(windowUnregistered(qlonglong)),
            this, SLOT(removeAuthForWindowId(qlonglong)));
}

KPasswdServer::~KPasswdServer()
{
    // TODO: what about clients waiting for requests? will they just
    //       notice kpasswdserver is gone from the dbus?
    qDeleteAll(m_authPending);
    qDeleteAll(m_authWait);
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
    QString path2 = info.url.directory(KUrl::AppendTrailingSlash | KUrl::ObeyTrailingSlash);
    Q_FOREACH(const Request *request, m_authPending) {
        if (request->key != key) {
            continue;
        }

        if (info.verifyPath) {
            QString path1 = request->info.url.directory(KUrl::AppendTrailingSlash |
                                                        KUrl::ObeyTrailingSlash);
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
    kDebug(debugArea()) << "User =" << info.username << ", WindowId =" << windowId << endl;
    if (usertime != 0) {
        kapp->updateUserTimestamp(usertime);
    }

    // if the check depends on a pending query, delay it
    // until that query is finished.
    QString key = createCacheKey(info);
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
        updateAuthExpire(key, result, windowId, false);
        info = copyAuthInfo(result);
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
    QString key = createCacheKey(info);
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
        updateAuthExpire(key, result, windowId, false);
        info = copyAuthInfo(result);
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
    kDebug(debugArea()) << "User =" << info.username << ", Message= " << info.prompt
                << ", WindowId =" << windowId << endl;
    if ( !info.password.isEmpty() ) // should we really allow the caller to pre-fill the password?
        kDebug(debugArea()) << "password was set by caller";
    if (usertime != 0) {
        kapp->updateUserTimestamp(usertime);
    }

    QString key = createCacheKey(info);
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
    kDebug(debugArea()) << "User =" << info.username << ", Message= " << info.prompt
                << ", WindowId =" << windowId << endl;
    if (!info.password.isEmpty()) {
        kDebug(debugArea()) << "password was set by caller";
    }
    if (usertime != 0) {
        kapp->updateUserTimestamp(usertime);
    }

    QString key = createCacheKey(info);
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
    kDebug(debugArea()) << "User =" << info.username << ", RealmValue= " << info.realmValue
                        << ", WindowId = " << windowId << endl;
    const QString key = createCacheKey(info);

    m_seqNr++;

    if (openWallet(windowId) && storeInWallet(m_wallet, key, info)) {
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
    kDebug(debugArea()) << "User =" << info.username << ", RealmValue= " << info.realmValue
                << ", WindowId = " << windowId << endl;
    addAuthInfo(info, windowId);
}

void
KPasswdServer::removeAuthInfo(const QString& host, const QString& protocol, const QString& user)
{
    kDebug(debugArea()) << "Wanted" << protocol << host << user;

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
    // guard so processRequest is only ever run once.
    static bool processing = false;
    if (processing || m_authPending.isEmpty()) {
        return;
    }
    processing = true;

    Request *request = m_authPending.takeFirst();
    KIO::AuthInfo &info = request->info;
    bool bypassCacheAndKWallet = info.getExtraField(AUTHINFO_EXTRAFIELD_BYPASS_CACHE_AND_KWALLET).toBool();
    bool skipAutoCaching = info.getExtraField(AUTHINFO_EXTRAFIELD_SKIP_CACHING_ON_QUERY).toBool();

    kDebug(debugArea()) << "User =" << info.username << ", Message =" << info.prompt;
    const AuthInfoContainer *result = findAuthInfoItem(request->key, request->info);

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
           info = copyAuthInfo(result);
        }
    }
    else
    {
        m_seqNr++;
        bool askPw = request->prompt;
        if (result && !request->errorMsg.isEmpty())
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

            dialogFlags |= (info.keepPassword ? ( KPasswordDialog::ShowUsernameLine
                  |  KPasswordDialog::ShowKeepPassword) : KPasswordDialog::ShowUsernameLine );

            // instantiate dialog
            KPasswordDialog dlg( 0l, dialogFlags) ;
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

            if (info.getExtraField(AUTHINFO_EXTRAFIELD_DOMAIN).isValid ())
                dlg.setDomain(info.getExtraField(AUTHINFO_EXTRAFIELD_DOMAIN).toString());

            if (info.getExtraField(AUTHINFO_EXTRAFIELD_ANONYMOUS).isValid () && password.isEmpty() && username.isEmpty())
                dlg.setAnonymousMode(info.getExtraField(AUTHINFO_EXTRAFIELD_ANONYMOUS).toBool());

#ifndef Q_WS_WIN
            KWindowSystem::setMainWindow(&dlg, request->windowId);
#else
            KWindowSystem::setMainWindow(&dlg, (HWND)(long)request->windowId);
#endif

            kDebug(debugArea()) << "Calling exec on" << &dlg;
            dlgResult = dlg.exec();
            kDebug(debugArea()) << "exec returned with" << dlgResult;

            if (dlgResult == QDialog::Accepted)
            {
               info.username = dlg.username();
               info.password = dlg.password();
               info.keepPassword = dlg.keepPassword();

               if (info.getExtraField(AUTHINFO_EXTRAFIELD_DOMAIN).isValid ())
                   info.setExtraField(AUTHINFO_EXTRAFIELD_DOMAIN, dlg.domain());
               if (info.getExtraField(AUTHINFO_EXTRAFIELD_ANONYMOUS).isValid ())
                   info.setExtraField(AUTHINFO_EXTRAFIELD_ANONYMOUS, dlg.anonymousMode());

               // When the user checks "keep password", that means:
               // * if the wallet is enabled, store it there for long-term, and in kpasswdserver
               // only for the duration of the window (#92928)
               // * otherwise store in kpasswdserver for the duration of the KDE session.
               if ( !bypassCacheAndKWallet && !skipAutoCaching && info.keepPassword ) {
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
            if (!bypassCacheAndKWallet && request->prompt)
            {
                addAuthInfoItem(request->key, info, 0, m_seqNr, true);
            }
            info.setModified( false );
        }
        else
        {
            if (!bypassCacheAndKWallet)
            {
                addAuthInfoItem(request->key, info, request->windowId, m_seqNr, false);
            }
            info.setModified( true );
        }
    }

    if (request->isAsync) {
        emit queryAuthInfoAsyncResult(request->requestId, m_seqNr, info);
    } else {
        QByteArray replyData;
        QDataStream stream2(&replyData, QIODevice::WriteOnly);
        stream2 << info;
        QDBusConnection::sessionBus().send(request->transaction.createReply(QVariantList() << replyData << m_seqNr));
    }

    delete request;

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
                rcinfo = copyAuthInfo(result);
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

    // reallow processing
    processing = false;

    if (m_authPending.count())
       QTimer::singleShot(0, this, SLOT(processRequest()));

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

KIO::AuthInfo
KPasswdServer::copyAuthInfo(const AuthInfoContainer *i)
{
    KIO::AuthInfo result = i->info;
    result.setModified(true);

    return result;
}

const KPasswdServer::AuthInfoContainer *
KPasswdServer::findAuthInfoItem(const QString &key, const KIO::AuthInfo &info)
{
   kDebug(debugArea()) << "key:" << key << ", user=" << info.username;

   AuthInfoContainerList *authList = m_authDict.value(key);
   if (!authList)
      return 0;

   QString path2 = info.url.directory(KUrl::AppendTrailingSlash|KUrl::ObeyTrailingSlash);
   Q_FOREACH(AuthInfoContainer *current, *authList)
   {
       if ((current->expire == AuthInfoContainer::expTime) &&
          (difftime(time(0), current->expireTime) > 0))
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
   AuthInfoContainerList *authList = m_authDict.value(key);
   if (!authList)
   {
      authList = new AuthInfoContainerList;
      m_authDict.insert(key, authList);
   }
   AuthInfoContainer *current = 0;
   Q_FOREACH(current, *authList)
   {
       if (current->info.realmValue == info.realmValue)
       {
          authList->removeAll(current);
          break;
       }
   }

   if (!current)
   {
      current = new AuthInfoContainer;
      current->expire = AuthInfoContainer::expTime;
      kDebug(debugArea()) << "Creating AuthInfoContainer";
   }
   else
   {
      kDebug(debugArea()) << "Updating AuthInfoContainer";
   }

   current->info = info;
   current->directory = info.url.directory(KUrl::AppendTrailingSlash|KUrl::ObeyTrailingSlash);
   current->seqNr = seqNr;
   current->isCanceled = canceled;

   updateAuthExpire(key, current, windowId, (info.keepPassword && !canceled));

   // Insert into list, keep the list sorted "longest path" first.
   authList->append(current);
   qSort(authList->begin(), authList->end(), AuthInfoContainer::Sorter());
}

void
KPasswdServer::updateAuthExpire(const QString &key, const AuthInfoContainer *auth, qlonglong windowId, bool keep)
{
   AuthInfoContainer *current = const_cast<AuthInfoContainer *>(auth);
   if (keep)
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
      current->expireTime = time(0)+10;
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

      Q_FOREACH(AuthInfoContainer *current, *authList)
      {
        if (current->expire == AuthInfoContainer::expWindowClose)
        {
           if (current->windowList.removeAll(windowId) && current->windowList.isEmpty())
           {
              authList->removeOne(current);
              delete current;
           }
        }
      }
   }
}

#include "kpasswdserver.moc"
