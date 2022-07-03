/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2001 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef _KIO_IOWORKER_DEFAULTS_H
#define _KIO_IOWORKER_DEFAULTS_H

// TIMEOUT VALUES
static constexpr int DEFAULT_RESPONSE_TIMEOUT = 600; // 10 min.
static constexpr int DEFAULT_CONNECT_TIMEOUT = 20; // 20 secs.
static constexpr int DEFAULT_READ_TIMEOUT = 15; // 15 secs.
static constexpr int DEFAULT_PROXY_CONNECT_TIMEOUT = 10; // 10 secs.
static constexpr int MIN_TIMEOUT_VALUE = 2; //  2 secs.

// MINIMUM SIZE FOR ABORTED DOWNLOAD TO BE KEPT
static constexpr int DEFAULT_MINIMUM_KEEP_SIZE = 5120; //  5 Kbs

// NORMAL PORT DEFAULTS
static constexpr int DEFAULT_FTP_PORT = 21;
static constexpr int DEFAULT_SFTP_PORT = 22;
static constexpr int DEFAULT_SMTP_PORT = 25;
static constexpr int DEFAULT_HTTP_PORT = 80;
static constexpr int DEFAULT_POP3_PORT = 110;
static constexpr int DEFAULT_NNTP_PORT = 119;
static constexpr int DEFAULT_IMAP_PORT = 143;
static constexpr int DEFAULT_IMAP3_PORT = 220;
static constexpr int DEFAULT_LDAP_PORT = 389;

// SECURE PORT DEFAULTS
static constexpr int DEFAULT_HTTPS_PORT = 443;
static constexpr int DEFAULT_NNTPS_PORT = 563;
static constexpr int DEFAULT_LDAPS_PORT = 389;
static constexpr int DEFAULT_IMAPS_PORT = 993;
static constexpr int DEFAULT_POP3S_PORT = 995;

// OTHER GENERIC PORT DEFAULTS
static constexpr int DEFAULT_PROXY_PORT = 8080;
static constexpr int MAX_PORT_VALUE = 65535;

#endif
