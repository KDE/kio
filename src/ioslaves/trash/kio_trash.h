/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_TRASH_H
#define KIO_TRASH_H

#include "trashimpl.h"
#include <kio/workerbase.h>

namespace KIO
{
class Job;
}

typedef TrashImpl::TrashedFileInfo TrashedFileInfo;
typedef TrashImpl::TrashedFileInfoList TrashedFileInfoList;

class TrashProtocol : public QObject, public KIO::WorkerBase
{
    Q_OBJECT
public:
    TrashProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app);
    ~TrashProtocol() override;
    KIO::WorkerResult stat(const QUrl &url) override;
    KIO::WorkerResult listDir(const QUrl &url) override;
    KIO::WorkerResult get(const QUrl &url) override;
    KIO::WorkerResult put(const QUrl &url, int, KIO::JobFlags flags) override;
    KIO::WorkerResult rename(const QUrl &src, const QUrl &dest, KIO::JobFlags) override;
    KIO::WorkerResult copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags) override;
    // TODO (maybe) chmod( const QUrl& url, int permissions );
    KIO::WorkerResult del(const QUrl &url, bool isfile) override;
    /**
     * Special actions: (first int in the byte array)
     * 1 : empty trash
     * 2 : migrate old (pre-kde-3.4) trash contents
     * 3 : restore a file to its original location. Args: QUrl trashURL.
     */
    KIO::WorkerResult special(const QByteArray &data) override;
    KIO::WorkerResult fileSystemFreeSpace(const QUrl &url) override;

Q_SIGNALS:
    void leaveModality(int errid, const QString &text);

private Q_SLOTS:
    void slotData(KIO::Job *, const QByteArray &);
    void slotMimetype(KIO::Job *, const QString &);
    void jobFinished(KJob *job);

private:
    KIO::WorkerResult initImpl();
    typedef enum { Copy, Move } CopyOrMove;
    KIO::WorkerResult copyOrMoveFromTrash(const QUrl &src, const QUrl &dest, bool overwrite, CopyOrMove action);
    KIO::WorkerResult copyOrMoveToTrash(const QUrl &src, const QUrl &dest, CopyOrMove action);
    void createTopLevelDirEntry(KIO::UDSEntry &entry);
    bool createUDSEntry(const QString &physicalPath,
                        const QString &displayFileName,
                        const QString &internalFileName,
                        KIO::UDSEntry &entry,
                        const TrashedFileInfo &info);
    KIO::WorkerResult listRoot();
    KIO::WorkerResult restore(const QUrl &trashURL);
    KIO::WorkerResult enterLoop();
    KIO::StatDetails getStatDetails();

    TrashImpl impl;
    QString m_userName;
    QString m_groupName;
};

#endif
