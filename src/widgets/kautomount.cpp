/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kautomount.h"
#include <KDirWatch>
#include "kio/job.h"
#include <KIO/JobUiDelegate>
#include <KIO/OpenUrlJob>
#include "kio_widgets_debug.h"
#include <kdirnotify.h>
#include <KJobUiDelegate>
#include <kmountpoint.h>
#include <QDebug>

/***********************************************************************
 *
 * Utility classes
 *
 ***********************************************************************/

class KAutoMountPrivate
{
public:
    KAutoMountPrivate(KAutoMount *qq, const QString &device, const QString &mountPoint,
                      const QString &desktopFile, bool showFileManagerWindow)
        : q(qq), m_strDevice(device), m_desktopFile(desktopFile), m_mountPoint(mountPoint),
          m_bShowFilemanagerWindow(showFileManagerWindow)
    { }

    KAutoMount *q;
    QString m_strDevice;
    QString m_desktopFile;
    QString m_mountPoint;
    bool m_bShowFilemanagerWindow;

    void slotResult(KJob *);
};

KAutoMount::KAutoMount(bool _readonly, const QByteArray &_format, const QString &_device,
                       const QString  &_mountpoint, const QString &_desktopFile,
                       bool _show_filemanager_window)
    : d(new KAutoMountPrivate(this, _device, _mountpoint, _desktopFile, _show_filemanager_window))
{
    KIO::Job *job = KIO::mount(_readonly, _format, _device, _mountpoint);
    connect(job, &KJob::result, this, [this, job]() { d->slotResult(job); });
}

KAutoMount::~KAutoMount()
{
    delete d;
}

void KAutoMountPrivate::slotResult(KJob *job)
{
    if (job->error()) {
        Q_EMIT q->error();
        job->uiDelegate()->showErrorMessage();
    } else {
        const KMountPoint::List mountPoints(KMountPoint::currentMountPoints());
        KMountPoint::Ptr mp = mountPoints.findByDevice(m_strDevice);
        // Mounting devices using "LABEL=" or "UUID=" will fail if we look for
        // the device using only its real name since /etc/mtab will never contain
        // the LABEL or UUID entries. Hence, we check using the mount point below
        // when device name lookup fails. #247235
        if (!mp) {
            mp = mountPoints.findByPath(m_mountPoint);
        }

        if (!mp) {
            qCWarning(KIO_WIDGETS) << m_strDevice << "was correctly mounted, but findByDevice() didn't find it."
                       << "This looks like a bug, please report it on https://bugs.kde.org, together with your /etc/fstab and /etc/mtab lines for this device";
        } else {
            const QUrl url = QUrl::fromLocalFile(mp->mountPoint());
            //qDebug() << "KAutoMount: m_strDevice=" << m_strDevice << " -> mountpoint=" << mountpoint;
            if (m_bShowFilemanagerWindow) {
                KIO::OpenUrlJob *job = new KIO::OpenUrlJob(url, QStringLiteral("inode/directory"));
                job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr /*TODO - window*/));
                job->setRunExecutables(true);
                job->start();
            }
            // Notify about the new stuff in that dir, in case of opened windows showing it
            org::kde::KDirNotify::emitFilesAdded(url);
        }

        // Update the desktop file which is used for mount/unmount (icon change)
        //qDebug() << " mount finished : updating " << m_desktopFile;
        org::kde::KDirNotify::emitFilesChanged(QList<QUrl>() << QUrl::fromLocalFile(m_desktopFile));
        //KDirWatch::self()->setFileDirty( m_desktopFile );

        Q_EMIT q->finished();
    }
    q->deleteLater();
}

class KAutoUnmountPrivate
{
public:
    KAutoUnmountPrivate(KAutoUnmount *qq, const QString &_mountpoint, const QString &_desktopFile)
        : q(qq), m_desktopFile(_desktopFile), m_mountpoint(_mountpoint)
    {}
    KAutoUnmount * const q;
    QString m_desktopFile;
    QString m_mountpoint;

    void slotResult(KJob *job);
};

KAutoUnmount::KAutoUnmount(const QString &_mountpoint, const QString &_desktopFile)
    : d(new KAutoUnmountPrivate(this, _mountpoint, _desktopFile))
{
    KIO::Job *job = KIO::unmount(d->m_mountpoint);
    connect(job, &KJob::result, this, [this, job]() { d->slotResult(job); });
}

void KAutoUnmountPrivate::slotResult(KJob *job)
{
    if (job->error()) {
        Q_EMIT q->error();
        job->uiDelegate()->showErrorMessage();
    } else {
        // Update the desktop file which is used for mount/unmount (icon change)
        //qDebug() << "unmount finished : updating " << m_desktopFile;
        org::kde::KDirNotify::emitFilesChanged(QList<QUrl>() << QUrl::fromLocalFile(m_desktopFile));
        //KDirWatch::self()->setFileDirty( m_desktopFile );

        // Notify about the new stuff in that dir, in case of opened windows showing it
        // You may think we removed files, but this may have also readded some
        // (if the mountpoint wasn't empty). The only possible behavior on FilesAdded
        // is to relist the directory anyway.
        const QUrl mp = QUrl::fromLocalFile(m_mountpoint);
        org::kde::KDirNotify::emitFilesAdded(mp);

        Q_EMIT q->finished();
    }

    q->deleteLater();
}

KAutoUnmount::~KAutoUnmount()
{
    delete d;
}

#include "moc_kautomount.cpp"
