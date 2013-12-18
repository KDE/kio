/*
   Copyright (C) 2000-2002 Stephan Kulow <coolo@kde.org>
   Copyright (C) 2000-2002 David Faure <faure@kde.org>
   Copyright (C) 2000-2002 Waldo Bastian <bastian@kde.org>
   Copyright (C) 2006 Allan Sandfeld Jensen <sandfeld@kde.org>
   Copyright (C) 2007 Thiago Macieira <thiago@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License (LGPL) as published by the Free Software Foundation;
   either version 2 of the License, or (at your option) any later
   version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef QT_NO_CAST_FROM_ASCII
#define QT_NO_CAST_FROM_ASCII
#endif

#include "file.h"

#include <config-kioslave-file.h>

#include <QDirIterator>
#include <qplatformdefs.h>

#include <assert.h>
#include <errno.h>
#include <utime.h>

#include <QtCore/QByteRef>
#include <QtCore/QDate>
#include <QtCore/QVarLengthArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QRegExp>
#include <QtCore/QFile>
#include <qtemporaryfile.h>
#ifdef Q_OS_WIN
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#endif

#include <QDebug>
#include <kconfiggroup.h>
#include <kshell.h>
#include <kmountpoint.h>
#include <QMimeDatabase>
#include <QStandardPaths>

#if HAVE_VOLMGT
#include <volmgt.h>
#include <sys/mnttab.h>
#endif

#include <kdirnotify.h>
#include <kio/ioslave_defaults.h>

using namespace KIO;

#define MAX_IPC_SIZE (1024*32)

static QString readLogFile( const QByteArray&_filename );
#if HAVE_POSIX_ACL
static void appendACLAtoms( const QByteArray & path, UDSEntry& entry,
                            mode_t type, bool withACL );
#endif

extern "C" Q_DECL_EXPORT int kdemain( int argc, char **argv )
{
  QCoreApplication app( argc, argv ); // needed for QSocketNotifier
  app.setApplicationName(QLatin1String("kio_file"));

  if (argc != 4)
  {
     fprintf(stderr, "Usage: kio_file protocol domain-socket1 domain-socket2\n");
     exit(-1);
  }

  FileProtocol slave(argv[2], argv[3]);

  // Make sure the first kDebug is after the slave ctor (which sets a SIGPIPE handler)
  // This is useful in case kdeinit was autostarted by another app, which then exited and closed fd2
  // (e.g. ctest does that, or closing the terminal window would do that)
  //qDebug() << "Starting" << getpid();

  slave.dispatchLoop();

  //qDebug() << "Done";
  return 0;
}

static QFile::Permissions modeToQFilePermissions(int mode)
{
    QFile::Permissions perms;
    if (mode & S_IRUSR) {
        perms |= QFile::ReadOwner;
    }
    if (mode & S_IWUSR) {
        perms |= QFile::WriteOwner;
    }
    if (mode & S_IXUSR) {
        perms |= QFile::ExeOwner;
    }
    if (mode & S_IRGRP) {
        perms |= QFile::ReadGroup;
    }
    if (mode & S_IWGRP) {
        perms |= QFile::WriteGroup;
    }
    if (mode & S_IXGRP) {
        perms |= QFile::ExeGroup;
    }
    if (mode & S_IROTH) {
        perms |= QFile::ReadOther;
    }
    if (mode & S_IWOTH) {
        perms |= QFile::WriteOther;
    }
    if (mode & S_IXOTH) {
        perms |= QFile::ExeOther;
    }

    return perms;
}

FileProtocol::FileProtocol( const QByteArray &pool, const QByteArray &app )
    : SlaveBase( "file", pool, app ), mFile(0)
{
}

FileProtocol::~FileProtocol()
{
}

#if HAVE_POSIX_ACL
static QString aclToText(acl_t acl) {
    ssize_t size = 0;
    char* txt = acl_to_text(acl, &size);
    const QString ret = QString::fromLatin1(txt, size);
    acl_free(txt);
    return ret;
}
#endif

int FileProtocol::setACL( const char *path, mode_t perm, bool directoryDefault )
{
    int ret = 0;
#if HAVE_POSIX_ACL

    const QString ACLString = metaData(QLatin1String("ACL_STRING"));
    const QString defaultACLString = metaData(QLatin1String("DEFAULT_ACL_STRING"));
    // Empty strings mean leave as is
    if ( !ACLString.isEmpty() ) {
        acl_t acl = 0;
        if (ACLString == QLatin1String("ACL_DELETE")) {
            // user told us to delete the extended ACL, so let's write only
            // the minimal (UNIX permission bits) part
            acl = acl_from_mode( perm );
        }
        acl = acl_from_text( ACLString.toLatin1() );
        if ( acl_valid( acl ) == 0 ) { // let's be safe
            ret = acl_set_file( path, ACL_TYPE_ACCESS, acl );
            // qDebug() << "Set ACL on:" << path << "to:" << aclToText(acl);
        }
        acl_free( acl );
        if ( ret != 0 ) return ret; // better stop trying right away
    }

    if ( directoryDefault && !defaultACLString.isEmpty() ) {
        if ( defaultACLString == QLatin1String("ACL_DELETE") ) {
            // user told us to delete the default ACL, do so
            ret += acl_delete_def_file( path );
        } else {
            acl_t acl = acl_from_text( defaultACLString.toLatin1() );
            if ( acl_valid( acl ) == 0 ) { // let's be safe
                ret += acl_set_file( path, ACL_TYPE_DEFAULT, acl );
                // qDebug() << "Set Default ACL on:" << path << "to:" << aclToText(acl);
            }
            acl_free( acl );
        }
    }
#else
    Q_UNUSED(path);
    Q_UNUSED(perm);
    Q_UNUSED(directoryDefault);
#endif
    return ret;
}

void FileProtocol::chmod( const QUrl& url, int permissions )
{
    const QString path(url.toLocalFile());
    const QByteArray _path( QFile::encodeName(path) );
    /* FIXME: Should be atomic */
    if (!QFile::setPermissions(path, modeToQFilePermissions(permissions)) ||
        ( setACL( _path.data(), permissions, false ) == -1 ) ||
        /* if not a directory, cannot set default ACLs */
        ( setACL( _path.data(), permissions, true ) == -1 && errno != ENOTDIR ) ) {

        switch (errno) {
            case EPERM:
            case EACCES:
                error(KIO::ERR_ACCESS_DENIED, path);
                break;
#if defined(ENOTSUP)
            case ENOTSUP: // from setACL since chmod can't return ENOTSUP
                error(KIO::ERR_UNSUPPORTED_ACTION, i18n("Setting ACL for %1", path));
                break;
#endif
            case ENOSPC:
                error(KIO::ERR_DISK_FULL, path);
                break;
            default:
                error(KIO::ERR_CANNOT_CHMOD, path);
        }
    } else
        finished();
}

void FileProtocol::setModificationTime( const QUrl& url, const QDateTime& mtime )
{
    const QString path(url.toLocalFile());
    QT_STATBUF statbuf;
    if (QT_LSTAT(QFile::encodeName(path).constData(), &statbuf) == 0) {
        struct utimbuf utbuf;
        utbuf.actime = statbuf.st_atime; // access time, unchanged
        utbuf.modtime = mtime.toTime_t(); // modification time
        if (::utime(QFile::encodeName(path).constData(), &utbuf) != 0) {
            // TODO: errno could be EACCES, EPERM, EROFS
            error(KIO::ERR_CANNOT_SETTIME, path);
        } else {
            finished();
        }
    } else {
        error(KIO::ERR_DOES_NOT_EXIST, path);
    }
}

void FileProtocol::mkdir( const QUrl& url, int permissions )
{
    const QString path(url.toLocalFile());

    // qDebug() << path << "permission=" << permissions;

    // Remove existing file or symlink, if requested (#151851)
    if (metaData(QLatin1String("overwrite")) == QLatin1String("true"))
        QFile::remove(path);

    QT_STATBUF buff;
    if (QT_LSTAT(QFile::encodeName(path).constData(), &buff) == -1) {
        if (!QDir().mkdir(path)) {
            //TODO: add access denied & disk full (or another reasons) handling (into Qt, possibly)
            error(KIO::ERR_COULD_NOT_MKDIR, path);
            return;
        } else {
            if ( permissions != -1 )
                chmod( url, permissions );
            else
                finished();
            return;
        }
    }

    if ( (buff.st_mode & QT_STAT_MASK) == QT_STAT_DIR ) {
        // qDebug() << "ERR_DIR_ALREADY_EXIST";
        error(KIO::ERR_DIR_ALREADY_EXIST, path);
        return;
    }
    error(KIO::ERR_FILE_ALREADY_EXIST, path);
    return;
}

void FileProtocol::get( const QUrl& url )
{
    if (!url.isLocalFile()) {
        QUrl redir(url);
	redir.setScheme(config()->readEntry("DefaultRemoteProtocol", "smb"));
	redirection(redir);
	finished();
	return;
    }

    const QString path(url.toLocalFile());
    QT_STATBUF buff;
    if (QT_STAT(QFile::encodeName(path).constData(), &buff) == -1) {
        if ( errno == EACCES )
           error(KIO::ERR_ACCESS_DENIED, path);
        else
           error(KIO::ERR_DOES_NOT_EXIST, path);
        return;
    }

    if ( (buff.st_mode & QT_STAT_MASK) == QT_STAT_DIR ) {
        error(KIO::ERR_IS_DIRECTORY, path);
        return;
    }
    if ( ( buff.st_mode & QT_STAT_MASK ) != QT_STAT_REG ) {
        error(KIO::ERR_CANNOT_OPEN_FOR_READING, path);
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        error(KIO::ERR_CANNOT_OPEN_FOR_READING, path);
        return;
    }

#if HAVE_FADVISE
    posix_fadvise(f.handle(), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

    // Determine the mimetype of the file to be retrieved, and emit it.
    // This is mandatory in all slaves (for KRun/BrowserRun to work)
    // In real "remote" slaves, this is usually done using mimeTypeForFileNameAndData
    // after receiving some data. But we don't know how much data the mimemagic rules
    // need, so for local files, better use mimeTypeForFile.
    QMimeDatabase db;
    QMimeType mt = db.mimeTypeForFile(url.toLocalFile());
    emit mimeType(mt.name());
    // Emit total size AFTER mimetype
    totalSize( buff.st_size );

    KIO::filesize_t processed_size = 0;

    const QString resumeOffset = metaData(QLatin1String("resume"));
    if ( !resumeOffset.isEmpty() )
    {
        bool ok;
        KIO::fileoffset_t offset = resumeOffset.toLongLong(&ok);
        if (ok && (offset > 0) && (offset < buff.st_size))
        {
            if (f.seek(offset))
            {
                canResume ();
                processed_size = offset;
                // qDebug() << "Resume offset:" << KIO::number(offset);
            }
        }
    }

    char buffer[ MAX_IPC_SIZE ];
    QByteArray array;

    while( 1 )
    {
       int n = f.read(buffer, MAX_IPC_SIZE);
       if (n == -1)
       {
          if (errno == EINTR)
              continue;
          error(KIO::ERR_COULD_NOT_READ, path);
          f.close();
          return;
       }
       if (n == 0)
          break; // Finished

       array = QByteArray::fromRawData(buffer, n);
       data( array );
       array.clear();

       processed_size += n;
       processedSize( processed_size );

       //qDebug() << "Processed: " << KIO::number (processed_size);
    }

    data( QByteArray() );

    f.close();

    processedSize( buff.st_size );
    finished();
}

void FileProtocol::open(const QUrl &url, QIODevice::OpenMode mode)
{
    // qDebug() << url;

    QString openPath = url.toLocalFile();
    QT_STATBUF buff;
    if (QT_STAT(QFile::encodeName(openPath).constData(), &buff) == -1) {
        if ( errno == EACCES )
           error(KIO::ERR_ACCESS_DENIED, openPath);
        else
           error(KIO::ERR_DOES_NOT_EXIST, openPath);
        return;
    }

    if ( (buff.st_mode & QT_STAT_MASK) == QT_STAT_DIR ) {
        error(KIO::ERR_IS_DIRECTORY, openPath);
        return;
    }
    if ( ( buff.st_mode & QT_STAT_MASK ) != QT_STAT_REG ) {
        error(KIO::ERR_CANNOT_OPEN_FOR_READING, openPath);
        return;
    }

    mFile = new QFile(openPath);
    if (!mFile->open(mode)) {
        if (mode & QIODevice::ReadOnly) {
            error(KIO::ERR_CANNOT_OPEN_FOR_READING, openPath);
        } else {
            error(KIO::ERR_CANNOT_OPEN_FOR_WRITING, openPath);
        }

        return;
    }
    // Determine the mimetype of the file to be retrieved, and emit it.
    // This is mandatory in all slaves (for KRun/BrowserRun to work).
    // If we're not opening the file ReadOnly or ReadWrite, don't attempt to
    // read the file and send the mimetype.
    if (mode & QIODevice::ReadOnly){
        QMimeDatabase db;
        QMimeType mt = db.mimeTypeForFile(url.toLocalFile());
        emit mimeType(mt.name());
   }

    totalSize( buff.st_size );
    position( 0 );

    emit opened();
}

void FileProtocol::read(KIO::filesize_t bytes)
{
    // qDebug() << "File::open -- read";
    Q_ASSERT(mFile && mFile->isOpen());

    QVarLengthArray<char> buffer(bytes);
    while (true) {
        QByteArray res = mFile->read(bytes);

        if (!res.isEmpty()) {
            data(res);
            bytes -= res.size();
        } else {
            // empty array designates eof
            data(QByteArray());
            if (!mFile->atEnd()) {
                error(KIO::ERR_COULD_NOT_READ, mFile->fileName());
                close();
            }
            break;
        }
        if (bytes <= 0) break;
    }
}

void FileProtocol::write(const QByteArray &data)
{
    // qDebug() << "File::open -- write";
    Q_ASSERT(mFile && mFile->isWritable());

    if (mFile->write(data) != data.size()) {
        if (mFile->error() == QFileDevice::ResourceError) { // disk full
            error(KIO::ERR_DISK_FULL, mFile->fileName());
            close();
        } else {
            qWarning() << "Couldn't write. Error:" << mFile->errorString();
            error(KIO::ERR_COULD_NOT_WRITE, mFile->fileName());
            close();
        }
    } else {
        written(data.size());
    }
}

void FileProtocol::seek(KIO::filesize_t offset)
{
    // qDebug() << "File::open -- seek";
    Q_ASSERT(mFile && mFile->isOpen());

    if (mFile->seek(offset)) {
        position( offset );
    } else {
        error(KIO::ERR_COULD_NOT_SEEK, mFile->fileName());
        close();
    }
}

void FileProtocol::close()
{
    // qDebug() << "File::open -- close ";
    Q_ASSERT(mFile);

    delete mFile;
    mFile = 0;

    finished();
}

void FileProtocol::put( const QUrl& url, int _mode, KIO::JobFlags _flags )
{
    const QString dest_orig = url.toLocalFile();

    // qDebug() << dest_orig << "mode=" << _mode;

    QString dest_part(dest_orig + QLatin1String(".part"));

    QT_STATBUF buff_orig;
    const bool bOrigExists = (QT_LSTAT(QFile::encodeName(dest_part).constData(), &buff_orig) != -1);
    bool bPartExists = false;
    const bool bMarkPartial = config()->readEntry("MarkPartial", true);

    if (bMarkPartial)
    {
        QT_STATBUF buff_part;
        bPartExists = (QT_LSTAT(QFile::encodeName(dest_part).constData(), &buff_part) != -1);

        if (bPartExists && !(_flags & KIO::Resume) && !(_flags & KIO::Overwrite) && buff_part.st_size > 0 && ((buff_part.st_mode & QT_STAT_MASK) == QT_STAT_REG))
        {
            // qDebug() << "calling canResume with" << KIO::number(buff_part.st_size);

            // Maybe we can use this partial file for resuming
            // Tell about the size we have, and the app will tell us
            // if it's ok to resume or not.
            _flags |= canResume( buff_part.st_size ) ? KIO::Resume : KIO::DefaultFlags;

            // qDebug() << "got answer" << (_flags & KIO::Resume);
        }
    }

    if ( bOrigExists && !(_flags & KIO::Overwrite) && !(_flags & KIO::Resume))
    {
        if ((buff_orig.st_mode & QT_STAT_MASK) == QT_STAT_DIR)
            error( KIO::ERR_DIR_ALREADY_EXIST, dest_orig );
        else
            error( KIO::ERR_FILE_ALREADY_EXIST, dest_orig );
        return;
    }

    int result;
    QString dest;
    QByteArray _dest;
    QFile f;

    // Loop until we got 0 (end of data)
    do
    {
        QByteArray buffer;
        dataReq(); // Request for data
        result = readData( buffer );

        if (result >= 0)
        {
            if (dest.isEmpty())
            {
                if (bMarkPartial)
                {
                    // qDebug() << "Appending .part extension to" << dest_orig;
                    dest = dest_part;
                    if ( bPartExists && !(_flags & KIO::Resume) )
                    {
                        // qDebug() << "Deleting partial file" << dest_part;
                        QFile::remove( dest_part );
                        // Catch errors when we try to open the file.
                    }
                }
                else
                {
                    dest = dest_orig;
                    if ( bOrigExists && !(_flags & KIO::Resume) )
                    {
                        // qDebug() << "Deleting destination file" << dest_orig;
                        QFile::remove( dest_orig );
                        // Catch errors when we try to open the file.
                    }
                }

                f.setFileName(dest);

                if ( (_flags & KIO::Resume) )
                {
                    f.open(QIODevice::ReadWrite | QIODevice::Append);
                }
                else
                {
                    // WABA: Make sure that we keep writing permissions ourselves,
                    // otherwise we can be in for a surprise on NFS.
                    mode_t initialMode;
                    if (_mode != -1)
                        initialMode = _mode | S_IWUSR | S_IRUSR;
                    else
                        initialMode = 0666;

                    f.open(QIODevice::Truncate | QIODevice::WriteOnly);
                    f.setPermissions(modeToQFilePermissions(initialMode));
                }

                if (!f.isOpen()) {
                    // qDebug() << "####################### COULD NOT WRITE" << dest << "_mode=" << _mode;
                    // qDebug() << "QFile error==" << f.error() << "(" << f.errorString() << ")";

                    if (f.error() == QFileDevice::PermissionsError) {
                        error(KIO::ERR_WRITE_ACCESS_DENIED, dest);
                    } else {
                        error(KIO::ERR_CANNOT_OPEN_FOR_WRITING, dest);
                    }
                    return;
                }
            }

            if (f.write(buffer) == -1) {
                if (f.error() == QFile::ResourceError) { // disk full
                    error(KIO::ERR_DISK_FULL, dest_orig);
                    result = -2; // means: remove dest file
                } else {
                    qWarning() << "Couldn't write. Error:" << f.errorString();
                    error(KIO::ERR_COULD_NOT_WRITE, dest_orig);
                    result = -1;
                }
            }
        }
    }
    while ( result > 0 );

    // An error occurred deal with it.
    if (result < 0)
    {
        // qDebug() << "Error during 'put'. Aborting.";

        if (f.isOpen()) {
            f.close();

            QT_STATBUF buff;
            if (QT_STAT(QFile::encodeName(dest).constData(), &buff) == 0) {
                int size = config()->readEntry("MinimumKeepSize", DEFAULT_MINIMUM_KEEP_SIZE);
                if (buff.st_size <  size) {
                    QFile::remove(dest);
                }
            }
        }

        ::exit(255);
    }

    if (!f.isOpen()) { // we got nothing to write out, so we never opened the file
        finished();
        return;
    }

    f.close();

    if (f.error() != QFile::NoError) {
        qWarning() << "Error when closing file descriptor:" << f.errorString();
        error(KIO::ERR_COULD_NOT_WRITE, dest_orig);
        return;
    }

    // after full download rename the file back to original name
    if ( bMarkPartial )
    {
        //QFile::rename() never overwrites the destination file unlike ::remove,
        //so we must remove it manually first
        if (_flags & KIO::Overwrite) {
            QFile::remove( dest_orig );
        }

        if (!QFile::rename(dest, dest_orig)) {
            qWarning() << " Couldn't rename " << dest << " to " << dest_orig;
            error(KIO::ERR_CANNOT_RENAME_PARTIAL, dest_orig);
            return;
        }
        org::kde::KDirNotify::emitFileRenamed(QUrl::fromLocalFile(dest), QUrl::fromLocalFile(dest_orig));
    }

    // set final permissions
    if ( _mode != -1 && !(_flags & KIO::Resume) )
    {
        if (!QFile::setPermissions(dest_orig, modeToQFilePermissions(_mode)))
        {
            // couldn't chmod. Eat the error if the filesystem apparently doesn't support it.
            KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByPath(dest_orig);
            if (mp && mp->testFileSystemFlag(KMountPoint::SupportsChmod))
                 warning( i18n( "Could not change permissions for\n%1" ,  dest_orig ) );
        }
    }

    // set modification time
    const QString mtimeStr = metaData(QLatin1String("modified"));
    if ( !mtimeStr.isEmpty() ) {
        QDateTime dt = QDateTime::fromString( mtimeStr, Qt::ISODate );
        if ( dt.isValid() ) {
            QT_STATBUF dest_statbuf;
            if (QT_STAT(QFile::encodeName(dest_orig).constData(), &dest_statbuf) == 0) {
                struct timeval utbuf[2];
                // access time
                utbuf[0].tv_sec = dest_statbuf.st_atime; // access time, unchanged  ## TODO preserve msec
                utbuf[0].tv_usec = 0;
                // modification time
                utbuf[1].tv_sec = dt.toTime_t();
                utbuf[1].tv_usec = dt.time().msec() * 1000;
                utimes( QFile::encodeName(dest_orig).constData(), utbuf );
            }
        }

    }

    // We have done our job => finish
    finished();
}

QString FileProtocol::getUserName( uid_t uid ) const
{
    if ( !mUsercache.contains( uid ) ) {
        struct passwd *user = getpwuid( uid );
        if ( user ) {
            mUsercache.insert( uid, QString::fromLatin1(user->pw_name) );
        }
        else
            return QString::number( uid );
    }
    return mUsercache[uid];
}

QString FileProtocol::getGroupName( gid_t gid ) const
{
    if ( !mGroupcache.contains( gid ) ) {
        struct group *grp = getgrgid( gid );
        if ( grp ) {
            mGroupcache.insert( gid, QString::fromLatin1(grp->gr_name) );
        }
        else
            return QString::number( gid );
    }
    return mGroupcache[gid];
}

bool FileProtocol::createUDSEntry( const QString & filename, const QByteArray & path, UDSEntry & entry,
                                   short int details, bool withACL )
{
    assert(entry.count() == 0); // by contract :-)
    // entry.reserve( 8 ); // speed up QHash insertion

    entry.insert( KIO::UDSEntry::UDS_NAME, filename );

    mode_t type;
    mode_t access;
    QT_STATBUF buff;

    if (QT_LSTAT(path.data(), &buff) == 0)  {

        if (details > 2) {
            entry.insert( KIO::UDSEntry::UDS_DEVICE_ID, buff.st_dev );
            entry.insert( KIO::UDSEntry::UDS_INODE, buff.st_ino );
        }

        if ((buff.st_mode & QT_STAT_MASK) == QT_STAT_LNK) {

            char buffer2[ 1000 ];
            int n = readlink( path.data(), buffer2, 999 );
            if ( n != -1 ) {
                buffer2[ n ] = 0;
            }

            entry.insert( KIO::UDSEntry::UDS_LINK_DEST, QFile::decodeName( buffer2 ) );

            // A symlink -> follow it only if details>1
            if (details > 1 && QT_STAT( path.constData(), &buff ) == -1) {
                // It is a link pointing to nowhere
                type = S_IFMT - 1;
                access = S_IRWXU | S_IRWXG | S_IRWXO;

                entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, type );
                entry.insert( KIO::UDSEntry::UDS_ACCESS, access );
                entry.insert( KIO::UDSEntry::UDS_SIZE, 0LL );
                goto notype;

            }
        }
    } else {
        // qWarning() << "lstat didn't work on " << path.data();
        return false;
    }

    type = buff.st_mode & S_IFMT; // extract file type
    access = buff.st_mode & 07777; // extract permissions

    entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, type );
    entry.insert( KIO::UDSEntry::UDS_ACCESS, access );

    entry.insert( KIO::UDSEntry::UDS_SIZE, buff.st_size );

#if HAVE_POSIX_ACL
    if (details > 0) {
        /* Append an atom indicating whether the file has extended acl information
         * and if withACL is specified also one with the acl itself. If it's a directory
         * and it has a default ACL, also append that. */
        appendACLAtoms( path, entry, type, withACL );
    }
#else
    Q_UNUSED(withACL);
#endif

 notype:
    if (details > 0) {
        entry.insert( KIO::UDSEntry::UDS_MODIFICATION_TIME, buff.st_mtime );
        entry.insert( KIO::UDSEntry::UDS_USER, getUserName( buff.st_uid ) );
        entry.insert( KIO::UDSEntry::UDS_GROUP, getGroupName( buff.st_gid ) );
        entry.insert( KIO::UDSEntry::UDS_ACCESS_TIME, buff.st_atime );
    }

    // Note: buff.st_ctime isn't the creation time !
    // We made that mistake for KDE 2.0, but it's in fact the
    // "file status" change time, which we don't care about.

    return true;
}

void FileProtocol::special( const QByteArray &data)
{
    int tmp;
    QDataStream stream(data);

    stream >> tmp;
    switch (tmp) {
    case 1:
      {
	QString fstype, dev, point;
	qint8 iRo;

	stream >> iRo >> fstype >> dev >> point;

	bool ro = ( iRo != 0 );

	// qDebug() << "MOUNTING fstype=" << fstype << " dev=" << dev << " point=" << point << " ro=" << ro;
	bool ok = pmount( dev );
	if (ok)
	    finished();
	else
	    mount(ro, fstype.toLatin1().constData(), dev, point);

      }
      break;
    case 2:
      {
	QString point;
	stream >> point;
	bool ok = pumount( point );
	if (ok)
	    finished();
	else
	    unmount( point );
      }
      break;

    default:
      break;
    }
}

static QStringList fallbackSystemPath() {
    return QStringList() << QLatin1String("/sbin") << QLatin1String("/bin");
}

void FileProtocol::mount( bool _ro, const char *_fstype, const QString& _dev, const QString& _point )
{
    // qDebug() << "fstype=" << _fstype;

#ifndef _WIN32_WCE
#if  HAVE_VOLMGT
	/*
	 *  support for Solaris volume management
	 */
	QString err;
	QByteArray devname = QFile::encodeName( _dev );

	if( volmgt_running() ) {
//		qDebug() << "VOLMGT: vold ok.";
		if( volmgt_check( devname.data() ) == 0 ) {
			// qDebug() << "VOLMGT: no media in " << devname.data();
			err = i18n("No Media inserted or Media not recognized.");
			error( KIO::ERR_COULD_NOT_MOUNT, err );
			return;
		} else {
			// qDebug() << "VOLMGT: " << devname.data() << ": media ok";
			finished();
			return;
		}
	} else {
		err = i18n("\"vold\" is not running.");
		// qDebug() << "VOLMGT: " << err;
		error( KIO::ERR_COULD_NOT_MOUNT, err );
		return;
	}
#else


    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove(false);
    tmpFile.open();
    QByteArray tmpFileName = QFile::encodeName(tmpFile.fileName());
    QByteArray dev;
    if (_dev.startsWith(QLatin1String("LABEL="))) { // turn LABEL=foo into -L foo (#71430)
        QString labelName = _dev.mid( 6 );
        dev = "-L ";
        dev += QFile::encodeName( KShell::quoteArg( labelName ) ); // is it correct to assume same encoding as filesystem?
    } else if (_dev.startsWith(QLatin1String("UUID="))) { // and UUID=bar into -U bar
        QString uuidName = _dev.mid( 5 );
        dev = "-U ";
        dev += QFile::encodeName( KShell::quoteArg( uuidName ) );
    }
    else
        dev = QFile::encodeName( KShell::quoteArg(_dev) ); // get those ready to be given to a shell

    QByteArray point = QFile::encodeName( KShell::quoteArg(_point) );
    bool fstype_empty = !_fstype || !*_fstype;
    QByteArray fstype = KShell::quoteArg(QString::fromLatin1(_fstype)).toLatin1(); // good guess
    QByteArray readonly = _ro ? "-r" : "";
    QByteArray mountProg = QStandardPaths::findExecutable(QLatin1String("mount")).toLocal8Bit();
    if (mountProg.isEmpty()) {
      mountProg = QStandardPaths::findExecutable(QLatin1String("mount"), fallbackSystemPath()).toLocal8Bit();
    }
    if (mountProg.isEmpty()){
      error( KIO::ERR_COULD_NOT_MOUNT, i18n("Could not find program \"mount\""));
      return;
    }

    // Two steps, in case mount doesn't like it when we pass all options
    for ( int step = 0 ; step <= 1 ; step++ )
    {
        QByteArray buffer = mountProg + ' ';
        // Mount using device only if no fstype nor mountpoint (KDE-1.x like)
        if ( !dev.isEmpty() && _point.isEmpty() && fstype_empty )
            buffer += dev;
        else
          // Mount using the mountpoint, if no fstype nor device (impossible in first step)
          if ( !_point.isEmpty() && dev.isEmpty() && fstype_empty )
              buffer += point;
          else
            // mount giving device + mountpoint but no fstype
            if ( !_point.isEmpty() && !dev.isEmpty() && fstype_empty )
                buffer += readonly + ' ' + dev + ' ' + point;
            else
              // mount giving device + mountpoint + fstype
#if defined(__svr4__) && defined(Q_OS_SOLARIS) // MARCO for Solaris 8 and I
                // believe this is true for SVR4 in general
                buffer += "-F " + fstype + ' ' + (_ro ? "-oro" : "") + ' ' + dev + ' ' + point;
#else
                buffer += readonly + " -t " + fstype + ' ' + dev + ' ' + point;
#endif
        buffer += " 2>" + tmpFileName;
        // qDebug() << buffer;

        int mount_ret = system( buffer.constData() );

        QString err = readLogFile( tmpFileName );
        if ( err.isEmpty() && mount_ret == 0)
        {
            finished();
            return;
        }
        else
        {
            // Didn't work - or maybe we just got a warning
            KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByDevice( _dev );
            // Is the device mounted ?
            if ( mp && mount_ret == 0)
            {
                // qDebug() << "mount got a warning:" << err;
                warning( err );
                finished();
                return;
            }
            else
            {
                if ( (step == 0) && !_point.isEmpty())
                {
                    // qDebug() << err;
                    // qDebug() << "Mounting with those options didn't work, trying with only mountpoint";
                    fstype = "";
                    fstype_empty = true;
                    dev = "";
                    // The reason for trying with only mountpoint (instead of
                    // only device) is that some people (hi Malte!) have the
                    // same device associated with two mountpoints
                    // for different fstypes, like /dev/fd0 /mnt/e2floppy and
                    // /dev/fd0 /mnt/dosfloppy.
                    // If the user has the same mountpoint associated with two
                    // different devices, well they shouldn't specify the
                    // mountpoint but just the device.
                }
                else
                {
                    error( KIO::ERR_COULD_NOT_MOUNT, err );
                    return;
                }
            }
        }
    }
#endif /* ! HAVE_VOLMGT */
#else
    QString err;
    err = i18n("mounting is not supported by wince.");
    error( KIO::ERR_COULD_NOT_MOUNT, err );
#endif

}


void FileProtocol::unmount( const QString& _point )
{
#ifndef _WIN32_WCE
    QByteArray buffer;

    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove(false);
    tmpFile.open();
    QByteArray tmpFileName = QFile::encodeName(tmpFile.fileName());
    QString err;

#if HAVE_VOLMGT
	/*
	 *  support for Solaris volume management
	 */
	char *devname;
	char *ptr;
	FILE *mnttab;
	struct mnttab mnt;

	if( volmgt_running() ) {
		// qDebug() << "VOLMGT: looking for "
			<< _point.toLocal8Bit();

		if( (mnttab = QT_FOPEN( MNTTAB, "r" )) == NULL ) {
			err = QLatin1String("could not open mnttab");
			// qDebug() << "VOLMGT: " << err;
			error( KIO::ERR_COULD_NOT_UNMOUNT, err );
			return;
		}

		/*
		 *  since there's no way to derive the device name from
		 *  the mount point through the volmgt library (and
		 *  media_findname() won't work in this case), we have to
		 *  look ourselves...
		 */
		devname = NULL;
		rewind( mnttab );
		while( getmntent( mnttab, &mnt ) == 0 ) {
			if( strcmp( _point.toLocal8Bit(), mnt.mnt_mountp ) == 0 ){
				devname = mnt.mnt_special;
				break;
			}
		}
		fclose( mnttab );

		if( devname == NULL ) {
			err = QLatin1String("not in mnttab");
			// qDebug() << "VOLMGT: "
				<< QFile::encodeName(_point).data()
				<< ": " << err;
			error( KIO::ERR_COULD_NOT_UNMOUNT, err );
			return;
		}

		/*
		 *  strip off the directory name (volume name)
		 *  the eject(1) command will handle unmounting and
		 *  physically eject the media (if possible)
		 */
		ptr = strrchr( devname, '/' );
		*ptr = '\0';
                QByteArray qdevname(QFile::encodeName(KShell::quoteArg(QFile::decodeName(QByteArray(devname)))).data());
		buffer = "/usr/bin/eject " + qdevname + " 2>" + tmpFileName;
		// qDebug() << "VOLMGT: eject " << qdevname;

		/*
		 *  from eject(1): exit status == 0 => need to manually eject
		 *                 exit status == 4 => media was ejected
		 */
		if( WEXITSTATUS( system( buffer.constData() )) == 4 ) {
			/*
			 *  this is not an error, so skip "readLogFile()"
			 *  to avoid wrong/confusing error popup. The
			 *  temporary file is removed by QTemporaryFile's
			 *  destructor, so don't do that manually.
			 */
			finished();
			return;
		}
	} else {
		/*
		 *  eject(1) should do its job without vold(1M) running,
		 *  so we probably could call eject anyway, but since the
		 *  media is mounted now, vold must've died for some reason
		 *  during the user's session, so it should be restarted...
		 */
		err = i18n("\"vold\" is not running.");
		// qDebug() << "VOLMGT: " << err;
		error( KIO::ERR_COULD_NOT_UNMOUNT, err );
		return;
	}
#else
    QByteArray umountProg = QStandardPaths::findExecutable(QLatin1String("umount")).toLocal8Bit();
    if (umountProg.isEmpty()) {
        umountProg = QStandardPaths::findExecutable(QLatin1String("umount"), fallbackSystemPath()).toLocal8Bit();
    }
    if (umountProg.isEmpty()) {
        error( KIO::ERR_COULD_NOT_UNMOUNT, i18n("Could not find program \"umount\""));
        return;
    }
    buffer = umountProg + ' ' + QFile::encodeName(KShell::quoteArg(_point)) + " 2>" + tmpFileName;
    system( buffer.constData() );
#endif /* HAVE_VOLMGT */

    err = readLogFile( tmpFileName );
    if ( err.isEmpty() )
        finished();
    else
        error( KIO::ERR_COULD_NOT_UNMOUNT, err );
#else
    QString err;
    err = i18n("unmounting is not supported by wince.");
    error( KIO::ERR_COULD_NOT_MOUNT, err );
#endif
}

/*************************************
 *
 * pmount handling
 *
 *************************************/

bool FileProtocol::pmount(const QString &dev)
{
#ifndef _WIN32_WCE
    QString pmountProg = QStandardPaths::findExecutable(QLatin1String("pmount"));
    if (pmountProg.isEmpty())
        pmountProg = QStandardPaths::findExecutable(QLatin1String("pmount"), fallbackSystemPath());
    if (pmountProg.isEmpty())
        return false;

    QByteArray buffer = QFile::encodeName(pmountProg) + ' ' +
                        QFile::encodeName(KShell::quoteArg(dev));

    int res = system( buffer.constData() );

    return res==0;
#else
    return false;
#endif
}

bool FileProtocol::pumount(const QString &point)
{
#ifndef _WIN32_WCE
    KMountPoint::Ptr mp = KMountPoint::currentMountPoints(KMountPoint::NeedRealDeviceName).findByPath(point);
    if (!mp)
        return false;
    QString dev = mp->realDeviceName();
    if (dev.isEmpty()) return false;

    QString pumountProg = QStandardPaths::findExecutable(QLatin1String("pumount"));
    if (pumountProg.isEmpty())
        pumountProg = QStandardPaths::findExecutable(QLatin1String("pumount"), fallbackSystemPath());
    if (pumountProg.isEmpty())
        return false;

    QByteArray buffer = QFile::encodeName(pumountProg);
    buffer += ' ';
    buffer += QFile::encodeName(KShell::quoteArg(dev));

    int res = system( buffer.data() );

    return res==0;
#else
    return false;
#endif
}

/*************************************
 *
 * Utilities
 *
 *************************************/

static QString readLogFile( const QByteArray& _filename )
{
    QString result;
    QFile file(QFile::decodeName(_filename));
    if (file.open(QIODevice::ReadOnly)) {
        result = QString::fromLocal8Bit(file.readAll());
    }
    (void)file.remove();
    return result;
}

/*************************************
 *
 * ACL handling helpers
 *
 *************************************/
#if HAVE_POSIX_ACL

bool FileProtocol::isExtendedACL( acl_t acl )
{
    return ( acl_equiv_mode( acl, 0 ) != 0 );
}

static void appendACLAtoms( const QByteArray & path, UDSEntry& entry, mode_t type, bool withACL )
{
    // first check for a noop
    if ( acl_extended_file( path.data() ) == 0 ) return;

    acl_t acl = 0;
    acl_t defaultAcl = 0;
    bool isDir = (type & QT_STAT_MASK) == QT_STAT_DIR;
    // do we have an acl for the file, and/or a default acl for the dir, if it is one?
    acl = acl_get_file( path.data(), ACL_TYPE_ACCESS );
    /* Sadly libacl does not provided a means of checking for extended ACL and default
     * ACL separately. Since a directory can have both, we need to check again. */
    if ( isDir ) {
        if ( acl ) {
            if ( !FileProtocol::isExtendedACL( acl ) ) {
                acl_free( acl );
                acl = 0;
            }
        }
        defaultAcl = acl_get_file( path.data(), ACL_TYPE_DEFAULT );
    }
    if ( acl || defaultAcl ) {
      // qDebug() << path.constData() << "has extended ACL entries";
      entry.insert( KIO::UDSEntry::UDS_EXTENDED_ACL, 1 );
    }
    if ( withACL ) {
        if ( acl ) {
            const QString str = aclToText(acl);
            entry.insert( KIO::UDSEntry::UDS_ACL_STRING, str );
            // qDebug() << path.constData() << "ACL:" << str;
        }
        if ( defaultAcl ) {
            const QString str = aclToText(defaultAcl);
            entry.insert( KIO::UDSEntry::UDS_DEFAULT_ACL_STRING, str );
            // qDebug() << path.constData() << "DEFAULT ACL:" << str;
        }
    }
    if ( acl ) acl_free( acl );
    if ( defaultAcl ) acl_free( defaultAcl );
}
#endif

// We could port this to KTempDir::removeDir but then we wouldn't be able to tell the user
// where exactly the deletion failed, in case of errors.
bool FileProtocol::deleteRecursive(const QString& path)
{
    //qDebug() << path;
    QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
                    QDirIterator::Subdirectories);
    QStringList dirsToDelete;
    while ( it.hasNext() ) {
        const QString itemPath = it.next();
        //qDebug() << "itemPath=" << itemPath;
        const QFileInfo info = it.fileInfo();
        if (info.isDir() && !info.isSymLink())
            dirsToDelete.prepend(itemPath);
        else {
            //qDebug() << "QFile::remove" << itemPath;
            if (!QFile::remove(itemPath)) {
                error(KIO::ERR_CANNOT_DELETE, itemPath);
                return false;
            }
        }
    }
    QDir dir;
    Q_FOREACH(const QString& itemPath, dirsToDelete) {
        //qDebug() << "QDir::rmdir" << itemPath;
        if (!dir.rmdir(itemPath)) {
            error(KIO::ERR_CANNOT_DELETE, itemPath);
            return false;
        }
    }
    return true;
}

