/* This file is part of the KDE libraries
   Copyright (C) 2012 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
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

