/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_TRASH_H
#define KIO_TRASH_H

#include <kio/slavebase.h>
#include "trashimpl.h"

namespace KIO
{
class Job;
}

typedef TrashImpl::TrashedFileInfo TrashedFileInfo;
typedef TrashImpl::TrashedFileInfoList TrashedFileInfoList;

class TrashProtocol : public QObject, public KIO::SlaveBase
{
    Q_OBJECT
public:
    TrashProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app);
    virtual ~TrashProtocol();
    void stat(const QUrl &url) override;
    void listDir(const QUrl &url) override;
    void get(const QUrl &url) override;
    void put(const QUrl &url, int, KIO::JobFlags flags) override;
    void rename(const QUrl &src, const QUrl &dest, KIO::JobFlags) override;
    void copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags) override;
    // TODO (maybe) chmod( const QUrl& url, int permissions );
    void del(const QUrl &url, bool isfile) override;
    /**
     * Special actions: (first int in the byte array)
     * 1 : empty trash
     * 2 : migrate old (pre-kde-3.4) trash contents
     * 3 : restore a file to its original location. Args: QUrl trashURL.
     */
    void special(const QByteArray &data) override;

Q_SIGNALS:
    void leaveModality();

protected:
    void virtual_hook(int id, void *data) override;

private Q_SLOTS:
    void slotData(KIO::Job *, const QByteArray &);
    void slotMimetype(KIO::Job *, const QString &);
    void jobFinished(KJob *job);

private:
    typedef enum { Copy, Move } CopyOrMove;
    void copyOrMoveFromTrash(const QUrl &src, const QUrl &dest, bool overwrite, CopyOrMove action);
    void copyOrMoveToTrash(const QUrl &src, const QUrl &dest, CopyOrMove action);
    void createTopLevelDirEntry(KIO::UDSEntry &entry);
    bool createUDSEntry(const QString &physicalPath, const QString &displayFileName, const QString &internalFileName,
                        KIO::UDSEntry &entry, const TrashedFileInfo &info);
    void listRoot();
    void restore(const QUrl &trashURL);
    void enterLoop();
    void fileSystemFreeSpace(const QUrl &url);
    KIO::StatDetails getStatDetails();

    TrashImpl impl;
    QString m_userName;
    QString m_groupName;
};

#endif
