/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2012 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef USERNOTIFICATIONHANDLER_P_H
#define USERNOTIFICATIONHANDLER_P_H

#include <QObject>
#include <QHash>
#include <QCache>
#include <QPointer>
#include <QVariant>

namespace KIO
{
class Slave;
class SlaveInterface;

class UserNotificationHandler : public QObject
{
    Q_OBJECT
public:
    enum MessageBoxDataType {
        MSG_TEXT,
        MSG_CAPTION,
        MSG_YES_BUTTON_TEXT,
        MSG_NO_BUTTON_TEXT,
        MSG_YES_BUTTON_ICON,
        MSG_NO_BUTTON_ICON,
        MSG_DONT_ASK_AGAIN,
        MSG_META_DATA,
    };

    class Request
    {

    public:
        QString key() const;

        int type;
        QPointer<Slave> slave;
        QHash<MessageBoxDataType, QVariant> data;
    };

    explicit UserNotificationHandler(QObject *parent = nullptr);
    virtual ~UserNotificationHandler();

    void requestMessageBox(SlaveInterface *iface, int type, const QHash<MessageBoxDataType, QVariant> &data);

private Q_SLOTS:
    void processRequest();

private:
    QCache<QString, int> m_cachedResults;
    QList<Request *> m_pendingRequests;
};
}

#endif
