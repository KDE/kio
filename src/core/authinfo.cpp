/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2001 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "authinfo.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDataStream>
#include <QDBusArgument>
#include <QDBusMetaType>

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
        : customTitle(other.customTitle),
          flags(other.flags),
          value(other.value)
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

static QDataStream &operator<< (QDataStream &s, const ExtraField &extraField)
{
    s << extraField.customTitle;
    s << static_cast<int>(extraField.flags);
    s << extraField.value;
    return s;
}

static QDataStream &operator>> (QDataStream &s, ExtraField &extraField)
{
    s >> extraField.customTitle;
    int i;
    s >> i;
    extraField.flags = AuthInfo::FieldFlags(i);
    s >> extraField.value;
    return s;
}

static QDBusArgument &operator<<(QDBusArgument &argument, const ExtraField &extraField)
{
    argument.beginStructure();
    argument << extraField.customTitle << static_cast<int>(extraField.flags)
             << QDBusVariant(extraField.value);
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

class KIO::AuthInfoPrivate
{
public:
    QMap<QString, ExtraField> extraFields;
};

//////

AuthInfo::AuthInfo() : d(new AuthInfoPrivate())
{
    modified = false;
    readOnly = false;
    verifyPath = false;
    keepPassword = false;
    AuthInfo::registerMetaTypes();
}

AuthInfo::AuthInfo(const AuthInfo &info) : d(new AuthInfoPrivate())
{
    (*this) = info;
    AuthInfo::registerMetaTypes();
}

AuthInfo::~AuthInfo()
{
    delete d;
}

AuthInfo &AuthInfo::operator= (const AuthInfo &info)
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
    qDBusRegisterMetaType<ExtraField>();
    qDBusRegisterMetaType<KIO::AuthInfo>();
}

/////

QDataStream &KIO::operator<< (QDataStream &s, const AuthInfo &a)
{
    s << quint8(1)
      << a.url << a.username << a.password << a.prompt << a.caption
      << a.comment << a.commentLabel << a.realmValue << a.digestInfo
      << a.verifyPath << a.readOnly << a.keepPassword << a.modified
      << a.d->extraFields;
    return s;
}

QDataStream &KIO::operator>> (QDataStream &s, AuthInfo &a)
{
    quint8 version;
    s >> version
      >> a.url >> a.username >> a.password >> a.prompt >> a.caption
      >> a.comment >> a.commentLabel >> a.realmValue >> a.digestInfo
      >> a.verifyPath >> a.readOnly >> a.keepPassword >> a.modified
      >> a.d->extraFields;
    return s;
}

QDBusArgument &KIO::operator<<(QDBusArgument &argument, const AuthInfo &a)
{
    argument.beginStructure();
    argument << quint8(1)
             << a.url.toString() << a.username << a.password << a.prompt << a.caption
             << a.comment << a.commentLabel << a.realmValue << a.digestInfo
             << a.verifyPath << a.readOnly << a.keepPassword << a.modified
             << a.d->extraFields;
    argument.endStructure();
    return argument;
}

const QDBusArgument &KIO::operator>>(const QDBusArgument &argument, AuthInfo &a)
{
    QString url;
    quint8 version;

    argument.beginStructure();
    argument >> version
             >> url >> a.username >> a.password >> a.prompt >> a.caption
             >> a.comment >> a.commentLabel >> a.realmValue >> a.digestInfo
             >> a.verifyPath >> a.readOnly >> a.keepPassword >> a.modified
             >> a.d->extraFields;
    argument.endStructure();

    a.url = QUrl(url);
    return argument;
}

typedef QList<NetRC::AutoLogin> LoginList;
typedef QMap<QString, LoginList> LoginMap;

class Q_DECL_HIDDEN NetRC::NetRCPrivate
{
public:
    NetRCPrivate()
        : isDirty(false),
          index(-1)
    {}
    QString extract(const QString &buf, const QString &key);
    void getMachinePart(const QString &line);
    void getMacdefPart(const QString &line);

    bool isDirty;
    LoginMap loginMap;
    QTextStream fstream;
    QString type;
    int index;
};

NetRC *NetRC::instance = nullptr;

NetRC::NetRC()
    : d(new NetRCPrivate)
{
}

NetRC::~NetRC()
{
    delete instance;
    instance = nullptr;
    delete d;
}

NetRC *NetRC::self()
{
    if (!instance) {
        instance = new NetRC;
    }
    return instance;
}

bool NetRC::lookup(const QUrl &url, AutoLogin &login, bool userealnetrc,
                   const QString &_type, LookUpMode mode)
{
    //qDebug() << "AutoLogin lookup for: " << url.host();
    if (!url.isValid()) {
        return false;
    }

    QString type = _type;
    if (type.isEmpty()) {
        type = url.scheme();
    }

    if (d->loginMap.isEmpty() || d->isDirty) {
        d->loginMap.clear();

        QString filename = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1String("/kionetrc");
        bool kionetrcStatus = parse(filename);
        bool netrcStatus = false;
        if (userealnetrc) {
            filename =  QDir::homePath() + QLatin1String("/.netrc");
            netrcStatus = parse(filename);
        }

        if (!(kionetrcStatus || netrcStatus)) {
            return false;
        }
    }

    const auto loginIt = d->loginMap.constFind(type);
    if (loginIt == d->loginMap.constEnd()) {
        return false;
    }

    const LoginList &l = *loginIt;
    if (l.isEmpty()) {
        return false;
    }

    for (const AutoLogin &log : l) {
        if ((mode & defaultOnly) == defaultOnly &&
                log.machine == QLatin1String("default") &&
                (login.login.isEmpty() || login.login == log.login)) {
            login.type = log.type;
            login.machine = log.machine;
            login.login = log.login;
            login.password = log.password;
            login.macdef = log.macdef;
        }

        if ((mode & presetOnly) == presetOnly &&
                log.machine == QLatin1String("preset") &&
                (login.login.isEmpty() || login.login == log.login)) {
            login.type = log.type;
            login.machine = log.machine;
            login.login = log.login;
            login.password = log.password;
            login.macdef = log.macdef;
        }

        if ((mode & exactOnly) == exactOnly &&
                log.machine == url.host() &&
                (login.login.isEmpty() || login.login == log.login)) {
            login.type = log.type;
            login.machine = log.machine;
            login.login = log.login;
            login.password = log.password;
            login.macdef = log.macdef;
            break;
        }
    }

    return true;
}

void NetRC::reload()
{
    d->isDirty = true;
}

bool NetRC::parse(const QString &fileName)
{
    QFile file(fileName);
    if (file.permissions() != (QFile::ReadOwner | QFile::WriteOwner
                               | QFile::ReadUser | QFile::WriteUser)) {
        return false;

    }
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    d->fstream.setDevice(&file);

    QString line;

    while (!d->fstream.atEnd()) {
        line = d->fstream.readLine().simplified();

        // If line is a comment or is empty, read next line
        if ((line.startsWith(QLatin1Char('#')) || line.isEmpty())) {
            continue;
        }

        // If line refers to a machine, maybe it is spread in more lines.
        // getMachinePart() will take care of getting all the info and putting it into loginMap.
        if ((line.startsWith(QLatin1String("machine"))
                || line.startsWith(QLatin1String("default"))
                || line.startsWith(QLatin1String("preset")))) {
            d->getMachinePart(line);
            continue;
        }

        // If line refers to a macdef, it will be more than one line.
        // getMacdefPart() will take care of getting all the lines of the macro
        // and putting them into loginMap
        if (line.startsWith(QLatin1String("macdef"))) {
            d->getMacdefPart(line);
            continue;
        }
    }
    return true;
}

QString NetRC::NetRCPrivate::extract(const QString &buf, const QString &key)
{
    QStringList stringList = buf.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    int i = stringList.indexOf(key);
    if ((i != -1) && (i + 1 < stringList.size())) {
        return stringList.at(i + 1);
    } else {
        return QString();
    }
}

void NetRC::NetRCPrivate::getMachinePart(const QString &line)
{
    QString buf = line;
    while (!(buf.contains(QLatin1String("login"))
             && (buf.contains(QLatin1String("password")) || buf.contains(QLatin1String("account")) || buf.contains(QLatin1String("type"))))) {
        buf += QLatin1Char(' ') + fstream.readLine().simplified();
    }

    // Once we've got all the info, process it.
    AutoLogin l;
    l.machine = extract(buf, QStringLiteral("machine"));
    if (l.machine.isEmpty()) {
        if (buf.contains(QLatin1String("default"))) {
            l.machine = QStringLiteral("default");
        } else if (buf.contains(QLatin1String("preset"))) {
            l.machine = QStringLiteral("preset");
        }
    }

    l.login = extract(buf, QStringLiteral("login"));
    l.password = extract(buf, QStringLiteral("password"));
    if (l.password.isEmpty()) {
        l.password = extract(buf, QStringLiteral("account"));
    }

    type = l.type = extract(buf, QStringLiteral("type"));
    if (l.type.isEmpty() && !l.machine.isEmpty()) {
        type = l.type = QStringLiteral("ftp");
    }

    loginMap[l.type].append(l);
    index = loginMap[l.type].count() - 1;
}

void NetRC::NetRCPrivate::getMacdefPart(const QString &line)
{
    QString buf = line;
    QString macro = extract(buf, QStringLiteral("macdef"));
    QString newLine;
    while (!fstream.atEnd()) {
        newLine = fstream.readLine().simplified();
        if (!newLine.isEmpty()) {
            buf += QLatin1Char('\n') + newLine;
        } else {
            break;
        }
    }
    loginMap[type][index].macdef[macro].append(buf);
}
