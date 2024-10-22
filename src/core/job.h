/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_JOB_H
#define KIO_JOB_H

#include "job_base.h" // IWYU pragma: export
#include "kiocore_export.h"

#include <QUrl>
namespace KIO
{
/*!
 * Returns a translated error message for \a errorCode using the
 * additional error information provided by \a errorText.
 *
 * \a errorCode the error code
 *
 * \a errorText the additional error text
 */
KIOCORE_EXPORT QString buildErrorString(int errorCode, const QString &errorText);

/*!
 * Returns translated error details for \a errorCode using the
 * additional error information provided by \a errorText , \a reqUrl
 * (the request URL), and the KIO worker \a method .
 *
 * \a errorCode the error code
 *
 * \a errorText the additional error text
 *
 * \a reqUrl the request URL
 *
 * \a method the KIO worker method
 *
 * Returns the following data:
 * \list
 * \li QString errorName - the name of the error
 * \li QString techName - if not null, the more technical name of the error
 * \li QString description - a description of the error
 * \li QStringList causes - a list of possible causes of the error
 * \li QStringList solutions - a liso of solutions for the error
 * \endlist
 */
KIOCORE_EXPORT QByteArray rawErrorDetail(int errorCode, const QString &errorText, const QUrl *reqUrl = nullptr, int method = -1);
}

#endif
