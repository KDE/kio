// SPDX-License-Identifier: LGPL-2.0-only
// SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>
// SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>

#include <buildsycocajob.h>

#include <QProcess>
#include <QStandardPaths>

#include <KSycoca>

#include "buildsycocainterface.h"
#include "jobuidelegatefactory.h"
#include "kiocoredebug.h"

namespace KIO
{

class BuildSycocaJobPrivate
{
};

BuildSycocaJob::BuildSycocaJob(QObject *parent)
    : KJob(parent)
    , d(new BuildSycocaJobPrivate)
{
}

BuildSycocaJob::~BuildSycocaJob() = default;

void BuildSycocaJob::start()
{
    const QString exec = QStandardPaths::findExecutable(QStringLiteral(KBUILDSYCOCA_EXENAME));
    if (exec.isEmpty()) {
        qCWarning(KIO_CORE) << "Could not find kbuildsycoca executable:" << KBUILDSYCOCA_EXENAME;
        return;
    }
    auto proc = new QProcess(this);

    auto iface = KIO::delegateExtension<BuildSycocaInterface *>(this);
    if (iface) {
        connect(proc, &QProcess::finished, iface, &BuildSycocaInterface::hideProgress);
        connect(iface, &BuildSycocaInterface::canceled, proc, &QProcess::terminate);
        iface->showProgress();
    } else {
        qCDebug(KIO_CORE) << "No BuildSycocaInterface in UIDelegate.";
    }

    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError) {
        setError(KJob::UserDefinedError);
        setErrorText(proc->errorString());
        emitResult();
    });
    connect(proc, &QProcess::finished, this, [this](qint64 /* pid */) {
        emitResult();
    });

    proc->start(exec, QStringList());
}

BuildSycocaJob *buildSycoca(JobFlags flags)
{
    auto job = new KIO::BuildSycocaJob;
    if (!flags.testFlag(JobFlag::HideProgressInfo)) {
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
    }
    return job;
}

} // namespace KIO
