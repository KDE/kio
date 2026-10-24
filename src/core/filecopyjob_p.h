// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_FILECOPYJOB_P_H
#define KIO_FILECOPYJOB_P_H

namespace KIO
{
class FileCopyJob;

/*
 * A move whose copy reports flush-destination keeps its source file. The data of the copy is then
 * possibly only in the page cache, and the caller deletes the source once the destination is
 * flushed.
 */
void keepSourceUntilFlushed(FileCopyJob *job);

// True when the move finished with its source file still in place.
bool sourceKeptUntilFlushed(FileCopyJob *job);
}

#endif
