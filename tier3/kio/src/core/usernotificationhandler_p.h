/* This file is part of the KDE libraries
   Copyright (C) 2012 Dawit Alemayehu <adawit@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef USERNOTIFICATIONHANDLER_P_H
#define USERNOTIFICATIONHANDLER_P_H

#include <QObject>
#include <QHash>
#include <QCache>
#include <QPointer>
#include <QVariant>


namespace KIO {
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
        MSG_META_DATA
    };

    class Request
    {

    public:
        QString key() const;

        int type;
        QPointer<Slave> slave;
        QHash<MessageBoxDataType, QVariant> data;
    };

    UserNotificationHandler(QObject* parent = 0);
    virtual ~UserNotificationHandler();

    void requestMessageBox(SlaveInterface* iface, int type, const QHash<MessageBoxDataType, QVariant>& data);

private Q_SLOTS:
    void processRequest();

private:
    QCache<QString, int> m_cachedResults;
    QList<Request*> m_pendingRequests;
};
}

#endif
