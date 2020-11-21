/*
    This file is part of the KDE Libraries
    SPDX-FileCopyrightText: 2001 Malte Starostik <malte@kde.org>
    SPDX-FileCopyrightText: 2011 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#define _BSD_SOURCE /* setgroups */
#define _DEFAULT_SOURCE /* stop glibc whining about the previous line */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>

#include <grp.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "dhcp.h"

#ifndef INADDR_NONE /* some OSes don't define this */
#define INADDR_NONE -1
#endif

static int set_gid(gid_t);
static int set_uid(uid_t);
static int get_port(const char *);
static int init_socket(void);
static uint32_t send_request(int);
static void get_reply(int, uint32_t);

static int set_gid(gid_t gid)
{
    if (setgroups(1, &gid) == -1) {
        return -1;
    }
    return setgid(gid); /* _should_ be redundant, but on some systems it isn't */
}

static int set_uid(uid_t uid)
{
    return setuid(uid);
}

/* All functions below do an exit(1) on the slightest error */

/* Returns the UDP port number for the given service name */
static int get_port(const char *service)
{
    struct servent *serv = getservbyname(service, "udp");
    if (serv == NULL) {
        exit(1);
    }

    return serv->s_port;
}

/* Opens the UDP socket, binds to the bootpc port and drops root privileges */
static int init_socket()
{
    struct sockaddr_in addr;
    struct protoent *proto;
    int sock;
    int bcast = 1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = get_port("bootpc");

    if ((proto = getprotobyname("udp")) == NULL ||
            (sock = socket(AF_INET, SOCK_DGRAM, proto->p_proto)) == -1) {
        exit(1);
    }

    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast)) == -1 ||
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &bcast, sizeof(bcast)) == -1 ||
            bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        exit(1);
    }

    if (set_gid(getgid()) != 0 || /* we don't need it anymore */
            set_uid(getuid()) != 0) {
        exit(1);
    }
    return sock;
}

struct response {
    uint32_t result;
    uint16_t err;
};

static struct response send_request_for(int sock, const char *hostname)
{
    struct sockaddr_in addr;
    struct in_addr inaddr;
    struct dhcp_msg request;
    uint8_t *offs = request.options;
    struct response r;

    if (!inet_aton(hostname, &inaddr)) {
        r.err = -1;
        r.result = 0;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_NONE;
    addr.sin_port = get_port("bootps");

    memset(&request, 0, sizeof(request));
    request.op = DHCP_BOOTREQUEST;
    srand(time(NULL));
    request.xid = rand();
    request.ciaddr = (uint32_t)inaddr.s_addr;

    *offs++ = DHCP_MAGIC1;
    *offs++ = DHCP_MAGIC2;
    *offs++ = DHCP_MAGIC3;
    *offs++ = DHCP_MAGIC4;

    *offs++ = DHCP_OPT_MSGTYPE;
    *offs++ = 1; /* length */
    *offs++ = DHCP_INFORM;

    *offs++ = DHCP_OPT_PARAMREQ;
    *offs++ = 1; /* length */
    *offs++ = DHCP_OPT_WPAD;

    *offs++ = DHCP_OPT_END;

    if (sendto(sock, &request, sizeof(request), 0,
               (struct sockaddr *)&addr, sizeof(addr)) != sizeof(request)) {
        r.err = -1;
        r.result = 0;
    }

    r.err = 0;
    r.result = request.xid;
    return r;
}

/* Fills the DHCPINFORM request packet, returns the transaction id */
static uint32_t send_request(int sock)
{
    char hostname[NI_MAXHOST];
    struct ifaddrs *ifaddr, *ifa;
    int status = -1;

    if (getifaddrs(&ifaddr) == -1) {
        exit(1);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        if (ifa->ifa_flags & IFF_LOOPBACK) {
            continue;
        }

        if (ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        status = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                             hostname, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

        if (status != 0) {
            continue;
        }

        struct response r = send_request_for(sock, hostname);

        if (r.err != 0) {
            continue;
        }

        status = r.result;
    }

    freeifaddrs(ifaddr);

    if (status == -1) {
        exit(1);
    }

    return status;
}

/* Reads the reply from the socket, checks it and outputs the URL to STDOUT */
static void get_reply(int sock, uint32_t xid)
{
    struct dhcp_msg reply;
    int len;
    char wpad[DHCP_OPT_LEN + 1];
    uint8_t wpad_len;
    uint8_t *offs = reply.options;
    uint8_t *end;

    if ((len = recvfrom(sock, &reply, sizeof(reply), 0, NULL, NULL)) <= 0) {
        exit(1);
    }

    end = (uint8_t *)&reply + len;
    if (end < offs + 4 ||
            end > &reply.options[DHCP_OPT_LEN] ||
            reply.op != DHCP_BOOTREPLY ||
            reply.xid != xid ||
            *offs++ != DHCP_MAGIC1 ||
            *offs++ != DHCP_MAGIC2 ||
            *offs++ != DHCP_MAGIC3 ||
            *offs++ != DHCP_MAGIC4) {
        exit(1);
    }

    for (; offs < end - 1; offs += *offs + 1) {
        switch (*offs++) {
        case DHCP_OPT_END:
            exit(1);
        case DHCP_OPT_MSGTYPE:
            if (*offs != 1 || (offs >= end - 1) || *(offs + 1) != DHCP_ACK) {
                exit(1);
            }
            break;
        case DHCP_OPT_WPAD:
            memset(wpad, 0, sizeof(wpad));
            wpad_len = *offs++;
            if (offs >= end) {
                exit(1);
            }
            if (wpad_len > end - offs) {
                wpad_len = end - offs;
            }
            strncpy(wpad, (char *)offs, wpad_len);
            wpad[wpad_len] = 0;
            printf("%s\n", wpad);
            close(sock);
            exit(0);
        }
    }
    exit(1);
}

int main()
{
    fd_set rfds;
    struct timeval tv;
    int sock;
    uint32_t xid;

    sock = init_socket();
    xid = send_request(sock);

    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (select(sock + 1, &rfds, NULL, NULL, &tv) == 1 && FD_ISSET(sock, &rfds)) {
        get_reply(sock, xid);
    }

    close(sock);
    exit(1);
}

