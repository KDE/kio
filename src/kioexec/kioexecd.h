/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2017 Elvis Angelaccio <elvis.angelaccio@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
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

