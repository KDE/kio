/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2001 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
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
