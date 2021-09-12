/*
    SPDX-FileCopyrightText: 2000-2002 Till Adam <adam@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef ACLHELPERS_P_H
#define ACLHELPERS_P_H

#include "../core/config-kiocore.h" // HAVE_POSIX_ACL

/*************************************
 *
 * ACL handling helpers
 *
 *************************************/
#if HAVE_POSIX_ACL

#include <KIO/UDSEntry>

#include <acl/libacl.h>
#include <sys/acl.h>

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
static void appendACLAtoms(const QByteArray &path, KIO::UDSEntry &entry, mode_t type)
{
    // first check for a noop
    if (acl_extended_file(path.data()) == 0) {
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
            if (acl_equiv_mode(acl, nullptr) == 0) {
                acl_free(acl);
                acl = nullptr;
            }
        }
        defaultAcl = acl_get_file(path.data(), ACL_TYPE_DEFAULT);
    }

    if (acl || defaultAcl) {
        // qDebug() << path.constData() << "has extended ACL entries";
        entry.fastInsert(KIO::UDSEntry::UDS_EXTENDED_ACL, 1);

        if (acl) {
            const QString str = aclToText(acl);
            entry.fastInsert(KIO::UDSEntry::UDS_ACL_STRING, str);
            // qDebug() << path.constData() << "ACL:" << str;
            acl_free(acl);
        }

        if (defaultAcl) {
            const QString str = aclToText(defaultAcl);
            entry.fastInsert(KIO::UDSEntry::UDS_DEFAULT_ACL_STRING, str);
            // qDebug() << path.constData() << "DEFAULT ACL:" << str;
            acl_free(defaultAcl);
        }
    }
}
#endif

#endif // ACLHELPERS_P_H
