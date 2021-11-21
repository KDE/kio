/*
    SPDX-FileCopyrightText: 2000-2002 Till Adam <adam@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef ACLHELPERS_P_H
#define ACLHELPERS_P_H

/*************************************
 *
 * ACL handling helpers
 *
 *************************************/

#include <config-kiocore.h>

#include <KIO/UDSEntry>

#include <sys/types.h>

#if HAVE_SYS_ACL_H
#include <sys/acl.h>
#endif
#if HAVE_ACL_LIBACL_H
#include <acl/libacl.h>
#endif

namespace KIO
{
/**
 * @internal
 * WARNING: DO NOT USE outside KIO Framework
 */
namespace ACLPortability
{
/// @internal
__attribute__((unused)) static inline int acl_cmp(acl_t acl1, acl_t acl2)
{
#ifdef Q_OS_FREEBSD
    return ::acl_cmp_np(acl1, acl2);
#else
    return ::acl_cmp(acl1, acl2);
#endif
}

/// @internal
__attribute__((unused)) static inline acl_t acl_from_mode(const mode_t mode)
{
#ifdef Q_OS_FREEBSD
    return ::acl_from_mode_np(mode);
#else
    return ::acl_from_mode(mode);
#endif
}

/// @internal
static inline int acl_equiv_mode(acl_t acl, mode_t *mode_p)
{
#ifdef Q_OS_FREEBSD
    return ::acl_equiv_mode_np(acl, mode_p);
#else
    return ::acl_equiv_mode(acl, mode_p);
#endif
}

/// @internal
__attribute__((unused)) static inline int acl_get_perm(acl_permset_t permset_d, acl_perm_t perm)
{
#ifdef Q_OS_FREEBSD
    return ::acl_get_perm_np(permset_d, perm);
#else
    return ::acl_get_perm(permset_d, perm);
#endif
}

/// @internal
__attribute__((unused)) static inline int acl_extended_file(const char *path_p)
{
#ifdef Q_OS_FREEBSD
    return ::acl_extended_file_np(path_p);
#else
    return ::acl_extended_file(path_p);
#endif
}

} // namespace ACLPortability
} // namespace KIO

static QString aclToText(acl_t acl)
{
    ssize_t size = 0;
    char *txt = acl_to_text(acl, &size);
    const QString ret = QString::fromLatin1(txt, size);
    acl_free(txt);
    return ret;
}

/* Append an atom indicating whether the file has extended acl information
 * and if withACL is specified also one with the acl itself. If it's a directory
 * and it has a default ACL, also append that. */
__attribute__((unused)) static void appendACLAtoms(const QByteArray &path, KIO::UDSEntry &entry, mode_t type)
{
    // first check for a noop
    if (KIO::ACLPortability::acl_extended_file(path.data()) == 0) {
        return;
    }

    acl_t acl = nullptr;
    acl_t defaultAcl = nullptr;
    bool isDir = (type & QT_STAT_MASK) == QT_STAT_DIR;
    // do we have an acl for the file, and/or a default acl for the dir, if it is one?
    acl = acl_get_file(path.data(), ACL_TYPE_ACCESS);
    /* Sadly libacl does not provided a means of checking for extended ACL and default
     * ACL separately. Since a directory can have both, we need to check again. */
    if (isDir) {
        if (acl) {
            if (KIO::ACLPortability::acl_equiv_mode(acl, nullptr) == 0) {
                acl_free(acl);
                acl = nullptr;
            }
        }
        defaultAcl = acl_get_file(path.data(), ACL_TYPE_DEFAULT);
    }

    if (acl || defaultAcl) {
        // qDebug() << path.constData() << "has extended ACL entries";
        entry.replace(KIO::UDSEntry::UDS_EXTENDED_ACL, 1);

        if (acl) {
            const QString str = aclToText(acl);
            entry.replace(KIO::UDSEntry::UDS_ACL_STRING, str);
            // qDebug() << path.constData() << "ACL:" << str;
            acl_free(acl);
        }

        if (defaultAcl) {
            const QString str = aclToText(defaultAcl);
            entry.replace(KIO::UDSEntry::UDS_DEFAULT_ACL_STRING, str);
            // qDebug() << path.constData() << "DEFAULT ACL:" << str;
            acl_free(defaultAcl);
        }
    }
}

#endif // ACLHELPERS_P_H
