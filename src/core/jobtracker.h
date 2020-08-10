/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2012 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KIO_JOBTRACKER_H
#define KIO_JOBTRACKER_H

#include "kiocore_export.h"

class KJobTrackerInterface;

namespace KIO
{
/**
 * Returns the job tracker to be used by all KIO jobs (in which HideProgressInfo is not set)
 */
KIOCORE_EXPORT KJobTrackerInterface *getJobTracker();

/**
 * @internal
 * Allows the KIO widgets library to register its widget-based job tracker automatically.
 * @since 5.0
 */
KIOCORE_EXPORT void setJobTracker(KJobTrackerInterface *tracker);
}

#endif /* KIO_JOBTRACKER_H */

