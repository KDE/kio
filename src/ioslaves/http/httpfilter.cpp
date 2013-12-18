/*
   This file is part of the KDE libraries
   Copyright (c) 2002 Waldo Bastian <bastian@kde.org>
   Copyright 2009 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "httpfilter.h"
#include <kcompressiondevice.h>
#include <kfilterbase.h>

#include <QDebug>

#include <stdio.h>

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
 : last(0)
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
   connect(last, SIGNAL(output(QByteArray)),
           this, SLOT(slotInput(QByteArray)));
}

HTTPFilterChain::HTTPFilterChain()
 : first(0)
{
}

void
HTTPFilterChain::addFilter(HTTPFilterBase *filter)
{
   if (!last)
   {
      first = filter;
   }
   else
   {
      disconnect(last, SIGNAL(output(QByteArray)), 0, 0);
      filter->chain(last);
   }
   last = filter;
   connect(filter, SIGNAL(output(QByteArray)),
           this, SIGNAL(output(QByteArray)));
   connect(filter, SIGNAL(error(QString)),
           this, SIGNAL(error(QString)));
}

void
HTTPFilterChain::slotInput(const QByteArray &d)
{
   if (first)
      first->slotInput(d);
   else
      emit output(d);
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
   emit output(d);
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
    if (d.isEmpty())
        return;

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
        case KFilterBase::End:
        {
            const int bytesOut = sizeof(buf) - m_gzipFilter->outBufferAvailable();
            if (bytesOut) {
                emit output(QByteArray(buf, bytesOut));
            }
            if (result == KFilterBase::End) {
                //qDebug() << "done, bHasFinished=true";
                emit output(QByteArray());
                m_finished = true;
            }
            break;
        }
        case KFilterBase::Error:
            qDebug() << "Error from KGZipFilter";
            emit error(tr("Receiving corrupt data."));
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
