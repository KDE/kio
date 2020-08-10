/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "emptytrashjob.h"
#include "job.h"
#include "job_p.h"
//#include <KNotification>

using namespace KIO;

class KIO::EmptyTrashJobPrivate: public SimpleJobPrivate
{
public:
    EmptyTrashJobPrivate(int command, const QByteArray &packedArgs)
        : SimpleJobPrivate(QUrl(QStringLiteral("trash:/")), command, packedArgs)
    { }

    Q_DECLARE_PUBLIC(EmptyTrashJob)

    static inline EmptyTrashJob *newJob(int command, const QByteArray &packedArgs)
    {
        EmptyTrashJob *job = new EmptyTrashJob(*new EmptyTrashJobPrivate(command, packedArgs));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        return job;
    }
};

EmptyTrashJob::EmptyTrashJob(EmptyTrashJobPrivate &dd)
    : SimpleJob(dd)
{
}

EmptyTrashJob::~EmptyTrashJob()
{
}

// TODO KF6: remove this
void EmptyTrashJob::slotFinished()
{
    SimpleJob::slotFinished();
}

KIO::EmptyTrashJob *KIO::emptyTrash()
{
    KIO_ARGS << int(1);
    return EmptyTrashJobPrivate::newJob(CMD_SPECIAL, packedArgs);
}

#include "moc_emptytrashjob.cpp"
