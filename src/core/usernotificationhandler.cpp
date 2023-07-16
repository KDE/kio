/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2012 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "askuseractioninterface.h"
#include "usernotificationhandler_p.h"

#include "job_p.h"
#include "kiocoredebug.h"
#include "worker_p.h"
#include "workerbase.h"

#include <QTimer>

using namespace KIO;

QString UserNotificationHandler::Request::key() const
{
    QString key;
    if (worker) {
        key = worker->protocol();
        key += worker->host();
        key += worker->port();
        key += QLatin1Char('-');
        key += QChar(type);
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

void UserNotificationHandler::requestMessageBox(WorkerInterface *iface, int type, const QHash<MessageBoxDataType, QVariant> &data)
{
    Request *r = new Request;
    r->type = type;
    r->worker = qobject_cast<KIO::Worker *>(iface);
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

    if (r->worker) {
        const QString key = r->key();

        if (m_cachedResults.contains(key)) {
            result = *(m_cachedResults[key]);
        } else {
            KIO::SimpleJob *job = r->worker->job();
            AskUserActionInterface *askUserIface = job ? KIO::delegateExtension<KIO::AskUserActionInterface *>(job) : nullptr;

            if (askUserIface) {
                connect(askUserIface, &AskUserActionInterface::messageBoxResult, this, &UserNotificationHandler::slotProcessRequest, Qt::UniqueConnection);

                const auto type = [r]() -> AskUserActionInterface::MessageDialogType {
                    switch (r->type) {
                    case WorkerBase::QuestionTwoActions:
                        return AskUserActionInterface::QuestionTwoActions;
                    case WorkerBase::WarningTwoActions:
                        return AskUserActionInterface::WarningTwoActions;
                    case WorkerBase::WarningContinueCancel:
                    case WorkerBase::WarningContinueCancelDetailed:
                        return AskUserActionInterface::WarningContinueCancel;
                    case WorkerBase::WarningTwoActionsCancel:
                        return AskUserActionInterface::WarningTwoActionsCancel;
                    case WorkerBase::Information:
                        return AskUserActionInterface::Information;
                    default:
                        Q_UNREACHABLE();
                        return AskUserActionInterface::MessageDialogType{};
                    }
                }();

                askUserIface->requestUserMessageBox(type,
                                                    r->data.value(MSG_TEXT).toString(),
                                                    r->data.value(MSG_TITLE).toString(),
                                                    r->data.value(MSG_PRIMARYACTION_TEXT).toString(),
                                                    r->data.value(MSG_SECONDARYACTION_TEXT).toString(),
                                                    r->data.value(MSG_PRIMARYACTION_ICON).toString(),
                                                    r->data.value(MSG_SECONDARYACTION_ICON).toString(),
                                                    r->data.value(MSG_DONT_ASK_AGAIN).toString(),
                                                    r->data.value(MSG_DETAILS).toString());
                return;
            }
        }
    } else {
        qCWarning(KIO_CORE) << "Cannot prompt user because the requesting KIO worker died!" << r->worker;
    }

    slotProcessRequest(result);
}

void UserNotificationHandler::slotProcessRequest(int result)
{
    Request *request = m_pendingRequests.takeFirst();
    m_cachedResults.insert(request->key(), new int(result));

    request->worker->sendMessageBoxAnswer(result);
    delete request;

    if (m_pendingRequests.isEmpty()) {
        m_cachedResults.clear();
    } else {
        processRequest();
    }
}

void UserNotificationHandler::sslError(WorkerInterface *iface, const QVariantMap &sslErrorData)
{
    KIO::SimpleJob *job = qobject_cast<KIO::Worker *>(iface)->job();
    AskUserActionInterface *askUserIface = job ? KIO::delegateExtension<KIO::AskUserActionInterface *>(job) : nullptr;

    if (askUserIface) {
        askUserIface->askIgnoreSslErrors(sslErrorData, nullptr);

        connect(askUserIface, &AskUserActionInterface::askIgnoreSslErrorsResult, this, [iface](int result) {
            iface->sendSslErrorAnswer(result);
        });
    }
}
#include "moc_usernotificationhandler_p.cpp"
