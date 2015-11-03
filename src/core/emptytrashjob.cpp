/* This file is part of the KDE libraries
    Copyright (C) 2014 David Faure <faure@kde.org>

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

#include "emptytrashjob.h"
#include "job.h"
#include "job_p.h"
#include <kdirnotify.h>
//#include <knotification.h>

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

void EmptyTrashJob::slotFinished()
{
    //KNotification::event("Trash: emptied", QString(), QPixmap(), 0, KNotification::DefaultEvent);
    org::kde::KDirNotify::emitFilesAdded(QUrl(QStringLiteral("trash:/")));
}

KIO::EmptyTrashJob *KIO::emptyTrash()
{
    KIO_ARGS << int(1);
    return EmptyTrashJobPrivate::newJob(CMD_SPECIAL, packedArgs);
}

#include "moc_emptytrashjob.cpp"
