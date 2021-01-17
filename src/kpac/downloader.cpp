/*
    SPDX-FileCopyrightText: 2003 Malte Starostik <malte@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "downloader.h"

#include <cstdlib>
#include <cstring>

#include <QTextCodec>

#include <KLocalizedString>
#include <kio/job.h>

namespace KPAC
{
Downloader::Downloader(QObject *parent)
    : QObject(parent)
{
}

void Downloader::download(const QUrl &url)
{
    m_data.resize(0);
    m_script.clear();
    m_scriptURL = url;

    KIO::TransferJob *job = KIO::get(url, KIO::NoReload, KIO::HideProgressInfo);
    connect(job, &KIO::TransferJob::data,
            this, &Downloader::data);
    connect(job, &KIO::TransferJob::redirection,
            this, &Downloader::redirection);
    connect(job, &KJob::result,
            this, QOverload<KJob*>::of(&Downloader::result));
}

void Downloader::failed()
{
    Q_EMIT result(false);
}

void Downloader::setError(const QString &error)
{
    m_error = error;
}

void Downloader::redirection(KIO::Job *, const QUrl &url)
{
    m_scriptURL = url;
}

void Downloader::data(KIO::Job *, const QByteArray &data)
{
    unsigned offset = m_data.size();
    m_data.resize(offset + data.size());
    std::memcpy(m_data.data() + offset, data.data(), data.size());
}

static bool hasErrorPage(KJob *job)
{
    KIO::TransferJob *tJob = qobject_cast<KIO::TransferJob *>(job);
    return (tJob && tJob->isErrorPage());
}

void Downloader::result(KJob *job)
{
    if (!job->error() && !hasErrorPage(job)) {
        const QString charset = static_cast<KIO::Job *>(job)->queryMetaData(QStringLiteral("charset"));
        QTextCodec *codec = QTextCodec::codecForName(charset.toLatin1());
        if (!codec) {
            codec = QTextCodec::codecForUtfText(m_data);
            Q_ASSERT(codec);
        }
        m_script = codec->toUnicode(m_data);
        Q_EMIT result(true);
    } else {
        if (job->error())
            setError(i18n("Could not download the proxy configuration script:\n%1",
                          job->errorString()));
        else {
            setError(i18n("Could not download the proxy configuration script"));    // error page
        }
        failed();
    }
}
}

