/*
 *   Copyright 2010 Rodrigo Belem <rclbelem@gmail.com>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) version 3, or any
 *   later version accepted by the membership of KDE e.V. (or its
 *   successor approved by the membership of KDE e.V.), which shall
 *   act as a proxy defined in Section 6 of version 3 of the license.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library.  If not, see <http://www.gnu.org/licenses/>
 */

#include <QtCore/QRegExp>
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>

#include "ksambashare.h"
#include "ksambashare_p.h"
#include "ksambasharedata.h"
#include "ksambasharedata_p.h"

//TODO: add support for this samba options
// usershare allow guests=P_BOOL,FLAG_ADVANCED
// usershare max shares=P_INTEGER,FLAG_ADVANCED
// usershare owner only=P_BOOL,FLAG_ADVANCED
// usershare path=P_STRING,FLAG_ADVANCED
// usershare prefix allow list=P_LIST,FLAG_ADVANCED
// usershare prefix deny list=P_LIST,FLAG_ADVANCED
// usershare template share=P_STRING,FLAG_ADVANCED

KSambaShareData::KSambaShareData()
    : dd(new KSambaShareDataPrivate)
{
}

KSambaShareData::KSambaShareData(const KSambaShareData &other)
    : dd(other.dd)
{
}

KSambaShareData::~KSambaShareData()
{
}

QString KSambaShareData::name() const
{
    return dd->name;
}

QString KSambaShareData::path() const
{
    return dd->path;
}

QString KSambaShareData::comment() const
{
    return dd->comment;
}

QString KSambaShareData::acl() const
{
    return dd->acl;
}

KSambaShareData::GuestPermission KSambaShareData::guestPermission() const
{
    return (dd->guestPermission == QLatin1String("n")) ? GuestsNotAllowed : GuestsAllowed;
}

KSambaShareData::UserShareError KSambaShareData::setName(const QString &name)
{
    if (!KSambaShare::instance()->d_func()->isShareNameValid(name)) {
        return UserShareNameInvalid;
    }

    if (!KSambaShare::instance()->d_func()->isShareNameAvailable(name)) {
        return UserShareNameInUse;
    }


    if (!dd->name.isEmpty()) {
        dd.detach();
    }

    dd->name = name;

    return UserShareNameOk;
}

KSambaShareData::UserShareError KSambaShareData::setPath(const QString &path)
{
    UserShareError result = KSambaShare::instance()->d_func()->isPathValid(path);
    if (result == UserSharePathOk) {
        dd->path = path;
    }

    return result;
}

KSambaShareData::UserShareError KSambaShareData::setComment(const QString &comment)
{
    dd->comment = comment;

    return UserShareCommentOk;
}

KSambaShareData::UserShareError KSambaShareData::setAcl(const QString &acl)
{
    UserShareError result = KSambaShare::instance()->d_func()->isAclValid(acl);
    if (result == UserShareAclOk) {
        dd->acl = acl;
    }

    return result;
}

KSambaShareData::UserShareError KSambaShareData::setGuestPermission(const GuestPermission &permission)
{
    UserShareError result = KSambaShare::instance()->d_func()->guestsAllowed(permission);
    if (result == UserShareGuestsOk) {
        dd->guestPermission = (permission == GuestsNotAllowed) ? "n" : "y";
    }

    return result;
}

KSambaShareData::UserShareError KSambaShareData::save()
{
    if (dd->name.isEmpty()) {
        return UserShareNameInvalid;
    } else if (dd->path.isEmpty()) {
        return UserSharePathInvalid;
    } else {
        return KSambaShare::instance()->d_func()->add(*this);
    }
}

KSambaShareData::UserShareError KSambaShareData::remove()
{
    if (dd->name.isEmpty()) {
        return UserShareNameInvalid;
    } else {
        return KSambaShare::instance()->d_func()->remove(*this);
    }
}

KSambaShareData &KSambaShareData::operator=(const KSambaShareData &other)
{
    if (&other != this) {
        dd = other.dd;
    }

    return *this;
}

bool KSambaShareData::operator==(const KSambaShareData &other) const
{
    return other.dd == dd;
}

bool KSambaShareData::operator!=(const KSambaShareData &other) const
{
    return !(&other == this);
}
