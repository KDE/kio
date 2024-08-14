#include "kopenwithjob.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>

#include <QFile>
#include <QGuiApplication>

#include <KAuthorized>
#include <KJobWindows>
#include <KLocalizedString>
#include <KSandbox>
#include <KWaylandExtras>
#include <KWindowSystem>

#include "applicationlauncherjob.h"
#include "job.h"
#include "jobuidelegatefactory.h"
#include "openwithhandlerinterface.h"

using namespace Qt::Literals::StringLiterals;

KOpenWithJob::KOpenWithJob()
    : KCompositeJob()
{
}

KOpenWithJob::~KOpenWithJob()
{
    qWarning() << "done";
}

void KOpenWithJob::start()
{
    if (KSandbox::isInside()) {
        useXdgPortal();
    } else {
        showOpenWithDialog();
    }
}

void KOpenWithJob::useXdgPortal()
{
    m_window = KJobWindows::window(this);
    if (!m_window) {
        m_window = qGuiApp->focusWindow();
    }

    if (!m_window) {
        m_window = qGuiApp->topLevelWindows().last();
    }

    // TODO no window?

    if (m_window) {
        if (KWindowSystem::isPlatformWayland()) {
            connect(
                KWaylandExtras::self(),
                &KWaylandExtras::windowExported,
                this,
                [this](QWindow * /*window*/, const QString &handle) {
                    m_portalWindowHandle = "wayland:"_L1 + handle;
                    slotGotWindow();
                },
                Qt::SingleShotConnection);

            KWaylandExtras::exportWindow(m_window);
        } else if (KWindowSystem::isPlatformX11()) {
            m_portalWindowHandle = "x11:"_L1 + QString::number(m_window->winId(), 16);
            slotGotWindow();
        } else {
            slotGotWindow();
        }
    } else {
        slotGotWindow();
    }
}

void KOpenWithJob::slotGotWindow()
{
    if (KWindowSystem::isPlatformWayland()) {
        connect(
            KWaylandExtras::self(),
            &KWaylandExtras::xdgActivationTokenArrived,
            this,
            [this](int /*serial*/, const QString &token) {
                m_activationToken = token;
                slotGotActivationToken();
            },
            Qt::SingleShotConnection);
        KWaylandExtras::requestXdgActivationToken(m_window, KWaylandExtras::lastInputSerial(m_window), QString());
    } else {
        slotGotActivationToken();
    }
}

void KOpenWithJob::slotGotActivationToken()
{
    QVariantMap options{
        {u"ask"_s, true},
        {u"activation_token"_s, m_activationToken},
    };

    auto msg = QDBusMessage::createMethodCall(u"org.freedesktop.portal.Desktop"_s,
                                              u"/org/freedesktop/portal/desktop"_s,
                                              u"org.freedesktop.portal.OpenURI"_s,
                                              u"OpenFile"_s);

    // TODO handle multiple URLs
    // TODO handle non-file URIs
    QFile file(m_urls.constFirst().toLocalFile());

    bool open = file.open(QIODevice::ReadWrite);

    if (!open) {
        setError(KJob::UserDefinedError);
        setErrorText(u"Could not open input file %1: %2"_s.arg(m_urls.constFirst().toLocalFile(), file.errorString()));
        emitResult();
        return;
    }

    QDBusUnixFileDescriptor fd;
    fd.giveFileDescriptor(file.handle());

    msg.setArguments({m_portalWindowHandle, QVariant::fromValue(fd), options});

    QDBusPendingReply<void> reply = QDBusConnection::sessionBus().asyncCall(msg);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<void> reply = *watcher;
        watcher->deleteLater();

        if (reply.isError()) {
            setError(KJob::UserDefinedError);
            setErrorText(u"Portal DBus call failed: %1"_s.arg(reply.error().message()));
        }

        emitResult();
    });
}

void KOpenWithJob::showOpenWithDialog()
{
    if (!KAuthorized::authorizeAction(QStringLiteral("openwith"))) {
        setError(KJob::UserDefinedError);
        setErrorText(i18n("You are not authorized to select an application to open this file."));
        emitResult();
        return;
    }

    auto *openWithHandler = KIO::delegateExtension<KIO::OpenWithHandlerInterface *>(this);
    if (!openWithHandler) {
        setError(KJob::UserDefinedError);
        setErrorText(i18n("Internal error: could not prompt the user for which application to start"));
        emitResult();
        return;
    }

    QObject::connect(openWithHandler, &KIO::OpenWithHandlerInterface::canceled, this, [this]() {
        setError(KIO::ERR_USER_CANCELED);
        emitResult();
    });

    QObject::connect(openWithHandler, &KIO::OpenWithHandlerInterface::serviceSelected, this, [this](const KService::Ptr &service) {
        Q_ASSERT(service);
        auto job = new KIO::ApplicationLauncherJob(service, this);
        addSubjob(job);
        job->start();
    });

    QObject::connect(openWithHandler, &KIO::OpenWithHandlerInterface::handled, this, [this]() {
        emitResult();
    });

    openWithHandler->promptUserForApplication(this, m_urls, m_mimeTypeName);
}

void KOpenWithJob::slotResult(KJob *job)
{
    // This is only used for the final application/launcher job, so we're done when it's done
    const int errCode = job->error();
    if (errCode) {
        setError(errCode);
        // We're a KJob, not a KIO::Job, so build the error string here
        setErrorText(KIO::buildErrorString(errCode, job->errorText()));
    }
    emitResult();
}

void KOpenWithJob::setUrls(const QList<QUrl> &urls)
{
    m_urls = urls;
}

void KOpenWithJob::setMimeType(const QString &mimeType)
{
    m_mimeTypeName = mimeType;
}

void KOpenWithJob::setStartupId(const QByteArray &startupId)
{
    m_activationToken = QString::fromUtf8(startupId);
}
