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

#include "usernotificationhandler_p.h"

#include "slave.h"
#include "job_p.h"

#include <QDebug>

#include <QTimer>

using namespace KIO;

QString UserNotificationHandler::Request::key() const
{
    QString key;
    if (slave) {
        key = slave->protocol();
        key += slave->host();
        key += slave->port();
        key += QLatin1Char('-');
        key += type;
    }
    return key;
}

UserNotificationHandler::UserNotificationHandler(QObject* parent)
    :QObject(parent)
{
}

UserNotificationHandler::~UserNotificationHandler()
{
    qDeleteAll(m_pendingRequests);
}

void UserNotificationHandler::requestMessageBox(SlaveInterface* iface, int type, const QHash<MessageBoxDataType, QVariant>& data)
{
    Request* r = new Request;
    r->type = type;
    r->slave = qobject_cast<KIO::Slave*>(iface);
    r->data = data;

    m_pendingRequests.append(r);
    if (m_pendingRequests.count() == 1) {
        QTimer::singleShot(0, this, SLOT(processRequest()));
    }
}

void UserNotificationHandler::processRequest()
{
    if (m_pendingRequests.isEmpty())
        return;

    int result = -1;
    Request* r = m_pendingRequests.first();

    if (r->slave) {
        const QString key = r->key();

        if (m_cachedResults.contains(key)) {
            result = *(m_cachedResults[key]);
        } else if (r->slave->job()) {
            SimpleJobPrivate* jobPrivate = SimpleJobPrivate::get(r->slave->job());
            if (jobPrivate) {
                result = jobPrivate->requestMessageBox(r->type,
                                                      r->data.value(MSG_TEXT).toString(),
                                                      r->data.value(MSG_CAPTION).toString(),
                                                      r->data.value(MSG_YES_BUTTON_TEXT).toString(),
                                                      r->data.value(MSG_NO_BUTTON_TEXT).toString(),
                                                      r->data.value(MSG_YES_BUTTON_ICON).toString(),
                                                      r->data.value(MSG_NO_BUTTON_ICON).toString(),
                                                      r->data.value(MSG_DONT_ASK_AGAIN).toString(),
                                                      r->data.value(MSG_META_DATA).toMap());
            }
            m_cachedResults.insert(key, new int(result));
        }
    } else {
        qWarning() << "Cannot prompt user because the requesting ioslave died!" << r->slave;
    }

    r->slave->sendMessageBoxAnswer(result);
    m_pendingRequests.removeFirst();

    if (!m_pendingRequests.isEmpty()) {
        QTimer::singleShot(0, this, SLOT(processRequest()));
    }
}
