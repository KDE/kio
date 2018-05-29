/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                  2000-2009 David Faure <faure@kde.org>

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

#ifndef KIO_MIMETYPEJOB_H
#define KIO_MIMETYPEJOB_H

#include <kio/global.h> // filesize_t
#include "transferjob.h"

namespace KIO
{

class MimetypeJobPrivate;
/**
 * @class KIO::MimetypeJob mimetypejob.h <KIO/MimetypeJob>
 *
 * A MimetypeJob is a TransferJob that  allows you to get
 * the mime type of an URL. Don't create directly,
 * but use KIO::mimetype() instead.
 * @see KIO::mimetype()
 */
class KIOCORE_EXPORT MimetypeJob : public TransferJob
{
    Q_OBJECT

public:
    ~MimetypeJob() override;

protected Q_SLOTS:
    void slotFinished() override;
protected:
    MimetypeJob(MimetypeJobPrivate &dd);
private:
    Q_DECLARE_PRIVATE(MimetypeJob)
};

/**
 * Find mimetype for one file or directory.
 *
 * If you are going to download the file right after determining its mimetype,
 * then don't use this, prefer using a KIO::get() job instead. See the note
 * about putting the job on hold once the mimetype is determined.
 *
 * @param url the URL of the file
 * @param flags Can be HideProgressInfo here
 * @return the job handling the operation.
 */
KIOCORE_EXPORT MimetypeJob *mimetype(const QUrl &url,
                                     JobFlags flags = DefaultFlags);

}

#endif
