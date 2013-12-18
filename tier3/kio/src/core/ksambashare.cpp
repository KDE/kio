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

#include <QtCore/QMap>
#include <QtCore/QMutableMapIterator>
#include <QtCore/QFile>
#include <QtCore/QRegExp>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <QtCore/QStringList>
#include <QtCore/QProcess>
#include <QDebug>

#include <kdirwatch.h>
#include <kuser.h>

// Default smb.conf locations
// sorted by priority, most priority first
static const char * const DefaultSambaConfigFilePathList[] =
{
  "/etc/samba/smb.conf",
  "/etc/smb.conf",
  "/usr/local/etc/smb.conf",
  "/usr/local/samba/lib/smb.conf",
  "/usr/samba/lib/smb.conf",
  "/usr/lib/smb.conf",
  "/usr/local/lib/smb.conf"
};
static const int DefaultSambaConfigFilePathListSize = sizeof(DefaultSambaConfigFilePathList)
        / sizeof(char*);

KSambaSharePrivate::KSambaSharePrivate(KSambaShare *parent)
    : q_ptr(parent)
    , data()
    , smbConf()
    , userSharePath()
    , skipUserShare(false)
{
    setUserSharePath();
    findSmbConf();
    sync();
}

KSambaSharePrivate::~KSambaSharePrivate()
{
}

bool KSambaSharePrivate::isSambaInstalled()
{
    if (QFile::exists("/usr/sbin/smbd")
        || QFile::exists("/usr/local/sbin/smbd")) {
        return true;
    }

    //qDebug() << "Samba is not installed!";

    return false;
}

// Try to find the samba config file path
// in several well-known paths
bool KSambaSharePrivate::findSmbConf()
{
    for (int i = 0; i < DefaultSambaConfigFilePathListSize; ++i) {
        const QString filePath(DefaultSambaConfigFilePathList[i]);
        if (QFile::exists(filePath)) {
            smbConf = filePath;
            return true;
        }
    }

    qWarning() << "KSambaShare: Could not find smb.conf!";

    return false;
}

void KSambaSharePrivate::setUserSharePath()
{
    const QString rawString = testparmParamValue(QLatin1String("usershare path"));
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

    QStringList args;
    QByteArray stdErr;
    QByteArray stdOut;

    args << QLatin1String("-d0") << QLatin1String("-s") << QLatin1String("--parameter-name")
         << parameterName;

    runProcess(QLatin1String("testparm"), args, stdOut, stdErr);

    //TODO: parse and process error messages.
    // create a parser for the error output and
    // send error message somewhere
    if (!stdErr.isEmpty()) {
        QList<QByteArray> err;
        err << stdErr.trimmed().split('\n');
        if ((err.count() == 2)
                && err.at(0).startsWith("Load smb config files from")
                && err.at(1).startsWith("Loaded services file OK.")) {
            //qDebug() << "Running testparm" << args;
        } else {
            qWarning() << "We got some errors while running testparm" << stdErr;
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

    QStringList args;
    QByteArray stdOut;
    QByteArray stdErr;

    args << QLatin1String("usershare") << QLatin1String("info");

    runProcess(QLatin1String("net"), args, stdOut, stdErr);

    if (!stdErr.isEmpty()) {
        if (stdErr.contains("You do not have permission to create a usershare")) {
            skipUserShare = true;
        } else if (stdErr.contains("usershares are currently disabled")) {
            skipUserShare = true;
        } else {
            //TODO: parse and process other error messages.
            // create a parser for the error output and
            // send error message somewhere
            qWarning() << "We got some errors while running 'net usershare info'";
            qWarning() << stdErr;
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
    const QRegExp notToMatchRx(QLatin1String("[%<>*\?|/\\+=;:\",]"));
    return (notToMatchRx.indexIn(name) == -1);
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
    return (!KUser::allUserNames().contains(name) || !data.keys().contains(name));
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
    if (KSambaSharePrivate::testparmParamValue(QLatin1String("usershare owner only"))
            == QLatin1String("Yes")) {
        if (!pathInfo.permission(QFile::ReadUser | QFile::WriteUser)) {
            return KSambaShareData::UserSharePathNotAllowed;
        }
    }

    return KSambaShareData::UserSharePathOk;
}

KSambaShareData::UserShareError KSambaSharePrivate::isAclValid(const QString &acl) const
{
    const QRegExp aclRx("(?:(?:(\\w+\\s*)\\\\|)(\\w+\\s*):([fFrRd]{1})(?:,|))*");
    // TODO: check if user is a valid smb user
    return aclRx.exactMatch(acl) ? KSambaShareData::UserShareAclOk
           : KSambaShareData::UserShareAclInvalid;
}

KSambaShareData::UserShareError KSambaSharePrivate::guestsAllowed(const
        KSambaShareData::GuestPermission &guestok) const
{
    if (guestok == KSambaShareData::GuestsAllowed) {
        if (KSambaSharePrivate::testparmParamValue("usershare allow guests")
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

    QStringList args;
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

    QString guestok = QString("guest_ok=%1").arg(
                          (shareData.guestPermission() == KSambaShareData::GuestsNotAllowed)
                          ? QLatin1String("n") : QLatin1String("y"));

    args << QLatin1String("usershare") << QLatin1String("add") << shareData.name()
         << shareData.path() << shareData.comment() << shareData.acl() << guestok;

    int ret = runProcess(QLatin1String("net"), args, stdOut, stdErr);

    //TODO: parse and process error messages.
    if (!stdErr.isEmpty()) {
        // create a parser for the error output and
        // send error message somewhere
        qWarning() << "We got some errors while running 'net usershare add'" << args;
        qWarning() << stdErr;
    }

    return (ret == 0) ? KSambaShareData::UserShareOk : KSambaShareData::UserShareSystemError;
}

KSambaShareData::UserShareError KSambaSharePrivate::remove(const KSambaShareData &shareData) const
{
    if (!isSambaInstalled()) {
        return KSambaShareData::UserShareSystemError;
    }

    QStringList args;

    if (!data.contains(shareData.name())) {
        return KSambaShareData::UserShareNameInvalid;
    }

    args << QLatin1String("usershare") << QLatin1String("delete") << shareData.name();

    int result = QProcess::execute(QLatin1String("net"), args);
    return (result == 0) ? KSambaShareData::UserShareOk : KSambaShareData::UserShareSystemError;
}

bool KSambaSharePrivate::sync()
{
    const QRegExp headerRx(QLatin1String("^\\s*\\["
                                         "([^%<>*\?|/\\+=;:\",]+)"
                                         "\\]"));

    const QRegExp OptValRx(QLatin1String("^\\s*([\\w\\d\\s]+)"
                                         "="
                                         "(.*)$"));

    QTextStream stream(getNetUserShareInfo());
    QString currentShare;
    QStringList shareList;

    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();

        if (headerRx.exactMatch(line)) {
            currentShare = headerRx.cap(1).trimmed();
            shareList << currentShare;

            if (!data.contains(currentShare)) {
                KSambaShareData shareData;
                shareData.dd->name = currentShare;
                data.insert(currentShare, shareData);
            }
        } else if (OptValRx.exactMatch(line)) {
            const QString key = OptValRx.cap(1).trimmed();
            const QString value = OptValRx.cap(2).trimmed();
            KSambaShareData shareData = getShareByName(currentShare);

            if (key == QLatin1String("path")) {
                shareData.dd->path = value;
            } else if (key == QLatin1String("comment")) {
                shareData.dd->comment = value;
            } else if (key == QLatin1String("usershare_acl")) {
                shareData.dd->acl = value;
            } else if (key == QLatin1String("guest_ok")) {
                shareData.dd->guestPermission = value;
            } else {
                qWarning() << "Something nasty happen while parsing 'net usershare info'"
                           << "share:" << currentShare << "key:" << key;
            }
        } else if (line.trimmed().isEmpty()) {
            continue;
        } else {
            return false;
        }
    }

    QMutableMapIterator<QString, KSambaShareData> i(data);
    while (i.hasNext()) {
        i.next();
        if (!shareList.contains(i.key())) {
            i.remove();
        }
    }

    return true;
}

void KSambaSharePrivate::_k_slotFileChange(const QString &/*path*/)
{
    sync();
    //qDebug() << "path changed:" << path;
    Q_Q(KSambaShare);
    emit q->changed();
}

KSambaShare::KSambaShare()
    : QObject(0)
    , d_ptr(new KSambaSharePrivate(this))
{
    Q_D(const KSambaShare);
    if (QFile::exists(d->userSharePath)) {
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

#ifndef KDE_NO_DEPRECATED
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
