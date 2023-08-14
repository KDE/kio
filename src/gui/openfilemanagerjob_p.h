/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef OPENFILEMANAGERWINDOWJOB_P_H
#define OPENFILEMANAGERWINDOWJOB_P_H

#include <KJob>

namespace KIO
{
class OpenFileManagerJob;

class AbstractOpenFileManagerJobStrategy
{
public:
    explicit AbstractOpenFileManagerJobStrategy(OpenFileManagerJob *job)
        : m_job(job)
    {
    }

    virtual ~AbstractOpenFileManagerJobStrategy()
    {
    }
    virtual void start(const QList<QUrl> &urls, const QByteArray &asn) = 0;

    void emitResultProxy(int error = KJob::NoError)
    {
        m_job->setError(error);
        m_job->emitResult();
    }

protected:
    OpenFileManagerJob *m_job;
};

class OpenFileManagerDBusStrategy : public AbstractOpenFileManagerJobStrategy
{
public:
    explicit OpenFileManagerDBusStrategy(OpenFileManagerJob *job)
        : AbstractOpenFileManagerJobStrategy(job)
    {
    }
    void start(const QList<QUrl> &urls, const QByteArray &asn) override;
};

class OpenFileManagerKRunStrategy : public AbstractOpenFileManagerJobStrategy
{
public:
    explicit OpenFileManagerKRunStrategy(OpenFileManagerJob *job)
        : AbstractOpenFileManagerJobStrategy(job)
    {
    }
    void start(const QList<QUrl> &urls, const QByteArray &asn) override;
};

}

#endif // OPENFILEMANAGERWINDOWJOB_P_H
