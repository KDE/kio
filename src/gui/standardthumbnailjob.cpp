/*
    SPDX-FileCopyrightText: 2024 Akseli Lahtinen <akselmo@akselmo.dev>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "global.h"
#include "kiogui_debug.h"
#include "standardthumbnailjob_p.h"
#include <KMacroExpander>
#include <KSandbox>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

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
        // The Exec value is split into arguments before the field codes are replaced, so each
        // file path stays one argument whatever characters it holds.
        QStringList fullCmd = QProcess::splitCommand(execString);
        for (QString &arg : fullCmd) {
            expandMacros(arg);
        }
        if (!fullCmd.isEmpty()) {
            m_binary = fullCmd.takeFirst();
        }
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
        ret << m_inputFile;
        break;
    case 'o':
        ret << m_outputFile;
        break;
    case '%':
        ret = QStringList(QStringLiteral("%"));
        break;
    default:
        return -2; // subst with same and skip
    }
    return 2;
}

// Paths that the thumbnailer sees inside the bwrap sandbox
static const QString s_sandboxInput = QStringLiteral("/tmp/input");
static const QString s_sandboxOutputDir = QStringLiteral("/tmp/output");
static const QString s_outputName = QStringLiteral("thumbnail.png");

// The system files a dynamically linked thumbnailer needs, read-only, and nothing of the user.
static QStringList sandboxSystemArguments()
{
    QStringList args{QStringLiteral("--ro-bind"), QStringLiteral("/usr"), QStringLiteral("/usr")};
    for (const auto dir : {"/bin", "/sbin", "/lib", "/lib32", "/lib64"}) {
        const QFileInfo info(QString::fromLatin1(dir));
        if (info.isSymLink()) {
            args << QStringLiteral("--symlink") << info.symLinkTarget() << info.filePath();
        } else if (info.isDir()) {
            args << QStringLiteral("--ro-bind") << info.filePath() << info.filePath();
        }
    }
    for (const auto path : {"/etc/ld.so.cache", "/etc/alternatives", "/etc/fonts"}) {
        args << QStringLiteral("--ro-bind-try") << QString::fromLatin1(path) << QString::fromLatin1(path);
    }
    args << QStringLiteral("--proc") << QStringLiteral("/proc") << QStringLiteral("--dev") << QStringLiteral("/dev") << QStringLiteral("--tmpfs")
         << QStringLiteral("/tmp") << QStringLiteral("--unshare-all") << QStringLiteral("--die-with-parent") << QStringLiteral("--new-session")
         << QStringLiteral("--clearenv") << QStringLiteral("--setenv") << QStringLiteral("PATH") << QStringLiteral("/usr/bin:/bin")
         << QStringLiteral("--setenv") << QStringLiteral("HOME") << QStringLiteral("/tmp");
    const QString lang = qEnvironmentVariable("LANG");
    if (!lang.isEmpty()) {
        args << QStringLiteral("--setenv") << QStringLiteral("LANG") << lang;
    }
    args << QStringLiteral("--chdir") << QStringLiteral("/");
    return args;
}

// bwrap, if it is installed and can create namespaces here. Checked once per process.
static QString bwrapExecutable()
{
    static const QString bwrap = [] {
        if (KSandbox::isInside()) {
            return QString();
        }
        const QString path = QStandardPaths::findExecutable(QStringLiteral("bwrap"));
        const QString truePath = QStandardPaths::findExecutable(QStringLiteral("true"));
        if (path.isEmpty() || truePath.isEmpty()) {
            qCWarning(KIO_GUI) << "bwrap was not found, standard thumbnailers run without a sandbox";
            return QString();
        }
        QProcess probe;
        probe.start(path, sandboxSystemArguments() << QStringLiteral("--") << truePath);
        if (!probe.waitForFinished(5000) || probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
            qCWarning(KIO_GUI) << "bwrap cannot create a sandbox here, standard thumbnailers run without a sandbox:" << probe.readAllStandardError();
            probe.kill();
            return QString();
        }
        return path;
    }();
    return bwrap;
}

bool KIO::StandardThumbnailJob::isSandboxAvailable()
{
    return !bwrapExecutable().isEmpty();
}

class Q_DECL_HIDDEN KIO::StandardThumbnailJob::Private
{
public:
    explicit Private(const QString &execString, int logicalWidth, qreal devicePixelRatio, const QString &inputFile, const QString &outputFolder)
        : m_execString(execString)
        , m_logicalWidth(logicalWidth)
        , m_devicePixelRatio(devicePixelRatio)
        , m_inputFile(inputFile)
        , m_outputFolder(outputFolder)
    {
    }

    QString m_execString;
    int m_logicalWidth;
    qreal m_devicePixelRatio;
    QString m_inputFile;
    QString m_outputFolder;
    QProcess *m_proc = nullptr;
    // The thumbnailer writes into a folder of its own, which is all it can write to in the sandbox.
    std::unique_ptr<QTemporaryDir> m_outputDir;
};

KIO::StandardThumbnailJob::StandardThumbnailJob(const QString &execString,
                                                int logicalWidth,
                                                qreal devicePixelRatio,
                                                const QString &inputFile,
                                                const QString &outputFolder)
    : d(new Private(execString, logicalWidth, devicePixelRatio, inputFile, outputFolder))
{
    setAutoDelete(true);
}

KIO::StandardThumbnailJob::~StandardThumbnailJob() = default;

bool KIO::StandardThumbnailJob::StandardThumbnailJob::doKill()
{
    if (d->m_proc) {
        d->m_proc->kill();
    }
    return true;
}

void KIO::StandardThumbnailJob::StandardThumbnailJob::start()
{
    d->m_outputDir = std::make_unique<QTemporaryDir>(QStringLiteral("%1/XXXXXX").arg(d->m_outputFolder));
    if (!d->m_outputDir->isValid()) {
        setErrorText(QStringLiteral("Standard Thumbnail Job had an error: could not create a temporary folder"));
        setError(KIO::ERR_CANNOT_OPEN_FOR_WRITING);
        emitResult();
        return;
    }
    const QString outputFile = d->m_outputDir->filePath(s_outputName);

    const QString bwrap = bwrapExecutable();
    QString program;
    QStringList args;
    if (bwrap.isEmpty()) {
        ThumbnailerExpander thumbnailer(d->m_execString, d->m_logicalWidth * d->m_devicePixelRatio, d->m_inputFile, outputFile);
        program = thumbnailer.binary();
        args = thumbnailer.args();
    } else {
        // Some thumbnailers look at the extension of the file name
        const QString suffix = QFileInfo(d->m_inputFile).suffix();
        const QString sandboxInput = suffix.isEmpty() ? s_sandboxInput : s_sandboxInput + QLatin1Char('.') + suffix;
        ThumbnailerExpander thumbnailer(d->m_execString,
                                        d->m_logicalWidth * d->m_devicePixelRatio,
                                        sandboxInput,
                                        s_sandboxOutputDir + QLatin1Char('/') + s_outputName);
        const QString binary = QStandardPaths::findExecutable(thumbnailer.binary());
        if (binary.isEmpty()) {
            setErrorText(QStringLiteral("Standard Thumbnail Job had an error: could not find %1").arg(thumbnailer.binary()));
            setError(KIO::ERR_CANNOT_LAUNCH_PROCESS);
            emitResult();
            return;
        }
        program = bwrap;
        args = sandboxSystemArguments();
        if (!binary.startsWith(QLatin1String("/usr/"))) {
            args << QStringLiteral("--ro-bind") << binary << binary;
        }
        args << QStringLiteral("--ro-bind") << d->m_inputFile << sandboxInput;
        args << QStringLiteral("--bind") << d->m_outputDir->path() << s_sandboxOutputDir;
        args << QStringLiteral("--") << binary << thumbnailer.args();
    }

    // Emit data on command exit
    d->m_proc = new QProcess();
    connect(d->m_proc, &QProcess::finished, this, [=, this](const int exitCode, const QProcess::ExitStatus /* exitStatus */) {
        d->m_proc->deleteLater();
        d->m_proc = nullptr;
        const QFileInfo output(outputFile);
        if (exitCode != 0 || !output.isFile() || output.isSymLink()) {
            setErrorText(QStringLiteral("Standard Thumbnail Job failed with exit code: %1 ").arg(exitCode));
            setError(KIO::ERR_CANNOT_LAUNCH_PROCESS);
            emitResult();
            return;
        }
        QImage thumb(outputFile);
        thumb.setDevicePixelRatio(supportedDevicePixelRatio(thumb.size(), QSize(d->m_logicalWidth, d->m_logicalWidth), d->m_devicePixelRatio));
        Q_EMIT data(this, thumb);
        emitResult();
    });
    connect(d->m_proc, &QProcess::errorOccurred, this, [=, this](const QProcess::ProcessError error) {
        d->m_proc->deleteLater();
        d->m_proc = nullptr;
        setErrorText(QStringLiteral("Standard Thumbnail Job had an error: %1").arg(error));
        setError(KIO::ERR_CANNOT_LAUNCH_PROCESS);
        emitResult();
    });
    d->m_proc->start(program, args);
}

#include "moc_standardthumbnailjob_p.cpp"
