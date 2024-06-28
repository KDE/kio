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
