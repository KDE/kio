/*
   Copyright (C) 2000-2002 Stephan Kulow <coolo@kde.org>
   Copyright (C) 2000-2002 David Faure <faure@kde.org>
   Copyright (C) 2000-2002 Waldo Bastian <bastian@kde.org>

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

#ifndef __file_h__
#define __file_h__

#include <kio/global.h>
#include <kio/slavebase.h>

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QFile>

#include <config-kioslave-file.h>

#if HAVE_POSIX_ACL
#include <sys/acl.h>
#include <acl/libacl.h>
#endif

class FileProtocol : public QObject, public KIO::SlaveBase
{
  Q_OBJECT
public:
  FileProtocol( const QByteArray &pool, const QByteArray &app);
  virtual ~FileProtocol();

  virtual void get( const QUrl& url );
  virtual void put( const QUrl& url, int _mode,
		    KIO::JobFlags _flags );
  virtual void copy( const QUrl &src, const QUrl &dest,
                     int mode, KIO::JobFlags flags );
  virtual void rename( const QUrl &src, const QUrl &dest,
                       KIO::JobFlags flags );
  virtual void symlink( const QString &target, const QUrl &dest,
                        KIO::JobFlags flags );

  virtual void stat( const QUrl& url );
  virtual void listDir( const QUrl& url );
  virtual void mkdir( const QUrl& url, int permissions );
  virtual void chmod( const QUrl& url, int permissions );
  virtual void chown( const QUrl& url, const QString& owner, const QString& group );
  virtual void setModificationTime( const QUrl& url, const QDateTime& mtime );
  virtual void del( const QUrl& url, bool isfile);
  virtual void open( const QUrl &url, QIODevice::OpenMode mode );
  virtual void read( KIO::filesize_t size );
  virtual void write( const QByteArray &data );
  virtual void seek( KIO::filesize_t offset );
  virtual void close();

  /**
   * Special commands supported by this slave:
   * 1 - mount
   * 2 - unmount
   */
  virtual void special( const QByteArray &data );
  void unmount( const QString& point );
  void mount( bool _ro, const char *_fstype, const QString& dev, const QString& point );
  bool pumount( const QString &point );
  bool pmount( const QString &dev );

#if HAVE_POSIX_ACL
  static bool isExtendedACL(acl_t acl);
#endif

private:
  bool createUDSEntry( const QString & filename, const QByteArray & path, KIO::UDSEntry & entry,
                       short int details, bool withACL );
  int setACL( const char *path, mode_t perm, bool _directoryDefault );

  QString getUserName( uid_t uid ) const;
  QString getGroupName( gid_t gid ) const;

    bool deleteRecursive(const QString& path);

private:
  mutable QHash<uid_t, QString> mUsercache;
  mutable QHash<gid_t, QString> mGroupcache;
  QFile *mFile;
};

#endif
