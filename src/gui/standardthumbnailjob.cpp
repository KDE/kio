#include "standardthumbnailjob.h"
#include <QImage>
#include <QProcess>
#include <QSaveFile>

KIO::StandardThumbnailJob::StandardThumbnailJob(QString processName, QStringList processArgs, QString path)
{
    qWarning() << "Created standard thumbnail jo";
    m_processName = processName;
    m_processArgs = processArgs;
    m_path = path;
    qWarning() << processName;
    qWarning() << processArgs;
    qWarning() << path;
    QProcess *proc = new QProcess();
    connect(proc, &QProcess::finished, this, [=, this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitCode != 0) {
            qWarning() << "thumbnail process failed " << exitStatus;
            return;
        }
        Q_EMIT data(this, QImage(m_path));
        proc->deleteLater();
        emitResult();
    });
    proc->start(m_processName, m_processArgs);
}


