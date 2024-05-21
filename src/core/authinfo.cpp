/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2001 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "authinfo.h"

#ifdef WITH_QTDBUS
#include <QDBusArgument>
#include <QDBusMetaType>
#endif
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include <QStandardPaths>

using namespace KIO;

//////

class ExtraField
{
public:
    ExtraField()
        : flags(AuthInfo::ExtraFieldNoFlags)
    {
    }

    ExtraField(const ExtraField &other)
        : customTitle(other.customTitle)
        , flags(other.flags)
        , value(other.value)
    {
    }

    ExtraField &operator=(const ExtraField &other)
    {
        customTitle = other.customTitle;
        flags = other.flags;
        value = other.value;
        return *this;
    }

    QString customTitle; // reserved for future use
    AuthInfo::FieldFlags flags;
    QVariant value;
};
Q_DECLARE_METATYPE(ExtraField)

static QDataStream &operator<<(QDataStream &s, const ExtraField &extraField)
{
    s << extraField.customTitle;
    s << static_cast<int>(extraField.flags);
    s << extraField.value;
    return s;
}

static QDataStream &operator>>(QDataStream &s, ExtraField &extraField)
{
    s >> extraField.customTitle;
    int i;
    s >> i;
    extraField.flags = AuthInfo::FieldFlags(i);
    s >> extraField.value;
    return s;
}

#ifdef WITH_QTDBUS
static QDBusArgument &operator<<(QDBusArgument &argument, const ExtraField &extraField)
{
    argument.beginStructure();
    argument << extraField.customTitle << static_cast<int>(extraField.flags) << QDBusVariant(extraField.value);
    argument.endStructure();
    return argument;
}

static const QDBusArgument &operator>>(const QDBusArgument &argument, ExtraField &extraField)
{
    QDBusVariant value;
    int flag;

    argument.beginStructure();
    argument >> extraField.customTitle >> flag >> value;
    argument.endStructure();

    extraField.value = value.variant();
    extraField.flags = KIO::AuthInfo::FieldFlags(flag);
    return argument;
}
#endif

class KIO::AuthInfoPrivate
{
public:
    QMap<QString, ExtraField> extraFields;
};

//////

AuthInfo::AuthInfo()
    : d(new AuthInfoPrivate())
{
    modified = false;
    readOnly = false;
    verifyPath = false;
    keepPassword = false;
    AuthInfo::registerMetaTypes();
}

AuthInfo::AuthInfo(const AuthInfo &info)
    : d(new AuthInfoPrivate())
{
    (*this) = info;
    AuthInfo::registerMetaTypes();
}

AuthInfo::~AuthInfo() = default;

AuthInfo &AuthInfo::operator=(const AuthInfo &info)
{
    url = info.url;
    username = info.username;
    password = info.password;
    prompt = info.prompt;
    caption = info.caption;
    comment = info.comment;
    commentLabel = info.commentLabel;
    realmValue = info.realmValue;
    digestInfo = info.digestInfo;
    verifyPath = info.verifyPath;
    readOnly = info.readOnly;
    keepPassword = info.keepPassword;
    modified = info.modified;
    d->extraFields = info.d->extraFields;
    return *this;
}

bool AuthInfo::isModified() const
{
    return modified;
}

void AuthInfo::setModified(bool flag)
{
    modified = flag;
}

/////

void AuthInfo::setExtraField(const QString &fieldName, const QVariant &value)
{
    d->extraFields[fieldName].value = value;
}

void AuthInfo::setExtraFieldFlags(const QString &fieldName, const FieldFlags flags)
{
    d->extraFields[fieldName].flags = flags;
}

QVariant AuthInfo::getExtraField(const QString &fieldName) const
{
    const auto it = d->extraFields.constFind(fieldName);
    if (it == d->extraFields.constEnd()) {
        return QVariant();
    }
    return it->value;
}

AuthInfo::FieldFlags AuthInfo::getExtraFieldFlags(const QString &fieldName) const
{
    const auto it = d->extraFields.constFind(fieldName);
    if (it == d->extraFields.constEnd()) {
        return AuthInfo::ExtraFieldNoFlags;
    }
    return it->flags;
}

void AuthInfo::registerMetaTypes()
{
    qRegisterMetaType<ExtraField>();
    qRegisterMetaType<KIO::AuthInfo>();
#ifdef WITH_QTDBUS
    qDBusRegisterMetaType<ExtraField>();
    qDBusRegisterMetaType<KIO::AuthInfo>();
#endif
}

/////

QDataStream &KIO::operator<<(QDataStream &s, const AuthInfo &a)
{
    s << quint8(1) << a.url << a.username << a.password << a.prompt << a.caption << a.comment << a.commentLabel << a.realmValue << a.digestInfo << a.verifyPath
      << a.readOnly << a.keepPassword << a.modified << a.d->extraFields;
    return s;
}

QDataStream &KIO::operator>>(QDataStream &s, AuthInfo &a)
{
    quint8 version;
    s >> version >> a.url >> a.username >> a.password >> a.prompt >> a.caption >> a.comment >> a.commentLabel >> a.realmValue >> a.digestInfo >> a.verifyPath
        >> a.readOnly >> a.keepPassword >> a.modified >> a.d->extraFields;
    return s;
}

#ifdef WITH_QTDBUS
QDBusArgument &KIO::operator<<(QDBusArgument &argument, const AuthInfo &a)
{
    argument.beginStructure();
    argument << quint8(1) << a.url.toString() << a.username << a.password << a.prompt << a.caption << a.comment << a.commentLabel << a.realmValue
             << a.digestInfo << a.verifyPath << a.readOnly << a.keepPassword << a.modified << a.d->extraFields;
    argument.endStructure();
    return argument;
}

const QDBusArgument &KIO::operator>>(const QDBusArgument &argument, AuthInfo &a)
{
    QString url;
    quint8 version;

    argument.beginStructure();
    argument >> version >> url >> a.username >> a.password >> a.prompt >> a.caption >> a.comment >> a.commentLabel >> a.realmValue >> a.digestInfo
        >> a.verifyPath >> a.readOnly >> a.keepPassword >> a.modified >> a.d->extraFields;
    argument.endStructure();

    a.url = QUrl(url);
    return argument;
}
#endif
