/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2012 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef USERNOTIFICATIONHANDLER_P_H
#define USERNOTIFICATIONHANDLER_P_H

#include <QCache>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QVariant>

namespace KIO
{
class Worker;
class WorkerInterface;

class UserNotificationHandler : public QObject
{
    Q_OBJECT
public:
    enum MessageBoxDataType {
        MSG_TEXT,
        MSG_TITLE,
        MSG_PRIMARYACTION_TEXT,
        MSG_SECONDARYACTION_TEXT,
        MSG_PRIMARYACTION_ICON,
        MSG_SECONDARYACTION_ICON,
        MSG_DONT_ASK_AGAIN,
        MSG_DETAILS,
        MSG_META_DATA,
    };

    class Request
    {
    public:
        QString key() const;

        int type;
        QPointer<Worker> worker;
        QHash<MessageBoxDataType, QVariant> data;
    };

    explicit UserNotificationHandler(QObject *parent = nullptr);
    ~UserNotificationHandler() override;

    void requestMessageBox(WorkerInterface *iface, int type, const QHash<MessageBoxDataType, QVariant> &data);

    void sslError(WorkerInterface *iface, const QVariantMap &sslErrorData);

private Q_SLOTS:
    void processRequest();
    void slotProcessRequest(int result);

private:
    QCache<QString, int> m_cachedResults;
    QList<Request *> m_pendingRequests;
};
}

#endif
