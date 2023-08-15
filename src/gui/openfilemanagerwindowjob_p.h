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
class OpenFileManagerWindowJob;

class AbstractOpenFileManagerWindowStrategy
{
public:
    explicit AbstractOpenFileManagerWindowStrategy(OpenFileManagerWindowJob *job)
        : m_job(job)
    {
    }

    virtual ~AbstractOpenFileManagerWindowStrategy()
    {
    }
    virtual void start(const QList<QUrl> &urls, const QByteArray &asn) = 0;

    void emitResultProxy(int error = KJob::NoError)
    {
        m_job->setError(error);
        m_job->emitResult();
    }

protected:
    OpenFileManagerWindowJob *m_job;
};

#ifdef Q_OS_LINUX
class OpenFileManagerWindowDBusStrategy : public AbstractOpenFileManagerWindowStrategy
{
public:
    explicit OpenFileManagerWindowDBusStrategy(OpenFileManagerWindowJob *job)
        : AbstractOpenFileManagerWindowStrategy(job)
    {
    }
    void start(const QList<QUrl> &urls, const QByteArray &asn) override;
};
#endif

#ifndef Q_OS_WINDOWS
class OpenFileManagerWindowKRunStrategy : public AbstractOpenFileManagerWindowStrategy
{
public:
    explicit OpenFileManagerWindowKRunStrategy(OpenFileManagerWindowJob *job)
        : AbstractOpenFileManagerWindowStrategy(job)
    {
    }
    void start(const QList<QUrl> &urls, const QByteArray &asn) override;
};
#endif

#ifdef Q_OS_WINDOWS
class OpenFileManagerWindowShellExecuteStrategy : public AbstractOpenFileManagerWindowStrategy
{
public:
    explicit OpenFileManagerWindowKRunStrategy(OpenFileManagerWindowJob *job)
        : AbstractOpenFileManagerWindowStrategy(job)
    {
    }
    void start(const QList<QUrl> &urls, const QByteArray &asn) override;
};
#endif
}

#endif // OPENFILEMANAGERWINDOWJOB_P_H
