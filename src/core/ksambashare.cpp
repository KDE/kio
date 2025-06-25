/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 Jan Schaefer <j_schaef@informatik.uni-kl.de>
    SPDX-FileCopyrightText: 2010 Rodrigo Belem <rclbelem@gmail.com>
    SPDX-FileCopyrightText: 2020 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "ksambashare.h"
#include "kiocoredebug.h"
#include "ksambashare_p.h"
#include "ksambasharedata.h"
#include "ksambasharedata_p.h"

#include "../utils_p.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QHostInfo>
#include <QLoggingCategory>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>

#include <KDirWatch>
#include <KUser>

Q_DECLARE_LOGGING_CATEGORY(KIO_CORE_SAMBASHARE)
Q_LOGGING_CATEGORY(KIO_CORE_SAMBASHARE, "kf.kio.core.sambashare", QtWarningMsg)

KSambaSharePrivate::KSambaSharePrivate(KSambaShare *parent)
    : q_ptr(parent)
    , data()
    , userSharePath()
    , skipUserShare(false)
{
    setUserSharePath();
    data = parse(getNetUserShareInfo());
}

KSambaSharePrivate::~KSambaSharePrivate()
{
}

void KSambaSharePrivate::setUserSharePath()
{
    const QString rawString = testparmParamValue(QStringLiteral("usershare path"));
    const QFileInfo fileInfo(rawString);
    if (fileInfo.isDir()) {
        userSharePath = rawString;
    }
}

int KSambaSharePrivate::runProcess(const QString &fullExecutablePath, const QStringList &args, QByteArray &stdOut, QByteArray &stdErr)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(fullExecutablePath, args);
    // TODO: make it async in future
    process.waitForFinished();

    stdOut = process.readAllStandardOutput();
    stdErr = process.readAllStandardError();
    return process.exitCode();
}

QString KSambaSharePrivate::testparmParamValue(const QString &parameterName)
{
    const QString exec = QStandardPaths::findExecutable(QStringLiteral("testparm"));
    if (exec.isEmpty()) {
        qCDebug(KIO_CORE_SAMBASHARE) << "Could not find the 'testparm' tool, most likely samba-client isn't installed";
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

    runProcess(exec, args, stdOut, stdErr);

    // TODO: parse and process error messages.
    // create a parser for the error output and
    // send error message somewhere
    if (!stdErr.isEmpty()) {
        QList<QByteArray> errArray = stdErr.trimmed().split('\n');
        errArray.removeAll("\n");
        errArray.erase(std::remove_if(errArray.begin(),
                                      errArray.end(),
                                      [](QByteArray &line) {
                                          return line.startsWith("Load smb config files from");
                                      }),
                       errArray.end());
        errArray.removeOne("Loaded services file OK.");
        errArray.removeOne("Weak crypto is allowed");

        const int netbiosNameErrorIdx = errArray.indexOf("WARNING: The 'netbios name' is too long (max. 15 chars).");
        if (netbiosNameErrorIdx >= 0) {
            // netbios name must be of at most 15 characters long
            // means either netbios name is badly configured
            // or not set and the default value is being used, it being "$(hostname)-W"
            // which means any hostname longer than 13 characters will cause this warning
            // when no netbios name was defined
            // See https://www.novell.com/documentation/open-enterprise-server-2018/file_samba_cifs_lx/data/bc855e3.html
            const QString defaultNetbiosName = QHostInfo::localHostName().append(QStringLiteral("-W"));
            if (defaultNetbiosName.length() > 14) {
                qCDebug(KIO_CORE) << "Your samba 'netbios name' parameter was longer than the authorized 15 characters.\n"
                                  << "It may be because your hostname is longer than 13 and samba default 'netbios name' defaults to 'hostname-W', here:"
                                  << defaultNetbiosName << "\n"
                                  << "If that it is the case simply define a 'netbios name' parameter in /etc/samba/smb.conf at most 15 characters long";
            } else {
                qCDebug(KIO_CORE) << "Your samba 'netbios name' parameter was longer than the authorized 15 characters."
                                  << "Please define a 'netbios name' parameter in /etc/samba/smb.conf at most 15 characters long";
            }
            errArray.removeAt(netbiosNameErrorIdx);
        }
        if (errArray.size() > 0) {
            qCDebug(KIO_CORE) << "We got some errors while running testparm" << errArray.join("\n");
        }
    }

    if (!stdOut.isEmpty()) {
        return QString::fromLocal8Bit(stdOut.trimmed());
    }

    return QString();
}

QByteArray KSambaSharePrivate::getNetUserShareInfo()
{
    if (skipUserShare) {
        return QByteArray();
    }

    const QString exec = QStandardPaths::findExecutable(QStringLiteral("net"));
    if (exec.isEmpty()) {
        qCDebug(KIO_CORE_SAMBASHARE) << "Could not find the 'net' tool, most likely samba-client isn't installed";
        return QByteArray();
    }

    QByteArray stdOut;
    QByteArray stdErr;

    const QStringList args{
        QStringLiteral("usershare"),
        QStringLiteral("info"),
    };

    runProcess(exec, args, stdOut, stdErr);

    if (!stdErr.isEmpty()) {
        if (stdErr.contains("You do not have permission to create a usershare")) {
            skipUserShare = true;
        } else if (stdErr.contains("usershares are currently disabled")) {
            skipUserShare = true;
        } else {
            // TODO: parse and process other error messages.
            // create a parser for the error output and
            // send error message somewhere
            qCDebug(KIO_CORE) << "We got some errors while running 'net usershare info'";
            qCDebug(KIO_CORE) << stdErr;
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
    return (!KUser::allUserNames().contains(name) && !data.contains(name));
}

KSambaShareData::UserShareError KSambaSharePrivate::isPathValid(const QString &path) const
{
    QFileInfo pathInfo(path);

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
    if (KSambaSharePrivate::testparmParamValue(QStringLiteral("usershare owner only")) == QLatin1String("Yes")) {
        if (!pathInfo.permission(QFile::ReadUser | QFile::WriteUser)) {
            return KSambaShareData::UserSharePathNotAllowed;
        }
    }

    return KSambaShareData::UserSharePathOk;
}

KSambaShareData::UserShareError KSambaSharePrivate::isAclValid(const QString &acl) const
{
    // NOTE: capital D is not missing from the regex net usershare will in fact refuse to consider it valid
    //   - verified 2020-08-20
    static const auto pattern = uR"--((?:(?:(\w[-.\w\s]*)\\|)(\w+[-.\w\s]*):([fFrRd]{1})(?:,|))*)--";
    static const QRegularExpression aclRx(QRegularExpression::anchoredPattern(pattern));
    // TODO: check if user is a valid smb user
    return aclRx.match(acl).hasMatch() ? KSambaShareData::UserShareAclOk : KSambaShareData::UserShareAclInvalid;
}

bool KSambaSharePrivate::areGuestsAllowed() const
{
    return KSambaSharePrivate::testparmParamValue(QStringLiteral("usershare allow guests")) != QLatin1String("No");
}

KSambaShareData::UserShareError KSambaSharePrivate::guestsAllowed(const KSambaShareData::GuestPermission &guestok) const
{
    if (guestok == KSambaShareData::GuestsAllowed && !areGuestsAllowed()) {
        return KSambaShareData::UserShareGuestsNotAllowed;
    }

    return KSambaShareData::UserShareGuestsOk;
}

KSambaShareData::UserShareError KSambaSharePrivate::add(const KSambaShareData &shareData)
{
    // TODO:
    // * check for usershare max shares

    const QString exec = QStandardPaths::findExecutable(QStringLiteral("net"));
    if (exec.isEmpty()) {
        qCDebug(KIO_CORE_SAMBASHARE) << "Could not find the 'net' tool, most likely samba-client isn't installed";
        return KSambaShareData::UserShareSystemError;
    }

    if (data.contains(shareData.name())) {
        if (data.value(shareData.name()).path() != shareData.path()) {
            return KSambaShareData::UserShareNameInUse;
        }
    }

    QString guestok =
        QStringLiteral("guest_ok=%1").arg((shareData.guestPermission() == KSambaShareData::GuestsNotAllowed) ? QStringLiteral("n") : QStringLiteral("y"));

    const QStringList args{
        QStringLiteral("usershare"),
        QStringLiteral("add"),
        shareData.name(),
        shareData.path(),
        shareData.comment(),
        shareData.acl(),
        guestok,
    };

    QByteArray stdOut;
    int ret = runProcess(exec, args, stdOut, m_stdErr);

    // TODO: parse and process error messages.
    if (!m_stdErr.isEmpty()) {
        // create a parser for the error output and
        // send error message somewhere
        qCWarning(KIO_CORE) << "We got some errors while running 'net usershare add'" << args;
        qCWarning(KIO_CORE) << m_stdErr;
    }

    if (ret == 0 && !data.contains(shareData.name())) {
        // It needs to be added in this function explicitly, otherwise another instance of
        // KSambaShareDataPrivate will be created and added to data when the share
        // definition changes on-disk and we re-parse the data.
        data.insert(shareData.name(), shareData);
    }

    return (ret == 0) ? KSambaShareData::UserShareOk : KSambaShareData::UserShareSystemError;
}

KSambaShareData::UserShareError KSambaSharePrivate::remove(const KSambaShareData &shareData)
{
    const QString exec = QStandardPaths::findExecutable(QStringLiteral("net"));
    if (exec.isEmpty()) {
        qCDebug(KIO_CORE_SAMBASHARE) << "Could not find the 'net' tool, most likely samba-client isn't installed";
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

    QByteArray stdOut;
    int ret = runProcess(exec, args, stdOut, m_stdErr);

    // TODO: parse and process error messages.
    if (!m_stdErr.isEmpty()) {
        // create a parser for the error output and
        // send error message somewhere
        qCWarning(KIO_CORE) << "We got some errors while running 'net usershare delete'" << args;
        qCWarning(KIO_CORE) << m_stdErr;
    }

    return (ret == 0) ? KSambaShareData::UserShareOk : KSambaShareData::UserShareSystemError;

    // NB: the share file gets deleted which leads us to reload and drop the ShareData, hence no explicit remove
}

QMap<QString, KSambaShareData> KSambaSharePrivate::parse(const QByteArray &usershareData)
{
    static const char16_t headerPattern[] = uR"--(^\s*\[([^%<>*?|/+=;:",]+)\])--";
    static const QRegularExpression headerRx(QRegularExpression::anchoredPattern(headerPattern));

    static const char16_t valPattern[] = uR"--(^\s*([\w\d\s]+)=(.*)$)--";
    static const QRegularExpression OptValRx(QRegularExpression::anchoredPattern(valPattern));

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
                shareData.dd->path = Utils::trailingSlashRemoved(value);
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

void KSambaSharePrivate::slotFileChange(const QString &path)
{
    if (path != userSharePath) {
        return;
    }
    data = parse(getNetUserShareInfo());
    qCDebug(KIO_CORE) << "reloading data; path changed:" << path;
    Q_Q(KSambaShare);
    Q_EMIT q->changed();
}

KSambaShare::KSambaShare()
    : QObject(nullptr)
    , d_ptr(new KSambaSharePrivate(this))
{
    Q_D(KSambaShare);
    if (!d->userSharePath.isEmpty() && QFileInfo::exists(d->userSharePath)) {
        KDirWatch::self()->addDir(d->userSharePath, KDirWatch::WatchFiles);
        connect(KDirWatch::self(), &KDirWatch::dirty, this, [d](const QString &path) {
            d->slotFileChange(path);
        });
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

QString KSambaShare::lastSystemErrorString() const
{
    Q_D(const KSambaShare);
    return QString::fromUtf8(d->m_stdErr);
}

bool KSambaShare::areGuestsAllowed() const
{
    Q_D(const KSambaShare);
    return d->areGuestsAllowed();
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
