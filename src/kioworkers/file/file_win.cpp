/*
    SPDX-FileCopyrightText: 2000-2002 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2002 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000-2002 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2006 Allan Sandfeld Jensen <sandfeld@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Ehrlicher <ch.ehrlicher@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "file.h"

#include <qt_windows.h>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QUrl>

#include <KConfigGroup>
#include <QDebug>

#include "kioglobal_p.h"

using namespace KIO;

static DWORD CALLBACK CopyProgressRoutine(LARGE_INTEGER TotalFileSize,
                                          LARGE_INTEGER TotalBytesTransferred,
                                          LARGE_INTEGER StreamSize,
                                          LARGE_INTEGER StreamBytesTransferred,
                                          DWORD dwStreamNumber,
                                          DWORD dwCallbackReason,
                                          HANDLE hSourceFile,
                                          HANDLE hDestinationFile,
                                          LPVOID lpData)
{
    FileProtocol *f = reinterpret_cast<FileProtocol *>(lpData);
    f->processedSize(TotalBytesTransferred.QuadPart);
    return PROGRESS_CONTINUE;
}

static UDSEntry createUDSEntryWin(const QFileInfo &fileInfo)
{
    UDSEntry entry;

    entry.fastInsert(KIO::UDSEntry::UDS_NAME, fileInfo.fileName());
    if (fileInfo.isSymLink()) {
        entry.fastInsert(KIO::UDSEntry::UDS_TARGET_URL, fileInfo.symLinkTarget());
        /* TODO - or not useful on windows?
                if ( details > 1 ) {
                    // It is a link pointing to nowhere
                    type = S_IFMT - 1;
                    access = S_IRWXU | S_IRWXG | S_IRWXO;

                    entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, type );
                    entry.insert( KIO::UDSEntry::UDS_ACCESS, access );
                    entry.insert( KIO::UDSEntry::UDS_SIZE, 0LL );
                    goto notype;

                }
        */
    }
    int type = S_IFREG;
    int access = 0;
    if (fileInfo.isDir()) {
        type = S_IFDIR;
    } else if (fileInfo.isSymLink()) {
        type = QT_STAT_LNK;
    }
    if (fileInfo.isReadable()) {
        access |= S_IRUSR;
    }
    if (fileInfo.isWritable()) {
        access |= S_IWUSR;
    }
    if (fileInfo.isExecutable()) {
        access |= S_IXUSR;
    }

    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, type);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, access);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, fileInfo.size());
    if (fileInfo.isHidden()) {
        entry.fastInsert(KIO::UDSEntry::UDS_HIDDEN, true);
    }

    entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, fileInfo.lastModified().toSecsSinceEpoch());
    entry.fastInsert(KIO::UDSEntry::UDS_USER, fileInfo.owner());
    entry.fastInsert(KIO::UDSEntry::UDS_GROUP, fileInfo.group());
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS_TIME, fileInfo.lastRead().toSecsSinceEpoch());
    entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, fileInfo.birthTime().toSecsSinceEpoch());

    QT_STATBUF buf;
    QUrl url(fileInfo.absoluteFilePath());
    const QString path = url.toString(QUrl::StripTrailingSlash | QUrl::PreferLocalFile);
    const QByteArray pathBA = QFile::encodeName(path);

    if (QT_LSTAT(pathBA.constData(), &buf) == 0) {
        entry.fastInsert(KIO::UDSEntry::UDS_DEVICE_ID, buf.st_dev);
        entry.fastInsert(KIO::UDSEntry::UDS_INODE, buf.st_ino);
    }

    return entry;
}

WorkerResult FileProtocol::copy(const QUrl &src, const QUrl &dest, int _mode, JobFlags _flags)
{
    // qDebug() << "copy(): " << src << " -> " << dest << ", mode=" << _mode;

    QFileInfo _src(src.toLocalFile());
    QFileInfo _dest(dest.toLocalFile());
    DWORD dwFlags = COPY_FILE_FAIL_IF_EXISTS;

    if (_src == _dest) {
        return WorkerResult::fail(KIO::ERR_IDENTICAL_FILES, _dest.filePath());
    }

    if (!_src.exists()) {
        return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, _src.filePath());
    }

    if (_src.isDir()) {
        return WorkerResult::fail(KIO::ERR_IS_DIRECTORY, _src.filePath());
    }

    if (_dest.exists()) {
        if (_dest.isDir()) {
            return WorkerResult::fail(KIO::ERR_DIR_ALREADY_EXIST, _dest.filePath());
        }

        if (!(_flags & KIO::Overwrite)) {
            return WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, _dest.filePath());
        }

        dwFlags = 0;
    }

    if (!QFileInfo(_dest.dir().absolutePath()).exists()) {
        _dest.dir().mkdir(_dest.dir().absolutePath());
    }

    if (CopyFileExW((LPCWSTR)_src.filePath().utf16(), (LPCWSTR)_dest.filePath().utf16(), CopyProgressRoutine, (LPVOID)this, FALSE, dwFlags) == 0) {
        DWORD dwLastErr = GetLastError();
        if (dwLastErr == ERROR_FILE_NOT_FOUND) {
            return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, _src.filePath());
        } else if (dwLastErr == ERROR_ACCESS_DENIED) {
            return WorkerResult::fail(KIO::ERR_ACCESS_DENIED, _dest.filePath());
        } else {
            // qDebug() <<  "Copying file " << _src.filePath() << " failed (" << dwLastErr << ")";
            return WorkerResult::fail(KIO::ERR_CANNOT_RENAME, _src.filePath());
        }
    }

    return WorkerResult::pass();
}

WorkerResult FileProtocol::listDir(const QUrl &url)
{
    // qDebug() << "========= LIST " << url << " =========";

    if (!url.isLocalFile()) {
        QUrl redir(url);
        redir.setScheme(configValue(QStringLiteral("DefaultRemoteProtocol"), QStringLiteral("smb")));
        redirection(redir);
        // qDebug() << "redirecting to " << redir;
        return WorkerResult::pass();
    }

    QString path = url.toLocalFile();
    // C: means current directory, a concept which makes no sense in a GUI
    // KCoreDireLister strips trailing slashes, let's put it back again here for C:/
    if (path.length() == 2 && path.at(1) == QLatin1Char(':'))
        path += QLatin1Char('/');
    const QFileInfo info(path);
    if (info.isFile()) {
        return WorkerResult::fail(KIO::ERR_IS_FILE, path);
    }

    QDir dir(path);
    dir.setFilter(QDir::AllEntries | QDir::Hidden);

    if (!dir.exists()) {
        // qDebug() << "========= ERR_DOES_NOT_EXIST  =========";
        return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, path);
    }

    if (!dir.isReadable()) {
        // qDebug() << "========= ERR_CANNOT_ENTER_DIRECTORY =========";
        return WorkerResult::fail(KIO::ERR_CANNOT_ENTER_DIRECTORY, path);
    }
    QDirIterator it(dir);
    UDSEntry entry;
    while (it.hasNext()) {
        it.next();
        UDSEntry entry = createUDSEntryWin(it.fileInfo());

        listEntry(entry);
        entry.clear();
    }

    // qDebug() << "============= COMPLETED LIST ============";

    return WorkerResult::pass();
}

WorkerResult FileProtocol::rename(const QUrl &src, const QUrl &dest, KIO::JobFlags _flags)
{
    // qDebug() << "rename(): " << src << " -> " << dest;

    QFileInfo _src(src.toLocalFile());
    QFileInfo _dest(dest.toLocalFile());
    DWORD dwFlags = 0;

    if (_src == _dest) {
        return WorkerResult::fail(KIO::ERR_IDENTICAL_FILES, _dest.filePath());
    }

    if (!_src.exists()) {
        return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, _src.filePath());
    }

    if (_dest.exists()) {
        if (_dest.isDir()) {
            return WorkerResult::fail(KIO::ERR_DIR_ALREADY_EXIST, _dest.filePath());
        }

        if (!(_flags & KIO::Overwrite)) {
            return WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, _dest.filePath());
        }

        dwFlags = MOVEFILE_REPLACE_EXISTING;
    }
    // To avoid error 17 - The system cannot move the file to a different disk drive.
    dwFlags |= MOVEFILE_COPY_ALLOWED;

    if (MoveFileExW((LPCWSTR)_src.filePath().utf16(), (LPCWSTR)_dest.filePath().utf16(), dwFlags) == 0) {
        DWORD dwLastErr = GetLastError();
        if (dwLastErr == ERROR_FILE_NOT_FOUND) {
            return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, _src.filePath());
        } else if (dwLastErr == ERROR_ACCESS_DENIED) {
            return WorkerResult::fail(KIO::ERR_ACCESS_DENIED, _dest.filePath());
        } else {
            qCDebug(KIO_FILE) << "Renaming file " << _src.filePath() << " failed (" << dwLastErr << ")";
            return WorkerResult::fail(KIO::ERR_CANNOT_RENAME, _src.filePath());
        }
    }

    return WorkerResult::pass();
}

WorkerResult FileProtocol::symlink(const QString &target, const QUrl &dest, KIO::JobFlags flags)
{
    QString localDest = dest.toLocalFile();
    // TODO handle overwrite, etc
    if (!KIOPrivate::createSymlink(target, localDest)) {
        return WorkerResult::fail(KIO::ERR_UNKNOWN, localDest);
    }

    return WorkerResult::pass();
}

WorkerResult FileProtocol::del(const QUrl &url, bool isfile)
{
    QString _path(url.toLocalFile());
    /*****
     * Delete files
     *****/

    if (isfile) {
        // qDebug() << "Deleting file " << _path;

        if (DeleteFileW((LPCWSTR)_path.utf16()) == 0) {
            DWORD dwLastErr = GetLastError();
            if (dwLastErr == ERROR_PATH_NOT_FOUND) {
                return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, _path);
            } else if (dwLastErr == ERROR_ACCESS_DENIED) {
                return WorkerResult::fail(KIO::ERR_ACCESS_DENIED, _path);
            } else {
                return WorkerResult::fail(KIO::ERR_CANNOT_DELETE, _path);
                // qDebug() <<  "Deleting file " << _path << " failed (" << dwLastErr << ")";
            }
        }
    } else {
        // qDebug() << "Deleting directory " << _path;
        auto deleteResult = deleteRecursive(_path);
        if (!deleteResult.success()) {
            return deleteResult;
        }
        if (RemoveDirectoryW((LPCWSTR)_path.utf16()) == 0) {
            DWORD dwLastErr = GetLastError();
            if (dwLastErr == ERROR_FILE_NOT_FOUND) {
                return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, _path);
            } else if (dwLastErr == ERROR_ACCESS_DENIED) {
                return WorkerResult::fail(KIO::ERR_ACCESS_DENIED, _path);
            } else {
                return WorkerResult::fail(KIO::ERR_CANNOT_DELETE, _path);
                // qDebug() <<  "Deleting directory " << _path << " failed (" << dwLastErr << ")";
            }
        }
    }
    return WorkerResult::pass();
}

WorkerResult FileProtocol::chown(const QUrl &url, const QString &, const QString &)
{
    return WorkerResult::fail(KIO::ERR_CANNOT_CHOWN, url.toLocalFile());
}

WorkerResult FileProtocol::stat(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return redirect(url);
    }

    const QString sDetails = metaData(QLatin1String("details"));
    int details = sDetails.isEmpty() ? 2 : sDetails.toInt();
    // qDebug() << "FileProtocol::stat details=" << details;

    const QString localFile = url.toLocalFile();
    QFileInfo fileInfo(localFile);
    if (!fileInfo.exists()) {
        return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, localFile);
    }

    UDSEntry entry = createUDSEntryWin(fileInfo);

    statEntry(entry);

    return WorkerResult::pass();
}

bool FileProtocol::privilegeOperationUnitTestMode()
{
    return false;
}

WorkerResult FileProtocol::execWithElevatedPrivilege(ActionType, const QVariantList &, int err)
{
    return WorkerResult::fail(err);
}
WorkerResult FileProtocol::tryOpen(QFile &f, const QByteArray &, int, int, int err)
{
    return WorkerResult::fail(err);
}

WorkerResult FileProtocol::tryChangeFileAttr(ActionType, const QVariantList &, int err)
{
    return WorkerResult::fail(err);
}

int FileProtocol::setACL(const char *path, mode_t perm, bool directoryDefault)
{
    Q_UNUSED(path);
    Q_UNUSED(perm);
    Q_UNUSED(directoryDefault);
    return 0;
}
