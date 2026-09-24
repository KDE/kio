// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kiocore_export.h"

#ifdef BUILD_TESTING

#include <chrono>

namespace KIO
{

class TestPrivate
{
public:
    /*!
     * How long a running job waits between reports, 200ms by default. A test cannot wait that long
     * to see a job report more than once, and cannot make the job slower, so it shortens the wait.
     */
    KIOCORE_EXPORT static void setReportTimeout(std::chrono::milliseconds timeout);
};
}
#endif
