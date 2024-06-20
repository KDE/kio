#include "standardthumbnailjob.h"
#include <QImage>
#include <QProcess>
#include <QSaveFile>

KIO::StandardThumbnailJob::StandardThumbnailJob(QString processName, QStringList processArgs, QString path)
{
    m_processName = processName;
    m_processArgs = processArgs;
}

void KIO::StandardThumbnailJob::start()
{
    QProcess proc(this);

    connect(&proc, &QProcess::finished, this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitCode != 0) {
            qWarning() << "thumbnail process failed " << exitStatus;
            return;
        }
    });
    proc.start(m_processName, m_processArgs);
}
