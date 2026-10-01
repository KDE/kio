/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "fusemountjob.h"

#include "global.h"
#include "job.h"
#include "kiocoredebug.h"

#include <KLocalizedString>

#ifdef WITH_QTDBUS
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

#include "kiofuse_interface.h"
#endif

#include <limits>

class KIO::FuseMountJobPrivate
{
public:
    explicit FuseMountJobPrivate(const QUrl &url, FuseMountJob *qq)
        : m_url(url)
        , q(qq)
    {
    }

    void mount();
    void fail(int code, const QString &text);

    QUrl m_url;
    QString m_localPath;
    int m_timeout = std::numeric_limits<int>::max();
#ifdef WITH_QTDBUS
    QDBusPendingCallWatcher *m_watcher = nullptr;

    void slotReply();
#endif

    KIO::FuseMountJob *const q;
};

KIO::FuseMountJob::FuseMountJob(const QUrl &url, QObject *parent)
    : KJob(parent)
    , d(new FuseMountJobPrivate(url, this))
{
    setCapabilities(KJob::Killable);
}

KIO::FuseMountJob::~FuseMountJob()
{
}

void KIO::FuseMountJob::setTimeout(int msecs)
{
    d->m_timeout = msecs;
}

void KIO::FuseMountJob::start()
{
    QMetaObject::invokeMethod(
        this,
        [this]() {
            d->mount();
        },
        Qt::QueuedConnection);
}

QUrl KIO::FuseMountJob::url() const
{
    return d->m_url;
}

QString KIO::FuseMountJob::localPath() const
{
    return d->m_localPath;
}

QString KIO::FuseMountJob::errorString() const
{
    return KIO::buildErrorString(error(), errorText());
}

bool KIO::FuseMountJob::doKill()
{
#ifdef WITH_QTDBUS
    // The reply can't be withdrawn, it is ignored instead
    delete d->m_watcher;
    d->m_watcher = nullptr;
#endif
    return true;
}

void KIO::FuseMountJobPrivate::fail(int code, const QString &text)
{
    q->setError(code);
    q->setErrorText(text);
    q->emitResult();
}

void KIO::FuseMountJobPrivate::mount()
{
    if (!m_url.isValid() || m_url.scheme().isEmpty()) {
        qCDebug(KIO_CORE) << "Malformed URL" << m_url;
        fail(KIO::ERR_MALFORMED_URL, m_url.toDisplayString());
        return;
    }

    if (m_url.isLocalFile()) {
        m_localPath = m_url.toLocalFile();
        q->emitResult();
        return;
    }

#ifdef WITH_QTDBUS
    org::kde::KIOFuse::VFS kiofuse_iface(QStringLiteral("org.kde.KIOFuse"), QStringLiteral("/org/kde/KIOFuse"), QDBusConnection::sessionBus());
    kiofuse_iface.setTimeout(m_timeout);
    m_watcher = new QDBusPendingCallWatcher(kiofuse_iface.mountUrl(m_url.toString(QUrl::FullyEncoded)), q);
    QObject::connect(m_watcher, &QDBusPendingCallWatcher::finished, q, [this]() {
        slotReply();
    });
#else
    fail(KIO::ERR_UNSUPPORTED_ACTION, i18n("Accessing %1 through a local path is not supported on this system.", m_url.toDisplayString()));
#endif
}

#ifdef WITH_QTDBUS
void KIO::FuseMountJobPrivate::slotReply()
{
    const QDBusPendingReply<QString> reply = *m_watcher;
    m_watcher->deleteLater();
    m_watcher = nullptr;

    if (!reply.isError()) {
        m_localPath = reply.value();
        q->emitResult();
        return;
    }

    const QDBusError error = reply.error();
    qCDebug(KIO_CORE) << "kio-fuse could not mount" << m_url.toDisplayString() << error;

    switch (error.type()) {
    case QDBusError::ServiceUnknown:
        fail(KIO::ERR_UNSUPPORTED_ACTION, i18n("Accessing %1 through a local path requires kio-fuse, which is not available.", m_url.toDisplayString()));
        return;
    case QDBusError::NoReply:
    case QDBusError::Timeout:
        fail(KIO::ERR_SERVER_TIMEOUT, m_url.host());
        return;
    default:
        break;
    }

    if (error.name() == QLatin1String("org.kde.KIOFuse.VFS.Error.SchemeNotSupported")) {
        fail(KIO::ERR_UNSUPPORTED_PROTOCOL, m_url.scheme());
        return;
    }

    fail(KIO::ERR_CANNOT_MOUNT, error.message());
}
#endif

#include "moc_fusemountjob.cpp"
