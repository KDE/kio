/*
    SPDX-FileCopyrightText: 2010 Rodrigo Belem <rclbelem@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef ksambasharedata_h
#define ksambasharedata_h

#include <QExplicitlySharedDataPointer>
#include "kiocore_export.h"

class QString;
class KSambaShare;
class KSambaSharePrivate;
class KSambaShareDataPrivate;

/**
 * @class KSambaShareData ksambasharedata.h <KSambaShareData>
 *
 * This class represents a Samba user share. It is possible to share a directory with one or more
 * different names, update the share details or remove.
 *
 * @author Rodrigo Belem <rclbelem@gmail.com>
 * @since  4.7
 */
class KIOCORE_EXPORT KSambaShareData
{

public:
    enum GuestPermission {
        GuestsNotAllowed,
        GuestsAllowed,
    };

    enum UserShareError {
        UserShareOk,
        UserShareExceedMaxShares,
        UserShareNameOk,
        UserShareNameInvalid,
        UserShareNameInUse,
        UserSharePathOk,
        UserSharePathInvalid,
        UserSharePathNotExists,
        UserSharePathNotDirectory,
        UserSharePathNotAbsolute,
        UserSharePathNotAllowed,
        UserShareAclOk,
        UserShareAclInvalid,
        UserShareAclUserNotValid,
        UserShareCommentOk,
        UserShareGuestsOk,
        UserShareGuestsInvalid,
        UserShareGuestsNotAllowed,
        UserShareSystemError, /* < A system error occurred; check KSambaShare::lastSystemErrorString */
    };

    KSambaShareData();
    KSambaShareData(const KSambaShareData &other);

    ~KSambaShareData();

    /**
     * @return @c the share name.
     */
    QString name() const;

    /**
     * @return @c the share path.
     */
    QString path() const;

    /**
     * @return @c the share comment.
     */
    QString comment() const;

    /**
     * Returns a @c containing a string describing the permission added to the users, such as
     * "[DOMAIN\]username1:X,[DOMAIN\]username2:X,...". X stands for "F" (full control), "R"
     * (read-only) and "D" (deny). By default the acl is Everyone:R.
     *
     * @return @c the share acl.
     */
    QString acl() const;

    /**
     * @return @c whether guest access to the share is allowed or not.
     */
    KSambaShareData::GuestPermission guestPermission() const;

    /**
     * Sets the share name. If the share name is changed and valid it will remove the existing
     * share and will create a new share.
     * The share name cannot use a name of a system user or containing the forbidden characters
     * '%, <, >, *, ?, |, /, \, +, =, ;, :, ",,. To check if the name is available or valid use
     * the method KSambaShare::isShareNameAvailable().
     *
     * @param name the name that will be given to the share.
     *
     * @return @c UserShareNameOk if the name is valid.
     * @return @c UserShareNameInvalid if the name contains invalid characters.
     * @return @c UserShareNameInUse if the name is already in use by another shared folder or a
     *            by a system user.
     */
    KSambaShareData::UserShareError setName(const QString &name);

    /**
     * Set the path for the share.
     *
     * @param path the path that will be given to the share.
     *
     * @return @c UserSharePathOk if valid.
     * @return @c UserSharePathInvalid if the path is in invalid format.
     * @return @c UserSharePathNotExists if the path does not exists.
     * @return @c UserSharePathNotDirectory if the path points to file instead of a directory.
     * @return @c UserSharePathNotAbsolute if the path is not is absolute form.
     * @return @c UserSharePathNotAllowed if the path is not owner by the user.
     */
    KSambaShareData::UserShareError setPath(const QString &path);

    /**
     * Sets the comment for the share.
     *
     * @param comment the comment that will be given to the share.
     *
     * @return @c UserShareCommentOk always.
     */
    KSambaShareData::UserShareError setComment(const QString &comment);

    /**
     * Sets the acl to the share.
     *
     * @param acl the acl that will be given to the share.
     *
     * @return @c UserShareAclOk if the acl is valid.
     * @return @c UserShareAclInvalid if the acl has invalid format.
     * @return @c UserShareAclUserNotValid if one of the users in the acl is invalid.
     */
    KSambaShareData::UserShareError setAcl(const QString &acl);

    /**
     * Flags if guest is allowed or not to access the share.
     *
     * @param permission the permission that will be given to the share.
     *
     * @return @c UserShareGuestsOk if the permission was set.
     * @return @c UserShareGuestsNotAllowed if the system does not allow guest access to the
     *            shares.
     */
    KSambaShareData::UserShareError setGuestPermission(const GuestPermission &permission = KSambaShareData::GuestsNotAllowed);

    /**
     * Share the folder with the information that has been set.
     *
     * @return @c UserShareOk if the share was added or other errors as applicable. Also see UserShareSystemError.
     */
    KSambaShareData::UserShareError save();

    /**
     * Unshare the folder held by the object.
     *
     * @return @c UserShareOk if the share was removed or other errors as applicable. Also see UserShareSystemError.
     */
    KSambaShareData::UserShareError remove();

    KSambaShareData &operator=(const KSambaShareData &other);
    bool operator==(const KSambaShareData &other) const;
    bool operator!=(const KSambaShareData &other) const;

private:
    QExplicitlySharedDataPointer<KSambaShareDataPrivate> dd;

    friend class KSambaSharePrivate;
};

#endif
