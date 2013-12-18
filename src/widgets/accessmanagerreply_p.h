/*
 * This file is part of the KDE project.
 *
 * Copyright (C) 2008 - 2009 Urs Wolfer <uwolfer @ kde.org>
 * Copyright (C) 2009 - 2012 Dawit Alemayehu <adawit @ kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef KIO_ACCESSMANAGERREPLY_P_H
#define KIO_ACCESSMANAGERREPLY_P_H

#include <QtCore/QPointer>
#include <QtNetwork/QNetworkReply>

namespace KIO
{
    class Job;
    class SimpleJob;
    class MetaData;
}
class KJob;
class QUrl;

namespace KDEPrivate {

/**
 * Used for KIO::AccessManager; KDE implementation of QNetworkReply.
 *
 * @since 4.3
 * @author Urs Wolfer \<uwolfer @ kde.org\>
 */

class AccessManagerReply : public QNetworkReply
{
    Q_OBJECT
public:
    explicit AccessManagerReply(const QNetworkAccessManager::Operation op,
                                const QNetworkRequest &request,
                                KIO::SimpleJob *kioJob,
                                bool emitReadyReadOnMetaDataChange = false,
                                QObject *parent = 0);

    explicit AccessManagerReply(const QNetworkAccessManager::Operation op,
                                const QNetworkRequest &request,
                                const QByteArray& data,
                                const QUrl& url,
                                const KIO::MetaData& metaData,
                                QObject *parent = 0);

    explicit AccessManagerReply(const QNetworkAccessManager::Operation op,
                                const QNetworkRequest &request,
                                QNetworkReply::NetworkError errorCode,
                                const QString& errorMessage,
                                QObject *parent = 0);

    virtual ~AccessManagerReply();
    virtual qint64 bytesAvailable() const;
    virtual void abort();

    void setIgnoreContentDisposition(bool on);
    void putOnHold();

    static bool isLocalRequest(const QUrl& url);

protected:
    virtual qint64 readData(char *data, qint64 maxSize);
    bool ignoreContentDisposition(const KIO::MetaData&);
    void setHeaderFromMetaData(const KIO::MetaData&);
    void readHttpResponseHeaders(KIO::Job *);
    int jobError(KJob *kJob);
    void emitFinished(bool state, Qt::ConnectionType type = Qt::AutoConnection);

private Q_SLOTS:
    void slotData(KIO::Job *kioJob, const QByteArray &data);
    void slotMimeType(KIO::Job *kioJob, const QString &mimeType);
    void slotResult(KJob *kJob);
    void slotStatResult(KJob *kJob);
    void slotRedirection(KIO::Job *job, const QUrl &url);
    void slotPercent(KJob *job, unsigned long percent);

private:
    QByteArray m_data;
    bool m_metaDataRead;
    bool m_ignoreContentDisposition;
    bool m_emitReadyReadOnMetaDataChange;
    QPointer<KIO::SimpleJob> m_kioJob;
};

}

#endif // KIO_ACCESSMANAGERREPLY_P_H
