/*
    SPDX-FileCopyrightText: 2024 Akseli Lahtinen <akselmo@akselmo.dev>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#pragma once

#include "kiogui_export.h"
#include <kio/job.h>

namespace KIO
{

class KIOGUI_EXPORT StandardThumbnailJob : public KIO::Job
{
    Q_OBJECT

public:
    StandardThumbnailJob(const QString execString, const int width, const QString inputFile, const QString outputFile);
Q_SIGNALS:
    void data(KIO::Job *job, const QImage &thumb);
};

}
