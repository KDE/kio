/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998, 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2000-2005 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#ifndef KIOEXEC_MAIN_H
#define KIOEXEC_MAIN_H

#include <QObject>
#include <QString>
#include <QList>

#include <QUrl>
#include <QDateTime>

namespace KIO
{
class Job;
}

class KJob;
class QCommandLineParser;

class KIOExec : public QObject
{
    Q_OBJECT
public:
    KIOExec(const QStringList &args, bool tempFiles, const QString &suggestedFileName);

    bool exited() const
    {
        return mExited;
    }

public Q_SLOTS:
    void slotResult(KJob *);
    void slotRunApp();

protected:
    bool mExited;
    bool mTempFiles;
    bool mUseDaemon;
    QString mSuggestedFileName;
    int counter;
    int expectedCounter;
    QString command;
    struct FileInfo {
        QString path;
        QUrl url;
        QDateTime time;
    };
    QList<FileInfo> fileList;
    int jobCounter;
    QList<KIO::Job *> jobList;
};

#endif
