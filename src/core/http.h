/* This file is part of the KDE libraries
   Copyright (C) 2002 Jan-Pascal van Best <janpascal@vanbest.org>

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

#ifndef KIOSLAVE_HTTP_H_
#define KIOSLAVE_HTTP_H_


namespace KIO {

  /** HTTP / DAV method **/
  enum HTTP_METHOD {HTTP_GET, HTTP_PUT, HTTP_POST, HTTP_HEAD, HTTP_DELETE,
                    HTTP_OPTIONS, DAV_PROPFIND, DAV_PROPPATCH, DAV_MKCOL,
                    DAV_COPY, DAV_MOVE, DAV_LOCK, DAV_UNLOCK, DAV_SEARCH,
                    DAV_SUBSCRIBE, DAV_UNSUBSCRIBE, DAV_POLL, DAV_NOTIFY,
                    DAV_REPORT,
                    HTTP_UNKNOWN = -1};

}

#endif
