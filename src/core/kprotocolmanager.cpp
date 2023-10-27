/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2000 Waldo Bastain <bastain@kde.org>
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2008 Jarosław Staniek <staniek@kde.org>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kprotocolmanager.h"
#include "kprotocolinfo_p.h"
#include "kprotocolmanager_p.h"

#include "hostinfo.h"

#include <config-kiocore.h>

#include <qplatformdefs.h>
#include <string.h>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#undef interface // windows.h defines this, breaks QtDBus since it has parameters named interface
#else
#include <sys/utsname.h>
#endif

#include <QCache>
#include <QCoreApplication>
#ifdef WITH_QTDBUS
#include <QDBusInterface>
#include <QDBusReply>
#endif
#include <QHostAddress>
#include <QHostInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSslSocket>
#include <QStandardPaths>
#include <QUrl>

#if !defined(QT_NO_NETWORKPROXY) && (defined(Q_OS_WIN32) || defined(Q_OS_MAC))
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#endif

#include <KConfigGroup>
#include <KSharedConfig>
#include <kio_version.h>

#include <kprotocolinfofactory_p.h>

#include "ioworker_defaults.h"
#include "workerconfig.h"

/*
    Domain suffix match. E.g. return true if host is "cuzco.inka.de" and
    nplist is "inka.de,hadiko.de" or if host is "localhost" and nplist is
    "localhost".
*/
static bool revmatch(const char *host, const char *nplist)
{
    if (host == nullptr) {
        return false;
    }

    const char *hptr = host + strlen(host) - 1;
    const char *nptr = nplist + strlen(nplist) - 1;
    const char *shptr = hptr;

    while (nptr >= nplist) {
        if (*hptr != *nptr) {
            hptr = shptr;

            // Try to find another domain or host in the list
            while (--nptr >= nplist && *nptr != ',' && *nptr != ' ') {
                ;
            }

            // Strip out multiple spaces and commas
            while (--nptr >= nplist && (*nptr == ',' || *nptr == ' ')) {
                ;
            }
        } else {
            if (nptr == nplist || nptr[-1] == ',' || nptr[-1] == ' ') {
                return true;
            }
            if (nptr[-1] == '/' && hptr == host) { // "bugs.kde.org" vs "http://bugs.kde.org", the config UI says URLs are ok
                return true;
            }
            if (hptr == host) { // e.g. revmatch("bugs.kde.org","mybugs.kde.org")
                return false;
            }

            hptr--;
            nptr--;
        }
    }

    return false;
}

Q_GLOBAL_STATIC(KProtocolManagerPrivate, kProtocolManagerPrivate)

static void syncOnExit()
{
    if (kProtocolManagerPrivate.exists()) {
        kProtocolManagerPrivate()->sync();
    }
}

KProtocolManagerPrivate::KProtocolManagerPrivate()
{
    // post routine since KConfig::sync() breaks if called too late
    qAddPostRoutine(syncOnExit);
    cachedProxyData.setMaxCost(200); // double the max cost.
}

KProtocolManagerPrivate::~KProtocolManagerPrivate()
{
}

/*
 * Returns true if url is in the no proxy list.
 */
bool KProtocolManagerPrivate::shouldIgnoreProxyFor(const QUrl &url)
{
    bool isMatch = false;
    const ProxyType type = proxyType();
    const bool useRevProxy = ((type == ManualProxy) && useReverseProxy());
    const bool useNoProxyList = (type == ManualProxy || type == EnvVarProxy);

    // No proxy only applies to ManualProxy and EnvVarProxy types...
    if (useNoProxyList && noProxyFor.isEmpty()) {
        QStringList noProxyForList(readNoProxyFor().split(QLatin1Char(',')));
        QMutableStringListIterator it(noProxyForList);
        while (it.hasNext()) {
            SubnetPair subnet = QHostAddress::parseSubnet(it.next());
            if (!subnet.first.isNull()) {
                noProxySubnets << subnet;
                it.remove();
            }
        }
        noProxyFor = noProxyForList.join(QLatin1Char(','));
    }

    if (!noProxyFor.isEmpty()) {
        QString qhost = url.host().toLower();
        QByteArray host = qhost.toLatin1();
        const QString qno_proxy = noProxyFor.trimmed().toLower();
        const QByteArray no_proxy = qno_proxy.toLatin1();
        isMatch = revmatch(host.constData(), no_proxy.constData());

        // If no match is found and the request url has a port
        // number, try the combination of "host:port". This allows
        // users to enter host:port in the No-proxy-For list.
        if (!isMatch && url.port() > 0) {
            qhost += QLatin1Char(':') + QString::number(url.port());
            host = qhost.toLatin1();
            isMatch = revmatch(host.constData(), no_proxy.constData());
        }

        // If the hostname does not contain a dot, check if
        // <local> is part of noProxy.
        if (!isMatch && !host.isEmpty() && (strchr(host.constData(), '.') == nullptr)) {
            isMatch = revmatch("<local>", no_proxy.constData());
        }
    }

    const QString host(url.host());

    if (!noProxySubnets.isEmpty() && !host.isEmpty()) {
        QHostAddress address(host);
        // If request url is not IP address, do a DNS lookup of the hostname.
        // TODO: Perhaps we should make configurable ?
        if (address.isNull()) {
            // qDebug() << "Performing DNS lookup for" << host;
            QHostInfo info = KIO::HostInfo::lookupHost(host, 2000);
            const QList<QHostAddress> addresses = info.addresses();
            if (!addresses.isEmpty()) {
                address = addresses.first();
            }
        }

        if (!address.isNull()) {
            for (const SubnetPair &subnet : std::as_const(noProxySubnets)) {
                if (address.isInSubnet(subnet)) {
                    isMatch = true;
                    break;
                }
            }
        }
    }

    return (useRevProxy != isMatch);
}

void KProtocolManagerPrivate::sync()
{
    QMutexLocker lock(&mutex);
    if (http_config) {
        http_config->sync();
    }
    if (configPtr) {
        configPtr->sync();
    }
}

void KProtocolManager::reparseConfiguration()
{
    KProtocolManagerPrivate *d = kProtocolManagerPrivate();
    QMutexLocker lock(&d->mutex);
    if (d->http_config) {
        d->http_config->reparseConfiguration();
    }
    if (d->configPtr) {
        d->configPtr->reparseConfiguration();
    }
    d->cachedProxyData.clear();
    d->noProxyFor.clear();
    d->modifiers.clear();
    d->useragent.clear();
    lock.unlock();

    // Force the slave config to re-read its config...
    KIO::WorkerConfig::self()->reset();
}

static KSharedConfig::Ptr config()
{
    KProtocolManagerPrivate *d = kProtocolManagerPrivate();
    Q_ASSERT(!d->mutex.tryLock()); // the caller must have locked the mutex
    if (!d->configPtr) {
        d->configPtr = KSharedConfig::openConfig(QStringLiteral("kioslaverc"), KConfig::NoGlobals);
    }
    return d->configPtr;
}

KProtocolManagerPrivate::ProxyType KProtocolManagerPrivate::proxyType()
{
    KConfigGroup cg(config(), QStringLiteral("Proxy Settings"));
    return static_cast<ProxyType>(cg.readEntry("ProxyType", 0));
}

bool KProtocolManagerPrivate::useReverseProxy()
{
    KConfigGroup cg(config(), QStringLiteral("Proxy Settings"));
    return cg.readEntry("ReversedException", false);
}

QString KProtocolManagerPrivate::readNoProxyFor()
{
    QString noProxy = config()->group(QStringLiteral("Proxy Settings")).readEntry("NoProxyFor");
    if (proxyType() == EnvVarProxy) {
        noProxy = QString::fromLocal8Bit(qgetenv(noProxy.toLocal8Bit().constData()));
    }
    return noProxy;
}

QMap<QString, QString> KProtocolManager::entryMap(const QString &group)
{
    KProtocolManagerPrivate *d = kProtocolManagerPrivate();
    QMutexLocker lock(&d->mutex);
    return config()->entryMap(group);
}

/*=============================== TIMEOUT SETTINGS ==========================*/

int KProtocolManager::readTimeout()
{
    KProtocolManagerPrivate *d = kProtocolManagerPrivate();
    QMutexLocker lock(&d->mutex);
    KConfigGroup cg(config(), QString());
    int val = cg.readEntry("ReadTimeout", DEFAULT_READ_TIMEOUT);
    return qMax(MIN_TIMEOUT_VALUE, val);
}

int KProtocolManager::connectTimeout()
{
    KProtocolManagerPrivate *d = kProtocolManagerPrivate();
    QMutexLocker lock(&d->mutex);
    KConfigGroup cg(config(), QString());
    int val = cg.readEntry("ConnectTimeout", DEFAULT_CONNECT_TIMEOUT);
    return qMax(MIN_TIMEOUT_VALUE, val);
}

int KProtocolManager::proxyConnectTimeout()
{
    KProtocolManagerPrivate *d = kProtocolManagerPrivate();
    QMutexLocker lock(&d->mutex);
    KConfigGroup cg(config(), QString());
    int val = cg.readEntry("ProxyConnectTimeout", DEFAULT_PROXY_CONNECT_TIMEOUT);
    return qMax(MIN_TIMEOUT_VALUE, val);
}

int KProtocolManager::responseTimeout()
{
    KProtocolManagerPrivate *d = kProtocolManagerPrivate();
    QMutexLocker lock(&d->mutex);
    KConfigGroup cg(config(), QString());
    int val = cg.readEntry("ResponseTimeout", DEFAULT_RESPONSE_TIMEOUT);
    return qMax(MIN_TIMEOUT_VALUE, val);
}

static QString adjustProtocol(const QString &scheme)
{
    if (scheme.compare(QLatin1String("webdav"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("http");
    }

    if (scheme.compare(QLatin1String("webdavs"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("https");
    }

    return scheme.toLower();
}

QString KProtocolManagerPrivate::proxyFor(const QString &protocol)
{
    const QString key = adjustProtocol(protocol) + QLatin1String("Proxy");
    QString proxyStr(config()->group(QStringLiteral("Proxy Settings")).readEntry(key));
    const int index = proxyStr.lastIndexOf(QLatin1Char(' '));

    if (index > -1) {
        const QStringView portStr = QStringView(proxyStr).right(proxyStr.length() - index - 1);
        const bool isDigits = std::all_of(portStr.cbegin(), portStr.cend(), [](const QChar c) {
            return c.isDigit();
        });

        if (isDigits) {
            proxyStr = QStringView(proxyStr).left(index) + QLatin1Char(':') + portStr;
        } else {
            proxyStr.clear();
        }
    }

    return proxyStr;
}

QStringList KProtocolManagerPrivate::getSystemProxyFor(const QUrl &url)
{
    QStringList proxies;

#if !defined(QT_NO_NETWORKPROXY) && (defined(Q_OS_WIN32) || defined(Q_OS_MAC))
    QNetworkProxyQuery query(url);
    const QList<QNetworkProxy> proxyList = QNetworkProxyFactory::systemProxyForQuery(query);
    proxies.reserve(proxyList.size());
    for (const QNetworkProxy &proxy : proxyList) {
        QUrl url;
        const QNetworkProxy::ProxyType type = proxy.type();
        if (type == QNetworkProxy::NoProxy || type == QNetworkProxy::DefaultProxy) {
            proxies << QLatin1String("DIRECT");
            continue;
        }

        if (type == QNetworkProxy::HttpProxy || type == QNetworkProxy::HttpCachingProxy) {
            url.setScheme(QLatin1String("http"));
        } else if (type == QNetworkProxy::Socks5Proxy) {
            url.setScheme(QLatin1String("socks"));
        } else if (type == QNetworkProxy::FtpCachingProxy) {
            url.setScheme(QLatin1String("ftp"));
        }

        url.setHost(proxy.hostName());
        url.setPort(proxy.port());
        url.setUserName(proxy.user());
        proxies << url.url();
    }
#else
    // On Unix/Linux use system environment variables if any are set.
    QString proxyVar(proxyFor(url.scheme()));
    // Check for SOCKS proxy, if not proxy is found for given url.
    if (!proxyVar.isEmpty()) {
        const QString proxy(QString::fromLocal8Bit(qgetenv(proxyVar.toLocal8Bit().constData())).trimmed());
        if (!proxy.isEmpty()) {
            proxies << proxy;
        }
    }
    // Add the socks proxy as an alternate proxy if it exists,
    proxyVar = proxyFor(QStringLiteral("socks"));
    if (!proxyVar.isEmpty()) {
        QString proxy = QString::fromLocal8Bit(qgetenv(proxyVar.toLocal8Bit().constData())).trimmed();
        // Make sure the scheme of SOCKS proxy is always set to "socks://".
        const int index = proxy.indexOf(QLatin1String("://"));
        const int offset = (index == -1) ? 0 : (index + 3);
        proxy = QLatin1String("socks://") + QStringView(proxy).mid(offset);
        if (!proxy.isEmpty()) {
            proxies << proxy;
        }
    }
#endif
    return proxies;
}

QStringList KProtocolManagerPrivate::proxiesForUrl(const QUrl &url)
{
    QStringList proxyList;

    KProtocolManagerPrivate *d = kProtocolManagerPrivate();
    QMutexLocker lock(&d->mutex);
    if (!d->shouldIgnoreProxyFor(url)) {
        switch (d->proxyType()) {
        case PACProxy:
        case WPADProxy: {
            QUrl u(url);
            const QString protocol = adjustProtocol(u.scheme());
            u.setScheme(protocol);

#ifdef WITH_QTDBUS
            if (protocol.startsWith(QLatin1String("http")) || protocol.startsWith(QLatin1String("ftp"))) {
                QDBusReply<QStringList> reply =
                    QDBusInterface(QStringLiteral("org.kde.kded6"), QStringLiteral("/modules/proxyscout"), QStringLiteral("org.kde.KPAC.ProxyScout"))
                        .call(QStringLiteral("proxiesForUrl"), u.toString());
                proxyList = reply;
            }
#endif
            break;
        }
        case EnvVarProxy:
            proxyList = d->getSystemProxyFor(url);
            break;
        case ManualProxy: {
            QString proxy(d->proxyFor(url.scheme()));
            if (!proxy.isEmpty()) {
                proxyList << proxy;
            }
            // Add the socks proxy as an alternate proxy if it exists,
            proxy = d->proxyFor(QStringLiteral("socks"));
            if (!proxy.isEmpty()) {
                // Make sure the scheme of SOCKS proxy is always set to "socks://".
                const int index = proxy.indexOf(QLatin1String("://"));
                const int offset = (index == -1) ? 0 : (index + 3);
                proxy = QLatin1String("socks://") + QStringView(proxy).mid(offset);
                proxyList << proxy;
            }
            break;
        }
        case NoProxy:
            break;
        }
    }

    if (proxyList.isEmpty()) {
        proxyList << QStringLiteral("DIRECT");
    }

    return proxyList;
}

// Generates proxy cache key from request given url.
static QString extractProxyCacheKeyFromUrl(const QUrl &u)
{
    QString key = u.scheme();
    key += u.host();

    if (u.port() > 0) {
        key += QString::number(u.port());
    }
    return key;
}

QString KProtocolManagerPrivate::workerProtocol(const QUrl &url, QStringList &proxyList)
{
    proxyList.clear();
    KProtocolManagerPrivate *d = kProtocolManagerPrivate();
    QMutexLocker lock(&d->mutex);
    // Do not perform a proxy lookup for any url classified as a ":local" url or
    // one that does not have a host component or if proxy is disabled.
    QString protocol(url.scheme());
    if (url.host().isEmpty() || KProtocolInfo::protocolClass(protocol) == QLatin1String(":local") || kProtocolManagerPrivate->proxyType() == NoProxy) {
        return protocol;
    }

    const QString proxyCacheKey = extractProxyCacheKeyFromUrl(url);

    // Look for cached proxy information to avoid more work.
    KProxyData *data = d->cachedProxyData.object(proxyCacheKey);
    if (data) {
        proxyList = data->proxyList;
        return data->protocol;
    }
    lock.unlock();

    const QStringList proxies = KProtocolManagerPrivate::proxiesForUrl(url);
    const int count = proxies.count();

    if (count > 0 && !(count == 1 && proxies.first() == QLatin1String("DIRECT"))) {
        for (const QString &proxy : proxies) {
            if (proxy == QLatin1String("DIRECT")) {
                proxyList << proxy;
            } else {
                QUrl u(proxy);
                if (!u.isEmpty() && u.isValid() && !u.scheme().isEmpty()) {
                    proxyList << proxy;
                }
            }
        }
    }

    // The idea behind worker protocols is not applicable to http
    // and webdav protocols as well as protocols unknown to KDE.
    /* clang-format off */
    if (!proxyList.isEmpty()
        && !protocol.startsWith(QLatin1String("http"))
        && !protocol.startsWith(QLatin1String("webdav"))
        && KProtocolInfo::isKnownProtocol(protocol)) { /* clang-format on */
        for (const QString &proxy : std::as_const(proxyList)) {
            QUrl u(proxy);
            if (u.isValid() && KProtocolInfo::isKnownProtocol(u.scheme())) {
                protocol = u.scheme();
                break;
            }
        }
    }

    lock.relock();
    // cache the proxy information...
    d->cachedProxyData.insert(proxyCacheKey, new KProxyData(protocol, proxyList));
    return protocol;
}

/*================================= USER-AGENT SETTINGS =====================*/

// This is not the OS, but the windowing system, e.g. X11 on Unix/Linux.
static QString platform()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN)
    return QStringLiteral("X11");
#elif defined(Q_OS_MAC)
    return QStringLiteral("Macintosh");
#elif defined(Q_OS_WIN)
    return QStringLiteral("Windows");
#else
    return QStringLiteral("Unknown");
#endif
}

QString KProtocolManagerPrivate::defaultUserAgent(const QString &_modifiers)
{
    KProtocolManagerPrivate *d = kProtocolManagerPrivate();
    QMutexLocker lock(&d->mutex);
    QString modifiers = _modifiers.toLower();
    if (modifiers.isEmpty()) {
        modifiers = QStringLiteral("om"); // Show OS, Machine
    }

    if (d->modifiers == modifiers && !d->useragent.isEmpty()) {
        return d->useragent;
    }

    d->modifiers = modifiers;

    QString systemName;
    QString systemVersion;
    QString machine;
    QString supp;
    const bool sysInfoFound = KProtocolManagerPrivate::getSystemNameVersionAndMachine(systemName, systemVersion, machine);

    supp += platform();

    if (sysInfoFound) {
        if (modifiers.contains(QLatin1Char('o'))) {
            supp += QLatin1String("; ") + systemName;
            if (modifiers.contains(QLatin1Char('v'))) {
                supp += QLatin1Char(' ') + systemVersion;
            }

            if (modifiers.contains(QLatin1Char('m'))) {
                supp += QLatin1Char(' ') + machine;
            }
        }

        if (modifiers.contains(QLatin1Char('l'))) {
            supp += QLatin1String("; ") + QLocale::languageToString(QLocale().language());
        }
    }

    QString appName = QCoreApplication::applicationName();
    if (appName.isEmpty() || appName.startsWith(QLatin1String("kcmshell"), Qt::CaseInsensitive)) {
        appName = QStringLiteral("KDE");
    }
    QString appVersion = QCoreApplication::applicationVersion();
    if (appVersion.isEmpty()) {
        appVersion += QLatin1String(KIO_VERSION_STRING);
    }

    d->useragent = QLatin1String("Mozilla/5.0 (%1) ").arg(supp)
        + QLatin1String("KIO/%1.%2 ").arg(QString::number(KIO_VERSION_MAJOR), QString::number(KIO_VERSION_MINOR))
        + QLatin1String("%1/%2").arg(appName, appVersion);

    // qDebug() << "USERAGENT STRING:" << d->useragent;
    return d->useragent;
}

bool KProtocolManagerPrivate::getSystemNameVersionAndMachine(QString &systemName, QString &systemVersion, QString &machine)
{
#if defined(Q_OS_WIN)
    // we do not use unameBuf.sysname information constructed in kdewin32
    // because we want to get separate name and version
    systemName = QStringLiteral("Windows");
    OSVERSIONINFOEX versioninfo;
    ZeroMemory(&versioninfo, sizeof(OSVERSIONINFOEX));
    // try calling GetVersionEx using the OSVERSIONINFOEX, if that fails, try using the OSVERSIONINFO
    versioninfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    bool ok = GetVersionEx((OSVERSIONINFO *)&versioninfo);
    if (!ok) {
        versioninfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        ok = GetVersionEx((OSVERSIONINFO *)&versioninfo);
    }
    if (ok) {
        systemVersion = QString::number(versioninfo.dwMajorVersion);
        systemVersion += QLatin1Char('.');
        systemVersion += QString::number(versioninfo.dwMinorVersion);
    }
#else
    struct utsname unameBuf;
    if (0 != uname(&unameBuf)) {
        return false;
    }
    systemName = QString::fromUtf8(unameBuf.sysname);
    systemVersion = QString::fromUtf8(unameBuf.release);
    machine = QString::fromUtf8(unameBuf.machine);
#endif
    return true;
}

/*==================================== OTHERS ===============================*/

bool KProtocolManager::markPartial()
{
    KProtocolManagerPrivate *d = kProtocolManagerPrivate();
    QMutexLocker lock(&d->mutex);
    return config()->group(QString()).readEntry("MarkPartial", true);
}

int KProtocolManager::minimumKeepSize()
{
    KProtocolManagerPrivate *d = kProtocolManagerPrivate();
    QMutexLocker lock(&d->mutex);
    return config()->group(QString()).readEntry("MinimumKeepSize",
                                                DEFAULT_MINIMUM_KEEP_SIZE); // 5000 byte
}

bool KProtocolManager::autoResume()
{
    KProtocolManagerPrivate *d = kProtocolManagerPrivate();
    QMutexLocker lock(&d->mutex);
    return config()->group(QString()).readEntry("AutoResume", false);
}

/* =========================== PROTOCOL CAPABILITIES ============== */

static KProtocolInfoPrivate *findProtocol(const QUrl &url)
{
    if (!url.isValid()) {
        return nullptr;
    }
    QString protocol = url.scheme();
    if (!KProtocolInfo::proxiedBy(protocol).isEmpty()) {
        QStringList dummy;
        protocol = KProtocolManagerPrivate::workerProtocol(url, dummy);
    }

    return KProtocolInfoFactory::self()->findProtocol(protocol);
}

KProtocolInfo::Type KProtocolManager::inputType(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return KProtocolInfo::T_NONE;
    }

    return prot->m_inputType;
}

KProtocolInfo::Type KProtocolManager::outputType(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return KProtocolInfo::T_NONE;
    }

    return prot->m_outputType;
}

bool KProtocolManager::isSourceProtocol(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_isSourceProtocol;
}

bool KProtocolManager::supportsListing(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_supportsListing;
}

QStringList KProtocolManager::listing(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return QStringList();
    }

    return prot->m_listing;
}

bool KProtocolManager::supportsReading(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_supportsReading;
}

bool KProtocolManager::supportsWriting(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_supportsWriting;
}

bool KProtocolManager::supportsMakeDir(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_supportsMakeDir;
}

bool KProtocolManager::supportsDeleting(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_supportsDeleting;
}

bool KProtocolManager::supportsLinking(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_supportsLinking;
}

bool KProtocolManager::supportsMoving(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_supportsMoving;
}

bool KProtocolManager::supportsOpening(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_supportsOpening;
}

bool KProtocolManager::supportsTruncating(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_supportsTruncating;
}

bool KProtocolManager::canCopyFromFile(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_canCopyFromFile;
}

bool KProtocolManager::canCopyToFile(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_canCopyToFile;
}

bool KProtocolManager::canRenameFromFile(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_canRenameFromFile;
}

bool KProtocolManager::canRenameToFile(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_canRenameToFile;
}

bool KProtocolManager::canDeleteRecursive(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return false;
    }

    return prot->m_canDeleteRecursive;
}

KProtocolInfo::FileNameUsedForCopying KProtocolManager::fileNameUsedForCopying(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return KProtocolInfo::FromUrl;
    }

    return prot->m_fileNameUsedForCopying;
}

QString KProtocolManager::defaultMimetype(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return QString();
    }

    return prot->m_defaultMimetype;
}

QString KProtocolManager::protocolForArchiveMimetype(const QString &mimeType)
{
    KProtocolManagerPrivate *d = kProtocolManagerPrivate();
    QMutexLocker lock(&d->mutex);
    if (d->protocolForArchiveMimetypes.isEmpty()) {
        const QList<KProtocolInfoPrivate *> allProtocols = KProtocolInfoFactory::self()->allProtocols();
        for (KProtocolInfoPrivate *allProtocol : allProtocols) {
            const QStringList archiveMimetypes = allProtocol->m_archiveMimeTypes;
            for (const QString &mime : archiveMimetypes) {
                d->protocolForArchiveMimetypes.insert(mime, allProtocol->m_name);
            }
        }
    }
    return d->protocolForArchiveMimetypes.value(mimeType);
}

QString KProtocolManager::charsetFor(const QUrl &url)
{
    return KIO::WorkerConfig::self()->configData(url.scheme(), url.host(), QStringLiteral("Charset"));
}

bool KProtocolManager::supportsPermissions(const QUrl &url)
{
    KProtocolInfoPrivate *prot = findProtocol(url);
    if (!prot) {
        return true;
    }

    return prot->m_supportsPermissions;
}

#undef PRIVATE_DATA

#include "moc_kprotocolmanager_p.cpp"
