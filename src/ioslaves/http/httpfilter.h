/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2002 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef _HTTPFILTER_H_
#define _HTTPFILTER_H_

class KFilterBase;
#include <QBuffer>

#include <QObject>
#include <QCryptographicHash>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(KIO_HTTP_FILTER)

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
    void slotInput(const QByteArray &d) override;

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
    void slotInput(const QByteArray &d) override;

private:
    QCryptographicHash context;
};

class HTTPFilterGZip : public HTTPFilterBase
{
    Q_OBJECT
public:
    explicit HTTPFilterGZip(bool deflate = false /* for subclass HTTPFilterDeflate */);
    ~HTTPFilterGZip();

public Q_SLOTS:
    void slotInput(const QByteArray &d) override;

private:
    bool m_deflateMode;
    bool m_firstData;
    bool m_finished;
    KFilterBase *m_gzipFilter;
};

class HTTPFilterDeflate : public HTTPFilterGZip
{
    Q_OBJECT
public:
    HTTPFilterDeflate();
};

#endif
