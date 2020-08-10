/*
    This file is part of the KDE project.
    SPDX-FileCopyrightText: 2008-2009 Urs Wolfer <uwolfer @ kde.org>
    SPDX-FileCopyrightText: 2009-2012 Dawit Alemayehu <adawit @ kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_ACCESSMANAGERREPLY_P_H
#define KIO_ACCESSMANAGERREPLY_P_H

#include <QPointer>
#include <QNetworkReply>

namespace KIO
{
class Job;
class SimpleJob;
class MetaData;
}
class KJob;
class QUrl;

namespace KDEPrivate
{

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
                                QObject *parent = nullptr);

    explicit AccessManagerReply(const QNetworkAccessManager::Operation op,
                                const QNetworkRequest &request,
                                const QByteArray &data,
                                const QUrl &url,
                                const KIO::MetaData &metaData,
                                QObject *parent = nullptr);

    explicit AccessManagerReply(const QNetworkAccessManager::Operation op,
                                const QNetworkRequest &request,
                                QNetworkReply::NetworkError errorCode,
                                const QString &errorMessage,
                                QObject *parent = nullptr);

    virtual ~AccessManagerReply();
    qint64 bytesAvailable() const override;
    void abort() override;

    void setIgnoreContentDisposition(bool on);
    void putOnHold();

    static bool isLocalRequest(const QUrl &url);

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    bool ignoreContentDisposition(const KIO::MetaData &);
    void setHeaderFromMetaData(const KIO::MetaData &);
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
    qint64 m_offset;
    bool m_metaDataRead;
    bool m_ignoreContentDisposition;
    bool m_emitReadyReadOnMetaDataChange;
    QPointer<KIO::SimpleJob> m_kioJob;
};

}

#endif // KIO_ACCESSMANAGERREPLY_P_H
