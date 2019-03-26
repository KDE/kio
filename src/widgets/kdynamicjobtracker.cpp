/*
 * Copyright 2008 by Rob Scheepmaker <r.scheepmaker@student.utwente.nl>
 * Copyright 2010 Shaun Reich <shaun.reich@kdemail.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "kdynamicjobtracker_p.h"
#include "kio_widgets_debug.h"
#include "kuiserver_interface.h"

#include <kuiserverjobtracker.h>
#include <kwidgetjobtracker.h>
#include <kjobtrackerinterface.h>
#include <kio/jobtracker.h>

#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QMap>
#include <QDebug>

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

            if (reply.isValid() && reply.value()) {
                // create a widget tracker in addition to kuiservertracker.
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
