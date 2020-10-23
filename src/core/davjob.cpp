// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2002 Jan-Pascal van Best <janpascal@vanbest.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "davjob.h"

#include <QDataStream>

#include "httpmethod_p.h"

#include "jobclasses.h"
#include "job.h"
#include "job_p.h"

using namespace KIO;

/** @internal */
class KIO::DavJobPrivate: public KIO::TransferJobPrivate
{
public:
    explicit DavJobPrivate(const QUrl &url)
        : TransferJobPrivate(url, KIO::CMD_SPECIAL, QByteArray(), QByteArray())
    {}
    QByteArray savedStaticData;
    QByteArray str_response;
    QDomDocument m_response;
    //TransferJob *m_subJob;
    //bool m_suspended;

    Q_DECLARE_PUBLIC(DavJob)

    static inline DavJob *newJob(const QUrl &url, int method, const QString &request,
                                 JobFlags flags)
    {
        DavJob *job = new DavJob(*new DavJobPrivate(url), method, request);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        return job;
    }
};

DavJob::DavJob(DavJobPrivate &dd, int method, const QString &request)
    : TransferJob(dd)
{
    // We couldn't set the args when calling the parent constructor,
    // so do it now.
    Q_D(DavJob);
    QDataStream stream(&d->m_packedArgs, QIODevice::WriteOnly);
    stream << (int) 7 << d->m_url << method;
    // Same for static data
    if (! request.isEmpty()) {
        d->staticData = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n" + request.toUtf8();
        d->staticData.chop(1);
        d->savedStaticData = d->staticData;
        stream << static_cast<qint64>(d->staticData.size());
    } else {
        stream << static_cast<qint64>(-1);
    }
}

QDomDocument &DavJob::response()
{
    return d_func()->m_response;
}

void DavJob::slotData(const QByteArray &data)
{
    Q_D(DavJob);
    if (d->m_redirectionURL.isEmpty() || !d->m_redirectionURL.isValid() || error()) {
        unsigned int oldSize = d->str_response.size();
        d->str_response.resize(oldSize + data.size());
        memcpy(d->str_response.data() + oldSize, data.data(), data.size());
    }
}

void DavJob::slotFinished()
{
    Q_D(DavJob);
    //qDebug() << d->str_response;
    if (!d->m_redirectionURL.isEmpty() && d->m_redirectionURL.isValid() &&
            (d->m_command == CMD_SPECIAL)) {
        QDataStream istream(d->m_packedArgs);
        int s_cmd, s_method;
        qint64 s_size;
        QUrl s_url;
        istream >> s_cmd;
        istream >> s_url;
        istream >> s_method;
        istream >> s_size;
        // PROPFIND
        if ((s_cmd == 7) && (s_method == (int)KIO::DAV_PROPFIND)) {
            d->m_packedArgs.truncate(0);
            QDataStream stream(&d->m_packedArgs, QIODevice::WriteOnly);
            stream << (int)7 << d->m_redirectionURL << (int)KIO::DAV_PROPFIND << s_size;
        }
    } else if (! d->m_response.setContent(d->str_response, true)) {
        // An error occurred parsing the XML response
        QDomElement root = d->m_response.createElementNS(QStringLiteral("DAV:"), QStringLiteral("error-report"));
        d->m_response.appendChild(root);

        QDomElement el = d->m_response.createElementNS(QStringLiteral("DAV:"), QStringLiteral("offending-response"));
        QDomText textnode = d->m_response.createTextNode(QString::fromUtf8(d->str_response));
        el.appendChild(textnode);
        root.appendChild(el);
    }
    //qDebug() << d->m_response.toString();
    TransferJob::slotFinished();
    d->staticData = d->savedStaticData; // Need to send DAV request to this host too
}

/* Convenience methods */

DavJob *KIO::davPropFind(const QUrl &url, const QDomDocument &properties, const QString &depth, JobFlags flags)
{
    DavJob *job = DavJobPrivate::newJob(url, (int) KIO::DAV_PROPFIND, properties.toString(), flags);
    job->addMetaData(QStringLiteral("davDepth"), depth);
    return job;
}

DavJob *KIO::davPropPatch(const QUrl &url, const QDomDocument &properties, JobFlags flags)
{
    return DavJobPrivate::newJob(url, (int) KIO::DAV_PROPPATCH, properties.toString(),
                                 flags);
}

DavJob *KIO::davSearch(const QUrl &url, const QString &nsURI, const QString &qName, const QString &query, JobFlags flags)
{
    QDomDocument doc;
    QDomElement searchrequest = doc.createElementNS(QStringLiteral("DAV:"), QStringLiteral("searchrequest"));
    QDomElement searchelement = doc.createElementNS(nsURI, qName);
    QDomText text = doc.createTextNode(query);
    searchelement.appendChild(text);
    searchrequest.appendChild(searchelement);
    doc.appendChild(searchrequest);
    return DavJobPrivate::newJob(url, KIO::DAV_SEARCH, doc.toString(), flags);
}

DavJob *KIO::davReport(const QUrl &url, const QString &report, const QString &depth, JobFlags flags)
{
    DavJob *job = DavJobPrivate::newJob(url, (int) KIO::DAV_REPORT, report, flags);
    job->addMetaData(QStringLiteral("davDepth"), depth);
    return job;
}

