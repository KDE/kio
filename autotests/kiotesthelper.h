/* This file is part of the KDE project
   Copyright (C) 2006 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

// This file can only be included once in a given binary

#include <QDebug>
#include <QtCore/qglobal.h>
#include <qstandardpaths.h>
#include <qplatformdefs.h>
#include <QDir>
#ifdef Q_OS_UNIX
#include <utime.h>
#else
#include <sys/utime.h>
#endif
#include <errno.h>

QString homeTmpDir()
{
    const QString dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/kiotests/");
    if (!QFile::exists(dir)) {
        const bool ok = QDir().mkpath(dir);
        if ( !ok )
            qFatal("Couldn't create %s", qPrintable(dir));
    }
    return dir;
}

QDateTime s_referenceTimeStamp;

static void setTimeStamp( const QString& path, const QDateTime& mtime )
{
#ifdef Q_OS_UNIX
    // Put timestamp in the past so that we can check that the listing is correct
    struct utimbuf utbuf;
    utbuf.actime = mtime.toTime_t();
    utbuf.modtime = utbuf.actime;
    utime( QFile::encodeName( path ), &utbuf );
    //qDebug( "Time changed for %s", qPrintable( path ) );
#elif defined(Q_OS_WIN)
    struct _utimbuf utbuf;
    utbuf.actime = mtime.toTime_t();
    utbuf.modtime = utbuf.actime;
    _wutime(reinterpret_cast<const wchar_t *>(path.utf16()), &utbuf);
#endif
}

static void createTestFile( const QString& path, bool plainText = false )
{
    QFile f( path );
    if ( !f.open( QIODevice::WriteOnly ) )
        qFatal("Couldn't create %s", qPrintable(path));
    QByteArray data(plainText ? "Hello world" : "Hello\0world", 11);
    QCOMPARE( data.size(), 11 );
    f.write(data);
    f.close();
    setTimeStamp( path, s_referenceTimeStamp );
}

static void createTestSymlink( const QString& path, const QByteArray& target = "/IDontExist" )
{
    QFile::remove(path);
    bool ok = symlink( target.constData(), QFile::encodeName( path ) ) == 0; // broken symlink
    if ( !ok )
        qFatal("couldn't create symlink: %s", strerror(errno));
    QT_STATBUF buf;
    QVERIFY( QT_LSTAT( QFile::encodeName( path ), &buf ) == 0 );
    QVERIFY( (buf.st_mode & QT_STAT_MASK) == QT_STAT_LNK );
    //qDebug( "symlink %s created", qPrintable( path ) );
    QVERIFY( QFileInfo( path ).isSymLink() );
}

enum CreateTestDirectoryOptions { DefaultOptions = 0, NoSymlink = 1 };
static void createTestDirectory( const QString& path, CreateTestDirectoryOptions opt = DefaultOptions )
{
    QDir dir;
    bool ok = dir.mkdir( path );
    if ( !ok && !dir.exists() )
        qFatal("Couldn't create %s", qPrintable(path));
    createTestFile( path + "/testfile" );
    if ( (opt & NoSymlink) == 0 ) {
#ifndef Q_OS_WIN
        createTestSymlink( path + "/testlink" );
        QVERIFY( QFileInfo( path + "/testlink" ).isSymLink() );
#else
    // to not change the filecount everywhere in the tests
        createTestFile( path + "/testlink" );
#endif
    }
    setTimeStamp( path, s_referenceTimeStamp );
}

#include <kio/jobuidelegateextension.h>
class PredefinedAnswerJobUiDelegate : public KIO::JobUiDelegateExtension
{
public:
    PredefinedAnswerJobUiDelegate()
        : JobUiDelegateExtension(),
          m_askFileRenameCalled(0),
          m_askSkipCalled(0),
          m_askDeleteCalled(0),
          m_messageBoxCalled(0),
          m_renameResult(KIO::R_SKIP),
          m_skipResult(KIO::S_SKIP),
          m_deleteResult(false),
          m_messageBoxResult(0)
    {
    }

    KIO::RenameDialog_Result askFileRename(KJob * job,
                                           const QString & caption,
                                           const QUrl & src,
                                           const QUrl & dest,
                                           KIO::RenameDialog_Mode mode,
                                           QString& newDest,
                                           KIO::filesize_t = (KIO::filesize_t) -1,
                                           KIO::filesize_t = (KIO::filesize_t) -1,
                                           const QDateTime &  = QDateTime(),
                                           const QDateTime &  = QDateTime(),
                                           const QDateTime &  = QDateTime(),
                                           const QDateTime &  = QDateTime()) Q_DECL_OVERRIDE {
        Q_UNUSED(job)
        Q_UNUSED(caption)
        Q_UNUSED(src)
        Q_UNUSED(dest)
        Q_UNUSED(mode)
        Q_UNUSED(newDest)
        ++m_askFileRenameCalled;
        return m_renameResult;
    }

    KIO::SkipDialog_Result askSkip(KJob * job,
                                   bool multi,
                                   const QString & error_text) Q_DECL_OVERRIDE
    {
        Q_UNUSED(job)
        Q_UNUSED(multi)
        Q_UNUSED(error_text)
        ++m_askSkipCalled;
        return m_skipResult;
    }

    bool askDeleteConfirmation(const QList<QUrl>& urls, DeletionType deletionType,
                               ConfirmationType confirmationType) Q_DECL_OVERRIDE
    {
        Q_UNUSED(urls);
        Q_UNUSED(deletionType);
        Q_UNUSED(confirmationType);
        ++m_askDeleteCalled;
        return m_deleteResult;
    }

    int requestMessageBox(MessageBoxType type, const QString &text,
                          const QString &caption,
                          const QString &buttonYes,
                          const QString &buttonNo,
                          const QString &iconYes = QString(),
                          const QString &iconNo = QString(),
                          const QString &dontAskAgainName = QString(),
                          const KIO::MetaData &sslMetaData = KIO::MetaData()) {
        Q_UNUSED(type);
        Q_UNUSED(text);
        Q_UNUSED(caption);
        Q_UNUSED(buttonYes);
        Q_UNUSED(buttonNo);
        Q_UNUSED(iconYes);
        Q_UNUSED(iconNo);
        Q_UNUSED(dontAskAgainName);
        Q_UNUSED(sslMetaData);
        ++m_messageBoxCalled;
        return m_messageBoxResult;
    }



    // yeah, public, for get and reset.
    int m_askFileRenameCalled;
    int m_askSkipCalled;
    int m_askDeleteCalled;
    int m_messageBoxCalled;

    KIO::RenameDialog_Result m_renameResult;
    KIO::SkipDialog_Result m_skipResult;
    bool m_deleteResult;
    int m_messageBoxResult;
};
