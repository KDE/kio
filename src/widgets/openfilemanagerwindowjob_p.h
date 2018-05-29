/* This file is part of the KDE libraries
    Copyright (C) 2016 Kai Uwe Broulik <kde@privat.broulik.de>

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

#ifndef OPENFILEMANAGERWINDOWJOB_P_H
#define OPENFILEMANAGERWINDOWJOB_P_H

#include <KJob>

namespace KIO
{

class OpenFileManagerWindowJob;

class AbstractOpenFileManagerWindowStrategy
{

public:
    explicit AbstractOpenFileManagerWindowStrategy(OpenFileManagerWindowJob *job)
        : job(job)
    {

    }

    virtual ~AbstractOpenFileManagerWindowStrategy() {}
    virtual void start(const QList<QUrl> &urls, const QByteArray &asn) = 0;

    void emitResultProxy(int error = KJob::NoError)
    {
        job->setError(error);
        job->emitResult();
    }

protected:
    OpenFileManagerWindowJob *job;

};

class OpenFileManagerWindowDBusStrategy : public AbstractOpenFileManagerWindowStrategy
{

public:
    explicit OpenFileManagerWindowDBusStrategy(OpenFileManagerWindowJob *job) : AbstractOpenFileManagerWindowStrategy(job) {}
    void start(const QList<QUrl> &urls, const QByteArray &asn) override;

};

class OpenFileManagerWindowKRunStrategy : public AbstractOpenFileManagerWindowStrategy
{

public:
    explicit OpenFileManagerWindowKRunStrategy(OpenFileManagerWindowJob *job) : AbstractOpenFileManagerWindowStrategy(job) {}
    void start(const QList<QUrl> &urls, const QByteArray &asn) override;

};

}

#endif // OPENFILEMANAGERWINDOWJOB_P_H
