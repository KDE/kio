/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2002 Jan-Pascal van Best <janpascal@vanbest.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIOSLAVE_HTTPMETHOD_P_H_
#define KIOSLAVE_HTTPMETHOD_P_H_

namespace KIO
{

/** HTTP / DAV method **/
enum HTTP_METHOD {HTTP_GET, HTTP_PUT, HTTP_POST, HTTP_HEAD, HTTP_DELETE,
                  HTTP_OPTIONS, DAV_PROPFIND, DAV_PROPPATCH, DAV_MKCOL,
                  DAV_COPY, DAV_MOVE, DAV_LOCK, DAV_UNLOCK, DAV_SEARCH,
                  DAV_SUBSCRIBE, DAV_UNSUBSCRIBE, DAV_POLL, DAV_NOTIFY,
                  DAV_REPORT,
                  HTTP_UNKNOWN = -1,
                 };

}

#endif
