/*
    SPDX-License-Identifier: LGPL-2.0-or-later
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2019-2022 Harald Sitter <sitter@kde.org>
*/

#ifndef WORKERBASE_P_H
#define WORKERBASE_P_H

#include "workerbase.h"

#include <config-kiocore.h>

#include "connection_p.h"
#include "kiocoredebug.h"
#include "kpasswdserverclient.h"
#include <commands_p.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QStandardPaths>

#include <KConfig>
#include <KConfigGroup>

#if defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID)
#include <KAuth/Action>
#endif

/* clang-format off */
#define KIO_DATA \
    QByteArray data; \
    QDataStream stream(&data, QIODevice::WriteOnly); \
    stream
/* clang-format on */

static constexpr int KIO_MAX_ENTRIES_PER_BATCH = 200;
static constexpr int KIO_MAX_SEND_BATCH_TIME = 300;

// TODO: Enable once file KIO worker is ported away and add endif, similar in the header file
// #if KIOCORE_BUILD_DEPRECATED_SINCE(version where file:/ KIO worker was ported)

#if KIO_ASSERT_WORKER_STATES
#define KIO_STATE_ASSERT(cond, where, what) Q_ASSERT_X(cond, where, what)
#else
/* clang-format off */
#define KIO_STATE_ASSERT(cond, where, what) \
    do { \
        if (!(cond)) { \
            qCWarning(KIO_CORE) << what; \
        } \
    } while (false)
#endif
/* clang-format on */

namespace KIO
{

class WorkerBasePrivate
{
public:
    inline QString protocolName() const
    {
        return QString::fromUtf8(mProtocol);
    }

    WorkerBase *const q;

    explicit WorkerBasePrivate(WorkerBase *owner)
        : q(owner)
        , nextTimeoutMsecs(0)
        , m_confirmationAsked(false)
        , m_privilegeOperationStatus(OperationNotAllowed)
    {
        if (!qEnvironmentVariableIsEmpty("KIOWORKER_ENABLE_TESTMODE")) {
            QStandardPaths::setTestModeEnabled(true);
        } else if (!qEnvironmentVariableIsEmpty("KIOSLAVE_ENABLE_TESTMODE")) {
            QStandardPaths::setTestModeEnabled(true);
            qCWarning(KIO_CORE)
                << "KIOSLAVE_ENABLE_TESTMODE is deprecated for KF6, and will be unsupported soon. Please use KIOWORKER_ENABLE_TESTMODE with KF6.";
        }
        pendingListEntries.reserve(KIO_MAX_ENTRIES_PER_BATCH);
        appConnection.setReadMode(Connection::ReadMode::Polled);
    }
    // ~SlaveBasePrivate() = default;

    UDSEntryList pendingListEntries;
    QElapsedTimer m_timeSinceLastBatch;
    Connection appConnection{Connection::Type::Worker};
    QString poolSocket;
    bool isConnectedToApp;

    QString slaveid;
    bool resume : 1;
    bool needSendCanResume : 1;
    bool onHold : 1;
    bool inOpenLoop : 1;
    std::atomic<bool> wasKilled = false;
    std::atomic<bool> exit_loop = false;
    std::atomic<bool> runInThread = false;
    MetaData configData;
    KConfig *config = nullptr;
    KConfigGroup *configGroup = nullptr;
    QMap<QString, QVariant> mapConfig;
    QUrl onHoldUrl;

    QElapsedTimer lastTimeout;
    QElapsedTimer nextTimeout;
    qint64 nextTimeoutMsecs;
    KIO::filesize_t totalSize;
    KRemoteEncoding *remotefile = nullptr;
    QByteArray timeoutData;

#ifdef WITH_QTDBUS
    std::unique_ptr<KPasswdServerClient> m_passwdServerClient;
#endif
    bool m_rootEntryListed = false;

    bool m_confirmationAsked;
    QSet<QString> m_tempAuths;
    QString m_warningTitle;
    QString m_warningMessage;
    int m_privilegeOperationStatus;

    /**
     * Name of the protocol supported by this slave
     */
    QByteArray mProtocol;
    // Often used by TcpSlaveBase and unlikely to change
    MetaData mOutgoingMetaData;
    MetaData mIncomingMetaData;

    void updateTempAuthStatus()
    {
#if defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID)
        QSet<QString>::iterator it = m_tempAuths.begin();
        while (it != m_tempAuths.end()) {
            KAuth::Action action(*it);
            if (action.status() != KAuth::Action::AuthorizedStatus) {
                it = m_tempAuths.erase(it);
            } else {
                ++it;
            }
        }
#endif
    }

    bool hasTempAuth() const
    {
        return !m_tempAuths.isEmpty();
    }

    // Reconstructs configGroup from configData and mIncomingMetaData
    void rebuildConfig()
    {
        mapConfig.clear();

        // mIncomingMetaData cascades over config, so we write config first,
        // to let it be overwritten
        MetaData::ConstIterator end = configData.constEnd();
        for (MetaData::ConstIterator it = configData.constBegin(); it != end; ++it) {
            mapConfig.insert(it.key(), it->toUtf8());
        }

        end = mIncomingMetaData.constEnd();
        for (MetaData::ConstIterator it = mIncomingMetaData.constBegin(); it != end; ++it) {
            mapConfig.insert(it.key(), it->toUtf8());
        }

        delete configGroup;
        configGroup = nullptr;
        delete config;
        config = nullptr;
    }

#ifdef WITH_QTDBUS
    KPasswdServerClient *passwdServerClient()
    {
        if (!m_passwdServerClient) {
            m_passwdServerClient = std::make_unique<KPasswdServerClient>();
        }

        return m_passwdServerClient.get();
    }
#endif
};

} // namespace KIO

#endif
