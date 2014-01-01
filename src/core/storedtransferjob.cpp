/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                  2000-2009 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "storedtransferjob.h"
#include "job_p.h"
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kurlauthorized.h>
#include <QTimer>

using namespace KIO;

class KIO::StoredTransferJobPrivate: public TransferJobPrivate
{
public:
    StoredTransferJobPrivate(const QUrl &url, int command,
                             const QByteArray &packedArgs,
                             const QByteArray &_staticData)
        : TransferJobPrivate(url, command, packedArgs, _staticData),
          m_uploadOffset(0)
    {}
    StoredTransferJobPrivate(const QUrl &url, int command,
                             const QByteArray &packedArgs,
                             QIODevice *ioDevice)
        : TransferJobPrivate(url, command, packedArgs, ioDevice),
          m_uploadOffset(0)
    {}

    QByteArray m_data;
    int m_uploadOffset;

    void slotStoredData(KIO::Job *job, const QByteArray &data);
    void slotStoredDataReq(KIO::Job *job, QByteArray &data);

    Q_DECLARE_PUBLIC(StoredTransferJob)

    static inline StoredTransferJob *newJob(const QUrl &url, int command,
                                            const QByteArray &packedArgs,
                                            const QByteArray &staticData, JobFlags flags)
    {
        StoredTransferJob *job = new StoredTransferJob(
            *new StoredTransferJobPrivate(url, command, packedArgs, staticData));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        return job;
    }

    static inline StoredTransferJob *newJob(const QUrl &url, int command,
                                            const QByteArray &packedArgs,
                                            QIODevice *ioDevice, JobFlags flags)
    {
        StoredTransferJob *job = new StoredTransferJob(
            *new StoredTransferJobPrivate(url, command, packedArgs, ioDevice));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        return job;
    }
};

StoredTransferJob::StoredTransferJob(StoredTransferJobPrivate &dd)
    : TransferJob(dd)
{
    connect(this, SIGNAL(data(KIO::Job*,QByteArray)),
            SLOT(slotStoredData(KIO::Job*,QByteArray)));
    connect(this, SIGNAL(dataReq(KIO::Job*,QByteArray&)),
            SLOT(slotStoredDataReq(KIO::Job*,QByteArray&)));
}

StoredTransferJob::~StoredTransferJob()
{
}

void StoredTransferJob::setData(const QByteArray &arr)
{
    Q_D(StoredTransferJob);
    Q_ASSERT(d->m_data.isNull());   // check that we're only called once
    Q_ASSERT(d->m_uploadOffset == 0);   // no upload started yet
    d->m_data = arr;
    setTotalSize(d->m_data.size());
}

QByteArray StoredTransferJob::data() const
{
    return d_func()->m_data;
}

void StoredTransferJobPrivate::slotStoredData(KIO::Job *, const QByteArray &data)
{
    // check for end-of-data marker:
    if (data.size() == 0) {
        return;
    }
    unsigned int oldSize = m_data.size();
    m_data.resize(oldSize + data.size());
    memcpy(m_data.data() + oldSize, data.data(), data.size());
}

void StoredTransferJobPrivate::slotStoredDataReq(KIO::Job *, QByteArray &data)
{
    // Inspired from kmail's KMKernel::byteArrayToRemoteFile
    // send the data in 64 KB chunks
    const int MAX_CHUNK_SIZE = 64 * 1024;
    int remainingBytes = m_data.size() - m_uploadOffset;
    if (remainingBytes > MAX_CHUNK_SIZE) {
        // send MAX_CHUNK_SIZE bytes to the receiver (deep copy)
        data = QByteArray(m_data.data() + m_uploadOffset, MAX_CHUNK_SIZE);
        m_uploadOffset += MAX_CHUNK_SIZE;
        //qDebug() << "Sending " << MAX_CHUNK_SIZE << " bytes ("
        //                << remainingBytes - MAX_CHUNK_SIZE << " bytes remain)\n";
    } else {
        // send the remaining bytes to the receiver (deep copy)
        data = QByteArray(m_data.data() + m_uploadOffset, remainingBytes);
        m_data = QByteArray();
        m_uploadOffset = 0;
        //qDebug() << "Sending " << remainingBytes << " bytes\n";
    }
}

StoredTransferJob *KIO::storedGet(const QUrl &url, LoadType reload, JobFlags flags)
{
    // Send decoded path and encoded query
    KIO_ARGS << url;
    StoredTransferJob *job = StoredTransferJobPrivate::newJob(url, CMD_GET, packedArgs, QByteArray(), flags);
    if (reload == Reload) {
        job->addMetaData("cache", "reload");
    }
    return job;
}

StoredTransferJob *KIO::storedPut(const QByteArray &arr, const QUrl &url, int permissions,
                                  JobFlags flags)
{
    KIO_ARGS << url << qint8((flags & Overwrite) ? 1 : 0) << qint8((flags & Resume) ? 1 : 0) << permissions;
    StoredTransferJob *job = StoredTransferJobPrivate::newJob(url, CMD_PUT, packedArgs, QByteArray(), flags);
    job->setData(arr);
    return job;
}

namespace KIO
{
class PostErrorJob : public StoredTransferJob
{
public:

    PostErrorJob(int _error, const QString &url, const QByteArray &packedArgs, const QByteArray &postData)
        : StoredTransferJob(*new StoredTransferJobPrivate(QUrl(), CMD_SPECIAL, packedArgs, postData))
    {
        setError(_error);
        setErrorText(url);
    }

    PostErrorJob(int _error, const QString &url, const QByteArray &packedArgs, QIODevice *ioDevice)
        : StoredTransferJob(*new StoredTransferJobPrivate(QUrl(), CMD_SPECIAL, packedArgs, ioDevice))
    {
        setError(_error);
        setErrorText(url);
    }
};
}

static int isUrlPortBad(const QUrl &url)
{
    int _error = 0;

    // filter out some malicious ports
    static const int bad_ports[] = {
        1,   // tcpmux
        7,   // echo
        9,   // discard
        11,   // systat
        13,   // daytime
        15,   // netstat
        17,   // qotd
        19,   // chargen
        20,   // ftp-data
        21,   // ftp-cntl
        22,   // ssh
        23,   // telnet
        25,   // smtp
        37,   // time
        42,   // name
        43,   // nicname
        53,   // domain
        77,   // priv-rjs
        79,   // finger
        87,   // ttylink
        95,   // supdup
        101,  // hostriame
        102,  // iso-tsap
        103,  // gppitnp
        104,  // acr-nema
        109,  // pop2
        110,  // pop3
        111,  // sunrpc
        113,  // auth
        115,  // sftp
        117,  // uucp-path
        119,  // nntp
        123,  // NTP
        135,  // loc-srv / epmap
        139,  // netbios
        143,  // imap2
        179,  // BGP
        389,  // ldap
        512,  // print / exec
        513,  // login
        514,  // shell
        515,  // printer
        526,  // tempo
        530,  // courier
        531,  // Chat
        532,  // netnews
        540,  // uucp
        556,  // remotefs
        587,  // sendmail
        601,  //
        989,  // ftps data
        990,  // ftps
        992,  // telnets
        993,  // imap/SSL
        995,  // pop3/SSL
        1080, // SOCKS
        2049, // nfs
        4045, // lockd
        6000, // x11
        6667, // irc
        0
    };
    if (url.port() != 80) {
        const int port = url.port();
        for (int cnt = 0; bad_ports[cnt] && bad_ports[cnt] <= port; ++cnt)
            if (port == bad_ports[cnt]) {
                _error = KIO::ERR_POST_DENIED;
                break;
            }
    }

    if (_error) {
        static bool override_loaded = false;
        static QList< int > *overriden_ports = NULL;
        if (!override_loaded) {
            KConfig cfg("kio_httprc");
            overriden_ports = new QList< int >;
            *overriden_ports = cfg.group(QString()).readEntry("OverriddenPorts", QList<int>());
            override_loaded = true;
        }
        for (QList< int >::ConstIterator it = overriden_ports->constBegin();
                it != overriden_ports->constEnd();
                ++it) {
            if (overriden_ports->contains(url.port())) {
                _error = 0;
            }
        }
    }

    // filter out non https? protocols
    if ((url.scheme() != "http") && (url.scheme() != "https")) {
        _error = KIO::ERR_POST_DENIED;
    }

    if (!_error && !KUrlAuthorized::authorizeUrlAction("open", QUrl(), url)) {
        _error = KIO::ERR_ACCESS_DENIED;
    }

    return _error;
}

static KIO::PostErrorJob *precheckHttpPost(const QUrl &url, QIODevice *ioDevice, JobFlags flags)
{
    // if request is not valid, return an invalid transfer job
    const int _error = isUrlPortBad(url);

    if (_error) {
        KIO_ARGS << (int)1 << url;
        PostErrorJob *job = new PostErrorJob(_error, url.toString(), packedArgs, ioDevice);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        return job;
    }

    // all is ok, return 0
    return 0;
}

static KIO::PostErrorJob *precheckHttpPost(const QUrl &url, const QByteArray &postData, JobFlags flags)
{
    // if request is not valid, return an invalid transfer job
    const int _error = isUrlPortBad(url);

    if (_error) {
        KIO_ARGS << (int)1 << url;
        PostErrorJob *job = new PostErrorJob(_error, url.toString(), packedArgs, postData);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        return job;
    }

    // all is ok, return 0
    return 0;
}

TransferJob *KIO::http_post(const QUrl &url, const QByteArray &postData, JobFlags flags)
{
    bool redirection = false;
    QUrl _url(url);
    if (_url.path().isEmpty()) {
        redirection = true;
        _url.setPath("/");
    }

    TransferJob *job = precheckHttpPost(_url, postData, flags);
    if (job) {
        return job;
    }

    // Send http post command (1), decoded path and encoded query
    KIO_ARGS << (int)1 << _url << static_cast<qint64>(postData.size());
    job = TransferJobPrivate::newJob(_url, CMD_SPECIAL, packedArgs, postData, flags);

    if (redirection) {
        QTimer::singleShot(0, job, SLOT(slotPostRedirection()));
    }

    return job;
}

TransferJob *KIO::http_post(const QUrl &url, QIODevice *ioDevice, qint64 size, JobFlags flags)
{
    bool redirection = false;
    QUrl _url(url);
    if (_url.path().isEmpty()) {
        redirection = true;
        _url.setPath("/");
    }

    TransferJob *job = precheckHttpPost(_url, ioDevice, flags);
    if (job) {
        return job;
    }

    // If no size is specified and the QIODevice is not a sequential one,
    // attempt to obtain the size information from it.
    Q_ASSERT(ioDevice);
    if (size < 0) {
        size = ((ioDevice && !ioDevice->isSequential()) ? ioDevice->size() : -1);
    }

    // Send http post command (1), decoded path and encoded query
    KIO_ARGS << (int)1 << _url << size;
    job = TransferJobPrivate::newJob(_url, CMD_SPECIAL, packedArgs, ioDevice, flags);

    if (redirection) {
        QTimer::singleShot(0, job, SLOT(slotPostRedirection()));
    }

    return job;
}

TransferJob *KIO::http_delete(const QUrl &url, JobFlags flags)
{
    // Send decoded path and encoded query
    KIO_ARGS << url;
    TransferJob *job = TransferJobPrivate::newJob(url, CMD_DEL, packedArgs,
                       QByteArray(), flags);
    return job;
}

StoredTransferJob *KIO::storedHttpPost(const QByteArray &postData, const QUrl &url, JobFlags flags)
{
    QUrl _url(url);
    if (_url.path().isEmpty()) {
        _url.setPath("/");
    }

    StoredTransferJob *job = precheckHttpPost(_url, postData, flags);
    if (job) {
        return job;
    }

    // Send http post command (1), decoded path and encoded query
    KIO_ARGS << (int)1 << _url << static_cast<qint64>(postData.size());
    job = StoredTransferJobPrivate::newJob(_url, CMD_SPECIAL, packedArgs, postData, flags);
    return job;
}

StoredTransferJob *KIO::storedHttpPost(QIODevice *ioDevice, const QUrl &url, qint64 size, JobFlags flags)
{
    QUrl _url(url);
    if (_url.path().isEmpty()) {
        _url.setPath("/");
    }

    StoredTransferJob *job = precheckHttpPost(_url, ioDevice, flags);
    if (job) {
        return job;
    }

    // If no size is specified and the QIODevice is not a sequential one,
    // attempt to obtain the size information from it.
    Q_ASSERT(ioDevice);
    if (size < 0) {
        size = ((ioDevice && !ioDevice->isSequential()) ? ioDevice->size() : -1);
    }

    // Send http post command (1), decoded path and encoded query
    KIO_ARGS << (int)1 << _url << size;
    job = StoredTransferJobPrivate::newJob(_url, CMD_SPECIAL, packedArgs, ioDevice, flags);
    return job;
}

// http post got redirected from http://host to http://host/ by TransferJob
// We must do this redirection ourselves because redirections by the
// slave change post jobs into get jobs.
void TransferJobPrivate::slotPostRedirection()
{
    Q_Q(TransferJob);
    //qDebug() << m_url;
    // Tell the user about the new url.
    emit q->redirection(q, m_url);
}

TransferJob *KIO::put(const QUrl &url, int permissions, JobFlags flags)
{
    KIO_ARGS << url << qint8((flags & Overwrite) ? 1 : 0) << qint8((flags & Resume) ? 1 : 0) << permissions;
    return TransferJobPrivate::newJob(url, CMD_PUT, packedArgs, QByteArray(), flags);
}

#include "moc_storedtransferjob.cpp"