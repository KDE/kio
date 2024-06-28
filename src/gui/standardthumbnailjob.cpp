#include "standardthumbnailjob.h"
#include <KMacroExpander>
#include <QImage>
#include <QProcess>
#include <QSaveFile>

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
    void subst(int option, QStringList &ret);
};

void ThumbnailerExpander::subst(int option, QStringList &ret)
{
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
    }
    return;
}

int ThumbnailerExpander::expandEscapedMacro(const QString &str, int pos, QStringList &ret)
{
    uint option = str[pos + 1].unicode();
    switch (option) {
    case 's':
        subst(option, ret);
        break;
    case 'i':
    case 'u':
        subst(option, ret);
        break;
    case 'o':
        subst(option, ret);
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
    connect(proc, &QProcess::errorOccurred, this, [=, this](QProcess::ProcessError error) {
        qWarning() << "Standard Thumbnail Job had an error:" << error;
        proc->deleteLater();
        emitResult();
    });
    proc->start(thumbnailer.binary(), thumbnailer.args());
}
