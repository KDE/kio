/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2023 Dave Vasilevsky <dave@vasilevsky.ca>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only
*/
#ifndef _KIO_GPUDETECTION_H_
#define _KIO_GPUDETECTION_H_

#include "kiogui_export.h"

class QProcessEnvironment;

namespace KIO
{

/**
 * Detects whether the system has a discrete GPU.
 */
KIOGUI_EXPORT bool hasDiscreteGpu();

/**
 * Environment variables that make a process run with the discrete GPU.
 */
KIOGUI_NO_EXPORT QProcessEnvironment discreteGpuEnvironment();
}

#endif
