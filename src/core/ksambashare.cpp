/* This file is part of the KDE project
   Copyright (c) 2004 Jan Schaefer <j_schaef@informatik.uni-kl.de>
   Copyright 2010 Rodrigo Belem <rclbelem@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "ksambashare.h"
#include "ksambashare_p.h"
#include "ksambasharedata.h"
#include "ksambasharedata_p.h"
#include "kiocoredebug.h"

#include <QMap>
#include <QFile>
#include <QHostInfo>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTextStream>
#include <QStringList>
#include <QProcess>
#include <QDebug>

#include <kdirwatch.h>
#include <kuser.h>

Q_DECLARE_LOGGING_CATEGORY(KIO_CORE_SAMBASHARE)
Q_LOGGING_CATEGORY(KIO_CORE_SAMBASHARE, "kf5.kio.core.sambashare", QtWarningMsg)

// Default smb.conf locations
// sorted by priority, most priority first
static const char *const DefaultSambaConfigFilePathList[] = {
    "/etc/samba/smb.conf",
    "/etc/smb.conf",
    "/usr/local/etc/smb.conf",
    "/usr/local/samba/lib/smb.conf",
    "/usr/samba/lib/smb.conf",
    "/usr/lib/smb.conf",
    "/usr/local/lib/smb.conf"
};

KSambaSharePrivate::KSambaSharePrivate(KSambaShare *parent)
    : q_ptr(parent)
    , data()
    , userSharePath()
    , skipUserShare(false)
{
    setUserSharePath();
#if KIOCORE_BUILD_DEPRECATED_SINCE(4, 6)
    findSmbConf();
#endif
    data = parse(getNetUserShareInfo());
}

KSambaSharePrivate::~KSambaSharePrivate()
{
}

bool KSambaSharePrivate::isSambaInstalled()
{
    if (QFile::exists(QStringLiteral("/usr/sbin/smbd"))
            || QFile::exists(QStringLiteral("/usr/local/sbin/smbd"))) {
        return true;
    }

    //qDebug() << "Samba is not installed!";

    return false;
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(4, 6)
// Try to find the samba config file path
// in several well-known paths
bool KSambaSharePrivate::findSmbConf()
{
    for (const char *str : DefaultSambaConfigFilePathList) {
        const QString filePath = QString::fromLatin1(str);
        if (QFile::exists(filePath)) {
            smbConf = filePath;
            return true;
        }
    }

    qCDebug(KIO_CORE_SAMBASHARE) << "KSambaShare: Could not find smb.conf!";

    return false;
}
#endif

void KSambaSharePrivate::setUserSharePath()
{
    const QString rawString = testparmParamValue(QStringLiteral("usershare path"));
    const QFileInfo fileInfo(rawString);
    if (fileInfo.isDir()) {
        userSharePath = rawString;
    }
}

int KSambaSharePrivate::runProcess(const QString &progName, const QStringList &args,
                                   QByteArray &stdOut, QByteArray &stdErr)
{
    QProcess process;

    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(progName, args);
    //TODO: make it async in future
    process.waitForFinished();

    stdOut = process.readAllStandardOutput();
    stdErr = process.readAllStandardError();
    return process.exitCode();
}

QString KSambaSharePrivate::testparmParamValue(const QString &parameterName)
{
    if (!isSambaInstalled()) {
        return QString();
    }

    QByteArray stdErr;
    QByteArray stdOut;

    const QStringList args{
        QStringLiteral("-d0"),
        QStringLiteral("-s"),
        QStringLiteral("--parameter-name"),
        parameterName,
    };

    runProcess(QStringLiteral("testparm"), args, stdOut, stdErr);

    //TODO: parse and process error messages.
    // create a parser for the error output and
    // send error message somewhere
    if (!stdErr.isEmpty()) {
        QList<QByteArray> err;
        err << stdErr.trimmed().split('\n');
        // ignore first two lines
        if (err.count() > 0 && err.at(0).startsWith("Load smb config files from")) {
            err.removeFirst();
        }
        if (err.count() > 0 && err.at(0).startsWith("Loaded services file OK.")) {
            err.removeFirst();
        }
        if (err.count() > 0 && err.at(0).startsWith("WARNING: The 'netbios name' is too long (max. 15 chars).")) {
            // netbios name must be of at most 15 characters long
            // means either netbios name is badly configured
            // or not set and the default value is being used, it being "$(hostname)-W"
            // which means any hostname longer than 13 characters will cause this warning
            // when no netbios name was defined
            // See https://www.novell.com/documentation/open-enterprise-server-2018/file_samba_cifs_lx/data/bc855e3.html
            const QString defaultNetbiosName = QHostInfo::localHostName().append(QStringLiteral("-W"));
            if (defaultNetbiosName.length() > 14) {
                qCWarning(KIO_CORE) << "Your samba 'netbios name' parameter was longer than the authorized 15 characters.\n"
                                    << "It may be because your hostname is longer than 13 and samba default 'netbios name' defaults to 'hostname-W', here:" << defaultNetbiosName<< "\n"
                                    << "If that it is the case simply define a 'netbios name' parameter in /etc/samba/smb.conf at most 15 characters long";
            } else {
                qCWarning(KIO_CORE) << "Your samba 'netbios name' parameter was longer than the authorized 15 characters."
                                    << "Please define a 'netbios name' parameter in /etc/samba/smb.conf at most 15 characters long";
            }
            err.removeFirst();
        }
        if (err.count() > 0) {
            qCWarning(KIO_CORE) << "We got some errors while running testparm" << err.join("\n");
        }
    }

    if (!stdOut.isEmpty()) {
        return QString::fromLocal8Bit(stdOut.trimmed());
    }

    return QString();
}

QByteArray KSambaSharePrivate::getNetUserShareInfo()
{
    if (skipUserShare || !isSambaInstalled()) {
        return QByteArray();
    }

    QByteArray stdOut;
    QByteArray stdErr;

    const QStringList args{
        QStringLiteral("usershare"),
        QStringLiteral("info"),
    };

    runProcess(QStringLiteral("net"), args, stdOut, stdErr);

    if (!stdErr.isEmpty()) {
        if (stdErr.contains("You do not have permission to create a usershare")) {
            skipUserShare = true;
        } else if (stdErr.contains("usershares are currently disabled")) {
            skipUserShare = true;
        } else {
            //TODO: parse and process other error messages.
            // create a parser for the error output and
            // send error message somewhere
            qCWarning(KIO_CORE) << "We got some errors while running 'net usershare info'";
            qCWarning(KIO_CORE) << stdErr;
        }
    }

    return stdOut;
}

QStringList KSambaSharePrivate::shareNames() const
{
    return data.keys();
}

QStringList KSambaSharePrivate::sharedDirs() const
{
    QStringList dirs;

    QMap<QString, KSambaShareData>::ConstIterator i;
    for (i = data.constBegin(); i != data.constEnd(); ++i) {
        if (!dirs.contains(i.value().path())) {
            dirs << i.value().path();
        }
    }

    return dirs;
}

KSambaShareData KSambaSharePrivate::getShareByName(const QString &shareName) const
{
    return data.value(shareName);
}

QList<KSambaShareData> KSambaSharePrivate::getSharesByPath(const QString &path) const
{
    QList<KSambaShareData> shares;

    QMap<QString, KSambaShareData>::ConstIterator i;
    for (i = data.constBegin(); i != data.constEnd(); ++i) {
        if (i.value().path() == path) {
            shares << i.value();
        }
    }

    return shares;
}

bool KSambaSharePrivate::isShareNameValid(const QString &name) const
{
    // Samba forbidden chars
    const QRegularExpression notToMatchRx(QStringLiteral("[%<>*\?|/+=;:\",]"));
    return !notToMatchRx.match(name).hasMatch();
}

bool KSambaSharePrivate::isDirectoryShared(const QString &path) const
{
    QMap<QString, KSambaShareData>::ConstIterator i;
    for (i = data.constBegin(); i != data.constEnd(); ++i) {
        if (i.value().path() == path) {
            return true;
        }
    }

    return false;
}

bool KSambaSharePrivate::isShareNameAvailable(const QString &name) const
{
    // Samba does not allow to name a share with a user name registered in the system
    return (!KUser::allUserNames().contains(name) || !data.contains(name));
}

KSambaShareData::UserShareError KSambaSharePrivate::isPathValid(const QString &path) const
{
    QFileInfo pathInfo = path;

    if (!pathInfo.exists()) {
        return KSambaShareData::UserSharePathNotExists;
    }

    if (!pathInfo.isDir()) {
        return KSambaShareData::UserSharePathNotDirectory;
    }

    if (pathInfo.isRelative()) {
        if (pathInfo.makeAbsolute()) {
            return KSambaShareData::UserSharePathNotAbsolute;
        }
    }

    // TODO: check if the user is root
    if (KSambaSharePrivate::testparmParamValue(QStringLiteral("usershare owner only"))
            == QLatin1String("Yes")) {
        if (!pathInfo.permission(QFile::ReadUser | QFile::WriteUser)) {
            return KSambaShareData::UserSharePathNotAllowed;
        }
    }

    return KSambaShareData::UserSharePathOk;
}

KSambaShareData::UserShareError KSambaSharePrivate::isAclValid(const QString &acl) const
{
    const QRegularExpression aclRx(QRegularExpression::anchoredPattern(
                                    QStringLiteral("(?:(?:(\\w(\\w|\\s)*)\\\\|)(\\w+\\s*):([fFrRd]{1})(?:,|))*")));
    // TODO: check if user is a valid smb user
    return aclRx.match(acl).hasMatch() ? KSambaShareData::UserShareAclOk
                                         : KSambaShareData::UserShareAclInvalid;
}

KSambaShareData::UserShareError KSambaSharePrivate::guestsAllowed(const
        KSambaShareData::GuestPermission &guestok) const
{
    if (guestok == KSambaShareData::GuestsAllowed) {
        if (KSambaSharePrivate::testparmParamValue(QStringLiteral("usershare allow guests"))
                == QLatin1String("No")) {
            return KSambaShareData::UserShareGuestsNotAllowed;
        }
    }

    return KSambaShareData::UserShareGuestsOk;
}

KSambaShareData::UserShareError KSambaSharePrivate::add(const KSambaShareData &shareData)
{
    // TODO:
    // * check for usershare max shares

    if (!isSambaInstalled()) {
        return KSambaShareData::UserShareSystemError;
    }

    QByteArray stdOut;
    QByteArray stdErr;

    if (data.contains(shareData.name())) {
        if (data.value(shareData.name()).path() != shareData.path()) {
            return KSambaShareData::UserShareNameInUse;
        }
    } else {
        // It needs to be added here, otherwise another instance of KSambaShareDataPrivate
        // will be created and added to data.
        data.insert(shareData.name(), shareData);
    }

    QString guestok = QStringLiteral("guest_ok=%1").arg(
                          (shareData.guestPermission() == KSambaShareData::GuestsNotAllowed)
                          ? QStringLiteral("n") : QStringLiteral("y"));

    const QStringList args{
        QStringLiteral("usershare"),
        QStringLiteral("add"),
        shareData.name(),
        shareData.path(),
        shareData.comment(),
        shareData.acl(),
        guestok,
    };

    int ret = runProcess(QStringLiteral("net"), args, stdOut, stdErr);

    //TODO: parse and process error messages.
    if (!stdErr.isEmpty()) {
        // create a parser for the error output and
        // send error message somewhere
        qCWarning(KIO_CORE) << "We got some errors while running 'net usershare add'" << args;
        qCWarning(KIO_CORE) << stdErr;
    }

    return (ret == 0) ? KSambaShareData::UserShareOk : KSambaShareData::UserShareSystemError;
}

KSambaShareData::UserShareError KSambaSharePrivate::remove(const KSambaShareData &shareData) const
{
    if (!isSambaInstalled()) {
        return KSambaShareData::UserShareSystemError;
    }


    if (!data.contains(shareData.name())) {
        return KSambaShareData::UserShareNameInvalid;
    }

    const QStringList args{
        QStringLiteral("usershare"),
        QStringLiteral("delete"),
        shareData.name(),
    };

    int result = QProcess::execute(QStringLiteral("net"), args);
    return (result == 0) ? KSambaShareData::UserShareOk : KSambaShareData::UserShareSystemError;
}

QMap<QString, KSambaShareData> KSambaSharePrivate::parse(const QByteArray &usershareData)
{
    const QRegularExpression headerRx(QRegularExpression::anchoredPattern(
                                        QStringLiteral("^\\s*\\["
                                                       "([^%<>*\?|/+=;:\",]+)"
                                                       "\\]")));

    const QRegularExpression OptValRx(QRegularExpression::anchoredPattern(
                                        QStringLiteral("^\\s*([\\w\\d\\s]+)"
                                                       "="
                                                       "(.*)$")));

    QTextStream stream(usershareData);
    QString currentShare;
    QMap<QString, KSambaShareData> shares;

    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();

        QRegularExpressionMatch match;
        if ((match = headerRx.match(line)).hasMatch()) {
            currentShare = match.captured(1).trimmed();

            if (!shares.contains(currentShare)) {
                KSambaShareData shareData;
                shareData.dd->name = currentShare;
                shares.insert(currentShare, shareData);
            }
        } else if ((match = OptValRx.match(line)).hasMatch()) {
            const QString key = match.captured(1).trimmed();
            const QString value = match.captured(2).trimmed();
            KSambaShareData shareData = shares[currentShare];

            if (key == QLatin1String("path")) {
                // Samba accepts paths with and w/o trailing slash, we
                // use and expect path without slash
                shareData.dd->path = value.endsWith(QLatin1Char('/')) ? value.chopped(1) : value;
            } else if (key == QLatin1String("comment")) {
                shareData.dd->comment = value;
            } else if (key == QLatin1String("usershare_acl")) {
                shareData.dd->acl = value;
            } else if (key == QLatin1String("guest_ok")) {
                shareData.dd->guestPermission = value;
            } else {
                qCWarning(KIO_CORE) << "Something nasty happen while parsing 'net usershare info'"
                           << "share:" << currentShare << "key:" << key;
            }
        } else if (line.trimmed().isEmpty()) {
            continue;
        } else {
            return shares;
        }
    }

    return shares;
}

void KSambaSharePrivate::_k_slotFileChange(const QString &path)
{
    if (path != userSharePath) {
        return;
    }
    data = parse(getNetUserShareInfo());
    //qDebug() << "path changed:" << path;
    Q_Q(KSambaShare);
    emit q->changed();
}

KSambaShare::KSambaShare()
    : QObject(nullptr)
    , d_ptr(new KSambaSharePrivate(this))
{
    Q_D(const KSambaShare);
    if (!d->userSharePath.isEmpty() && QFileInfo::exists(d->userSharePath)) {
        KDirWatch::self()->addDir(d->userSharePath, KDirWatch::WatchFiles);
        connect(KDirWatch::self(), SIGNAL(dirty(QString)), this,
                SLOT(_k_slotFileChange(QString)));
    }
}

KSambaShare::~KSambaShare()
{
    Q_D(const KSambaShare);
    if (KDirWatch::exists() && KDirWatch::self()->contains(d->userSharePath)) {
        KDirWatch::self()->removeDir(d->userSharePath);
    }
    delete d_ptr;
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(4, 6)
QString KSambaShare::smbConfPath() const
{
    Q_D(const KSambaShare);
    return d->smbConf;
}
#endif

bool KSambaShare::isDirectoryShared(const QString &path) const
{
    Q_D(const KSambaShare);
    return d->isDirectoryShared(path);
}

bool KSambaShare::isShareNameAvailable(const QString &name) const
{
    Q_D(const KSambaShare);
    return d->isShareNameValid(name) && d->isShareNameAvailable(name);
}

QStringList KSambaShare::shareNames() const
{
    Q_D(const KSambaShare);
    return d->shareNames();
}

QStringList KSambaShare::sharedDirectories() const
{
    Q_D(const KSambaShare);
    return d->sharedDirs();
}

KSambaShareData KSambaShare::getShareByName(const QString &name) const
{
    Q_D(const KSambaShare);
    return d->getShareByName(name);
}

QList<KSambaShareData> KSambaShare::getSharesByPath(const QString &path) const
{
    Q_D(const KSambaShare);
    return d->getSharesByPath(path);
}

class KSambaShareSingleton
{
public:
    KSambaShare instance;
};

Q_GLOBAL_STATIC(KSambaShareSingleton, _instance)

KSambaShare *KSambaShare::instance()
{
    return &_instance()->instance;
}

#include "moc_ksambashare.cpp"
