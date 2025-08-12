/*
    SPDX-FileCopyrightText: 2008 Rob Scheepmaker <r.scheepmaker@student.utwente.nl>
    SPDX-FileCopyrightText: 2010 Shaun Reich <shaun.reich@kdemail.net>
    SPDX-FileCopyrightText: 2021 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "kdynamicjobtracker_p.h"
#include "kio_widgets_debug.h"
#include "kuiserver_interface.h"

#include <KInhibitionJobTracker>
#include <KJobTrackerInterface>
#include <KUiServerV2JobTracker>
#include <KWidgetJobTracker>
#include <kio/jobtracker.h>

#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QMap>
#include <QXmlStreamReader>

struct AllTrackers {
    KUiServerV2JobTracker *kuiserverV2Tracker;
    KWidgetJobTracker *widgetTracker;
    KInhibitionJobTracker *inhibitionTracker;
};

class KDynamicJobTrackerPrivate
{
public:
    KDynamicJobTrackerPrivate()
    {
    }

    ~KDynamicJobTrackerPrivate()
    {
        delete kuiserverV2Tracker;
        delete widgetTracker;
        delete inhibitionTracker;
    }

    static bool hasDBusInterface(const QString &introspectionData, const QString &interface)
    {
        QXmlStreamReader xml(introspectionData);
        while (!xml.atEnd() && !xml.hasError()) {
            xml.readNext();

            if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QLatin1String("interface")) {
                if (xml.attributes().value(QLatin1String("name")) == interface) {
                    return true;
                }
            }
        }
        return false;
    }

    KUiServerV2JobTracker *kuiserverV2Tracker = nullptr;
    KWidgetJobTracker *widgetTracker = nullptr;
    KInhibitionJobTracker *inhibitionTracker = nullptr;
    QMap<KJob *, AllTrackers> trackers;

    enum JobViewServerSupport {
        NeedsChecking,
        Error,
        V2Supported,
        V2NotSupported,
    };
    JobViewServerSupport jobViewServerSupport = NeedsChecking;
    QDBusServiceWatcher *jobViewServerWatcher = nullptr;
};

KDynamicJobTracker::KDynamicJobTracker(QObject *parent)
    : KJobTrackerInterface(parent)
    , d(new KDynamicJobTrackerPrivate)
{
}

KDynamicJobTracker::~KDynamicJobTracker() = default;

void KDynamicJobTracker::registerJob(KJob *job)
{
    if (d->trackers.contains(job)) {
        return;
    }

    // only interested in finished() signal,
    // so catching ourselves instead of using KJobTrackerInterface::registerJob()
    connect(job, &KJob::finished, this, &KDynamicJobTracker::unregisterJob);

    const bool canHaveWidgets = (qobject_cast<QApplication *>(qApp) != nullptr);

    // always add an entry, even with no trackers used at all,
    // so unregisterJob() will work as normal
    AllTrackers &trackers = d->trackers[job];
    trackers.kuiserverV2Tracker = nullptr;
    trackers.widgetTracker = nullptr;
    trackers.inhibitionTracker = nullptr;

    auto useWidgetsFallback = [this, canHaveWidgets, &trackers, job] {
        if (canHaveWidgets) {
            // fallback to widget tracker only!
            if (!d->widgetTracker) {
                d->widgetTracker = new KWidgetJobTracker();
            }

            trackers.widgetTracker = d->widgetTracker;
            trackers.widgetTracker->registerJob(job);
        }
    };

    // do not try to use kuiserver on Windows/macOS
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    useWidgetsFallback();
    return;
#endif

    // do not try to query kuiserver if dbus is not available
    if (!QDBusConnection::sessionBus().interface()) {
        useWidgetsFallback();
        return;
    }

    if (!job->property("kio_no_inhibit_suspend").toBool()) {
        if (!d->inhibitionTracker) {
            d->inhibitionTracker = new KInhibitionJobTracker();
        }

        trackers.inhibitionTracker = d->inhibitionTracker;
        trackers.inhibitionTracker->registerJob(job);
    }

    const QString kuiserverService = QStringLiteral("org.kde.kuiserver");

    if (!d->jobViewServerWatcher) {
        d->jobViewServerWatcher = new QDBusServiceWatcher(kuiserverService,
                                                          QDBusConnection::sessionBus(),
                                                          QDBusServiceWatcher::WatchForOwnerChange | QDBusServiceWatcher::WatchForUnregistration,
                                                          this);
        connect(d->jobViewServerWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this] {
            d->jobViewServerSupport = KDynamicJobTrackerPrivate::NeedsChecking;
        });
    }

    if (d->jobViewServerSupport == KDynamicJobTrackerPrivate::NeedsChecking) {
        // Unfortunately no DBus ObjectManager support in Qt DBus.
        QDBusMessage msg = QDBusMessage::createMethodCall(kuiserverService,
                                                          QStringLiteral("/JobViewServer"),
                                                          QStringLiteral("org.freedesktop.DBus.Introspectable"),
                                                          QStringLiteral("Introspect"));
        auto reply = QDBusConnection::sessionBus().call(msg);
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().count() != 1) {
            qCWarning(KIO_WIDGETS) << "Failed to check which JobView API is supported" << reply.errorMessage();
            d->jobViewServerSupport = KDynamicJobTrackerPrivate::Error;
        } else {
            const QString introspectionData = reply.arguments().first().toString();

            if (KDynamicJobTrackerPrivate::hasDBusInterface(introspectionData, QStringLiteral("org.kde.JobViewServerV2"))) {
                d->jobViewServerSupport = KDynamicJobTrackerPrivate::V2Supported;
            } else {
                d->jobViewServerSupport = KDynamicJobTrackerPrivate::V2NotSupported;
            }
        }

        org::kde::kuiserver interface(kuiserverService, QStringLiteral("/JobViewServer"), QDBusConnection::sessionBus(), this);

        QDBusReply<bool> requiresTrackerReply = interface.requiresJobTracker();
        if (!requiresTrackerReply.isValid() || requiresTrackerReply.value()) {
            d->jobViewServerSupport = KDynamicJobTrackerPrivate::Error;
        }

        QDBusConnection::sessionBus().connect(kuiserverService,
                                              QStringLiteral("/JobViewServer"),
                                              QStringLiteral("org.kde.kuiserver"),
                                              QStringLiteral("requiresJobTrackerChanged"),
                                              this,
                                              SLOT(handleRequiresJobTrackerChanged(bool)));
    }

    if (d->jobViewServerSupport == KDynamicJobTrackerPrivate::V2Supported) {
        if (!d->kuiserverV2Tracker) {
            d->kuiserverV2Tracker = new KUiServerV2JobTracker();
        }

        trackers.kuiserverV2Tracker = d->kuiserverV2Tracker;
        trackers.kuiserverV2Tracker->registerJob(job);
        return;
    }

    // If kuiserver isn't available or it tells us a job tracker is required
    // create a widget tracker.
    if (d->jobViewServerSupport == KDynamicJobTrackerPrivate::Error || d->jobViewServerSupport == KDynamicJobTrackerPrivate::V2NotSupported) {
        useWidgetsFallback();
    }
}

void KDynamicJobTracker::unregisterJob(KJob *job)
{
    job->disconnect(this);

    QMap<KJob *, AllTrackers>::Iterator it = d->trackers.find(job);

    if (it == d->trackers.end()) {
        qCWarning(KIO_WIDGETS) << "Tried to unregister a kio job that hasn't been registered.";
        return;
    }

    const AllTrackers &trackers = it.value();
    KUiServerV2JobTracker *kuiserverV2Tracker = trackers.kuiserverV2Tracker;
    KWidgetJobTracker *widgetTracker = trackers.widgetTracker;
    KInhibitionJobTracker *inhibitionTracker = trackers.inhibitionTracker;

    if (kuiserverV2Tracker) {
        kuiserverV2Tracker->unregisterJob(job);
    }

    if (widgetTracker) {
        widgetTracker->unregisterJob(job);
    }

    if (inhibitionTracker) {
        inhibitionTracker->unregisterJob(job);
    }

    d->trackers.erase(it);
}

void KDynamicJobTracker::handleRequiresJobTrackerChanged(bool req)
{
    if (req) {
        d->jobViewServerSupport = KDynamicJobTrackerPrivate::Error;
    } else {
        d->jobViewServerSupport = KDynamicJobTrackerPrivate::V2Supported;
    }
}

Q_GLOBAL_STATIC(KDynamicJobTracker, globalJobTracker)

// Simply linking to this library, creates a GUI job tracker for all KIO jobs
static int registerDynamicJobTracker()
{
    KIO::setJobTracker(globalJobTracker());

    return 0; // something
}

Q_CONSTRUCTOR_FUNCTION(registerDynamicJobTracker)

#include "moc_kdynamicjobtracker_p.cpp"
