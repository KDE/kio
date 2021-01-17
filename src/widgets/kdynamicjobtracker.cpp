/*
    SPDX-FileCopyrightText: 2008 Rob Scheepmaker <r.scheepmaker@student.utwente.nl>
    SPDX-FileCopyrightText: 2010 Shaun Reich <shaun.reich@kdemail.net>
    SPDX-FileCopyrightText: 2021 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "kdynamicjobtracker_p.h"
#include "kio_widgets_debug.h"
#include "kuiserver_interface.h"

#include <KJobTrackerInterface>
#include <KUiServerJobTracker>
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
    KUiServerJobTracker *kuiserverTracker;
    KUiServerV2JobTracker *kuiserverV2Tracker;
    KWidgetJobTracker *widgetTracker;
};

class KDynamicJobTrackerPrivate
{
public:
    KDynamicJobTrackerPrivate()
    {
    }

    ~KDynamicJobTrackerPrivate()
    {
        delete kuiserverTracker;
        delete kuiserverV2Tracker;
        delete widgetTracker;
    }

    static bool hasDBusInterface(const QString &introspectionData, const QString &interface)
    {
        QXmlStreamReader xml(introspectionData);
        while (!xml.atEnd() && !xml.hasError()) {
            xml.readNext();

            if (xml.tokenType() == QXmlStreamReader::StartElement
                    && xml.name() == QLatin1String("interface")) {

                if (xml.attributes().value(QLatin1String("name")) == interface) {
                    return true;
                }
            }
        }
        return false;
    }

    KUiServerJobTracker *kuiserverTracker = nullptr;
    KUiServerV2JobTracker *kuiserverV2Tracker = nullptr;
    KWidgetJobTracker *widgetTracker = nullptr;
    QMap<KJob *, AllTrackers> trackers;

    enum JobViewServerSupport {
        NeedsChecking,
        Error,
        V2Supported,
        V2NotSupported
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
    trackers.kuiserverTracker = nullptr;
    trackers.kuiserverV2Tracker = nullptr;
    trackers.widgetTracker = nullptr;

    // do not try to query kuiserver if dbus is not available
    if (!QDBusConnection::sessionBus().interface()) {
        if (canHaveWidgets) {
            // fallback to widget tracker only!
            if (!d->widgetTracker) {
                d->widgetTracker = new KWidgetJobTracker();
            }

            trackers.widgetTracker = d->widgetTracker;
            trackers.widgetTracker->registerJob(job);
        }
        return;
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
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().count() != 1)  {
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
    }

    if (d->jobViewServerSupport == KDynamicJobTrackerPrivate::V2Supported) {
        if (!d->kuiserverV2Tracker) {
            d->kuiserverV2Tracker = new KUiServerV2JobTracker();
        }

        trackers.kuiserverV2Tracker = d->kuiserverV2Tracker;
        trackers.kuiserverV2Tracker->registerJob(job);
        return;
    }

    // No point in trying to set up V1 if calling the service above failed.
    if (d->jobViewServerSupport != KDynamicJobTrackerPrivate::Error) {
        if (!d->kuiserverTracker) {
            d->kuiserverTracker = new KUiServerJobTracker();
        }

        trackers.kuiserverTracker = d->kuiserverTracker;
        trackers.kuiserverTracker->registerJob(job);
    }

    if (canHaveWidgets) {
        bool needsWidgetTracker = d->jobViewServerSupport == KDynamicJobTrackerPrivate::Error;

        if (!needsWidgetTracker) {
            org::kde::kuiserver interface(kuiserverService,
                                          QStringLiteral("/JobViewServer"),
                                          QDBusConnection::sessionBus(),
                                          this);
            QDBusReply<bool> reply = interface.requiresJobTracker();
            needsWidgetTracker = !reply.isValid() || reply.value();
        }

        // If kuiserver isn't available or it tells us a job tracker is required
        // create a widget tracker.
        if (needsWidgetTracker) {
            if (!d->widgetTracker) {
                d->widgetTracker = new KWidgetJobTracker();
            }
            trackers.widgetTracker = d->widgetTracker;
            trackers.widgetTracker->registerJob(job);
        }
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
    KUiServerJobTracker *kuiserverTracker = trackers.kuiserverTracker;
    KUiServerV2JobTracker *kuiserverV2Tracker = trackers.kuiserverV2Tracker;
    KWidgetJobTracker *widgetTracker = trackers.widgetTracker;

    if (kuiserverTracker) {
        kuiserverTracker->unregisterJob(job);
    }

    if (kuiserverV2Tracker) {
        kuiserverV2Tracker->unregisterJob(job);
    }

    if (widgetTracker) {
        widgetTracker->unregisterJob(job);
    }

    d->trackers.erase(it);
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
