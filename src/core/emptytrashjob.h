/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef EMPTYTRASHJOB_H
#define EMPTYTRASHJOB_H

#include "kiocore_export.h"
#include "simplejob.h"

namespace KIO
{
class EmptyTrashJobPrivate;
/*!
 * \class KIO::EmptyTrashJob
 * \inheaderfile KIO/EmptyTrashJob
 * \inmodule KIOCore
 *
 * A KIO job for emptying the trash
 * \sa KIO::trash()
 * \sa KIO::restoreFromTrash()
 * \since 5.2
 */
class KIOCORE_EXPORT EmptyTrashJob : public SimpleJob
{
    Q_OBJECT

public:
    ~EmptyTrashJob() override;

protected:
    void slotFinished() override;

private:
    KIOCORE_NO_EXPORT explicit EmptyTrashJob(EmptyTrashJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(EmptyTrashJob)
};

/*!
 * \relates KIO::EmptyTrashJob
 *
 * Empties the trash.
 *
 * Returns a pointer to the job handling the operation.
 * \since 5.2
 */
KIOCORE_EXPORT EmptyTrashJob *emptyTrash();

}

#endif
