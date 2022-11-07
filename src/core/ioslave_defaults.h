/*
    SPDX-FileCopyrightText: 2022 Friedrich W. H. Kossebau <kossebau@kde.org>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <kiocore_export.h>

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 101)
#include "ioworker_defaults.h"
#if KIOCORE_DEPRECATED_WARNINGS_SINCE >= 0x056500
#pragma message("Deprecated header. Since 5.101, use #include <kio/ioworker_defaults.h> instead")
#endif
#else
#error "Include of deprecated header is disabled"
#endif
