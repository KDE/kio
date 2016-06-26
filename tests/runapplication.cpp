/* This file is part of the KDE libraries
    Copyright (c) 1999 Waldo Bastian <bastian@kde.org>
    Copyright (c) 2009 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <KRun>
#include <KService>
#include <QApplication>
#include <QDebug>

int
main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QString serviceId = QStringLiteral("kwrite.desktop");
    if (argc > 1) {
        serviceId = QString::fromLocal8Bit(argv[1]);
    }
    QList<QUrl> urls;
    if (argc > 2) {
        urls << QUrl::fromUserInput(QString::fromLocal8Bit(argv[2]));
    }

    KService::Ptr service = KService::serviceByDesktopPath(serviceId);
    if (!service) {
        service = new KService(serviceId);
    }
    qint64 pid = KRun::runApplication(*service, urls, nullptr);
    qDebug() << "Started. pid=" << pid;

    return 0;
}
