/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2012 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "usernotificationhandler_p.h"

#include "slave.h"
#include "job_p.h"
#include "kiocoredebug.h"


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

UserNotificationHandler::UserNotificationHandler(QObject *parent)
    : QObject(parent)
{
}

UserNotificationHandler::~UserNotificationHandler()
{
    qDeleteAll(m_pendingRequests);
}

void UserNotificationHandler::requestMessageBox(SlaveInterface *iface, int type, const QHash<MessageBoxDataType, QVariant> &data)
{
    Request *r = new Request;
    r->type = type;
    r->slave = qobject_cast<KIO::Slave *>(iface);
    r->data = data;

    m_pendingRequests.append(r);
    if (m_pendingRequests.count() == 1) {
        QTimer::singleShot(0, this, &UserNotificationHandler::processRequest);
    }
}

void UserNotificationHandler::processRequest()
{
    if (m_pendingRequests.isEmpty()) {
        return;
    }

    int result = -1;
    Request *r = m_pendingRequests.first();

    if (r->slave) {
        const QString key = r->key();

        if (m_cachedResults.contains(key)) {
            result = *(m_cachedResults[key]);
        } else {
            JobUiDelegateExtension *delegateExtension = nullptr;
            if (r->slave->job())
                delegateExtension = SimpleJobPrivate::get(r->slave->job())->m_uiDelegateExtension;
            if (!delegateExtension)
                delegateExtension = KIO::defaultJobUiDelegateExtension();
            if (delegateExtension) {
                const JobUiDelegateExtension::MessageBoxType type = static_cast<JobUiDelegateExtension::MessageBoxType>(r->type);
                result = delegateExtension->requestMessageBox(type,
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
        qCWarning(KIO_CORE) << "Cannot prompt user because the requesting ioslave died!" << r->slave;
    }

    r->slave->sendMessageBoxAnswer(result);
    m_pendingRequests.removeFirst();
    delete r;

    if (m_pendingRequests.isEmpty()) {
        m_cachedResults.clear();
    } else {
        QTimer::singleShot(0, this, &UserNotificationHandler::processRequest);
    }
}
