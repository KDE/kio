/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2012 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "askuseractioninterface.h"
#include "usernotificationhandler_p.h"

#include "job_p.h"
#include "kiocoredebug.h"
#include "slave.h"
#include "slavebase.h"

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
        key += QChar(askUserDlgType);
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

static AskUserActionInterface::MessageDialogType toAskUserDlgType(int type)
{
    switch (static_cast<KIO::SlaveBase::MessageBoxType>(type)) {
    case SlaveBase::QuestionYesNo:
        return AskUserActionInterface::QuestionYesNo;
    case SlaveBase::WarningYesNo:
        return AskUserActionInterface::WarningYesNo;
    case SlaveBase::WarningContinueCancelDetailed:
    case SlaveBase::WarningContinueCancel:
        return AskUserActionInterface::WarningContinueCancel;
    case SlaveBase::WarningYesNoCancel:
        return AskUserActionInterface::WarningYesNoCancel;
    case SlaveBase::Information:
        return AskUserActionInterface::Information;
    case SlaveBase::SSLMessageBox:
        return AskUserActionInterface::SSLMessageBox;
    default:
        Q_UNREACHABLE();
        return AskUserActionInterface::MessageDialogType{};
    }
}

void UserNotificationHandler::requestMessageBox(SlaveInterface *iface, int type, const QHash<MessageBoxDataType, QVariant> &data, const QString &details)
{
    Request *r = new Request{toAskUserDlgType(type), qobject_cast<KIO::Slave *>(iface), data, details};

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
            KIO::SimpleJob *job = r->slave->job();
            AskUserActionInterface *askUserIface = job ? KIO::delegateExtension<KIO::AskUserActionInterface *>(job) : nullptr;

            if (askUserIface) {
                connect(askUserIface, &AskUserActionInterface::messageBoxResult, this, [this, key](int result) {
                    m_cachedResults.insert(key, new int(result));
                    slotProcessRequest(result);
                });

                askUserIface->requestUserMessageBox( //
                    r->askUserDlgType,
                    r->data.value(MSG_TEXT).toString(),
                    r->data.value(MSG_TITLE).toString(),
                    r->data.value(MSG_YES_BUTTON_TEXT).toString(),
                    r->data.value(MSG_NO_BUTTON_TEXT).toString(),
                    r->data.value(MSG_YES_BUTTON_ICON).toString(),
                    r->data.value(MSG_NO_BUTTON_ICON).toString(),
                    r->data.value(MSG_DONT_ASK_AGAIN).toString(),
                    r->details,
                    r->data.value(MSG_META_DATA).toMap());
                return;
            }
        }
    } else {
        qCWarning(KIO_CORE) << "Cannot prompt user because the requesting ioslave died!" << r->slave;
    }

    slotProcessRequest(result);
}

void UserNotificationHandler::slotProcessRequest(int result)
{
    Request *request = m_pendingRequests.takeFirst();
    request->slave->sendMessageBoxAnswer(result);
    delete request;

    if (m_pendingRequests.isEmpty()) {
        m_cachedResults.clear();
    } else {
        processRequest();
    }
}
