/* This file is part of the KDE libraries
   Copyright (C) 2001 Waldo Bastian <bastian@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef _KIO_IOSLAVE_DEFAULTS_H
#define _KIO_IOSLAVE_DEFAULTS_H

// TIMEOUT VALUES
#define DEFAULT_RESPONSE_TIMEOUT           600     // 10 min.
#define DEFAULT_CONNECT_TIMEOUT             20     // 20 secs.
#define DEFAULT_READ_TIMEOUT                15     // 15 secs.
#define DEFAULT_PROXY_CONNECT_TIMEOUT       10     // 10 secs.
#define MIN_TIMEOUT_VALUE                    2     //  2 secs.

// MINMUM SIZE FOR ABORTED DOWNLOAD TO BE KEPT
#define DEFAULT_MINIMUM_KEEP_SIZE         5120  //  5 Kbs

// NORMAL PORT DEFAULTS
#define DEFAULT_FTP_PORT                    21
#define DEFAULT_SFTP_PORT                   22
#define DEFAULT_SMTP_PORT                   25
#define DEFAULT_HTTP_PORT                   80
#define DEFAULT_POP3_PORT                  110
#define DEFAULT_NNTP_PORT                  119
#define DEFAULT_IMAP_PORT                  143
#define DEFAULT_IMAP3_PORT                 220
#define DEFAULT_LDAP_PORT                  389

// SECURE PORT DEFAULTS
#define DEFAULT_HTTPS_PORT                 443
#define DEFAULT_NNTPS_PORT                 563
#define DEFAULT_LDAPS_PORT                 389
#define DEFAULT_IMAPS_PORT                 993
#define DEFAULT_POP3S_PORT                 995

// OTHER GENERIC PORT DEFAULTS
#define DEFAULT_PROXY_PORT                8080
#define MAX_PORT_VALUE                   65535

#endif
