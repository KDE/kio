/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2001 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef _KIO_HTTP_SLAVE_DEFAULTS_H
#define _KIO_HTTP_SLAVE_DEFAULTS_H

// CONNECTION
#define DEFAULT_KEEP_ALIVE_TIMEOUT      60              // 60 seconds

// CACHE SETTINGS
#define DEFAULT_MAX_CACHE_SIZE          50*1024         // 50 MB
#define DEFAULT_MAX_CACHE_AGE           60*60*24*14     // 14 DAYS
#define DEFAULT_CACHE_EXPIRE            3*60            // 3 MINS
#define DEFAULT_CLEAN_CACHE_INTERVAL    30*60           // 30 MINS
#define DEFAULT_CACHE_CONTROL           KIO::CC_Refresh // Verify with remote
#define CACHE_REVISION                  "9\n"           // Cache version

// DEFAULT USER AGENT KEY - ENABLES OS NAME
#define DEFAULT_USER_AGENT_KEYS         "om"            // Show OS, Machine

// MAXIMUM AMOUNT OF DATA THAT CAN BE SAFELY SENT OVER IPC
#define MAX_IPC_SIZE                    1024*8

// AMOUNT OF DATA TO OBTAIN FROM THE SERVER BY DEFAULT
#define DEFAULT_BUF_SIZE                1024*4

// SOME DEFAULT HEADER VALUES
#define DEFAULT_LANGUAGE_HEADER         "en"
#define DEFAULT_MIME_TYPE               "text/html"
#define DEFAULT_PARTIAL_CHARSET_HEADER  "utf-8, *;q=0.5"
#define DEFAULT_ACCEPT_HEADER           "text/html, text/*;q=0.9, image/jpeg;q=0.9, image/png;q=0.9, image/*;q=0.9, */*;q=0.8"

#endif //KIO_HTTP_SLAVE_DEFAULTS_H
