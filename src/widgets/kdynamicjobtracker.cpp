/*
    SPDX-FileCopyrightText: 2008 Rob Scheepmaker <r.scheepmaker@student.utwente.nl>
    SPDX-FileCopyrightText: 2010 Shaun Reich <shaun.reich@kdemail.net>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "kdynamicjobtracker_p.h"
#include "kio_widgets_debug.h"
#include "kuiserver_interface.h"

#include <KUiServerJobTracker>
#include <KWidgetJobTracker>
#include <KJobTrackerInterface>
#include <kio/jobtracker.h>

#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QMap>

struct AllTrackers {
    KUiServerJobTracker *kuiserverTracker;
    KWidgetJobTracker *widgetTracker;
};

class Q_DECL_HIDDEN KDynamicJobTracker::Private
{
public:
    Private() : kuiserverTracker(nullptr),
        widgetTracker(nullptr)
    {
    }

    ~Private()
    {
        delete kuiserverTracker;
        delete widgetTracker;
    }

    KUiServerJobTracker *kuiserverTracker;
    KWidgetJobTracker *widgetTracker;
    QMap<KJob *, AllTrackers> trackers;
};

KDynamicJobTracker::KDynamicJobTracker(QObject *parent)
    : KJobTrackerInterface(parent),
      d(new Private)
{
}

KDynamicJobTracker::~KDynamicJobTracker()
{
    delete d;
}

void KDynamicJobTracker::registerJob(KJob *job)
{
    if (d->trackers.contains(job)) {
        return;
    }

    // only interested in finished() signal,
    // so catching ourselves instead of using KJobTrackerInterface::registerJob()
    connect(job, &KJob::finished,
            this, &KDynamicJobTracker::unregisterJob);

    const bool canHaveWidgets = (qobject_cast<QApplication *>(qApp) != nullptr);

    // always add an entry, even with no trackers used at all,
    // so unregisterJob() will work as normal
    AllTrackers& trackers = d->trackers[job];

    // do not try to query kuiserver if dbus is not available
    if (!QDBusConnection::sessionBus().interface()) {
        if (canHaveWidgets) {
            // fallback to widget tracker only!
            if (!d->widgetTracker) {
                d->widgetTracker = new KWidgetJobTracker();
            }

            trackers.widgetTracker = d->widgetTracker;
            trackers.widgetTracker->registerJob(job);
        } else {
            trackers.widgetTracker = nullptr;
        }

        trackers.kuiserverTracker = nullptr;
    } else {
        if (!d->kuiserverTracker) {
            d->kuiserverTracker = new KUiServerJobTracker();
        }

        trackers.kuiserverTracker = d->kuiserverTracker;
        trackers.kuiserverTracker->registerJob(job);

        trackers.widgetTracker = nullptr;
        if (canHaveWidgets) {
            org::kde::kuiserver interface(QStringLiteral("org.kde.kuiserver"),
                                          QStringLiteral("/JobViewServer"),
                                          QDBusConnection::sessionBus(),
                                          this);
            QDBusReply<bool> reply = interface.requiresJobTracker();

            // If kuiserver isn't available or it tells us a job tracker is required
            // create a widget tracker.
            if (!reply.isValid() || reply.value()) {
                if (!d->widgetTracker) {
                    d->widgetTracker = new KWidgetJobTracker();
                }
                trackers.widgetTracker = d->widgetTracker;
                trackers.widgetTracker->registerJob(job);
            }
        }
    }
}

void KDynamicJobTracker::unregisterJob(KJob *job)
{
    job->disconnect(this);

    QMap<KJob*, AllTrackers>::Iterator it = d->trackers.find(job);

    if (it == d->trackers.end()) {
        qCWarning(KIO_WIDGETS) << "Tried to unregister a kio job that hasn't been registered.";
        return;
    }

    const AllTrackers& trackers = it.value();
    KUiServerJobTracker *kuiserverTracker = trackers.kuiserverTracker;
    KWidgetJobTracker *widgetTracker = trackers.widgetTracker;

    if (kuiserverTracker) {
        kuiserverTracker->unregisterJob(job);
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
