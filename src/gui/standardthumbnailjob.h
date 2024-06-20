#pragma once

#include "kiogui_export.h"
#include <kio/job.h>

namespace KIO
{

class KIOGUI_EXPORT StandardThumbnailJob : public KIO::Job
{
    Q_OBJECT
    QString m_processName;
    QStringList m_processArgs;
    QString m_path;

public:
    StandardThumbnailJob(QString processName, QStringList processArgs, QString path);
Q_SIGNALS:
    void data(KIO::Job *job, const QImage &thumb);
};

}
