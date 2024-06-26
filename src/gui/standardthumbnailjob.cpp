#include "standardthumbnailjob.h"
#include <QImage>
#include <QProcess>
#include <QSaveFile>

KIO::StandardThumbnailJob::StandardThumbnailJob(QString processName, QStringList processArgs, QString path)
{
    m_processName = processName;
    m_processArgs = processArgs;
    m_path = path;
    QProcess *proc = new QProcess();
    connect(proc, &QProcess::finished, this, [=, this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitCode != 0) {
            qWarning() << "Standard Thumbnail Job failed with ExitStatus: " << exitStatus;
            return;
        }
        Q_EMIT data(this, QImage(m_path));
        proc->deleteLater();
        emitResult();
    });
    proc->start(m_processName, m_processArgs);
}
