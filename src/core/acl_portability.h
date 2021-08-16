/*
    SPDX-FileCopyrightText: 2021 Gleb Popov <arrowd@FreeBSD.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-2.1-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef ACL_PORTABILITY_H_
#define ACL_PORTABILITY_H_

#include <config-kiocore.h>

extern "C" {
#include <sys/types.h>

#if HAVE_SYS_ACL_H
#include <sys/acl.h>
#endif
#if HAVE_ACL_LIBACL_H
#include <acl/libacl.h>
#endif
}

namespace KIO
{
/**
 * @internal
 * WARNING: DO NOT USE outside KIO Framework
 */
namespace ACLPortability
{
/// @internal
static inline int acl_cmp(acl_t acl1, acl_t acl2)
{
#ifdef Q_OS_FREEBSD
    return ::acl_cmp_np(acl1, acl2);
#else
    return ::acl_cmp(acl1, acl2);
#endif
}

/// @internal
static inline acl_t acl_from_mode(const mode_t mode)
{
#ifdef Q_OS_FREEBSD
    return ::acl_from_mode_np(mode);
#else
    return ::acl_from_mode(mode);
#endif
}

/// @internal
static inline int acl_equiv_mode(acl_t acl, mode_t* mode_p)
{
#ifdef Q_OS_FREEBSD
    return ::acl_equiv_mode_np(acl, mode_p);
#else
    return ::acl_equiv_mode(acl, mode_p);
#endif
}

/// @internal
static inline int acl_get_perm(acl_permset_t permset_d, acl_perm_t perm)
{
#ifdef Q_OS_FREEBSD
    return ::acl_get_perm_np(permset_d, perm);
#else
    return ::acl_get_perm(permset_d, perm);
#endif
}

/// @internal
static inline int acl_extended_file(const char* path_p)
{
#ifdef Q_OS_FREEBSD
    return ::acl_extended_file_np(path_p);
#else
    return ::acl_extended_file(path_p);
#endif
}

}
}

#endif // ACL_PORTABILITY_H_
