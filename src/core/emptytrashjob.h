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
