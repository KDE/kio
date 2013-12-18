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

#ifndef _HTTPFILTER_H_
#define _HTTPFILTER_H_

class KFilterBase;
#include <QBuffer>

#include <QObject>
#include <QCryptographicHash>

class HTTPFilterBase : public QObject
{
    Q_OBJECT
public:
    HTTPFilterBase();
    ~HTTPFilterBase();

    void chain(HTTPFilterBase *previous);

public Q_SLOTS:
    virtual void slotInput(const QByteArray &d) = 0;

Q_SIGNALS:
    void output(const QByteArray &d);
    void error(const QString &);

protected:
    HTTPFilterBase *last;
};

class HTTPFilterChain : public HTTPFilterBase
{
    Q_OBJECT
public:
    HTTPFilterChain();

    void addFilter(HTTPFilterBase *filter);

public Q_SLOTS:
    void slotInput(const QByteArray &d);

private:
    HTTPFilterBase *first;
};

class HTTPFilterMD5 : public HTTPFilterBase
{
    Q_OBJECT
public:
    HTTPFilterMD5();

    QString md5();

public Q_SLOTS:
    void slotInput(const QByteArray &d);

private:
    QCryptographicHash context;
};


class HTTPFilterGZip : public HTTPFilterBase
{
    Q_OBJECT
public:
    HTTPFilterGZip(bool deflate = false /* for subclass HTTPFilterDeflate */ );
    ~HTTPFilterGZip();

public Q_SLOTS:
    void slotInput(const QByteArray &d);

private:
    bool m_deflateMode;
    bool m_firstData;
    bool m_finished;
    KFilterBase* m_gzipFilter;
};

class HTTPFilterDeflate : public HTTPFilterGZip
{
    Q_OBJECT
public:
    HTTPFilterDeflate();
};

#endif
