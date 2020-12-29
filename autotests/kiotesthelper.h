/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

// This file can only be included once in a given binary

#include <QTest>
#include <QDateTime>
#include <QDebug>
#include <qglobal.h>
#include <QStandardPaths>
#include <qplatformdefs.h>
#include <QDir>
#ifdef Q_OS_UNIX
#include <utime.h>
#else
#include <sys/utime.h>
#endif
#include <errno.h>

#include "kioglobal_p.h"

QString homeTmpDir()
{
    const QString dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/kiotests/"));
    if (!QFile::exists(dir)) {
        const bool ok = QDir().mkpath(dir);
        if (!ok) {
            qFatal("Couldn't create %s", qPrintable(dir));
        }
    }
    return dir;
}

static QDateTime s_referenceTimeStamp;

static void setTimeStamp(const QString &path, const QDateTime &mtime)
{
#ifdef Q_OS_UNIX
    // Put timestamp in the past so that we can check that the listing is correct
    struct utimbuf utbuf;
    utbuf.actime = mtime.toSecsSinceEpoch();
    utbuf.modtime = utbuf.actime;
    utime(QFile::encodeName(path), &utbuf);
    //qDebug( "Time changed for %s", qPrintable( path ) );
#elif defined(Q_OS_WIN)
    struct _utimbuf utbuf;
    utbuf.actime = mtime.toSecsSinceEpoch();
    utbuf.modtime = utbuf.actime;
    _wutime(reinterpret_cast<const wchar_t *>(path.utf16()), &utbuf);
#endif
}

static void createTestFile(const QString &path, bool plainText = false)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qFatal("Couldn't create %s", qPrintable(path));
    }
    QByteArray data(plainText ? "Hello world" : "Hello\0world", 11);
    QCOMPARE(data.size(), 11);
    f.write(data);
    f.close();
    setTimeStamp(path, s_referenceTimeStamp);
}

static void createTestSymlink(const QString &path, const QByteArray &target = "/IDontExist")
{
    QFile::remove(path);
    bool ok = KIOPrivate::createSymlink(QString::fromLatin1(target), path);     // broken symlink
    if (!ok) {
        qFatal("couldn't create symlink: %s", strerror(errno));
    }
    QT_STATBUF buf;
    QVERIFY(QT_LSTAT(QFile::encodeName(path), &buf) == 0);
    QVERIFY((buf.st_mode & QT_STAT_MASK) == QT_STAT_LNK);
    //qDebug( "symlink %s created", qPrintable( path ) );
    QVERIFY(QFileInfo(path).isSymLink());
}

void createTestPipe(const QString &path)
{
#ifndef Q_OS_WIN
    int ok = mkfifo(QFile::encodeName(path), S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    if (ok != 0) {
        qFatal("couldn't create named pipe: %s", strerror(errno));
    }
    QT_STATBUF buf;
    QVERIFY(QT_LSTAT(QFile::encodeName(path), &buf) == 0);
    QVERIFY(S_ISFIFO(buf.st_mode));
#else
    // to not change the filecount everywhere in the tests
    createTestFile(path);
#endif
    QVERIFY(QFileInfo::exists(path));
}

enum CreateTestDirectoryOptions { DefaultOptions = 0, NoSymlink = 1, Empty = 2 };
static inline void createTestDirectory(const QString &path, CreateTestDirectoryOptions opt = DefaultOptions)
{
    QDir dir;
    bool ok = dir.mkdir(path);
    if (!ok && !dir.exists()) {
        qFatal("Couldn't create %s", qPrintable(path));
    }

    if((opt & Empty) == 0){
        createTestFile(path + QStringLiteral("/testfile"));
        if ((opt & NoSymlink) == 0) {
#ifndef Q_OS_WIN
            createTestSymlink(path + QStringLiteral("/testlink"));
            QVERIFY(QFileInfo(path + QStringLiteral("/testlink")).isSymLink());
#else
            // to not change the filecount everywhere in the tests
            createTestFile(path + QStringLiteral("/testlink"));
#endif
        }
    }
    setTimeStamp(path, s_referenceTimeStamp);
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
          m_renameResult(KIO::Result_Skip),
          m_skipResult(KIO::Result_Skip),
          m_deleteResult(false),
          m_messageBoxResult(0)
    {
    }

    KIO::RenameDialog_Result askFileRename(KJob *job,
                                           const QString &caption,
                                           const QUrl &src,
                                           const QUrl &dest,
                                           KIO::RenameDialog_Options options,
                                           QString &newDest,
                                           KIO::filesize_t = (KIO::filesize_t) - 1,
                                           KIO::filesize_t = (KIO::filesize_t) - 1,
                                           const QDateTime  & = QDateTime(),
                                           const QDateTime  & = QDateTime(),
                                           const QDateTime  & = QDateTime(),
                                           const QDateTime  & = QDateTime()) override {
        Q_UNUSED(job)
        Q_UNUSED(caption)
        Q_UNUSED(src)
        Q_UNUSED(dest)
        Q_UNUSED(options)
        newDest = m_renamedest;
        ++m_askFileRenameCalled;
        return m_renameResult;
    }

    KIO::SkipDialog_Result askSkip(KJob *job,
                                   KIO::SkipDialog_Options options,
                                   const QString &error_text) override {
        Q_UNUSED(job)
        Q_UNUSED(options)
        Q_UNUSED(error_text)
        ++m_askSkipCalled;
        return m_skipResult;
    }

    bool askDeleteConfirmation(const QList<QUrl> &urls, DeletionType deletionType,
                               ConfirmationType confirmationType) override {
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
                          const KIO::MetaData &metaData = KIO::MetaData()) override {
        Q_UNUSED(type);
        Q_UNUSED(text);
        Q_UNUSED(caption);
        Q_UNUSED(buttonYes);
        Q_UNUSED(buttonNo);
        Q_UNUSED(iconYes);
        Q_UNUSED(iconNo);
        Q_UNUSED(dontAskAgainName);
        Q_UNUSED(metaData);
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
    QString m_renamedest;
};

