/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2002 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "httpfilter.h"
#include <KCompressionDevice>
#include <KFilterBase>
#include <KLocalizedString>
#include <QDebug>

#include <stdio.h>

Q_LOGGING_CATEGORY(KIO_HTTP_FILTER, "kf.kio.slaves.http.filter")

/*
Testcases:
 - http://david.fullrecall.com/browser-http-compression-test?compression=deflate-http (bug 160289)
 - http://demo.serv-u.com/?user=demo-WC&password=demo-WC  (Content-Encoding: deflate)  (bug 188935)
 - http://web.davidfaure.fr/cgi-bin/deflate_test (Content-Encoding: deflate) (bug 114830)
 - http://www.zlib.net/zlib_faq.html#faq39 (Content-Encoding: gzip)
 - wikipedia (Content-Encoding: gzip)
 - cnn.com (Content-Encoding: gzip)
 - http://arstechnica.com/ (Content-Encoding: gzip)
 - mailman admin interface on mail.kde.org (see r266769, but can't confirm these days)
*/

HTTPFilterBase::HTTPFilterBase()
    : last(nullptr)
{
}

HTTPFilterBase::~HTTPFilterBase()
{
    delete last;
}

void
HTTPFilterBase::chain(HTTPFilterBase *previous)
{
    last = previous;
    connect(last, &HTTPFilterBase::output,
            this, &HTTPFilterBase::slotInput);
}

HTTPFilterChain::HTTPFilterChain()
    : first(nullptr)
{
}

void
HTTPFilterChain::addFilter(HTTPFilterBase *filter)
{
    if (!last) {
        first = filter;
    } else {
        disconnect(last, &HTTPFilterBase::output, nullptr, nullptr);
        filter->chain(last);
    }
    last = filter;
    connect(filter, &HTTPFilterBase::output,
            this, &HTTPFilterBase::output);
    connect(filter, &HTTPFilterBase::error,
            this, &HTTPFilterBase::error);
}

void
HTTPFilterChain::slotInput(const QByteArray &d)
{
    if (first) {
        first->slotInput(d);
    } else {
        Q_EMIT output(d);
    }
}

HTTPFilterMD5::HTTPFilterMD5() : context(QCryptographicHash::Md5)
{
}

QString
HTTPFilterMD5::md5()
{
    return QString::fromLatin1(context.result().toBase64().constData());
}

void
HTTPFilterMD5::slotInput(const QByteArray &d)
{
    context.addData(d);
    Q_EMIT output(d);
}

HTTPFilterGZip::HTTPFilterGZip(bool deflate)
    : m_deflateMode(deflate),
      m_firstData(true),
      m_finished(false)
{
    // We can't use KFilterDev because it assumes it can read as much data as necessary
    // from the underlying device. It's a pull strategy, while we have to do
    // a push strategy.
    m_gzipFilter = KCompressionDevice::filterForCompressionType(KCompressionDevice::GZip);
}

HTTPFilterGZip::~HTTPFilterGZip()
{
    m_gzipFilter->terminate();
    delete m_gzipFilter;

}

/*
  The data format used by the zlib library is described by RFCs (Request for
  Comments) 1950 to 1952 in the files ftp://ds.internic.net/rfc/rfc1950.txt
  (zlib format), rfc1951.txt (deflate format) and rfc1952.txt (gzip format).

  Use /usr/include/zlib.h as the primary source of documentation though.
*/

void
HTTPFilterGZip::slotInput(const QByteArray &d)
{
    if (d.isEmpty()) {
        return;
    }

    //qDebug() << "Got" << d.size() << "bytes as input";
    if (m_firstData) {
        if (m_deflateMode) {
            bool zlibHeader = true;
            // Autodetect broken webservers (thanks Microsoft) who send raw-deflate
            // instead of zlib-headers-deflate when saying Content-Encoding: deflate.
            const unsigned char firstChar = d[0];
            if ((firstChar & 0x0f) != 8) {
                // In a zlib header, CM should be 8 (cf RFC 1950)
                zlibHeader = false;
            } else if (d.size() > 1) {
                const unsigned char flg = d[1];
                if ((firstChar * 256 + flg) % 31 != 0) { // Not a multiple of 31? invalid zlib header then
                    zlibHeader = false;
                }
            }
            //if (!zlibHeader)
            //    qDebug() << "Bad webserver, uses raw-deflate instead of zlib-deflate...";
            if (zlibHeader) {
                m_gzipFilter->setFilterFlags(KFilterBase::ZlibHeaders);
            } else {
                m_gzipFilter->setFilterFlags(KFilterBase::NoHeaders);
            }
            m_gzipFilter->init(QIODevice::ReadOnly);
        } else {
            m_gzipFilter->setFilterFlags(KFilterBase::WithHeaders);
            m_gzipFilter->init(QIODevice::ReadOnly);
        }
        m_firstData = false;
    }

    m_gzipFilter->setInBuffer(d.constData(), d.size());

    while (!m_gzipFilter->inBufferEmpty() && !m_finished) {
        char buf[8192];
        m_gzipFilter->setOutBuffer(buf, sizeof(buf));
        KFilterBase::Result result = m_gzipFilter->uncompress();
        //qDebug() << "uncompress returned" << result;
        switch (result) {
        case KFilterBase::Ok:
        case KFilterBase::End: {
            const int bytesOut = sizeof(buf) - m_gzipFilter->outBufferAvailable();
            if (bytesOut) {
                Q_EMIT output(QByteArray(buf, bytesOut));
            }
            if (result == KFilterBase::End) {
                //qDebug() << "done, bHasFinished=true";
                Q_EMIT output(QByteArray());
                m_finished = true;
            }
            break;
        }
        case KFilterBase::Error:
            qCDebug(KIO_HTTP_FILTER) << "Error from KGZipFilter";
            Q_EMIT error(i18n("Receiving corrupt data."));
            m_finished = true; // exit this while loop
            break;
        }
    }
}

HTTPFilterDeflate::HTTPFilterDeflate()
    : HTTPFilterGZip(true)
{
}

#include "moc_httpfilter.cpp"
