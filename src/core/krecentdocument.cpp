/* -*- c++ -*-
    SPDX-FileCopyrightText: 2000 Daniel M. Duley <mosfet@kde.org>

    SPDX-License-Identifier: BSD-2-Clause
*/

#include "krecentdocument.h"

#ifdef Q_OS_WIN
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include <QDebug>
#include <kio/global.h>
#include <KDesktopFile>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>
#include <qplatformdefs.h>

#include <KConfigGroup>
#include <KSharedConfig>

QString KRecentDocument::recentDocumentDirectory()
{
    // need to change this path, not sure where
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/RecentDocuments/");
}

QStringList KRecentDocument::recentDocuments()
{
    QDir d(recentDocumentDirectory(), QStringLiteral("*.desktop"), QDir::Time,
           QDir::Files | QDir::Readable | QDir::Hidden);

    if (!d.exists()) {
        d.mkdir(recentDocumentDirectory());
    }

    const QStringList list = d.entryList();
    QStringList fullList;

    for (const QString &fileName : list) {
        QString pathDesktop;
        if (fileName.startsWith(QLatin1Char(':'))) {
            // See: https://bugreports.qt.io/browse/QTBUG-11223
            pathDesktop = KRecentDocument::recentDocumentDirectory() + fileName;
        } else {
            pathDesktop = d.absoluteFilePath(fileName);
        }
        KDesktopFile tmpDesktopFile(pathDesktop);
        QUrl urlDesktopFile(tmpDesktopFile.desktopGroup().readPathEntry("URL", QString()));
        if (urlDesktopFile.isLocalFile() && !QFile(urlDesktopFile.toLocalFile()).exists()) {
            d.remove(pathDesktop);
        } else {
            fullList.append(pathDesktop);
        }
    }

    return fullList;
}

void KRecentDocument::add(const QUrl &url)
{
    // desktopFileName is in QGuiApplication but we're in KIO Core here
    QString desktopEntryName = QCoreApplication::instance()->property("desktopFileName").toString();
    if (desktopEntryName.isEmpty()) {
        desktopEntryName = QCoreApplication::applicationName();
    }
    KRecentDocument::add(url, desktopEntryName);
    // ### componentName might not match the service filename...
}

void KRecentDocument::add(const QUrl &url, const QString &desktopEntryName)
{
    if (url.isLocalFile() && url.toLocalFile().startsWith(QDir::tempPath())) {
        return;    // inside tmp resource, do not save
    }

    QString openStr = url.toDisplayString();
    openStr.replace(QRegularExpression(QStringLiteral("\\$")), QStringLiteral("$$"));   // Desktop files with type "Link" are $-variable expanded

    // qDebug() << "KRecentDocument::add for " << openStr;
    KConfigGroup config = KSharedConfig::openConfig()->group(QByteArray("RecentDocuments"));
    bool useRecent = config.readEntry(QStringLiteral("UseRecent"), true);
    int maxEntries = config.readEntry(QStringLiteral("MaxEntries"), 10);

    if (!useRecent || maxEntries <= 0) {
        return;
    }

    const QString path = recentDocumentDirectory();
    const QString fileName = url.fileName();
    // don't create a file called ".desktop", it will lead to an empty name in kio_recentdocuments
    const QString dStr = path + (fileName.isEmpty() ? QStringLiteral("unnamed") : fileName);

    QString ddesktop = dStr + QLatin1String(".desktop");

    int i = 1;
    // check for duplicates
    while (QFile::exists(ddesktop)) {
        // see if it points to the same file and application
        KDesktopFile tmp(ddesktop);
        if (tmp.desktopGroup().readPathEntry("URL", QString()) == url.toDisplayString()
            && tmp.desktopGroup().readEntry("X-KDE-LastOpenedWith") == desktopEntryName) {
            // Set access and modification time to current time
            ::utime(QFile::encodeName(ddesktop).constData(), nullptr);
            return;
        }
        // if not append a (num) to it
        ++i;
        if (i > maxEntries) {
            break;
        }
        ddesktop = dStr + QStringLiteral("[%1].desktop").arg(i);
    }

    QDir dir(path);
    // check for max entries, delete oldest files if exceeded
    const QStringList list = dir.entryList(QDir::Files | QDir::Hidden, QFlags<QDir::SortFlag>(QDir::Time | QDir::Reversed));
    i = list.count();
    if (i > maxEntries - 1) {
        QStringList::ConstIterator it;
        it = list.begin();
        while (i > maxEntries - 1) {
            QFile::remove(dir.absolutePath() + QLatin1Char('/') + (*it));
            --i;
            ++it;
        }
    }

    // create the applnk
    KDesktopFile configFile(ddesktop);
    KConfigGroup conf = configFile.desktopGroup();
    conf.writeEntry("Type", QStringLiteral("Link"));
    conf.writePathEntry("URL", openStr);
    // If you change the line below, change the test in the above loop
    conf.writeEntry("X-KDE-LastOpenedWith", desktopEntryName);
    conf.writeEntry("Name", url.fileName());
    conf.writeEntry("Icon", KIO::iconNameForUrl(url));
}

void KRecentDocument::clear()
{
    const QStringList list = recentDocuments();
    QDir dir;
    for (const QString &desktopFilePath : list) {
        dir.remove(desktopFilePath);
    }
}

int KRecentDocument::maximumItems()
{
    KConfigGroup cg(KSharedConfig::openConfig(), QStringLiteral("RecentDocuments"));
    return cg.readEntry(QStringLiteral("MaxEntries"), 10);
}

