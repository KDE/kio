/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef MOCKGUIDELEGATEEXTENSIONS_H
#define MOCKGUIDELEGATEEXTENSIONS_H

#include <openorexecutefileinterface.h>
#include <openwithhandlerinterface.h>

class MockOpenOrExecuteHandler : public KIO::OpenOrExecuteFileInterface
{
public:
    explicit MockOpenOrExecuteHandler(QObject *parent) : KIO::OpenOrExecuteFileInterface(parent) {}
    void promptUserOpenOrExecute(KJob *job, const QString &mimeType) override
    {
        Q_UNUSED(job)
        Q_UNUSED(mimeType);
        if (m_cancelIt) {
            Q_EMIT canceled();
            m_cancelIt = false;
            return;
        }

        Q_EMIT executeFile(m_executeFile);
    }

    void setExecuteFile(bool b) { m_executeFile = b; }
    void setCanceled() { m_cancelIt = true; }

private:
    bool m_executeFile = false;
    bool m_cancelIt = false;
};

class MockOpenWithHandler : public KIO::OpenWithHandlerInterface
{
public:
    explicit MockOpenWithHandler(QObject *parent) : KIO::OpenWithHandlerInterface(parent) {}
    void promptUserForApplication(KJob *job, const QList<QUrl> &url, const QString &mimeType) override
    {
        Q_UNUSED(job);
        m_urls << url;
        m_mimeTypes << mimeType;
        if (m_chosenService) {
            Q_EMIT serviceSelected(m_chosenService);
        } else {
            Q_EMIT canceled();
        }
    }
    QList<QUrl> m_urls;
    QStringList m_mimeTypes;
    KService::Ptr m_chosenService;
};

#endif // MOCKGUIDELEGATEEXTENSIONS_H
