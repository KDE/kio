/*
    SPDX-FileCopyrightText: 2024 Akseli Lahtinen <akselmo@akselmo.dev>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "global.h"
#include "standardthumbnailjob_p.h"
#include <KMacroExpander>
#include <QImage>
#include <QProcess>
#include <QTemporaryFile>

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

class Q_DECL_HIDDEN KIO::StandardThumbnailJob::Private
{
public:
    explicit Private(const QString &execString, int width, const QString &inputFile, const QString &outputFolder)
        : m_execString(execString)
        , m_width(width)
        , m_inputFile(inputFile)
        , m_outputFolder(outputFolder)
    {
    }
    ~Private()
    {
    }

    QString m_execString;
    int m_width;
    QString m_inputFile;
    QString m_outputFolder;
    QProcess *m_proc;
    QTemporaryFile *m_tempFile;
};

KIO::StandardThumbnailJob::StandardThumbnailJob(const QString &execString, int width, const QString &inputFile, const QString &outputFolder)
    : d(new Private(execString, width, inputFile, outputFolder))
{
    setAutoDelete(true);
}

KIO::StandardThumbnailJob::~StandardThumbnailJob() = default;

bool KIO::StandardThumbnailJob::StandardThumbnailJob::doKill()
{
    d->m_proc->kill();
    return true;
}

void KIO::StandardThumbnailJob::StandardThumbnailJob::start()
{
    // Prepare the command
    d->m_tempFile = new QTemporaryFile(QStringLiteral("%1/XXXXXX.png").arg(d->m_outputFolder));
    if (!d->m_tempFile->open()) {
        setErrorText(QStringLiteral("Standard Thumbnail Job had an error: could not open temporary file"));
        setError(KIO::ERR_CANNOT_OPEN_FOR_WRITING);
        emitResult();
    }
    d->m_tempFile->setAutoRemove(false);

    ThumbnailerExpander thumbnailer(d->m_execString, d->m_width, d->m_inputFile, d->m_tempFile->fileName());
    // Emit data on command exit
    d->m_proc = new QProcess();
    connect(d->m_proc, &QProcess::finished, this, [=, this](const int exitCode, const QProcess::ExitStatus /* exitStatus */) {
        d->m_proc->deleteLater();
        if (exitCode != 0) {
            setErrorText(QStringLiteral("Standard Thumbnail Job failed with exit code: %1 ").arg(exitCode));
            setError(KIO::ERR_CANNOT_LAUNCH_PROCESS);
            emitResult();

            // clean temp file
            d->m_tempFile->remove();
            return;
        }
        Q_EMIT data(this, QImage(d->m_tempFile->fileName()));
        emitResult();

        // clean temp file
        d->m_tempFile->remove();
    });
    connect(d->m_proc, &QProcess::errorOccurred, this, [=, this](const QProcess::ProcessError error) {
        d->m_proc->deleteLater();
        setErrorText(QStringLiteral("Standard Thumbnail Job had an error: %1").arg(error));
        setError(KIO::ERR_CANNOT_LAUNCH_PROCESS);
        emitResult();

        // clean temp file
        d->m_tempFile->remove();
    });
    d->m_proc->start(thumbnailer.binary(), thumbnailer.args());
}
