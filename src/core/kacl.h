/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2005-2007 Till Adam <adam@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KACL_H
#define KACL_H

#include "kiocore_export.h"
#include <qplatformdefs.h>

#include <QList>
#include <QPair>

#include <memory>

typedef QPair<QString, unsigned short> ACLUserPermissions;
typedef QList<ACLUserPermissions> ACLUserPermissionsList;
typedef QList<ACLUserPermissions>::iterator ACLUserPermissionsIterator;
typedef QList<ACLUserPermissions>::const_iterator ACLUserPermissionsConstIterator;

typedef QPair<QString, unsigned short> ACLGroupPermissions;
typedef QList<ACLGroupPermissions> ACLGroupPermissionsList;
typedef QList<ACLGroupPermissions>::iterator ACLGroupPermissionsIterator;
typedef QList<ACLGroupPermissions>::const_iterator ACLGroupPermissionsConstIterator;

/*!
 * \class KACL
 * \inmodule KIOCore
 *
 * \brief a POSIX ACL encapsulation.
 *
 * The KACL class encapsulates a POSIX Access Control List. It follows the
 * little standard that couldn't, 1003.1e/1003.2c, which died in draft status.
 */
class KIOCORE_EXPORT KACL
{
public:
    /*!
     * Creates a new KACL from \a aclString. If the string is a valid acl
     * string, isValid() will afterwards return true.
     */
    KACL(const QString &aclString);

    /*! Copy ctor */
    KACL(const KACL &rhs);

    /*!
     * Creates a new KACL from the basic permissions passed in \a basicPermissions.
     * isValid() will return true, afterwards.
     */
    KACL(mode_t basicPermissions);

    /*!
     * Creates an empty KACL. Until a valid acl string is set via setACL,
     * isValid() will return false.
     */
    KACL();

    virtual ~KACL();

    KACL &operator=(const KACL &rhs);

    bool operator==(const KACL &rhs) const;

    bool operator!=(const KACL &rhs) const;

    /*!
     * Returns whether the KACL object represents a valid acl.
     */
    bool isValid() const;

    /* The standard (non-extended) part of an ACL. These map directly to
     * standard unix file permissions. Setting them will never make a valid
     * ACL invalid. */

    /*! Returns the owner's permissions entry */
    unsigned short ownerPermissions() const;

    /*! Set the owner's permissions entry.
     * Returns success or failure */
    bool setOwnerPermissions(unsigned short);

    /*! Returns the owning group's permissions entry */
    unsigned short owningGroupPermissions() const;

    /*! Set the owning group's permissions entry.
     * Returns success or failure */
    bool setOwningGroupPermissions(unsigned short);

    /*! Returns the permissions entry for others */
    unsigned short othersPermissions() const;

    /*! Set the permissions entry for others.
     * Returns success or failure */
    bool setOthersPermissions(unsigned short);

    /*! Returns the basic (owner/group/others) part of the ACL as a mode_t */
    mode_t basePermissions() const;

    /* The interface to the extended ACL. This is a mask, permissions for
     * n named users and permissions for m named groups. */

    /*!
     * Return whether the ACL contains extended entries or can be expressed
     * using only basic file permissions.
     */
    bool isExtended() const;

    /*!
     * Return the entry for the permissions mask if there is one and sets
     * \a exists to true. If there is no such entry, \a exists is set to false.
     * Returns the permissions mask entry */
    unsigned short maskPermissions(bool &exists) const;

    /*! Set the permissions mask for the ACL. Permissions set for individual
     * entries will be masked with this, such that their effective permissions
     * are the result of the logical and of their entry and the mask.
     * Returns success or failure */
    bool setMaskPermissions(unsigned short);

    /*!
     * Access to the permissions entry for a named user, if such an entry
     * exists. If \a exists is non-null, the boolean variable it points to
     * is set to true if a matching entry exists and to false otherwise.
     * Returns the permissions for a user entry with the name in \a name */
    unsigned short namedUserPermissions(const QString &name, bool *exists) const;

    /*! Set the permissions for a user with the name \a name. Will fail
     * if the user doesn't exist, in which case the ACL will be unchanged.
     * Returns success or failure. */
    bool setNamedUserPermissions(const QString &name, unsigned short);

    /*! Returns the list of all group permission entries. Each entry consists
     * of a name/permissions pair. This is a QPair, therefore access is provided
     * via the .first and .next members.
     * Returns the list of all group permission entries. */
    ACLUserPermissionsList allUserPermissions() const;

    /*! Replace the list of all user permissions with \a list. If one
     * of the entries in the list does not exists, or setting of the ACL
     * entry fails for any reason, the ACL will be left unchanged.
     * Returns success or failure */
    bool setAllUserPermissions(const ACLUserPermissionsList &list);

    /*!
     * Access to the permissions entry for a named group, if such an entry
     * exists. If \a exists is non-null, the boolean variable it points to is
     * set to true if a matching entry exists and to false otherwise.
     * Returns the permissions for a group with the name in \a name */
    unsigned short namedGroupPermissions(const QString &name, bool *exists) const;

    /*! Set the permissions for a group with the name \a name. Will fail
     * if the group doesn't exist, in which case the ACL be unchanged.
     * Returns success or failure. */
    bool setNamedGroupPermissions(const QString &name, unsigned short);

    /*! Returns the list of all group permission entries. Each entry consists
     * of a name/permissions pair. This is a QPair, therefore access is provided
     * via the .first and .next members.
     * Returns the list of all group permission entries. */

    ACLGroupPermissionsList allGroupPermissions() const;
    /*! Replace the list of all user permissions with @p list. If one
     * of the entries in the list does not exists, or setting of the ACL
     * entry fails for any reason, the ACL will be left unchanged.
     * Returns success or failure */
    bool setAllGroupPermissions(const ACLGroupPermissionsList &);

    /*! Sets the whole list from a string. If the string in @p aclStr represents
     * a valid ACL, it will be set, otherwise the ACL remains unchanged.
     * Returns whether setting the ACL was successful. */
    bool setACL(const QString &aclStr);

    /*! Return a string representation of the ACL.
     * Returns a string version of the ACL in the format compatible with libacl and
     * POSIX 1003.1e. Implementations conforming to that standard should be able
     * to take such strings as input. */
    QString asString() const;

protected:
    virtual void virtual_hook(int id, void *data);

private:
    class KACLPrivate;
    std::unique_ptr<KACLPrivate> const d;
    KIOCORE_EXPORT friend QDataStream &operator<<(QDataStream &s, const KACL &a);
    KIOCORE_EXPORT friend QDataStream &operator>>(QDataStream &s, KACL &a);
};

KIOCORE_EXPORT QDataStream &operator<<(QDataStream &s, const KACL &a);
KIOCORE_EXPORT QDataStream &operator>>(QDataStream &s, KACL &a);

#endif
