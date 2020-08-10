/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "specialjob.h"
#include "job_p.h"

using namespace KIO;

class KIO::SpecialJobPrivate: public TransferJobPrivate
{
    SpecialJobPrivate(const QUrl &url, int command,
                      const QByteArray &packedArgs,
                      const QByteArray &_staticData)
        : TransferJobPrivate(url, command, packedArgs, _staticData)
    {}
};

SpecialJob::SpecialJob(const QUrl &url, const QByteArray &packedArgs)
    : TransferJob(*new TransferJobPrivate(url, CMD_SPECIAL, packedArgs, QByteArray()))
{
}

SpecialJob::~SpecialJob()
{
}

void SpecialJob::setArguments(const QByteArray &data)
{
    Q_D(SpecialJob);
    d->m_packedArgs = data;
}

QByteArray SpecialJob::arguments() const
{
    return d_func()->m_packedArgs;
}

#include "moc_specialjob.cpp"
