// SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
// SPDX-FileCopyrightText: 2022-2026 Harald Sitter <sitter@kde.org>

#include "applicationchooserjob.h"

#include <fcntl.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QUrl>

#include <KWaylandExtras>

#include "global.h"

using namespace Qt::StringLiterals;

namespace
{

class WindowExporter : public QObject
{
    Q_OBJECT
public:
    explicit WindowExporter(QWindow *window, QObject *parent = nullptr)
        : QObject(parent)
    {
        auto wayland = KWaylandExtras::self();
        exportConnection = connect(
            wayland,
            &KWaylandExtras::windowExported,
            this,
            [this, window](QWindow *exportedWindow, const QString &handle) {
                if (exportedWindow != window) { // not our event
                    return;
                }
                // only emit once, lest the same window be used by another exporter before this one gets destroyed
                disconnect(exportConnection);

                auto tokenFuture = KWaylandExtras::xdgActivationToken(window, {});
                tokenFuture.then(this, [this, handle](const QString &activationToken) {
                    Q_EMIT portalIdentifierAndActivationToken(u"wayland:"_s.append(handle), activationToken);
                });
            },
            Qt::QueuedConnection);
        wayland->exportWindow(window);
    }

Q_SIGNALS:
    void portalIdentifierAndActivationToken(const QString &portalIdentifier, const QString &activationToken);

private:
    QMetaObject::Connection exportConnection;
};

struct ResultError {
    KIO::Error error = KIO::ERR_NONE;
    QString errorText;
};

template<class... Ts>
struct overload : Ts... {
    using Ts::operator()...;

    consteval void operator()(auto /*unused*/) const
    {
        static_assert(false, "Unhandled case in std::visit");
    }
};

} // namespace

namespace KIO
{

class ApplicationChooserJobPrivate : public QObject
{
    Q_OBJECT
public:
    ApplicationChooserJobPrivate(const QUrl &url, bool exportWritable, QWindow *parentWindow)
        : m_url(url)
        , m_exportWritable(exportWritable)
        , m_window(parentWindow)
    {
    }

    ApplicationChooserJob *q = nullptr; // initialized by the constructor consuming this private
    QUrl m_url;
    bool m_exportWritable;
    QWindow *m_window;

    [[nodiscard]] std::variant<QDBusMessage, ResultError> createMessage(const QString &portalIdentifier, const QString &activationToken) const
    {
        if (m_url.scheme() == "file"_L1) {
            QDBusMessage message = QDBusMessage::createMethodCall(u"org.freedesktop.portal.Desktop"_s,
                                                                  u"/org/freedesktop/portal/desktop"_s,
                                                                  u"org.freedesktop.portal.OpenURI"_s,
                                                                  u"OpenFile"_s);

            auto writable = m_exportWritable;
            auto fd = open(m_url.toLocalFile().toUtf8().constData(), O_RDWR | O_CLOEXEC);
            if (fd < 0) { // try read only
                fd = open(m_url.toLocalFile().toUtf8().constData(), O_RDONLY | O_CLOEXEC);
                writable = false;
                if (fd < 0) { // give up
                    qWarning() << "Could not open file to read or readwrite for portal openwith:" << m_url;
                    return ResultError{.error = KIO::ERR_INTERNAL, .errorText = m_url.toLocalFile()};
                }
            }
            QDBusUnixFileDescriptor dbusFd;
            dbusFd.giveFileDescriptor(fd);

            message << portalIdentifier //
                    << QVariant::fromValue(dbusFd) //
                    << QVariantMap{
                           {QStringLiteral("ask"), true}, //
                           {QStringLiteral("writable"), writable}, //
                           {QStringLiteral("activation_token"), activationToken}, //
                       };

            return message;
        }

        QDBusMessage message = QDBusMessage::createMethodCall(u"org.freedesktop.portal.Desktop"_s,
                                                              u"/org/freedesktop/portal/desktop"_s,
                                                              u"org.freedesktop.portal.OpenURI"_s,
                                                              u"OpenURI"_s);

        message << portalIdentifier //
                << m_url.toString() //
                << QVariantMap{
                       {QStringLiteral("ask"), true}, //
                       {QStringLiteral("writable"), m_exportWritable}, //
                       {QStringLiteral("activation_token"), activationToken}, //
                   };
        return message;
    }

    void sendMessage(const QDBusMessage &message)
    {
        QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message, std::numeric_limits<int>::max());
        auto watcher = new QDBusPendingCallWatcher(pendingCall, q);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, q, [this, message](QDBusPendingCallWatcher *watcher) {
            watcher->deleteLater();

            QDBusReply<QDBusObjectPath> reply = *watcher;
            if (!reply.isValid()) {
                qWarning() << "Error: " << reply.error().message();
                emitError({.error = KIO::ERR_UNSUPPORTED_ACTION, .errorText = reply.error().message()});
                return;
            }

            QDBusConnection::sessionBus().connect(u"org.freedesktop.portal.Desktop"_s,
                                                  reply.value().path(),
                                                  u"org.freedesktop.portal.Request"_s,
                                                  u"Response"_s,
                                                  this,
                                                  SLOT(onApplicationChosen(uint, QVariantMap)));
        });
    }

    void emitError(const ResultError &error) const
    {
        q->setError(error.error);
        q->setErrorText(error.errorText);
        q->emitResult();
    }

    void startInternal(const QString &portalIdentifier, const QString &activationToken)
    {
        const auto messageResult = createMessage(portalIdentifier, activationToken);
        std::visit(overload{
                       [this](const QDBusMessage &message) {
                           sendMessage(message);
                       },
                       [this](const ResultError &error) {
                           emitError(error);
                       },
                   },
                   messageResult);
    }

public Q_SLOTS:
    void onApplicationChosen(uint response, [[maybe_unused]] const QVariantMap &results) const
    {
        // results carries no useful data for us at the moment. Ignore it.
        switch (response) {
        case 0:
            q->emitResult();
            return;
        case 1:
            emitError({.error = KIO::ERR_USER_CANCELED, .errorText = {}});
            return;
        }
        emitError({.error = KIO::ERR_UNKNOWN, .errorText = {}});
    }
};

ApplicationChooserJob *ApplicationChooserJob::create(const QUrl &url, bool exportWritable, QWindow *parentWindow)
{
    return new ApplicationChooserJob(std::make_unique<ApplicationChooserJobPrivate>(url, exportWritable, parentWindow));
}

ApplicationChooserJob::ApplicationChooserJob(std::unique_ptr<ApplicationChooserJobPrivate> dd)
    : d(std::move(dd))
{
    d->q = this;
}

// needed for unique_ptr to be fully qualified in the cpp rather than the header
ApplicationChooserJob::~ApplicationChooserJob() = default;

void ApplicationChooserJob::start()
{
    auto exporter = new WindowExporter(d->m_window, this);
    connect(exporter,
            &WindowExporter::portalIdentifierAndActivationToken,
            this,
            [this, exporter](const QString &portalIdentifier, const QString &activationToken) {
                exporter->deleteLater();
                d->startInternal(portalIdentifier, activationToken);
            });
}

} // namespace KIO

#include "applicationchooserjob.moc"
