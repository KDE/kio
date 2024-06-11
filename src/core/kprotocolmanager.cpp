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
#include <QHostAddress>
#include <QHostInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSslSocket>
#include <QStandardPaths>
#include <QUrl>

#include <KConfigGroup>
#include <KSharedConfig>
#include <kio_version.h>

#include <kprotocolinfofactory_p.h>

#include "ioworker_defaults.h"
#include "workerconfig.h"

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
}

KProtocolManagerPrivate::~KProtocolManagerPrivate()
{
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
