/*
    SPDX-FileCopyrightText: 2008 Rob Scheepmaker <r.scheepmaker@student.utwente.nl>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef KDYNAMICJOBTRACKER_H
#define KDYNAMICJOBTRACKER_H

#include <KJobTrackerInterface>

#include <memory>

class KDynamicJobTrackerPrivate;

/*!
 * \internal
 *
 * This class implements a simple job tracker which registers any job to the KWidgetJobTracker if a
 * kuiserver isn't available on the DBus, or to the KUiServerJobTracker, if a kuiserver is
 * available. This way, we have the old dialogs as fallback when the user doesn't use a kuiserver
 * applet or application.
 */
class KDynamicJobTracker : public KJobTrackerInterface
{
    Q_OBJECT

public:
    /*!
     * Creates a new KDynamicJobTracker
     *
     * \a parent the parent of this object.
     */
    explicit KDynamicJobTracker(QObject *parent = nullptr);

    /*!
     * Destroys this KDynamicJobTracker
     */
    ~KDynamicJobTracker() override;

public Q_SLOTS:
    /*!
     * Register a new job in this tracker. This call will get forwarded to either KWidgetJobTracker
     * or KUiServerJobTracker, depending on the availability of the Kuiserver.
     *
     * \a job the job to register
     */
    void registerJob(KJob *job) override;

    /*!
     * Unregister a job from the tracker it was registered to.
     *
     * \a job the job to unregister
     */
    void unregisterJob(KJob *job) override;

private:
    std::unique_ptr<KDynamicJobTrackerPrivate> const d;
    Q_SLOT void handleRequiresJobTrackerChanged(bool);
};

#endif
