/*
    This file is part of the KDE Libraries
    SPDX-FileCopyrightText: 2001 Malte Starostik <malte@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

/* See RFC 2131 for details */

#ifndef __dhcp_h__
#define __dhcp_h__

#include <config-kpac.h>

#if HAVE_STDINT_H
# include <stdint.h>
#endif

#define DHCP_OPT_LEN 312

struct dhcp_msg {
#define DHCP_BOOTREQUEST 1
#define DHCP_BOOTREPLY   2
    uint8_t op;           /* operation */
    uint8_t htype;        /* hwaddr type */
    uint8_t hlen;         /* hwaddr len */
    uint8_t hops;
    uint32_t xid;          /* transaction id */
    uint16_t secs;        /* seconds since protocol start */
#define DHCP_BROADCAST 1
    uint16_t flags;
    uint32_t ciaddr;      /* client IP */
    uint32_t yiaddr;      /* "your" IP */
    uint32_t siaddr;      /* server IP */
    uint32_t giaddr;      /* gateway IP */
    uint8_t chaddr[16];   /* client hwaddr */
    uint8_t sname[64];    /* server name */
    uint8_t file[128];    /* bootstrap file */
    uint8_t options[DHCP_OPT_LEN];
};

/* first four bytes in options */
#define DHCP_MAGIC1   0x63
#define DHCP_MAGIC2   0x82
#define DHCP_MAGIC3   0x53
#define DHCP_MAGIC4   0x63

/* DHCP message types */
#define DHCP_DISCOVER 1
#define DHCP_OFFER    2
#define DHCP_REQUEST  3
#define DHCP_DECLINE  4
#define DHCP_ACK      5
#define DHCP_NAK      6
#define DHCP_RELEASE  7
#define DHCP_INFORM   8

/* option types */
#define DHCP_OPT_MSGTYPE  0x35
#define DHCP_OPT_PARAMREQ 0x37
#define DHCP_OPT_WPAD     0xfc
#define DHCP_OPT_END      0xff

#endif

