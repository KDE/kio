/*
    SPDX-FileCopyrightText: 2010 Rodrigo Belem <rclbelem@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef ksambasharedata_p_h
#define ksambasharedata_p_h

#include <QSharedData>

class QString;

class KSambaShareDataPrivate : public QSharedData
{
public:
    KSambaShareDataPrivate() {}
    KSambaShareDataPrivate(const KSambaShareDataPrivate &other)
        : QSharedData(other)
        , name(other.name)
        , path(other.path)
        , comment(other.comment)
        , acl(other.acl)
        , guestPermission(other.guestPermission) {}

    ~KSambaShareDataPrivate() {}

    QString name;
    QString path;
    QString comment;
    QString acl;
    QString guestPermission;
};

#endif
