/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kfileitemtest.h"
#include <kfileitemlistproperties.h>
#include <QTest>
#include <kfileitem.h>

#include <QTemporaryDir>
#include <QTemporaryFile>
#include <KUser>
#include <KDesktopFile>
#include <KConfigGroup>
#include "kiotesthelper.h"

#include <QMimeDatabase>
#include <KProtocolInfo>

QTEST_MAIN(KFileItemTest)

void KFileItemTest::initTestCase()
{
}

void KFileItemTest::testPermissionsString()
{
    // Directory
    QTemporaryDir tempDir;
    KFileItem dirItem(QUrl::fromLocalFile(tempDir.path() + '/'));
    QCOMPARE((uint)dirItem.permissions(), (uint)0700);
    QCOMPARE(dirItem.permissionsString(), QStringLiteral("drwx------"));
    QVERIFY(dirItem.isReadable());

    // File
    QFile file(tempDir.path() + "/afile");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadOther); // 0604
    KFileItem fileItem(QUrl::fromLocalFile(file.fileName()), QString(), KFileItem::Unknown);
    QCOMPARE((uint)fileItem.permissions(), (uint)0604);
    QCOMPARE(fileItem.permissionsString(), QStringLiteral("-rw----r--"));
    QVERIFY(fileItem.isReadable());

    // Symlink to file
    QString symlink = tempDir.path() + "/asymlink";
    QVERIFY(file.link(symlink));
    QUrl symlinkUrl = QUrl::fromLocalFile(symlink);
    KFileItem symlinkItem(symlinkUrl, QString(), KFileItem::Unknown);
    QCOMPARE((uint)symlinkItem.permissions(), (uint)0604);
    // This is a bit different from "ls -l": we get the 'l' but we see the permissions of the target.
    // This is actually useful though; the user sees it's a link, and can check if he can read the [target] file.
    QCOMPARE(symlinkItem.permissionsString(), QStringLiteral("lrw----r--"));
    QVERIFY(symlinkItem.isReadable());

    // Symlink to directory (#162544)
    QVERIFY(QFile::remove(symlink));
    QVERIFY(QFile(tempDir.path() + '/').link(symlink));
    KFileItem symlinkToDirItem(symlinkUrl, QString(), KFileItem::Unknown);
    QCOMPARE((uint)symlinkToDirItem.permissions(), (uint)0700);
    QCOMPARE(symlinkToDirItem.permissionsString(), QStringLiteral("lrwx------"));
}

void KFileItemTest::testNull()
{
    KFileItem null;
    QVERIFY(null.isNull());
    KFileItem fileItem(QUrl::fromLocalFile(QStringLiteral("/")), QString(), KFileItem::Unknown);
    QVERIFY(!fileItem.isNull());
    null = fileItem; // ok, now 'null' isn't so null anymore
    QVERIFY(!null.isNull());
    QVERIFY(null.isReadable());
    QVERIFY(!null.isHidden());
}

void KFileItemTest::testDoesNotExist()
{
    KFileItem fileItem(QUrl::fromLocalFile(QStringLiteral("/doesnotexist")), QString(), KFileItem::Unknown);
    QVERIFY(!fileItem.isNull());
    QVERIFY(!fileItem.isReadable());
    QVERIFY(fileItem.user().isEmpty());
    QVERIFY(fileItem.group().isEmpty());
}

void KFileItemTest::testDetach()
{
    KFileItem fileItem(QUrl::fromLocalFile(QStringLiteral("/one")), QString(), KFileItem::Unknown);
    QCOMPARE(fileItem.name(), QStringLiteral("one"));
    KFileItem fileItem2(fileItem);
    QVERIFY(fileItem == fileItem2);
    QVERIFY(fileItem.d == fileItem2.d);
    fileItem2.setName(QStringLiteral("two"));
    QCOMPARE(fileItem2.name(), QStringLiteral("two"));
    QCOMPARE(fileItem.name(), QStringLiteral("one")); // it detached
    QVERIFY(fileItem == fileItem2);
    QVERIFY(fileItem.d != fileItem2.d);

    fileItem = fileItem2;
    QCOMPARE(fileItem.name(), QStringLiteral("two"));
    QVERIFY(fileItem == fileItem2);
    QVERIFY(fileItem.d == fileItem2.d);
    QVERIFY(!(fileItem != fileItem2));
}

void KFileItemTest::testMove()
{
    // Test move constructor
    {
        KFileItem fileItem(QUrl::fromLocalFile(QStringLiteral("/one")), QString(), KFileItem::Unknown);
        QCOMPARE(fileItem.name(), QStringLiteral("one"));
        KFileItem fileItem2(std::move(fileItem));
        QCOMPARE(fileItem2.name(), QStringLiteral("one"));
    }

    // Test move assignment
    {
        KFileItem fileItem(QUrl::fromLocalFile(QStringLiteral("/one")), QString(), KFileItem::Unknown);
        QCOMPARE(fileItem.name(), QStringLiteral("one"));
        KFileItem fileItem2(QUrl::fromLocalFile(QStringLiteral("/two")), QString(), KFileItem::Unknown);
        fileItem2 = std::move(fileItem);
        QCOMPARE(fileItem2.name(), QStringLiteral("one"));
    }

    // Now to test some value changes to make sure moving works as intended.
    KFileItem fileItem(QUrl::fromLocalFile(QStringLiteral("/one")), QString(), KFileItem::Unknown);
    QCOMPARE(fileItem.name(), QStringLiteral("one"));
    fileItem.setUrl(QUrl::fromLocalFile(QStringLiteral("/two")));
    QCOMPARE(fileItem.name(), QStringLiteral("two"));

    // Move fileitem to fileItem2, it should now contain everything fileItem had.
    // Just testing a property to make sure it does.
    KFileItem fileItem2(std::move(fileItem));
    QCOMPARE(fileItem2.name(), QStringLiteral("two"));
}

void KFileItemTest::testBasic()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    QFile fileObj(file.fileName());
    QVERIFY(fileObj.open(QIODevice::WriteOnly));
    fileObj.write(QByteArray("Hello"));
    fileObj.close();

    QUrl url = QUrl::fromLocalFile(file.fileName());
    KFileItem fileItem(url, QString(), KFileItem::Unknown);
    QCOMPARE(fileItem.text(), url.fileName());
    QVERIFY(fileItem.isLocalFile());
    QCOMPARE(fileItem.localPath(), url.toLocalFile());
    QCOMPARE(fileItem.size(), KIO::filesize_t(5));
    QVERIFY(fileItem.linkDest().isEmpty());
    QVERIFY(!fileItem.isHidden());
    QVERIFY(fileItem.isReadable());
    QVERIFY(fileItem.isWritable());
    QVERIFY(fileItem.isFile());
    QVERIFY(!fileItem.isDir());
    QVERIFY(!fileItem.isDesktopFile());
#ifndef Q_OS_WIN
    QCOMPARE(fileItem.user(), KUser().loginName());
#endif
}

void KFileItemTest::testRootDirectory()
{
    const QString rootPath = QDir::rootPath();
    QUrl url = QUrl::fromLocalFile(rootPath);
    KIO::UDSEntry entry;
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    KFileItem fileItem(entry, url);
    QCOMPARE(fileItem.text(), QStringLiteral("."));
    QVERIFY(fileItem.isLocalFile());
    QCOMPARE(fileItem.localPath(), url.toLocalFile());
    QVERIFY(fileItem.linkDest().isEmpty());
    QVERIFY(!fileItem.isHidden());
    QVERIFY(!fileItem.isFile());
    QVERIFY(fileItem.isDir());
    QVERIFY(!fileItem.isDesktopFile());
}

void KFileItemTest::testHiddenFile()
{
    QTemporaryDir tempDir;
    QFile file(tempDir.path() + "/.hiddenfile");
    QVERIFY(file.open(QIODevice::WriteOnly));
    KFileItem fileItem(QUrl::fromLocalFile(file.fileName()), QString(), KFileItem::Unknown);
    QCOMPARE(fileItem.text(), QStringLiteral(".hiddenfile"));
    QVERIFY(fileItem.isLocalFile());
    QVERIFY(fileItem.isHidden());
}

void KFileItemTest::testMimeTypeOnDemand()
{
    QTemporaryFile file;
    QVERIFY(file.open());

    {
        KFileItem fileItem(QUrl::fromLocalFile(file.fileName()));
        fileItem.setDelayedMimeTypes(true);
        QVERIFY(fileItem.currentMimeType().isDefault());
        QVERIFY(!fileItem.isMimeTypeKnown());
        QVERIFY(!fileItem.isFinalIconKnown());
        //qDebug() << fileItem.determineMimeType().name();
        QCOMPARE(fileItem.determineMimeType().name(), QStringLiteral("application/x-zerosize"));
        QCOMPARE(fileItem.mimetype(), QStringLiteral("application/x-zerosize"));
        QVERIFY(fileItem.isMimeTypeKnown());
        QVERIFY(fileItem.isFinalIconKnown());
    }

    {
        // Calling mimeType directly also does MIME type determination
        KFileItem fileItem(QUrl::fromLocalFile(file.fileName()));
        fileItem.setDelayedMimeTypes(true);
        QVERIFY(!fileItem.isMimeTypeKnown());
        QCOMPARE(fileItem.mimetype(), QStringLiteral("application/x-zerosize"));
        QVERIFY(fileItem.isMimeTypeKnown());
    }

    {
        // Calling overlays should NOT do MIME type determination (#237668)
        KFileItem fileItem(QUrl::fromLocalFile(file.fileName()));
        fileItem.setDelayedMimeTypes(true);
        QVERIFY(!fileItem.isMimeTypeKnown());
        fileItem.overlays();
        QVERIFY(!fileItem.isMimeTypeKnown());
    }

    {
        QTemporaryFile file;
        QVERIFY(file.open());
        // Check whether mime-magic is used.
        // No known extension, so it should be used by determineMimeType.
        file.write(QByteArray("%PDF-"));
        QString fileName = file.fileName();
        QVERIFY(!fileName.isEmpty());
        file.close();
        KFileItem fileItem(QUrl::fromLocalFile(fileName));
        fileItem.setDelayedMimeTypes(true);
        QCOMPARE(fileItem.currentMimeType().name(), QLatin1String("application/octet-stream"));
        QVERIFY(fileItem.currentMimeType().isValid());
        QVERIFY(fileItem.currentMimeType().isDefault());
        QVERIFY(!fileItem.isMimeTypeKnown());
        QCOMPARE(fileItem.determineMimeType().name(), QStringLiteral("application/pdf"));
        QCOMPARE(fileItem.mimetype(), QStringLiteral("application/pdf"));
    }

    {
        QTemporaryFile file(QDir::tempPath() + QLatin1String("/kfileitemtest_XXXXXX.txt"));
        QVERIFY(file.open());
        // Check whether mime-magic is used.
        // Known extension, so it should NOT be used.
        file.write(QByteArray("<smil"));
        QString fileName = file.fileName();
        QVERIFY(!fileName.isEmpty());
        file.close();
        KFileItem fileItem(QUrl::fromLocalFile(fileName));
        fileItem.setDelayedMimeTypes(true);
        QCOMPARE(fileItem.currentMimeType().name(), QStringLiteral("text/plain"));
        QVERIFY(fileItem.isMimeTypeKnown());
        QCOMPARE(fileItem.determineMimeType().name(), QStringLiteral("text/plain"));
        QCOMPARE(fileItem.mimetype(), QStringLiteral("text/plain"));

        // And if the MIME type is not on demand?
        KFileItem fileItem2(QUrl::fromLocalFile(fileName));
        QCOMPARE(fileItem2.currentMimeType().name(), QStringLiteral("text/plain")); // XDG says: application/smil; but can't sniff all files so this can't work
        QVERIFY(fileItem2.isMimeTypeKnown());
    }
}

void KFileItemTest::testCmp()
{
    QTemporaryFile file;
    QVERIFY(file.open());

    KFileItem fileItem(QUrl::fromLocalFile(file.fileName()));
    fileItem.setDelayedMimeTypes(true);
    KFileItem fileItem2(QUrl::fromLocalFile(file.fileName()));
    QVERIFY(fileItem == fileItem2); // created independently, but still 'equal'
    QVERIFY(fileItem.d != fileItem2.d);
    QVERIFY(!(fileItem != fileItem2));
    QVERIFY(fileItem.cmp(fileItem2));
}

void KFileItemTest::testCmpAndInit()
{
    QTemporaryDir tempDir;
    KFileItem dirItem(QUrl::fromLocalFile(tempDir.path()));
    QVERIFY(dirItem.isDir()); // this calls init()

    KFileItem dirItem2(QUrl::fromLocalFile(tempDir.path()));
    // not yet init() called on dirItem2, but must be equal
    // compare init()ialized to un-init()ialized KFileItem
    QVERIFY(dirItem.cmp(dirItem2));
    QVERIFY(dirItem2.isDir());
    QVERIFY(dirItem.cmp(dirItem2));
    QVERIFY(dirItem == dirItem2);
    QVERIFY(dirItem.d != dirItem2.d);
    QVERIFY(!(dirItem != dirItem2));

    // now the other way around, compare un-init()ialized to init()ialized KFileItem
    KFileItem dirItem3(QUrl::fromLocalFile(tempDir.path()));
    // not yet init() called on dirItem3, but must be equal
    QVERIFY(dirItem3.cmp(dirItem));
    QVERIFY(dirItem3.isDir());
    QVERIFY(dirItem3.cmp(dirItem));
    QVERIFY(dirItem == dirItem3);
    QVERIFY(dirItem.d != dirItem3.d);
    QVERIFY(!(dirItem != dirItem3));
}

void KFileItemTest::testCmpByUrl()
{
    const QUrl nulUrl;
    const QUrl url  = QUrl::fromLocalFile(QStringLiteral("1foo"));
    const QUrl url2 = QUrl::fromLocalFile(QStringLiteral("fo1"));
    const QUrl url3 = QUrl::fromLocalFile(QStringLiteral("foo"));
    KFileItem nulFileItem;
    KFileItem nulFileItem2(nulUrl);
    KFileItem fileItem(url);
    KFileItem fileItem2(url2);
    KFileItem fileItem3(url3);

    // an invalid KFileItem is considered equal to any other invalid KFileItem or invalid QUrl.
    QVERIFY(!(nulFileItem < nulFileItem));
    QVERIFY(!(nulFileItem < nulFileItem2));
    QVERIFY(!(nulFileItem2 < nulFileItem));
    QVERIFY(!(nulFileItem < nulUrl));
    // an invalid KFileItem is considered less than any valid KFileItem.
    QVERIFY(nulFileItem < fileItem);
    // a valid KFileItem is not less than an invalid KFileItem or invalid QUrl
    QVERIFY(!(fileItem < nulUrl));
    QVERIFY(!(fileItem < nulFileItem));
    QVERIFY(!(fileItem < nulFileItem2));

    QVERIFY(fileItem < fileItem2);
    QVERIFY(fileItem < url2);
    QVERIFY(!(fileItem2 < fileItem));
    QVERIFY(fileItem2 < fileItem3);
    QVERIFY(fileItem < url3);
    QVERIFY(!(fileItem3 < fileItem2));
    QVERIFY(!(fileItem3 < fileItem));
    // Must be false as they are considered equal
    QVERIFY(!(fileItem < fileItem));
    QVERIFY(!(fileItem < url));
}

void KFileItemTest::testRename()
{
    KIO::UDSEntry entry;
    const QString origName = QStringLiteral("foo");
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, origName);
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    KFileItem fileItem(entry, QUrl::fromLocalFile(QStringLiteral("/dir/foo")));
    QCOMPARE(fileItem.name(), origName);
    QCOMPARE(fileItem.text(), origName);
    const QString newName = QStringLiteral("FiNeX_rocks");
    fileItem.setName(newName);
    QCOMPARE(fileItem.name(), newName);
    QCOMPARE(fileItem.text(), newName);
    QCOMPARE(fileItem.entry().stringValue(KIO::UDSEntry::UDS_NAME), newName); // #195385
}

void KFileItemTest::testRefresh()
{
    QTemporaryDir tempDir;
    QFileInfo dirInfo(tempDir.path());
    // Refresh on a dir
    KFileItem dirItem(QUrl::fromLocalFile(tempDir.path()));
    QVERIFY(dirItem.isDir());
    QVERIFY(dirItem.entry().isDir());
    QDateTime lastModified = dirInfo.lastModified();
    // Qt 5.8 adds milliseconds (but UDSEntry has no support for that)
    lastModified = lastModified.addMSecs(-lastModified.time().msec());
    QCOMPARE(dirItem.time(KFileItem::ModificationTime), lastModified);
    dirItem.refresh();
    QVERIFY(dirItem.isDir());
    QVERIFY(dirItem.entry().isDir());
    QCOMPARE(dirItem.time(KFileItem::ModificationTime), lastModified);

    // Refresh on a file
    QFile file(tempDir.path() + "/afile");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("Hello world\n");
    file.close();
    QFileInfo fileInfo(file.fileName());
    const KIO::filesize_t expectedSize = 12;
    QCOMPARE(KIO::filesize_t(fileInfo.size()), expectedSize);
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadOther); // 0604
    KFileItem fileItem(QUrl::fromLocalFile(file.fileName()));
    QVERIFY(fileItem.isFile());
    QVERIFY(!fileItem.isLink());
    QCOMPARE(fileItem.size(), expectedSize);
#ifndef Q_OS_WIN
    QCOMPARE(fileItem.user(), KUser().loginName());
#endif
    // Qt 5.8 adds milliseconds (but UDSEntry has no support for that)
    lastModified = dirInfo.lastModified();
    // Truncate away the milliseconds...
    lastModified = lastModified.addMSecs(-lastModified.time().msec());
    // ...but it looks like the kernel rounds up when the msecs are .998 or .999,
    // so add a bit of tolerance
    auto expectedLastModified = lastModified;
    if (fileItem.time(KFileItem::ModificationTime) != lastModified &&
            fileItem.time(KFileItem::ModificationTime) == lastModified.addSecs(1)) {
        expectedLastModified = expectedLastModified.addSecs(1);
    }
    QCOMPARE(fileItem.time(KFileItem::ModificationTime), expectedLastModified);
    fileItem.refresh();
    QVERIFY(fileItem.isFile());
    QVERIFY(!fileItem.isLink());
    QCOMPARE(fileItem.size(), expectedSize);
#ifndef Q_OS_WIN
    QCOMPARE(fileItem.user(), KUser().loginName());
#endif
    QCOMPARE(fileItem.time(KFileItem::ModificationTime), expectedLastModified);

    // Refresh on a symlink to a file
    const QString symlink = tempDir.path() + "/asymlink";
    QVERIFY(file.link(symlink));
    QDateTime symlinkTime = QDateTime::currentDateTime().addSecs(-20);
    // we currently lose milliseconds....
    symlinkTime = symlinkTime.addMSecs(-symlinkTime.time().msec());
    setTimeStamp(symlink, symlinkTime); // differentiate link time and source file time
    const QUrl symlinkUrl = QUrl::fromLocalFile(symlink);
    KFileItem symlinkItem(symlinkUrl);
    QVERIFY(symlinkItem.isFile());
    QVERIFY(symlinkItem.isLink());
    QCOMPARE(symlinkItem.size(), expectedSize);
    QCOMPARE(symlinkItem.time(KFileItem::ModificationTime), symlinkTime);
    symlinkItem.refresh();
    QVERIFY(symlinkItem.isFile());
    QVERIFY(symlinkItem.isLink());
    QCOMPARE(symlinkItem.size(), expectedSize);
    QCOMPARE(symlinkItem.time(KFileItem::ModificationTime), symlinkTime);

    // Symlink to directory (#162544)
    QVERIFY(QFile::remove(symlink));
    QVERIFY(QFile(tempDir.path() + '/').link(symlink));
    KFileItem symlinkToDirItem(symlinkUrl);
    QVERIFY(symlinkToDirItem.isDir());
    QVERIFY(symlinkToDirItem.isLink());
    symlinkToDirItem.refresh();
    QVERIFY(symlinkToDirItem.isDir());
    QVERIFY(symlinkToDirItem.isLink());
}

void KFileItemTest::testDotDirectory()
{
    QTemporaryDir tempDir;
    QFile file(tempDir.path() + "/.directory");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[Desktop Entry]\nIcon=foo\nComment=com\n");
    file.close();
    {
        KFileItem fileItem(QUrl::fromLocalFile(tempDir.path() + '/'), QString(), KFileItem::Unknown);
        QVERIFY(fileItem.isLocalFile());
        QCOMPARE(fileItem.mimeComment(), QStringLiteral("com"));
        QCOMPARE(fileItem.iconName(), QStringLiteral("foo"));
    }
    // Test for calling iconName first, to trigger MIME type resolution
    {
        KFileItem fileItem(QUrl::fromLocalFile(tempDir.path()), QString(), KFileItem::Unknown);
        QVERIFY(fileItem.isLocalFile());
        QCOMPARE(fileItem.iconName(), QStringLiteral("foo"));
    }
}

void KFileItemTest::testDecodeFileName_data()
{
    QTest::addColumn<QString>("filename");
    QTest::addColumn<QString>("expectedText");

    QTest::newRow("simple") << "filename" << "filename";
    QTest::newRow("/ at end") << QString(QStringLiteral("foo") + QChar(0x2044)) << QString(QStringLiteral("foo") + QChar(0x2044));
    QTest::newRow("/ at begin") << QString(QChar(0x2044)) << QString(QChar(0x2044));
}

void KFileItemTest::testDecodeFileName()
{
    QFETCH(QString, filename);
    QFETCH(QString, expectedText);
    QCOMPARE(KIO::decodeFileName(filename), expectedText);
}

void KFileItemTest::testEncodeFileName_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expectedFileName");

    QTest::newRow("simple") << "filename" << "filename";
    QTest::newRow("/ at end") << "foo/" << QString(QStringLiteral("foo") + QChar(0x2044));
    QTest::newRow("/ at begin") << "/" << QString(QChar(0x2044));
}

void KFileItemTest::testEncodeFileName()
{
    QFETCH(QString, text);
    QFETCH(QString, expectedFileName);
    QCOMPARE(KIO::encodeFileName(text), expectedFileName);
}

void KFileItemTest::testListProperties_data()
{
    QTest::addColumn<QString>("itemDescriptions");
    QTest::addColumn<bool>("expectedReading");
    QTest::addColumn<bool>("expectedDeleting");
    QTest::addColumn<bool>("expectedIsLocal");
    QTest::addColumn<bool>("expectedIsDirectory");
    QTest::addColumn<bool>("expectedIsFile");
    QTest::addColumn<QString>("expectedMimeType");
    QTest::addColumn<QString>("expectedMimeGroup");

    QTest::newRow("one file") << "f" << true << true << true << false << true << "text/plain" << "text";
    QTest::newRow("one dir") << "d" << true << true << true << true << false << "inode/directory" << "inode";
    QTest::newRow("root dir") << "/" << true << false << true << true << false << "inode/directory" << "inode";
    QTest::newRow("file+dir") << "fd" << true << true << true << false << false << "" << "";
    QTest::newRow("two dirs") << "dd" << true << true << true << true << false << "inode/directory" << "inode";
    QTest::newRow("dir+root dir") << "d/" << true << false << true << true << false << "inode/directory" << "inode";
    QTest::newRow("two (text+html) files") << "ff" << true << true << true << false << true << "" << "text";
    QTest::newRow("three (text+html+empty) files") << "fff" << true << true << true << false << true << "" << "";
    QTest::newRow("http url") << "h" << true << true /*says kio_http...*/
                              << false << false << true << "application/octet-stream" << "application";
    QTest::newRow("2 http urls") << "hh" << true << true /*says kio_http...*/
                                 << false << false << true << "application/octet-stream" << "application";
}

void KFileItemTest::testListProperties()
{
    QFETCH(QString, itemDescriptions);
    QFETCH(bool, expectedReading);
    QFETCH(bool, expectedDeleting);
    QFETCH(bool, expectedIsLocal);
    QFETCH(bool, expectedIsDirectory);
    QFETCH(bool, expectedIsFile);
    QFETCH(QString, expectedMimeType);
    QFETCH(QString, expectedMimeGroup);

    QTemporaryDir tempDir;
    QDir baseDir(tempDir.path());
    KFileItemList items;
    for (int i = 0; i < itemDescriptions.size(); ++i) {
        QString fileName = tempDir.path() + "/file" + QString::number(i);
        switch (itemDescriptions[i].toLatin1()) {
        case 'f': {
            if (i == 1) { // 2nd file is html
                fileName += QLatin1String(".html");
            }
            QFile file(fileName);
            QVERIFY(file.open(QIODevice::WriteOnly));
            if (i == 0) {
                file.write("Hello");
            } else if (i == 1) {
                file.write("<html>");
            } // i == 2: leave the file empty
            file.close();
            KFileItem item(QUrl::fromLocalFile(fileName), QString(), KFileItem::Unknown);
            if (i == 0)
                QCOMPARE(item.mimetype(), "text/plain");
            else if (i == 1)
                QCOMPARE(item.mimetype(), "text/html");
            else if (i == 2)
                QCOMPARE(item.mimetype(), "application/x-zerosize");
            items.push_back(std::move(item));
        }
        break;
        case 'd':
            QVERIFY(baseDir.mkdir(fileName));
            items << KFileItem(QUrl::fromLocalFile(fileName), QString(), KFileItem::Unknown);
            break;
        case '/':
            items << KFileItem(QUrl::fromLocalFile(QStringLiteral("/")), QString(), KFileItem::Unknown);
            break;
        case 'h':
            items << KFileItem(QUrl(QStringLiteral("http://www.kde.org")), QString(), KFileItem::Unknown);
            break;
        default:
            QVERIFY(false);
        }
    }
    KFileItemListProperties props(items);
    QCOMPARE(props.supportsReading(), expectedReading);
    QCOMPARE(props.supportsDeleting(), expectedDeleting);
    QCOMPARE(props.isLocal(), expectedIsLocal);
    QCOMPARE(props.isDirectory(), expectedIsDirectory);
    QCOMPARE(props.isFile(), expectedIsFile);
    QCOMPARE(props.mimeType(), expectedMimeType);
    QCOMPARE(props.mimeGroup(), expectedMimeGroup);
}

void KFileItemTest::testIconNameForUrl_data()
{
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QString>("expectedIcon");

    QTest::newRow("root") << QUrl("file:/") << "inode-directory"; // the icon comes from KFileItem
    if (QFile::exists(QStringLiteral("/tmp"))) {
        QTest::newRow("subdir") << QUrl::fromLocalFile("/tmp") << "folder-temp";
    }

    QTest::newRow("home") << QUrl::fromLocalFile(QDir::homePath()) << "user-home";
    const QString moviesPath = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).constFirst();
    if (QFileInfo::exists(moviesPath)) {
        QTest::newRow("videos") << QUrl::fromLocalFile(moviesPath) << (moviesPath == QDir::homePath() ? "user-home" : "folder-videos");
    }

    QTest::newRow("empty") << QUrl() << "unknown";
    QTest::newRow("relative") << QUrl("foo") << "unknown";
    QTest::newRow("tilde") << QUrl("~") << "unknown";

    QTest::newRow("unknownscheme folder") << QUrl("unknownscheme:/") << "inode-directory";
    QTest::newRow("unknownscheme file") << QUrl("unknownscheme:/test") << "application-octet-stream";

    QTest::newRow("trash:/ itself") << QUrl("trash:/") << "user-trash-full";
    QTest::newRow("folder under trash:/") << QUrl("trash:/folder/") << "inode-directory";
    QTest::newRow("file under trash:/") << QUrl("trash:/test") << "application-octet-stream";
    QTest::newRow("image file under trash:/") << QUrl("trash:/test.png") << "image-png";

    QTest::newRow("https scheme") << QUrl("https://kde.org/") << "text-html";

    if (KProtocolInfo::isKnownProtocol("smb")) {
        QTest::newRow("smb root") << QUrl("smb:/") << "network-workgroup";
        QTest::newRow("smb unknown file") << QUrl("smb:/test") << "network-workgroup";
        QTest::newRow("smb directory/") << QUrl("smb:/unknown/") << "inode-directory";
        QTest::newRow("smb image file") << QUrl("smb:/test.png") << "image-png";
    }
}

void KFileItemTest::testIconNameForUrl()
{
    QFETCH(QUrl, url);
    QFETCH(QString, expectedIcon);

    if (KIO::iconNameForUrl(url) != expectedIcon) {
        qDebug() << url;
        QCOMPARE(KIO::iconNameForUrl(url), expectedIcon);
    }
}

void KFileItemTest::testMimetypeForRemoteFolder()
{
    KIO::UDSEntry entry;
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("foo"));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    QUrl url(QStringLiteral("smb://remoteFolder/foo"));
    KFileItem fileItem(entry, url);

    QCOMPARE(fileItem.mimetype(), QStringLiteral("inode/directory"));
}

void KFileItemTest::testMimetypeForRemoteFolderWithFileType()
{
    QString udsMimeType = QStringLiteral("application/x-smb-workgroup");
    QVERIFY2(QMimeDatabase().mimeTypeForName(udsMimeType).isValid(),
             qPrintable(QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation).join(':'))); // kcoreaddons installed? XDG_DATA_DIRS set?
    KIO::UDSEntry entry;
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("foo"));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, udsMimeType);

    QUrl url(QStringLiteral("smb://remoteFolder/foo"));
    KFileItem fileItem(entry, url);

    QCOMPARE(fileItem.mimetype(), udsMimeType);
}

void KFileItemTest::testCurrentMimetypeForRemoteFolder()
{
    KIO::UDSEntry entry;
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("foo"));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    QUrl url(QStringLiteral("smb://remoteFolder/foo"));
    KFileItem fileItem(entry, url);

    QCOMPARE(fileItem.currentMimeType().name(), QStringLiteral("inode/directory"));
}

void KFileItemTest::testCurrentMimetypeForRemoteFolderWithFileType()
{
    QString udsMimeType = QStringLiteral("application/x-smb-workgroup");
    KIO::UDSEntry entry;
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("foo"));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, udsMimeType);

    QUrl url(QStringLiteral("smb://remoteFolder/foo"));
    KFileItem fileItem(entry, url);

    QCOMPARE(fileItem.currentMimeType().name(), udsMimeType);
}

void KFileItemTest::testIconNameForCustomFolderIcons()
{
    // Custom folder icons should be displayed (bug 350612)

    const QString iconName = QStringLiteral("folder-music");

    QTemporaryDir tempDir;
    const QUrl url = QUrl::fromLocalFile(tempDir.path());
    KDesktopFile cfg(tempDir.path() + QLatin1String("/.directory"));
    cfg.desktopGroup().writeEntry("Icon", iconName);
    cfg.sync();

    KIO::UDSEntry entry;
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    KFileItem fileItem(entry, url);

    QCOMPARE(fileItem.iconName(), iconName);
}

void KFileItemTest::testIconNameForStandardPath()
{
    const QString iconName = QStringLiteral("folder-videos");
    const QUrl url = QUrl::fromLocalFile(QDir::homePath() + QLatin1String("/Videos"));
    QStandardPaths::setTestModeEnabled(true);

    KIO::UDSEntry entry;
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    KFileItem fileItem(entry, url);

    QCOMPARE(fileItem.iconName(), iconName);
}

#ifndef Q_OS_WIN // user/group/other write permissions are not handled on windows

void KFileItemTest::testIsReadable_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<bool>("readable");

    QTest::newRow("fully-readable") << 0444 << true;
    QTest::newRow("user-readable") << 0400 << true;
    QTest::newRow("not-readable-by-us") << 0004 << false;
    QTest::newRow("not-readable-at-all") << 0000 << false;
}

void KFileItemTest::testIsReadable()
{
    QFETCH(int, mode);
    QFETCH(bool, readable);

    QTemporaryFile file;
    QVERIFY(file.open());
    int ret = fchmod(file.handle(), (mode_t)mode);
    QCOMPARE(ret, 0);

    KFileItem fileItem(QUrl::fromLocalFile(file.fileName()));
    QCOMPARE(fileItem.isReadable(), readable);
}

// Restore permissions so that the QTemporaryDir cleanup can happen (taken from tst_qsavefile.cpp)
class PermissionRestorer
{
    Q_DISABLE_COPY(PermissionRestorer)
public:
    explicit PermissionRestorer(const QString& path) : m_path(path) {}
    ~PermissionRestorer()  { restore(); }

    inline void restore()
    {
        QFile file(m_path);
#ifdef Q_OS_UNIX
        file.setPermissions(QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
#else
        file.setPermissions(QFile::WriteOwner);
        file.remove();
#endif
    }
private:
    const QString m_path;
};

void KFileItemTest::testNonWritableDirectory()
{
    // Given a directory with a file in it
    QTemporaryDir dir;
    QVERIFY2(dir.isValid(), qPrintable(dir.errorString()));
    QFile file(dir.path() + "/file1");
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("Hello"), Q_INT64_C(5));
    file.close();
    // ... which is then made non-writable
    QVERIFY(QFile(dir.path()).setPermissions(QFile::ReadOwner | QFile::ExeOwner));
    PermissionRestorer permissionRestorer(dir.path());

    // When using KFileItemListProperties on the file
    const KFileItem item(QUrl::fromLocalFile(file.fileName()));
    KFileItemListProperties props(KFileItemList{item});

    // Then it should say moving is not supported
    QVERIFY(!props.supportsMoving());
    QVERIFY(props.supportsWriting()); // but we can write to the file itself
}

#endif // Q_OS_WIN
