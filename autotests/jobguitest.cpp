/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2011 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QApplication>
#include <QTest>
#include <kio/copyjob.h>
#include <kio/paste.h>
#include <kio/deletejob.h>
#include "kiotesthelper.h" // createTestFile etc.

#include <QClipboard>
#include <QMimeData>

static QString otherTmpDir()
{
#ifdef Q_OS_WIN
    return QDir::tempPath() + "/jobtest/";
#else
    // This one needs to be on another partition
    return QStringLiteral("/tmp/jobtest/");
#endif
}

class JobGuiTest : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void initTestCase()
    {
        // Start with a clean base dir
        cleanupTestCase();
        homeTmpDir(); // create it
        if (!QFile::exists(otherTmpDir())) {
            bool ok = QDir().mkdir(otherTmpDir());
            if (!ok) {
                qFatal("Couldn't create %s", qPrintable(homeTmpDir()));
            }
        }
    }

    void cleanupTestCase()
    {
        delDir(homeTmpDir());
        delDir(otherTmpDir());
    }

    void pasteFileToOtherPartition()
    {
        const QString filePath = homeTmpDir() + "fileFromHome";
        const QString dest = otherTmpDir() + "fileFromHome_copied";
        QFile::remove(dest);
        createTestFile(filePath);

        QMimeData *mimeData = new QMimeData;
        QUrl fileUrl = QUrl::fromLocalFile(filePath);
        mimeData->setUrls(QList<QUrl>{fileUrl});
        QApplication::clipboard()->setMimeData(mimeData);

        KIO::Job *job = KIO::pasteClipboard(QUrl::fromLocalFile(otherTmpDir()), static_cast<QWidget *>(nullptr));
        job->setUiDelegate(nullptr);
        bool ok = job->exec();
        QVERIFY(ok);

        QVERIFY(QFile::exists(dest));
        QVERIFY(QFile::exists(filePath));     // still there
    }

private:
    static void delDir(const QString &pathOrUrl)
    {
        KIO::Job *job = KIO::del(QUrl::fromLocalFile(pathOrUrl), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        job->exec();
    }

};

QTEST_MAIN(JobGuiTest)

#include "jobguitest.moc"
