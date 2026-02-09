// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2025 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KIO_WORKER_P_H
#define KIO_WORKER_P_H

#include "workerinterface_p.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>

namespace KIO
{

class WorkerThread;
class WorkerManager;
class SimpleJob;
class SchedulerPrivate;
class DataProtocol;
class ProtoQueue;
class SimpleJobPrivate;
class UserNotificationHandler;
class WorkerFactory;

// Do not use this class directly, outside of KIO. Only use the Worker pointer
// that is returned by the scheduler for passing it around.
class Worker : public KIO::WorkerInterface
{
    Q_OBJECT
public:
    explicit Worker(const QString &protocol, QObject *parent = nullptr);

    ~Worker() override;

    /*!
     * Sends the given command to the KIO worker.
     * Called by the jobs.
     * \a cmd command id
     * \a arr byte array containing data
     */
    virtual void send(int cmd, const QByteArray &arr = QByteArray());

    /*!
     * Returns Host this worker is (was?) connected to
     */
    QString host() const;

    /*!
     * Returns port this worker is (was?) connected to
     */
    quint16 port() const;

    /*!
     * Returns User this worker is (was?) logged in as
     */
    QString user() const;

    /*!
     * Returns Passwd used to log in
     */
    QString passwd() const;

    /*!
     * Creates a new worker.
     *
     * \a protocol the protocol
     *
     * \a url is the url
     *
     * \a error is the error code on failure and undefined else.
     *
     * \a error_text is the error text on failure and undefined else.
     *
     * Returns 0 on failure, or a pointer to a worker otherwise.
     */
    static Worker *createWorker(const QString &protocol, const QUrl &url, int &error, QString &error_text);

#ifdef BUILD_TESTING
    /*!
     * Can be used for testing to inject a mock worker for the kio-test fake protocol.\
     * This function does not participate in ownership. The caller must ensure the factory is valid throughout worker creation needs.
     */
    KIOCORE_EXPORT static void setTestWorkerFactory(const std::weak_ptr<KIO::WorkerFactory> &factory);
#endif

    // == communication with connected kioworker ==
    // whenever possible prefer these methods over the respective
    // methods in connection()
    /*!
     * Suspends the operation of the attached kioworker.
     */
    virtual void suspend();

    /*!
     * Resumes the operation of the attached kioworker.
     */
    virtual void resume();

    /*!
     * Tells whether the kioworker is suspended.
     * Returns true if the kioworker is suspended.
     */
    virtual bool suspended();

    // == end communication with connected kioworker ==
private:
    friend class SchedulerPrivate;
    friend class DataProtocol;
    friend class WorkerManager;
    friend class ProtoQueue;
    friend class SimpleJobPrivate;
    friend class UserNotificationHandler;

    void setPID(qint64);
    qint64 worker_pid() const;

    void setJob(KIO::SimpleJob *job);
    KIO::SimpleJob *job() const;

    /*!
     * Force termination
     */
    void kill();

    /*!
     * Returns true if the worker survived the last mission.
     */
    bool isAlive() const;

    /*!
     * Set host for url
     *
     * \a host to connect to.
     *
     * \a port to connect to.
     *
     * \a user to login as
     *
     * \a passwd to login with
     */
    virtual void setHost(const QString &host, quint16 port, const QString &user, const QString &passwd);

    /*!
     * Clear host info.
     */
    void resetHost();

    /*!
     * Configure worker
     */
    virtual void setConfig(const MetaData &config);

    /*!
     * The protocol this worker handles.
     *
     * Returns name of protocol handled by this worker, as seen by the user
     */
    QString protocol() const;

    void setProtocol(const QString &protocol);

    /*!
     * Returns The number of seconds this worker has been idle.
     */
    int idleTime() const;

    /*!
     * Marks this worker as idle.
     */
    void setIdle();

    void ref();
    void deref();
    void aboutToDelete();

    void setWorkerThread(WorkerThread *thread);

public Q_SLOTS: // TODO KF6: make all three slots private
    void accept();
    void gotInput();

Q_SIGNALS:
    void workerDied(KIO::Worker *worker);

private:
    WorkerThread *m_workerThread = nullptr; // only set for in-process workers
    QString m_protocol;
    QString m_host;
    QString m_user;
    QString m_passwd;
    KIO::ConnectionServer *m_workerConnServer;
    KIO::SimpleJob *m_job = nullptr;
    qint64 m_pid = 0; // only set for out-of-process workers
    quint16 m_port = 0;
    bool m_dead = false;
    QElapsedTimer m_idleSince;
    int m_refCount = 1;
#ifdef BUILD_TESTING
    static inline std::weak_ptr<KIO::WorkerFactory> s_testFactory; // for testing purposes, can be set to a mock factory
#endif
};

}

#endif
