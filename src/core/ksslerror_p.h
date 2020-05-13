/* This file is part of the KDE libraries
    Copyright (C) 2007, 2008 Andreas Hartmetz <ahartmetz@gmail.com>

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
