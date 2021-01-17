/*
    This file is part of the KDE Password Server
    SPDX-FileCopyrightText: 2002 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2005 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2012 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2020 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: GPL-2.0-only
*/

// KDE Password Server

#include "kpasswdserver.h"

#include "kpasswdserveradaptor.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <KPasswordDialog>
#include <KUserTimestamp>
#include <kwindowsystem.h>

#ifdef HAVE_KF5WALLET
#include <KWallet>
#endif

#include <QPushButton>
#include <QTimer>
#include <ctime>

static QLoggingCategory category("kf.kio.kpasswdserver", QtInfoMsg);

#define AUTHINFO_EXTRAFIELD_DOMAIN QStringLiteral("domain")
#define AUTHINFO_EXTRAFIELD_ANONYMOUS QStringLiteral("anonymous")
#define AUTHINFO_EXTRAFIELD_BYPASS_CACHE_AND_KWALLET QStringLiteral("bypass-cache-and-kwallet")
#define AUTHINFO_EXTRAFIELD_SKIP_CACHING_ON_QUERY QStringLiteral("skip-caching-on-query")
#define AUTHINFO_EXTRAFIELD_HIDE_USERNAME_INPUT QStringLiteral("hide-username-line")
#define AUTHINFO_EXTRAFIELD_USERNAME_CONTEXT_HELP QStringLiteral("username-context-help")

static qlonglong getRequestId()
{
    static qlonglong nextRequestId = 0;
    return nextRequestId++;
}

bool KPasswdServer::AuthInfoContainer::Sorter::operator()(AuthInfoContainer *n1, AuthInfoContainer *n2) const
{
    if (!n1 || !n2) {
        return 0;
    }

    const int l1 = n1->directory.length();
    const int l2 = n2->directory.length();
    return l1 < l2;
}

KPasswdServer::KPasswdServer(QObject *parent, const QList<QVariant> &)
    : KDEDModule(parent)
{
    KIO::AuthInfo::registerMetaTypes();

    m_seqNr = 0;
    m_wallet = nullptr;
    m_walletDisabled = false;

    KPasswdServerAdaptor *adaptor = new KPasswdServerAdaptor(this);
    // connect signals to the adaptor
    connect(this, &KPasswdServer::checkAuthInfoAsyncResult, adaptor, &KPasswdServerAdaptor::checkAuthInfoAsyncResult);
    connect(this, &KPasswdServer::queryAuthInfoAsyncResult, adaptor, &KPasswdServerAdaptor::queryAuthInfoAsyncResult);

    connect(this, &KDEDModule::windowUnregistered, this, &KPasswdServer::removeAuthForWindowId);

    connect(KWindowSystem::self(), &KWindowSystem::windowRemoved, this, &KPasswdServer::windowRemoved);
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

#ifdef HAVE_KF5WALLET
    delete m_wallet;
#endif
}

#ifdef HAVE_KF5WALLET

// Helper - returns the wallet key to use for read/store/checking for existence.
static QString makeWalletKey(const QString &key, const QString &realm)
{
    return realm.isEmpty() ? key : key + QLatin1Char('-') + realm;
}

// Helper for storeInWallet/readFromWallet
static QString makeMapKey(const char *key, int entryNumber)
{
    QString str = QLatin1String(key);
    if (entryNumber > 1) {
        str += QLatin1Char('-') + QString::number(entryNumber);
    }
    return str;
}

static bool storeInWallet(KWallet::Wallet *wallet, const QString &key, const KIO::AuthInfo &info)
{
    if (!wallet->hasFolder(KWallet::Wallet::PasswordFolder())) {
        if (!wallet->createFolder(KWallet::Wallet::PasswordFolder())) {
            return false;
        }
    }
    wallet->setFolder(KWallet::Wallet::PasswordFolder());
    // Before saving, check if there's already an entry with this login.
    // If so, replace it (with the new password). Otherwise, add a new entry.
    typedef QMap<QString, QString> Map;
    int entryNumber = 1;
    Map map;
    QString walletKey = makeWalletKey(key, info.realmValue);
    qCDebug(category) << "walletKey =" << walletKey << "  reading existing map";
    if (wallet->readMap(walletKey, map) == 0) {
        Map::ConstIterator end = map.constEnd();
        Map::ConstIterator it = map.constFind(QStringLiteral("login"));
        while (it != end) {
            if (it.value() == info.username) {
                break; // OK, overwrite this entry
            }
            it = map.constFind(QStringLiteral("login-") + QString::number(++entryNumber));
        }
        // If no entry was found, create a new entry - entryNumber is set already.
    }
    const QString loginKey = makeMapKey("login", entryNumber);
    const QString passwordKey = makeMapKey("password", entryNumber);
    qCDebug(category) << "writing to " << loginKey << "," << passwordKey;
    // note the overwrite=true by default
    map.insert(loginKey, info.username);
    map.insert(passwordKey, info.password);
    wallet->writeMap(walletKey, map);
    return true;
}

static bool readFromWallet(KWallet::Wallet *wallet, const QString &key, const QString &realm, QString &username,
                           QString &password, bool userReadOnly, QMap<QString, QString> &knownLogins)
{
    // qCDebug(category) << "key =" << key << " username =" << username << " password =" /*<< password*/
    //                   << " userReadOnly =" << userReadOnly << " realm =" << realm;
    if (wallet->hasFolder(KWallet::Wallet::PasswordFolder())) {
        wallet->setFolder(KWallet::Wallet::PasswordFolder());

        QMap<QString, QString> map;
        if (wallet->readMap(makeWalletKey(key, realm), map) == 0) {
            typedef QMap<QString, QString> Map;
            int entryNumber = 1;
            Map::ConstIterator end = map.constEnd();
            Map::ConstIterator it = map.constFind(QStringLiteral("login"));
            while (it != end) {
                // qCDebug(category) << "found " << it.key() << "=" << it.value();
                Map::ConstIterator pwdIter = map.constFind(makeMapKey("password", entryNumber));
                if (pwdIter != end) {
                    if (it.value() == username) {
                        password = pwdIter.value();
                    }
                    knownLogins.insert(it.value(), pwdIter.value());
                }

                it = map.constFind(QStringLiteral("login-") + QString::number(++entryNumber));
            }
            // qCDebug(category) << knownLogins.count() << " known logins";

            if (!userReadOnly && !knownLogins.isEmpty() && username.isEmpty()) {
                // Pick one, any one...
                username = knownLogins.begin().key();
                password = knownLogins.begin().value();
                // qCDebug(category) << "picked the first one:" << username;
            }

            return true;
        }
    }
    return false;
}

#endif

bool KPasswdServer::hasPendingQuery(const QString &key, const KIO::AuthInfo &info)
{
    const QString path2(info.url.path().left(info.url.path().indexOf(QLatin1Char('/')) + 1));
    for (const Request *request : qAsConst(m_authPending)) {
        if (request->key != key) {
            continue;
        }

        if (info.verifyPath) {
            const QString path1(request->info.url.path().left(info.url.path().indexOf(QLatin1Char('/')) + 1));
            if (!path2.startsWith(path1)) {
                continue;
            }
        }

        return true;
    }

    return false;
}

// deprecated method, not used anymore. TODO KF6: REMOVE
QByteArray KPasswdServer::checkAuthInfo(const QByteArray &data, qlonglong windowId, qlonglong usertime)
{
    KIO::AuthInfo info;
    QDataStream stream(data);
    stream >> info;
    if (usertime != 0) {
        KUserTimestamp::updateUserTimestamp(usertime);
    }

    // if the check depends on a pending query, delay it
    // until that query is finished.
    const QString key(createCacheKey(info));
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
        return data; // return value will be ignored
    }

    // qCDebug(category) << "key =" << key << "user =" << info.username << "windowId =" << windowId;
    const AuthInfoContainer *result = findAuthInfoItem(key, info);
    if (!result || result->isCanceled) {
#ifdef HAVE_KF5WALLET
        if (!result
            && !m_walletDisabled
            && (info.username.isEmpty() || info.password.isEmpty())
            && !KWallet::Wallet::keyDoesNotExist(KWallet::Wallet::NetworkWallet(), KWallet::Wallet::PasswordFolder(),
                                                 makeWalletKey(key, info.realmValue))) {
            QMap<QString, QString> knownLogins;
            if (openWallet(windowId)) {
                if (readFromWallet(m_wallet, key, info.realmValue, info.username,
                                   info.password, info.readOnly, knownLogins)) {
                    info.setModified(true);
                    // fall through
                }
            }
        } else {
            info.setModified(false);
        }
#else
        info.setModified(false);
#endif
    } else {
        qCDebug(category) << "Found cached authentication for" << key;
        updateAuthExpire(key, result, windowId, false);
        copyAuthInfo(result, info);
    }

    QByteArray data2;
    QDataStream stream2(&data2, QIODevice::WriteOnly);
    stream2 << info;
    return data2;
}

qlonglong KPasswdServer::checkAuthInfoAsync(KIO::AuthInfo info, qlonglong windowId, qlonglong usertime)
{
    if (usertime != 0) {
        KUserTimestamp::updateUserTimestamp(usertime);
    }

    // send the request id back to the client
    qlonglong requestId = getRequestId();
    qCDebug(category) << "User =" << info.username << ", WindowId =" << windowId;
    if (calledFromDBus()) {
        QDBusMessage reply(message().createReply(requestId));
        QDBusConnection::sessionBus().send(reply);
    }

    // if the check depends on a pending query, delay it
    // until that query is finished.
    const QString key(createCacheKey(info));
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
    if (!result || result->isCanceled) {
#ifdef HAVE_KF5WALLET
        if (!result
            && !m_walletDisabled
            && (info.username.isEmpty() || info.password.isEmpty())
            && !KWallet::Wallet::keyDoesNotExist(KWallet::Wallet::NetworkWallet(), KWallet::Wallet::PasswordFolder(),
                                                 makeWalletKey(key, info.realmValue))) {
            QMap<QString, QString> knownLogins;
            if (openWallet(windowId)) {
                if (readFromWallet(m_wallet, key, info.realmValue, info.username,
                                   info.password, info.readOnly, knownLogins)) {
                    info.setModified(true);
                    // fall through
                }
            }
        } else {
            info.setModified(false);
        }
#else
        info.setModified(false);
#endif
    } else {
        // qCDebug(category) << "Found cached authentication for" << key;
        updateAuthExpire(key, result, windowId, false);
        copyAuthInfo(result, info);
    }

    Q_EMIT checkAuthInfoAsyncResult(requestId, m_seqNr, info);
    return 0; // ignored
}

// deprecated method, not used anymore. TODO KF6: REMOVE
QByteArray KPasswdServer::queryAuthInfo(const QByteArray &data, const QString &errorMsg,
                                        qlonglong windowId, qlonglong seqNr, qlonglong usertime)
{
    KIO::AuthInfo info;
    QDataStream stream(data);
    stream >> info;

    qCDebug(category) << "User =" << info.username << ", WindowId =" << windowId
                      << "seqNr =" << seqNr << ", errorMsg =" << errorMsg;

    if (!info.password.isEmpty()) { // should we really allow the caller to pre-fill the password?
        qCDebug(category) << "password was set by caller";
    }
    if (usertime != 0) {
        KUserTimestamp::updateUserTimestamp(usertime);
    }

    const QString key(createCacheKey(info));
    Request *request = new Request;
    setDelayedReply(true);
    request->isAsync = false;
    request->transaction = message();
    request->key = key;
    request->info = info;
    request->windowId = windowId;
    request->seqNr = seqNr;
    if (errorMsg == QLatin1String("<NoAuthPrompt>")) {
        request->errorMsg.clear();
        request->prompt = false;
    } else {
        request->errorMsg = errorMsg;
        request->prompt = true;
    }
    m_authPending.append(request);

    if (m_authPending.count() == 1) {
        QTimer::singleShot(0, this, &KPasswdServer::processRequest);
    }

    return QByteArray(); // return value is going to be ignored
}

qlonglong KPasswdServer::queryAuthInfoAsync(const KIO::AuthInfo &info, const QString &errorMsg,
                                            qlonglong windowId,qlonglong seqNr, qlonglong usertime)
{
    qCDebug(category) << "User =" << info.username << ", WindowId =" << windowId
                      << "seqNr =" << seqNr << ", errorMsg =" << errorMsg;

    if (!info.password.isEmpty()) {
        qCDebug(category) << "password was set by caller";
    }
    if (usertime != 0) {
        KUserTimestamp::updateUserTimestamp(usertime);
    }

    const QString key(createCacheKey(info));
    Request *request = new Request;
    request->isAsync = true;
    request->requestId = getRequestId();
    request->key = key;
    request->info = info;
    request->windowId = windowId;
    request->seqNr = seqNr;
    if (errorMsg == QLatin1String("<NoAuthPrompt>")) {
        request->errorMsg.clear();
        request->prompt = false;
    } else {
        request->errorMsg = errorMsg;
        request->prompt = true;
    }
    m_authPending.append(request);

    if (m_authPending.count() == 1) {
        QTimer::singleShot(0, this, &KPasswdServer::processRequest);
    }

    return request->requestId;
}

void KPasswdServer::addAuthInfo(const KIO::AuthInfo &info, qlonglong windowId)
{
    qCDebug(category) << "User =" << info.username << ", Realm =" << info.realmValue
                      << ", WindowId =" << windowId;
    if (!info.keepPassword) {
        qWarning() << "This kioslave is caching a password in KWallet even though the user didn't ask for it!";
    }
    const QString key(createCacheKey(info));

    m_seqNr++;

#ifdef HAVE_KF5WALLET
    if (!m_walletDisabled && openWallet(windowId) && storeInWallet(m_wallet, key, info)) {
        // Since storing the password in the wallet succeeded, make sure the
        // password information is stored in memory only for the duration the
        // windows associated with it are still around.
        KIO::AuthInfo authToken(info);
        authToken.keepPassword = false;
        addAuthInfoItem(key, authToken, windowId, m_seqNr, false);
        return;
    }
#endif

    addAuthInfoItem(key, info, windowId, m_seqNr, false);
}

// deprecated method, not used anymore. TODO KF6: REMOVE
void KPasswdServer::addAuthInfo(const QByteArray &data, qlonglong windowId)
{
    KIO::AuthInfo info;
    QDataStream stream(data);
    stream >> info;
    addAuthInfo(info, windowId);
}

void KPasswdServer::removeAuthInfo(const QString &host, const QString &protocol, const QString &user)
{
    qCDebug(category) << protocol << host << user;

    QHashIterator<QString, AuthInfoContainerList *> dictIterator(m_authDict);
    while (dictIterator.hasNext()) {
        dictIterator.next();

        const AuthInfoContainerList *authList = dictIterator.value();
        if (!authList) {
            continue;
        }

        for (const AuthInfoContainer *current : *authList) {
            qCDebug(category) << "Evaluating: " << current->info.url.scheme()
                              << current->info.url.host() << current->info.username;
            if (current->info.url.scheme() == protocol
                && current->info.url.host() == host
                && (current->info.username == user || user.isEmpty())) {
                qCDebug(category) << "Removing this entry";
                removeAuthInfoItem(dictIterator.key(), current->info);
            }
        }
    }
}

#ifdef HAVE_KF5WALLET
bool KPasswdServer::openWallet(qlonglong windowId)
{
    if (m_wallet && !m_wallet->isOpen()) { // forced closed
        delete m_wallet;
        m_wallet = nullptr;
    }
    if (!m_wallet) {
        m_wallet = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), (WId)(windowId));
    }
    return m_wallet != nullptr;
}
#endif

void KPasswdServer::processRequest()
{
    if (m_authPending.isEmpty()) {
        return;
    }

    QScopedPointer<Request> request(m_authPending.takeFirst());

    // Prevent multiple prompts originating from the same window or the same
    // key (server address).
    const QString windowIdStr = QString::number(request->windowId);
    if (m_authPrompted.contains(windowIdStr) || m_authPrompted.contains(request->key)) {
        m_authPending.prepend(request.take()); // put it back.
        return;
    }

    m_authPrompted.append(windowIdStr);
    m_authPrompted.append(request->key);

    KIO::AuthInfo &info = request->info;

    // NOTE: If info.username is empty and info.url.userName() is not, set
    // info.username to info.url.userName() to ensure proper caching. See
    // note passwordDialogDone.
    if (info.username.isEmpty() && !info.url.userName().isEmpty()) {
        info.username = info.url.userName();
    }
    const bool bypassCacheAndKWallet = info.getExtraField(AUTHINFO_EXTRAFIELD_BYPASS_CACHE_AND_KWALLET).toBool();

    const AuthInfoContainer *result = findAuthInfoItem(request->key, request->info);
    qCDebug(category) << "key=" << request->key << ", user=" << info.username
                      << "seqNr: request=" << request->seqNr
                      <<", result=" << (result ? result->seqNr : -1);

    if (!bypassCacheAndKWallet && result && (request->seqNr < result->seqNr)) {
        qCDebug(category) << "auto retry!";
        if (result->isCanceled) {
            info.setModified(false);
        } else {
            updateAuthExpire(request->key, result, request->windowId, false);
            copyAuthInfo(result, info);
        }
    } else {
        m_seqNr++;
        if (result && !request->errorMsg.isEmpty()) {
            const QString prompt = request->errorMsg.trimmed() + QLatin1Char('\n') + i18n("Do you want to retry?");

            QDialog *dlg = new QDialog;
            connect(dlg, &QDialog::finished, this, &KPasswdServer::retryDialogDone);
            connect(this, &QObject::destroyed, dlg, &QObject::deleteLater);
            dlg->setWindowTitle(i18n("Retry Authentication"));
            dlg->setWindowIcon(QIcon::fromTheme(QStringLiteral("dialog-password")));
            dlg->setObjectName(QStringLiteral("warningOKCancel"));
            QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::Cancel);
            buttonBox->button(QDialogButtonBox::Yes)->setText(i18nc("@action:button filter-continue", "Retry"));

            KMessageBox::createKMessageBox(dlg, buttonBox, QMessageBox::Warning, prompt, QStringList(),
                                           QString(), nullptr, (KMessageBox::Notify | KMessageBox::NoExec));

            dlg->setAttribute(Qt::WA_NativeWindow, true);
            KWindowSystem::setMainWindow(dlg->windowHandle(), request->windowId);

            qCDebug(category) << "Calling open on retry dialog" << dlg;
            m_authRetryInProgress.insert(dlg, request.take());
            dlg->open();
            return;
        }

        if (request->prompt) {
            showPasswordDialog(request.take());
            return;
        } else {
            if (!bypassCacheAndKWallet && request->prompt) {
                addAuthInfoItem(request->key, info, 0, m_seqNr, true);
            }
            info.setModified(false);
        }
    }

    sendResponse(request.data());
}

QString KPasswdServer::createCacheKey(const KIO::AuthInfo &info)
{
    if (!info.url.isValid()) {
        // Note that a null key will break findAuthInfoItem later on...
        qCWarning(category) << "createCacheKey: invalid URL " << info.url;
        return QString();
    }

    // Generate the basic key sequence.
    QString key = info.url.scheme();
    key += QLatin1Char('-');
    if (!info.url.userName().isEmpty()) {
        key += info.url.userName() + QLatin1Char('@');
    }
    key += info.url.host();
    int port = info.url.port();
    if (port) {
        key += QLatin1Char(':') + QString::number(port);
    }

    return key;
}

void KPasswdServer::copyAuthInfo(const AuthInfoContainer *i, KIO::AuthInfo &info)
{
    info = i->info;
    info.setModified(true);
}

const KPasswdServer::AuthInfoContainer *KPasswdServer::findAuthInfoItem(const QString &key, const KIO::AuthInfo &info)
{
    // qCDebug(category) << "key=" << key << ", user=" << info.username;

    AuthInfoContainerList *authList = m_authDict.value(key);
    if (authList) {
        QString path2 = info.url.path().left(info.url.path().indexOf(QLatin1Char('/')) + 1);
        auto it = authList->begin();
        while (it != authList->end()) {
            AuthInfoContainer *current = (*it);
            if (current->expire == AuthInfoContainer::expTime
                && static_cast<qulonglong>(time(nullptr)) > current->expireTime) {
                delete current;
                it = authList->erase(it);
                continue;
            }

            if (info.verifyPath) {
                QString path1 = current->directory;
                if (path2.startsWith(path1)
                    && (info.username.isEmpty() || info.username == current->info.username))
                    return current;
            } else {
                if (current->info.realmValue == info.realmValue
                    && (info.username.isEmpty() || info.username == current->info.username))
                    return current; // TODO: Update directory info,
            }

            ++it;
        }
    }
    return nullptr;
}

void KPasswdServer::removeAuthInfoItem(const QString &key, const KIO::AuthInfo &info)
{
    AuthInfoContainerList *authList = m_authDict.value(key);
    if (!authList)
        return;

    auto it = authList->begin();
    while (it != authList->end()) {
        if ((*it)->info.realmValue == info.realmValue) {
            delete (*it);
            it = authList->erase(it);
        } else {
            ++it;
        }
    }
    if (authList->isEmpty()) {
        delete m_authDict.take(key);
    }
}

void KPasswdServer::addAuthInfoItem(const QString &key, const KIO::AuthInfo &info,
                                    qlonglong windowId, qlonglong seqNr, bool canceled)
{
    qCDebug(category) << "key=" << key << "window-id=" << windowId << "username=" << info.username
                      << "realm=" << info.realmValue << "seqNr=" << seqNr
                      << "keepPassword?" << info.keepPassword << "canceled?" << canceled;
    AuthInfoContainerList *authList = m_authDict.value(key);
    if (!authList) {
        authList = new AuthInfoContainerList;
        m_authDict.insert(key, authList);
    }
    AuthInfoContainer *authItem = nullptr;
    auto it = authList->begin();
    while (it != authList->end()) {
        if ((*it)->info.realmValue == info.realmValue) {
            authItem = (*it);
            it = authList->erase(it);
            break;
        } else {
            ++it;
        }
    }

    if (!authItem) {
        qCDebug(category) << "Creating AuthInfoContainer";
        authItem = new AuthInfoContainer;
        authItem->expire = AuthInfoContainer::expTime;
    }

    authItem->info = info;
    authItem->directory = info.url.path().left(info.url.path().indexOf(QLatin1Char('/')) + 1);
    authItem->seqNr = seqNr;
    authItem->isCanceled = canceled;

    updateAuthExpire(key, authItem, windowId, (info.keepPassword && !canceled));

    // Insert into list, keep the list sorted "longest path" first.
    authList->append(authItem);
    std::sort(authList->begin(), authList->end(), AuthInfoContainer::Sorter());
}

void KPasswdServer::updateAuthExpire(const QString &key, const AuthInfoContainer *auth,
                                     qlonglong windowId, bool keep)
{
    AuthInfoContainer *current = const_cast<AuthInfoContainer *>(auth);
    Q_ASSERT(current);

    qCDebug(category) << "key=" << key << "expire=" << current->expire
                      << "window-id=" << windowId << "keep=" << keep;

    if (keep && !windowId) {
        current->expire = AuthInfoContainer::expNever;
    } else if (windowId && (current->expire != AuthInfoContainer::expNever)) {
        current->expire = AuthInfoContainer::expWindowClose;
        if (!current->windowList.contains(windowId)) {
            current->windowList.append(windowId);
        }
    } else if (current->expire == AuthInfoContainer::expTime) {
        current->expireTime = time(nullptr) + 10;
    }

    // Update mWindowIdList
    if (windowId) {
        QStringList &keysChanged = mWindowIdList[windowId]; // find or insert
        if (!keysChanged.contains(key))
            keysChanged.append(key);
    }
}

void KPasswdServer::removeAuthForWindowId(qlonglong windowId)
{
    const QStringList keysChanged = mWindowIdList.value(windowId);
    for (const QString &key : keysChanged) {
        AuthInfoContainerList *authList = m_authDict.value(key);
        if (!authList) {
            continue;
        }

        QMutableListIterator<AuthInfoContainer *> it(*authList);
        while (it.hasNext()) {
            AuthInfoContainer *current = it.next();
            if (current->expire == AuthInfoContainer::expWindowClose) {
                if (current->windowList.removeAll(windowId) && current->windowList.isEmpty()) {
                    delete current;
                    it.remove();
                }
            }
        }
    }
}

void KPasswdServer::showPasswordDialog(KPasswdServer::Request *request)
{
    KIO::AuthInfo &info = request->info;
    QString username = info.username;
    QString password = info.password;
    bool hasWalletData = false;
    QMap<QString, QString> knownLogins;

#ifdef HAVE_KF5WALLET
    const bool bypassCacheAndKWallet = info.getExtraField(AUTHINFO_EXTRAFIELD_BYPASS_CACHE_AND_KWALLET).toBool();
    if (!bypassCacheAndKWallet
        && (username.isEmpty() || password.isEmpty())
        && !m_walletDisabled
        && !KWallet::Wallet::keyDoesNotExist(KWallet::Wallet::NetworkWallet(), KWallet::Wallet::PasswordFolder(),
                                             makeWalletKey(request->key, info.realmValue))) {
        // no login+pass provided, check if kwallet has one
        if (openWallet(request->windowId)) {
            hasWalletData = readFromWallet(m_wallet, request->key, info.realmValue,
                                           username, password, info.readOnly, knownLogins);
        }
    }
#endif

    // assemble dialog-flags
    KPasswordDialog::KPasswordDialogFlags dialogFlags;

    if (info.getExtraField(AUTHINFO_EXTRAFIELD_DOMAIN).isValid()) {
        dialogFlags |= KPasswordDialog::ShowDomainLine;
        if (info.getExtraFieldFlags(AUTHINFO_EXTRAFIELD_DOMAIN) & KIO::AuthInfo::ExtraFieldReadOnly) {
            dialogFlags |= KPasswordDialog::DomainReadOnly;
        }
    }

    if (info.getExtraField(AUTHINFO_EXTRAFIELD_ANONYMOUS).isValid()) {
        dialogFlags |= KPasswordDialog::ShowAnonymousLoginCheckBox;
    }

    if (!info.getExtraField(AUTHINFO_EXTRAFIELD_HIDE_USERNAME_INPUT).toBool()) {
        dialogFlags |= KPasswordDialog::ShowUsernameLine;
    }

#ifdef HAVE_KF5WALLET
    // If wallet is not enabled and the caller explicitly requested for it,
    // do not show the keep password checkbox.
    if (info.keepPassword && KWallet::Wallet::isEnabled()) {
        dialogFlags |= KPasswordDialog::ShowKeepPassword;
    }
#endif

    // instantiate dialog
    qCDebug(category) << "Widget for" << request->windowId << QWidget::find(request->windowId);

    KPasswordDialog *dlg = new KPasswordDialog(nullptr, dialogFlags);
    connect(dlg, &QDialog::finished, this, &KPasswdServer::passwordDialogDone);
    connect(this, &QObject::destroyed, dlg, &QObject::deleteLater);

    dlg->setPrompt(info.prompt);
    dlg->setUsername(username);
    if (info.caption.isEmpty()) {
        dlg->setWindowTitle(i18n("Authentication Dialog"));
    } else {
        dlg->setWindowTitle(info.caption);
    }

    if (!info.comment.isEmpty()) {
        dlg->addCommentLine(info.commentLabel, info.comment);
    }

    if (!password.isEmpty()) {
        dlg->setPassword(password);
    }

    if (info.readOnly) {
        dlg->setUsernameReadOnly(true);
    } else {
        dlg->setKnownLogins(knownLogins);
    }

    if (hasWalletData) {
        dlg->setKeepPassword(true);
    }

    if (info.getExtraField(AUTHINFO_EXTRAFIELD_DOMAIN).isValid()) {
        dlg->setDomain(info.getExtraField(AUTHINFO_EXTRAFIELD_DOMAIN).toString());
    }

    if (info.getExtraField(AUTHINFO_EXTRAFIELD_ANONYMOUS).isValid()
        && password.isEmpty()
        && username.isEmpty()) {
        dlg->setAnonymousMode(info.getExtraField(AUTHINFO_EXTRAFIELD_ANONYMOUS).toBool());
    }

    const QVariant userContextHelp = info.getExtraField(AUTHINFO_EXTRAFIELD_USERNAME_CONTEXT_HELP);
    if (userContextHelp.isValid()) {
        dlg->setUsernameContextHelp(userContextHelp.toString());
    }

#ifndef Q_OS_MACOS
    dlg->setAttribute(Qt::WA_NativeWindow, true);
    KWindowSystem::setMainWindow(dlg->windowHandle(), request->windowId);
#else
    KWindowSystem::forceActiveWindow(dlg->winId(), 0);
#endif

    qCDebug(category) << "Showing password dialog" << dlg << ", window-id=" << request->windowId;
    m_authInProgress.insert(dlg, request);
    dlg->open();
}

void KPasswdServer::sendResponse(KPasswdServer::Request *request)
{
    Q_ASSERT(request);
    if (!request) {
        return;
    }

    qCDebug(category) << "key=" << request->key;
    if (request->isAsync) {
        Q_EMIT queryAuthInfoAsyncResult(request->requestId, m_seqNr, request->info);
    } else {
        QByteArray replyData;
        QDataStream stream2(&replyData, QIODevice::WriteOnly);
        stream2 << request->info;
        QDBusConnection::sessionBus().send(
            request->transaction.createReply(QVariantList{QVariant(replyData), QVariant(m_seqNr)}));
    }

    // Check all requests in the wait queue.
    Request *waitRequest;
    QMutableListIterator<Request *> it(m_authWait);
    while (it.hasNext()) {
        waitRequest = it.next();

        if (!hasPendingQuery(waitRequest->key, waitRequest->info)) {
            const AuthInfoContainer *result = findAuthInfoItem(waitRequest->key, waitRequest->info);
            QByteArray replyData;

            QDataStream stream2(&replyData, QIODevice::WriteOnly);

            KIO::AuthInfo rcinfo;
            if (!result || result->isCanceled) {
                waitRequest->info.setModified(false);
                stream2 << waitRequest->info;
            } else {
                updateAuthExpire(waitRequest->key, result, waitRequest->windowId, false);
                copyAuthInfo(result, rcinfo);
                stream2 << rcinfo;
            }

            if (waitRequest->isAsync) {
                Q_EMIT checkAuthInfoAsyncResult(waitRequest->requestId, m_seqNr, rcinfo);
            } else {
                QDBusConnection::sessionBus().send(
                    waitRequest->transaction.createReply(QVariantList{QVariant(replyData), QVariant(m_seqNr)}));
            }

            delete waitRequest;
            it.remove();
        }
    }

    // Re-enable password request processing for the current window id again.
    m_authPrompted.removeAll(QString::number(request->windowId));
    m_authPrompted.removeAll(request->key);

    if (!m_authPending.isEmpty()) {
        QTimer::singleShot(0, this, &KPasswdServer::processRequest);
    }
}

void KPasswdServer::passwordDialogDone(int result)
{
    KPasswordDialog *dlg = qobject_cast<KPasswordDialog *>(sender());
    Q_ASSERT(dlg);

    QScopedPointer<Request> request(m_authInProgress.take(dlg));
    Q_ASSERT(request); // request should never be nullptr.

    if (request) {
        KIO::AuthInfo &info = request->info;
        const bool bypassCacheAndKWallet = info.getExtraField(AUTHINFO_EXTRAFIELD_BYPASS_CACHE_AND_KWALLET).toBool();

        qCDebug(category) << "dialog result=" << result << ", bypassCacheAndKWallet?" << bypassCacheAndKWallet;
        if (dlg && result == QDialog::Accepted) {
            Q_ASSERT(dlg);
            info.username = dlg->username();
            info.password = dlg->password();
            info.keepPassword = dlg->keepPassword();

            if (info.getExtraField(AUTHINFO_EXTRAFIELD_DOMAIN).isValid()) {
                info.setExtraField(AUTHINFO_EXTRAFIELD_DOMAIN, dlg->domain());
            }
            if (info.getExtraField(AUTHINFO_EXTRAFIELD_ANONYMOUS).isValid()) {
                info.setExtraField(AUTHINFO_EXTRAFIELD_ANONYMOUS, dlg->anonymousMode());
            }

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
                if (!info.url.userName().isEmpty() && info.username != info.url.userName()) {
                    const QString oldKey(request->key);
                    removeAuthInfoItem(oldKey, info);
                    info.url.setUserName(info.username);
                    request->key = createCacheKey(info);
                    updateCachedRequestKey(m_authPending, oldKey, request->key);
                    updateCachedRequestKey(m_authWait, oldKey, request->key);
                }

#ifdef HAVE_KF5WALLET
                const bool skipAutoCaching = info.getExtraField(AUTHINFO_EXTRAFIELD_SKIP_CACHING_ON_QUERY).toBool();
                if (!skipAutoCaching && info.keepPassword && openWallet(request->windowId)) {
                    if (storeInWallet(m_wallet, request->key, info)) {
                        // password is in wallet, don't keep it in memory after window is closed
                        info.keepPassword = false;
                    }
                }
#endif
                addAuthInfoItem(request->key, info, request->windowId, m_seqNr, false);
            }
            info.setModified(true);
        } else {
            if (!bypassCacheAndKWallet && request->prompt) {
                addAuthInfoItem(request->key, info, 0, m_seqNr, true);
            }
            info.setModified(false);
        }

        sendResponse(request.data());
    }

    dlg->deleteLater();
}

void KPasswdServer::retryDialogDone(int result)
{
    QDialog *dlg = qobject_cast<QDialog *>(sender());
    Q_ASSERT(dlg);

    QScopedPointer<Request> request(m_authRetryInProgress.take(dlg));
    Q_ASSERT(request);

    if (request) {
        if (result == QDialogButtonBox::Yes) {
            showPasswordDialog(request.take());
        } else {
            // NOTE: If the user simply cancels the retry dialog, we remove the
            // credential stored under this key because the original attempt to
            // use it has failed. Otherwise, the failed credential would be cached
            // and used subsequently.
            //
            // TODO: decide whether it should be removed from the wallet too.
            KIO::AuthInfo &info = request->info;
            removeAuthInfoItem(request->key, request->info);
            info.setModified(false);
            sendResponse(request.data());
        }
    }
}

void KPasswdServer::windowRemoved(WId id)
{
    bool foundMatch = false;
    if (!m_authInProgress.isEmpty()) {
        const qlonglong windowId = (qlonglong)(id);
        QMutableHashIterator<QObject *, Request *> it(m_authInProgress);
        while (it.hasNext()) {
            it.next();
            if (it.value()->windowId == windowId) {
                Request *request = it.value();
                QObject *obj = it.key();
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
        QMutableHashIterator<QObject *, Request *> it(m_authRetryInProgress);
        while (it.hasNext()) {
            it.next();
            if (it.value()->windowId == windowId) {
                Request *request = it.value();
                QObject *obj = it.key();
                it.remove();
                delete obj;
                delete request;
            }
        }
    }
}

void KPasswdServer::updateCachedRequestKey(QList<KPasswdServer::Request *> &list,
                                           const QString &oldKey, const QString &newKey)
{
    QListIterator<Request *> it(list);
    while (it.hasNext()) {
        Request *r = it.next();
        if (r->key == oldKey) {
            r->key = newKey;
        }
    }
}
