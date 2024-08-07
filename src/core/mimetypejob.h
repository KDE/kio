/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_MIMETYPEJOB_H
#define KIO_MIMETYPEJOB_H

#include "transferjob.h"
#include <kio/global.h> // filesize_t

namespace KIO
{
class MimetypeJobPrivate;
/*!
 * \class KIO::MimetypeJob
 * \inheaderfile KIO/MimetypeJob
 * \inmodule KIOCore
 *
 * A MimetypeJob is a TransferJob that  allows you to get
 * the MIME type of a URL. Don't create directly,
 * but use KIO::mimetype() instead.
 *
 * \sa KIO::mimetype()
 */
class KIOCORE_EXPORT MimetypeJob : public TransferJob
{
    Q_OBJECT

public:
    ~MimetypeJob() override;

protected Q_SLOTS:
    void slotFinished() override;

protected:
    KIOCORE_NO_EXPORT explicit MimetypeJob(MimetypeJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(MimetypeJob)
};

/*!
 * \relates KIO::MimetypeJob
 *
 * Find MIME type for one file or directory.
 *
 * If you are going to download the file right after determining its MIME type,
 * then don't use this, prefer using a KIO::get() job instead. See the note
 * about putting the job on hold once the MIME type is determined.
 *
 * \a url the URL of the file
 *
 * \a flags Can be HideProgressInfo here
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT MimetypeJob *mimetype(const QUrl &url, JobFlags flags = DefaultFlags);

}

#endif
