/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_JOB_H
#define KIO_JOB_H

#include "kiocore_export.h"

#include <QUrl>
namespace KIO
{

/**
 * Returns a translated error message for @p errorCode using the
 * additional error information provided by @p errorText.
 * @param errorCode the error code
 * @param errorText the additional error text
 * @return the created error string
 */
KIOCORE_EXPORT QString buildErrorString(int errorCode, const QString &errorText);

/**
 * Returns a translated html error message for @p errorCode using the
 * additional error information provided by @p errorText , @p reqUrl
 * (the request URL), and the ioslave @p method .
 * @param errorCode the error code
 * @param errorText the additional error text
 * @param reqUrl the request URL
 * @param method the ioslave method
 * @return the created error string
 */
KIOCORE_EXPORT QString buildHTMLErrorString(int errorCode, const QString &errorText,
        const QUrl *reqUrl = nullptr, int method = -1);

/**
 * Returns translated error details for @p errorCode using the
 * additional error information provided by @p errorText , @p reqUrl
 * (the request URL), and the ioslave @p method .
 *
 * @param errorCode the error code
 * @param errorText the additional error text
 * @param reqUrl the request URL
 * @param method the ioslave method
 * @return the following data:
 * @li QString errorName - the name of the error
 * @li QString techName - if not null, the more technical name of the error
 * @li QString description - a description of the error
 * @li QStringList causes - a list of possible causes of the error
 * @li QStringList solutions - a liso of solutions for the error
 */
KIOCORE_EXPORT QByteArray rawErrorDetail(int errorCode, const QString &errorText,
        const QUrl *reqUrl = nullptr, int method = -1);
}

// kio/job.h is what apps include to get all jobs -> we include jobclasses.h to preserve SC
// This required to move KIO::Job in a separate header kio/job_base.h
#include "jobclasses.h"

#include <QDateTime>

#endif
