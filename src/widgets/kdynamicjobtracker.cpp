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

#include <kuiserverjobtracker.h>
#include <kwidgetjobtracker.h>
#include <kjobtrackerinterface.h>
#include <kio/jobtracker.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QMap>
#include <QDebug>

struct AllTrackers
{
    KUiServerJobTracker *kuiserverTracker;
    KWidgetJobTracker *widgetTracker;
};

class KDynamicJobTracker::Private
{
public:
    Private() : kuiserverTracker(0),
                widgetTracker(0)
    {
    }

    ~Private()
    {
        delete kuiserverTracker;
        delete widgetTracker;
    }

    KUiServerJobTracker *kuiserverTracker;
    KWidgetJobTracker *widgetTracker;
    QMap<KJob*, AllTrackers> trackers;
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
    if (!d->kuiserverTracker) {
        d->kuiserverTracker = new KUiServerJobTracker();
    }

    d->trackers[job].kuiserverTracker = d->kuiserverTracker;
    d->trackers[job].kuiserverTracker->registerJob(job);

    QDBusInterface interface("org.kde.kuiserver", "/JobViewServer", "",
    QDBusConnection::sessionBus(), this);
    QDBusReply<bool> reply = interface.call("requiresJobTracker");

    if (reply.isValid() && reply.value()) {
        //create a widget tracker in addition to kuiservertracker.
        if (!d->widgetTracker) {
            d->widgetTracker = new KWidgetJobTracker();
        }
        d->trackers[job].widgetTracker = d->widgetTracker;
        d->trackers[job].widgetTracker->registerJob(job);
    }

    Q_ASSERT(d->trackers[job].kuiserverTracker || d->trackers[job].widgetTracker);
}

void KDynamicJobTracker::unregisterJob(KJob *job)
{
    KUiServerJobTracker *kuiserverTracker = d->trackers[job].kuiserverTracker;
    KWidgetJobTracker *widgetTracker = d->trackers[job].widgetTracker;

    if (!(widgetTracker || kuiserverTracker)) {
        qWarning() << "Tried to unregister a kio job that hasn't been registered.";
        return;
    }

    if(kuiserverTracker)
        kuiserverTracker->unregisterJob(job);

    if(widgetTracker)
        widgetTracker->unregisterJob(job);
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
