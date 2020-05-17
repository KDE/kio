/* This file is part of the KDE libraries
    Copyright (C) 2020 David Faure <faure@kde.org>

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

#include <QApplication>
#include <QList>
#include <QUrl>
#include <QDebug>

#include <KIO/OpenFileManagerWindowJob>

int main(int argc, char **argv)
{
    QApplication::setApplicationName(QStringLiteral("openfilemanagerwindowtest"));
    QApplication app(argc, argv);

    const QList<QUrl> urls{ QUrl(QStringLiteral("file:///etc/fstab")), QUrl(QStringLiteral("file:///etc/passwd")) };

    auto *job = new KIO::OpenFileManagerWindowJob();
    job->setHighlightUrls(urls);
    job->start();

    QObject::connect(job, &KJob::result, job, [&](KJob *job) {
            app.exit(job->error());
    });

    return app.exec();
}

