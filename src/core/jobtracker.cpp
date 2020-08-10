/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2012 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "jobtracker.h"
#include <KJobTrackerInterface>

static KJobTrackerInterface *s_tracker = nullptr;
Q_GLOBAL_STATIC(KJobTrackerInterface, globalDummyTracker)

KJobTrackerInterface *KIO::getJobTracker()
{
    if (!s_tracker) {
        s_tracker = globalDummyTracker();    // don't return nullptr, caller doesn't expect that
    }
    return s_tracker;
}

void KIO::setJobTracker(KJobTrackerInterface *tracker)
{
    s_tracker = tracker;
}
