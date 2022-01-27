/*
    SPDX-FileCopyrightText: 2022 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDebug>
#include <QEventLoop>

#include <PolkitQt1/Authority>
#include <PolkitQt1/Subject>

#include <cstring>
#include <sys/stat.h>
#include <sys/time.h>

#include <filesystem>
#include <optional>
#include <unistd.h>

#include "filemanagement.h"

FileManagement::FileManagement(QObject *parent)
    : QObject(parent)
{
}
FileManagement::~FileManagement()
{
}

class FileDescriptorHolder
{

    struct SharedHandle {
        int handle = -1;
        void reset() {
            handle = -1;
        }
        ~SharedHandle() {
            if (handle != -1) {
                close(handle);
            }
        }
    };
    QSharedPointer<SharedHandle> d;

public:

    Q_DISABLE_COPY(FileDescriptorHolder)

    explicit FileDescriptorHolder(int handle = -1) {
        d.reset(new SharedHandle{handle});
    }
    ~FileDescriptorHolder() { }

    void takeDescriptorFrom(FileDescriptorHolder& other) {
        d = other.d;
        other.d->reset();
    }
    void copyDescriptorFrom(FileDescriptorHolder& other) {
        d = other.d;
    }
    bool isValid() const {
        return d->handle != -1;
    }
    void takeRawDescriptorFrom(int i) {
        if (isValid()) {
            close(d->handle);
        }

        d->handle = i;
    }
    int rawDescriptor() const {
        return d->handle;
    }
    void reset() {
        takeRawDescriptorFrom(-1);
    }
};

using NullableError = std::optional<QString>;

// TODO: user-facing warnings
inline NullableError verifyPath(const QString& path)
{
    enum OpenFlags {
#ifdef Q_OS_FREEBSD
        PathOnly = 0,
#else
        PathOnly = O_PATH,
#endif
        CloseOnExec = O_CLOEXEC,
        DontFollowSymlinks = O_NOFOLLOW,
    };

    // use std::string because it's easier to work with C APIs using it
    auto restOfThePath = std::filesystem::path(path.toStdString()).parent_path().string();
    std::string pathComponent;
    bool traversedToTarget = false;

    FileDescriptorHolder pathHandle;
    FileDescriptorHolder parentHandle;
    FileDescriptorHolder rootHandle;
    struct stat pathStatInformation;
    struct stat rootStatInformation;
    struct stat parentStatInformation;

    QString ret;
    QDebug dbg(&ret);

    while (!traversedToTarget) {
        if (!rootHandle.isValid()) {
            rootHandle.takeRawDescriptorFrom(open("/", OpenFlags::PathOnly | OpenFlags::CloseOnExec));


            if (!rootHandle.isValid()) {
                dbg << "Failed to open filesystem root while verifiying path:" << std::strerror(errno);
                return ret;
            }

            if (fstat(rootHandle.rawDescriptor(), &rootStatInformation) == -1) {
                dbg << "Failed to stat filesystem root while verifiying path:" << std::strerror(errno);
                return ret;
            }

            pathHandle.copyDescriptorFrom(rootHandle);
        }

        Q_ASSERT(restOfThePath[0] == '/');
        auto separatorPosition = restOfThePath.find_first_of('/', 1);
        pathComponent = restOfThePath.substr(1, separatorPosition - 1);
        restOfThePath = restOfThePath.substr(pathComponent.length() + 1);
        traversedToTarget = restOfThePath.empty() || restOfThePath == "/";

        if (!traversedToTarget && pathComponent.empty()) {
            continue;
        }

        auto childName = pathComponent.empty() ? "." : pathComponent.c_str();
        int temporaryChildHandle = openat(pathHandle.rawDescriptor(), childName, OpenFlags::PathOnly | OpenFlags::CloseOnExec | OpenFlags::DontFollowSymlinks);

        if (temporaryChildHandle == -1) {
            dbg << "Failed to find child" << childName << "in directory" << QString::fromStdString(restOfThePath) << std::strerror(errno);
            return ret;
        }
        parentStatInformation = pathStatInformation;
        parentHandle.takeDescriptorFrom(pathHandle);
        pathHandle.takeRawDescriptorFrom(temporaryChildHandle);

        if (fstat(pathHandle.rawDescriptor(), &pathStatInformation) == -1) {
            return QLatin1String("failed to fstat directory");
        }

        // if (buf.st_uid != 0 && buf.st_uid == geteuid() && !traversedToTarget) {
        //     return false;
        // }
        // if (!S_ISLNK(buf.st_mode) && (buf.st_mode & 07) && !traversedToTarget) {
        //     return false;
        // }

        if (pathStatInformation.st_uid && pathStatInformation.st_uid != parentStatInformation.st_uid && pathStatInformation.st_uid != geteuid()) {
            dbg << "Wrong user:" << pathStatInformation.st_uid << "expected:" << geteuid() << "or:" << parentStatInformation.st_uid;
            return ret;
        }

        if (S_ISLNK(pathStatInformation.st_mode)) {
            if (traversedToTarget) {
                return {};
            }

            if (pathStatInformation.st_uid && pathStatInformation.st_uid != geteuid()) {
                dbg << "Wrong user:" << pathStatInformation.st_uid << "expected:" << geteuid();
                return ret;
            }

            std::string link(PATH_MAX, '\0');
            const size_t length = readlinkat(pathHandle.rawDescriptor(), "", &link[0], link.size());

            if (length <= 0 || length >= link.size()) {
                dbg << "Bad length while reading symlink, expected <= 0 or >=" << link.size() << ", got" << length;
                return ret;
            }

            link.resize(length);
            // TODO: strip trailing slashes

            if (link[0] == '/') {
                rootHandle.reset();
            } else {
                if (!parentHandle.isValid()) {
                    pathHandle.reset();
                } else {
                    pathHandle.copyDescriptorFrom(parentHandle);
                }

                link.insert(link.begin(), '/');
            }

            restOfThePath = link + restOfThePath;
        } else if (S_ISDIR(pathStatInformation.st_mode)) {
            parentHandle.copyDescriptorFrom(pathHandle);
        }
    }

    if (S_ISDIR(pathStatInformation.st_mode) && (pathStatInformation.st_mode & 07)) {
        return QLatin1String("Directory is world readable");
    }

    return {};
}

bool FileManagement::isAuthorized()
{
    const auto action = QStringLiteral("org.kde.kio.filemanagement.exec");

    PolkitQt1::SystemBusNameSubject subject(message().service());
    auto authority = PolkitQt1::Authority::instance();

    PolkitQt1::Authority::Result result = authority->checkAuthorizationSync(action, subject, PolkitQt1::Authority::AllowUserInteraction);

    if (authority->hasError()) {
        authority->clearError();
        sendErrorReply(QDBusError::InternalError);
        return false;
    }

    switch (result) {
    case PolkitQt1::Authority::Yes:
        return true;
    default:
        sendErrorReply(QDBusError::AccessDenied);
        return false;
    }
}

QDBusUnixFileDescriptor FileManagement::Open(const QString &path, uint flags, uint mode, uint &errnum)
{
    if (!isAuthorized())
        return {};

    auto err = verifyPath(path);
    if (err.has_value()) {
        sendErrorReply(QDBusError::InvalidArgs, err.value());
        return {};
    }

    int fid = open(qPrintable(path), flags | O_NOFOLLOW, mode); 
    if (fid == -1) {
        errnum = errno;
        return {};
    }
    errnum = 0;
    auto dfid = QDBusUnixFileDescriptor(fid);
    close(fid);

    return dfid;
}

uint FileManagement::ChangeMode(const QString &file, int mode)
{
    if (!isAuthorized())
        return {};

    auto err = verifyPath(file);
    if (err.has_value()) {
        sendErrorReply(QDBusError::InvalidArgs, err.value());
        return {};
    }

    if (lchmod(qPrintable(file), mode) != -1) {
        errno = 0;
    }

    return errno;
}

uint FileManagement::ChangeOwner(const QString &file, uint user, uint group)
{
    if (!isAuthorized())
        return {};

    auto err = verifyPath(file);
    if (err.has_value()) {
        sendErrorReply(QDBusError::InvalidArgs, err.value());
        return {};
    }

    if (lchown(qPrintable(file), user, group) != -1) {
        errno = 0;
    }

    return errno;
}

uint FileManagement::CreateSymlink(const QString &destination, const QString &pointingTo)
{
    if (!isAuthorized())
        return {};

    auto err1 = verifyPath(destination), err2 = verifyPath(pointingTo);
    if (err1.has_value()) {
        sendErrorReply(QDBusError::InvalidArgs, err1.value());
        return {};
    }
    if (err2.has_value()) {
        sendErrorReply(QDBusError::InvalidArgs, err2.value());
        return {};
    }

    if (symlink(qPrintable(pointingTo), qPrintable(destination)) != -1) {
        errno = 0;
    }

    return errno;
}

uint FileManagement::Delete(const QString &file)
{
    if (!isAuthorized())
        return {};

    auto err = verifyPath(file);
    if (err.has_value()) {
        sendErrorReply(QDBusError::InvalidArgs, err.value());
        return {};
    }

    if (unlink(qPrintable(file)) != -1) {
        errno = 0;
    }

    return errno;
}

uint FileManagement::MakeDirectory(const QString &directory, uint permissions)
{
    if (!isAuthorized())
        return {};

    auto err = verifyPath(directory);
    if (err.has_value()) {
        sendErrorReply(QDBusError::InvalidArgs, err.value());
        return {};
    }

    if (mkdir(qPrintable(directory), permissions) != -1) {
        errno = 0;
    }

    return errno;
}

uint FileManagement::RemoveDir(const QString &directory)
{
    if (!isAuthorized())
        return {};

    auto err = verifyPath(directory);
    if (err.has_value()) {
        sendErrorReply(QDBusError::InvalidArgs, err.value());
        return {};
    }

    if (rmdir(qPrintable(directory)) != -1) {
        errno = 0;
    }

    return errno;
}

uint FileManagement::Rename(const QString &source, const QString &destination)
{
    if (!isAuthorized())
        return {};

    auto err1 = verifyPath(destination), err2 = verifyPath(source);
    if (err1.has_value()) {
        sendErrorReply(QDBusError::InvalidArgs, err1.value());
        return {};
    }
    if (err2.has_value()) {
        sendErrorReply(QDBusError::InvalidArgs, err2.value());
        return {};
    }

    if (rename(qPrintable(source), qPrintable(destination)) != -1) {
        errno = 0;
    }

    return errno;
}

uint FileManagement::UpdateTime(const QString &file, uint accessTime, uint modifiedTime)
{
    if (!isAuthorized())
        return {};

    auto err = verifyPath(file);
    if (err.has_value()) {
        sendErrorReply(QDBusError::InvalidArgs, err.value());
        return {};
    }

    timespec times[2];
    time_t actime = accessTime;
    time_t modtime = modifiedTime;
    times[0].tv_sec = actime / 1000;
    times[0].tv_nsec = actime * 1000;
    times[1].tv_sec = modtime / 1000;
    times[1].tv_nsec = modtime * 1000;

    int fid = open(qPrintable(file), O_WRONLY); 
    if (fid == -1) {
        return errno;
    }
    if (futimens(fid, times) != -1) {
        errno = -1;
    }
    close(fid);

    return errno;
}
