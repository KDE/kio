/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
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
 * the MIME type of a URL. Don't create directly,
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
 * Find MIME type for one file or directory.
 *
 * If you are going to download the file right after determining its MIME type,
 * then don't use this, prefer using a KIO::get() job instead. See the note
 * about putting the job on hold once the MIME type is determined.
 *
 * @param url the URL of the file
 * @param flags Can be HideProgressInfo here
 * @return the job handling the operation.
 */
KIOCORE_EXPORT MimetypeJob *mimetype(const QUrl &url,
                                     JobFlags flags = DefaultFlags);

}

#endif
