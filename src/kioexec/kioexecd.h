/*  This file is part of the KDE libraries
    Copyright (C) 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KIOEXECD_H
#define KIOEXECD_H

#include <KDEDModule>

#include <QMap>
#include <QUrl>
#include <QTimer>

class KDirWatch;

class KIOExecd : public KDEDModule
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KIOExecd")

public:
    KIOExecd(QObject *parent, const QList<QVariant>&);
    virtual ~KIOExecd();

public Q_SLOTS:
    void watch(const QString &path, const QString &destUrl);

private Q_SLOTS:
    void slotDirty(const QString &path);
    void slotDeleted(const QString &path);
    void slotCreated(const QString &path);
    void slotCheckDeletedFiles();

private:
    KDirWatch *m_watcher;
    // temporary file and associated remote file
    QMap<QString, QUrl> m_watched;
    // temporary file and the last date it was removed
    QMap<QString, QDateTime> m_deleted;
    QTimer m_timer;
};

#endif

