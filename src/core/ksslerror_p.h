/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007, 2008 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/


#ifndef KSSLERROR_P_H
#define KSSLERROR_P_H

#include "kiocore_export.h"
#include "ktcpsocket.h"

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 65)
class KSslErrorPrivate
{
public:
    KIOCORE_EXPORT static KSslError::Error errorFromQSslError(QSslError::SslError e);
    KIOCORE_EXPORT static QSslError::SslError errorFromKSslError(KSslError::Error e);

    QSslError error;
};
#endif

#endif
