#include "standardthumbnailjob.h"
#include <QImage>
#include <QProcess>
#include <QSaveFile>

KIO::StandardThumbnailJob::StandardThumbnailJob(const QString execString, const int width, const QString inputFile, const QString outputFile)
{
    // Prepare the command
    QString runCmd(execString);
    auto inputPath = QStringLiteral("\"%1\"").arg(inputFile);
    runCmd.replace(QStringLiteral("%s"), QString::number(width));
    runCmd.replace(QStringLiteral("%i"), inputPath);
    runCmd.replace(QStringLiteral("%u"), inputPath);
    runCmd.replace(QStringLiteral("%o"), outputFile);
    auto args = QProcess::splitCommand(runCmd);
    if (args.isEmpty()) {
        return;
    }
    auto bin = args.first();
    args.removeFirst();
    // Emit data on command exit
    QProcess *proc = new QProcess();
    connect(proc, &QProcess::finished, this, [=, this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitCode != 0) {
            qWarning() << "Standard Thumbnail Job failed with ExitStatus: " << exitStatus << " - ExitCode: " << exitCode;
            emitResult();
            return;
        }
        Q_EMIT data(this, QImage(outputFile));
        proc->deleteLater();
        emitResult();
    });
    proc->start(bin, args);
}
