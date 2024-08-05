/*
    SPDX-FileCopyrightText: 2024 Akseli Lahtinen <akselmo@akselmo.dev>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "global.h"
#include "standardthumbnailjob_p.h"
#include <KMacroExpander>
#include <QImage>
#include <QProcess>

class ThumbnailerExpander : public KMacroExpanderBase
{
public:
    explicit ThumbnailerExpander(const QString execString, const int width, const QString inputFile, const QString outputFile)
        : KMacroExpanderBase(QLatin1Char('%'))
        , m_width(width)
        , m_execString(execString)
        , m_inputFile(inputFile)
        , m_outputFile(outputFile)
    {
        QString newString(execString);
        expandMacros(newString);
        auto fullCmd = QProcess::splitCommand(newString);
        m_binary = fullCmd.first();
        fullCmd.removeFirst();
        m_args = fullCmd;
    }

    QString binary()
    {
        return m_binary;
    }

    QStringList args()
    {
        return m_args;
    }

private:
    int expandEscapedMacro(const QString &str, int pos, QStringList &ret) override;
    const int m_width;
    const QString m_execString;
    const QString m_inputFile;
    const QString m_outputFile;
    QString m_binary;
    QStringList m_args;
};

int ThumbnailerExpander::expandEscapedMacro(const QString &str, int pos, QStringList &ret)
{
    uint option = str[pos + 1].unicode();
    switch (option) {
    case 's':
        ret << QString::number(m_width);
        break;
    case 'i':
    case 'u':
        ret << QStringLiteral(R"("%1")").arg(m_inputFile);
        break;
    case 'o':
        ret << QStringLiteral(R"("%1")").arg(m_outputFile);
        break;
    case '%':
        ret = QStringList(QStringLiteral("%"));
        break;
    default:
        return -2; // subst with same and skip
    }
    return 2;
}

KIO::StandardThumbnailJob::StandardThumbnailJob(const QString execString, const int width, const QString inputFile, const QString outputFile)
{
    // Prepare the command
    ThumbnailerExpander thumbnailer(execString, width, inputFile, outputFile);
    // Emit data on command exit
    QProcess *proc = new QProcess();
    connect(proc, &QProcess::finished, this, [=, this](const int exitCode, const QProcess::ExitStatus exitStatus) {
        proc->deleteLater();
        if (exitCode != 0) {
            setErrorText(QStringLiteral("Standard Thumbnail Job failed with exit code: %1 ").arg(exitCode));
            setError(KIO::ERR_CANNOT_LAUNCH_PROCESS);
            emitResult();
            return;
        }
        Q_EMIT data(this, QImage(outputFile));
        emitResult();
    });
    connect(proc, &QProcess::errorOccurred, this, [=, this](const QProcess::ProcessError error) {
        proc->deleteLater();
        setErrorText(QStringLiteral("Standard Thumbnail Job had an error: %1").arg(error));
        setError(KIO::ERR_CANNOT_LAUNCH_PROCESS);
        emitResult();
    });
    proc->start(thumbnailer.binary(), thumbnailer.args());
}
