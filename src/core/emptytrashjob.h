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

#ifndef EMPTYTRASHJOB_H
#define EMPTYTRASHJOB_H

#include "kiocore_export.h"
#include "simplejob.h"

namespace KIO
{

class EmptyTrashJobPrivate;
/**
 * @class KIO::EmptyTrashJob emptytrashjob.h <KIO/EmptyTrashJob>
 *
 * A KIO job for emptying the trash
 * @see KIO::trash()
 * @see KIO::restoreFromTrash()
 * @since 5.2
 */
class KIOCORE_EXPORT EmptyTrashJob : public SimpleJob
{

    Q_OBJECT

public:
    ~EmptyTrashJob() override;

protected:
    void slotFinished() override;

private:
    EmptyTrashJob(EmptyTrashJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(EmptyTrashJob)
};

/**
 * Empties the trash.
 *
 * @return A pointer to the job handling the operation.
 * @since 5.2
 */
KIOCORE_EXPORT EmptyTrashJob *emptyTrash();

}

#endif
