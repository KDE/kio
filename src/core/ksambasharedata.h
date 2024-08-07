/*
    SPDX-FileCopyrightText: 2010 Rodrigo Belem <rclbelem@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef ksambasharedata_h
#define ksambasharedata_h

#include "kiocore_export.h"
#include <QExplicitlySharedDataPointer>

class QString;
class KSambaShare;
class KSambaSharePrivate;
class KSambaShareDataPrivate;

/*!
 * \class KSambaShareData
 * \inmodule KIOCore
 *
 * \brief This class represents a Samba user share.
 *
 * It is possible to share a directory with one or more
 * different names, update the share details or remove.
 *
 * \since 4.7
 * \sa KSambaShare
 */
class KIOCORE_EXPORT KSambaShareData
{
public:
    /*!
     * \value GuestsNotAllowed
     * \value GuestsAllowed
     */
    enum GuestPermission {
        GuestsNotAllowed,
        GuestsAllowed,
    };

    /*!
     * \value UserShareOk
     * \value UserShareExceedMaxShares
     * \value UserShareNameOk
     * \value UserShareNameInvalid
     * \value UserShareNameInUse
     * \value UserSharePathOk
     * \value UserSharePathInvalid
     * \value UserSharePathNotExists
     * \value UserSharePathNotDirectory
     * \value UserSharePathNotAbsolute
     * \value UserSharePathNotAllowed
     * \value UserShareAclOk
     * \value UserShareAclInvalid
     * \value UserShareAclUserNotValid
     * \value UserShareCommentOk
     * \value UserShareGuestsOk
     * \value UserShareGuestsInvalid
     * \value UserShareGuestsNotAllowed
     * \value UserShareSystemError A system error occurred; check KSambaShare::lastSystemErrorString
     */
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
        UserShareSystemError,
    };

    KSambaShareData();
    KSambaShareData(const KSambaShareData &other);

    ~KSambaShareData();

    /*!
     * Returns the share name.
     */
    QString name() const;

    /*!
     * Returns the share path.
     */
    QString path() const;

    /*!
     * Returns the share comment.
     */
    QString comment() const;

    /*!
     * Containing a string describing the permission added to the users, such as
     * "[DOMAIN\]username1:X,[DOMAIN\]username2:X,...". X stands for "F" (full control), "R"
     * (read-only) and "D" (deny). By default the acl is Everyone:R.
     *
     * Returns the share acl.
     */
    QString acl() const;

    /*!
     * Returns whether guest access to the share is allowed or not.
     */
    KSambaShareData::GuestPermission guestPermission() const;

    /*!
     * Sets the share name. If the share name is changed and valid it will remove the existing
     * share and will create a new share.
     * The share name cannot use a name of a system user or containing the forbidden characters
     * '%, <, >, *, ?, |, /, \, +, =, ;, :, ",,. To check if the name is available or valid use
     * the method KSambaShare::isShareNameAvailable().
     *
     * \a name the name that will be given to the share.
     *
     * Returns UserShareNameOk if the name is valid, UserShareNameInvalid if the name contains invalid characters, UserShareNameInUse if the name is already in
     * use by another shared folder or a by a system user.
     */
    KSambaShareData::UserShareError setName(const QString &name);

    /*!
     * Set the path for the share.
     *
     * \a path the path that will be given to the share.
     *
     * Returns UserSharePathOk if valid.
     *
     * Returns UserSharePathInvalid if the path is in invalid format.
     *
     * Returns UserSharePathNotExists if the path does not exists.
     *
     * Returns UserSharePathNotDirectory if the path points to file instead of a directory.
     *
     * Returns UserSharePathNotAbsolute if the path is not is absolute form.
     *
     * Returns UserSharePathNotAllowed if the path is not owner by the user.
     */
    KSambaShareData::UserShareError setPath(const QString &path);

    /*!
     * Sets the comment for the share.
     *
     * \a comment the comment that will be given to the share.
     *
     * Returns UserShareCommentOk always.
     */
    KSambaShareData::UserShareError setComment(const QString &comment);

    /*!
     * Sets the acl to the share.
     *
     * \a acl the acl that will be given to the share.
     *
     * Returns UserShareAclOk if the acl is valid.
     *
     * Returns UserShareAclInvalid if the acl has invalid format.
     *
     * Returns UserShareAclUserNotValid if one of the users in the acl is invalid.
     */
    KSambaShareData::UserShareError setAcl(const QString &acl);

    /*!
     * Flags if guest is allowed or not to access the share.
     *
     * \a permission the permission that will be given to the share.
     *
     * Returns UserShareGuestsOk if the permission was set.
     *
     * Returns UserShareGuestsNotAllowed if the system does not allow guest access to the
     *            shares.
     */
    KSambaShareData::UserShareError setGuestPermission(const GuestPermission &permission = KSambaShareData::GuestsNotAllowed);

    /*!
     * Share the folder with the information that has been set.
     *
     * Returns UserShareOk if the share was added or other errors as applicable. Also see UserShareSystemError.
     */
    KSambaShareData::UserShareError save();

    /*!
     * Unshare the folder held by the object.
     *
     * Returns UserShareOk if the share was removed or other errors as applicable. Also see UserShareSystemError.
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
