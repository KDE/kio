/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                  2000-2013 David Faure <faure@kde.org>

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
