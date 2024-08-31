/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef OPENFILEMANAGERWINDOWJOB_P_H
#define OPENFILEMANAGERWINDOWJOB_P_H

#include <KJob>

#include "openfilemanagerwindowjob.h"

namespace KIO
{
class AbstractOpenFileManagerWindowStrategy : public QObject
{
    Q_OBJECT
public:
    explicit AbstractOpenFileManagerWindowStrategy()
        : QObject()
    {
    }

    virtual ~AbstractOpenFileManagerWindowStrategy()
    {
    }
    virtual void start(const QList<QUrl> &urls, const QByteArray &asn) = 0;

Q_SIGNALS:
    void finished(int error);
};

#ifdef WITH_QTDBUS
class OpenFileManagerWindowDBusStrategy : public AbstractOpenFileManagerWindowStrategy
{
public:
    explicit OpenFileManagerWindowDBusStrategy()
        : AbstractOpenFileManagerWindowStrategy()
    {
    }
    void start(const QList<QUrl> &urls, const QByteArray &asn) override;
};
#endif

class OpenFileManagerWindowKRunStrategy : public AbstractOpenFileManagerWindowStrategy
{
public:
    explicit OpenFileManagerWindowKRunStrategy(OpenFileManagerWindowJob *job)
        : AbstractOpenFileManagerWindowStrategy()
        , m_job(job)
    {
    }
    void start(const QList<QUrl> &urls, const QByteArray &asn) override;

private:
    OpenFileManagerWindowJob *m_job;
};

#if defined(Q_OS_WINDOWS)
class OpenFileManagerWindowWindowsShellStrategy : public AbstractOpenFileManagerWindowStrategy
{
public:
    explicit OpenFileManagerWindowWindowsShellStrategy()
        : AbstractOpenFileManagerWindowStrategy()
    {
    }
    void start(const QList<QUrl> &urls, const QByteArray &asn) override;
};
#endif
}

#endif // OPENFILEMANAGERWINDOWJOB_P_H
