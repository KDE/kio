// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>

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

// Do not use this class directly, outside of KIO. Only use the Worker pointer
// that is returned by the scheduler for passing it around.
class Worker : public KIO::WorkerInterface
{
    Q_OBJECT
public:
    explicit Worker(const QString &protocol, QObject *parent = nullptr);

    ~Worker() override;

    /**
     * Sends the given command to the KIO worker.
     * Called by the jobs.
     * @param cmd command id
     * @param arr byte array containing data
     */
    virtual void send(int cmd, const QByteArray &arr = QByteArray());

    /**
     * The actual protocol used to handle the request.
     *
     * This method will return a different protocol than
     * the one obtained by using protocol() if a
     * proxy-server is used for the given protocol.  This
     * usually means that this method will return "http"
     * when the actual request was to retrieve a resource
     * from an "ftp" server by going through a proxy server.
     *
     * @return the actual protocol (KIO worker) that handled the request
     */
    QString workerProtocol() const;

    /**
     * @return Host this worker is (was?) connected to
     */
    QString host() const;

    /**
     * @return port this worker is (was?) connected to
     */
    quint16 port() const;

    /**
     * @return User this worker is (was?) logged in as
     */
    QString user() const;

    /**
     * @return Passwd used to log in
     */
    QString passwd() const;

    /**
     * Creates a new worker.
     *
     * @param protocol the protocol
     * @param url is the url
     * @param error is the error code on failure and undefined else.
     * @param error_text is the error text on failure and undefined else.
     *
     * @return 0 on failure, or a pointer to a worker otherwise.
     */
    static Worker *createWorker(const QString &protocol, const QUrl &url, int &error, QString &error_text);

    // == communication with connected kioworker ==
    // whenever possible prefer these methods over the respective
    // methods in connection()
    /**
     * Suspends the operation of the attached kioworker.
     */
    virtual void suspend();

    /**
     * Resumes the operation of the attached kioworker.
     */
    virtual void resume();

    /**
     * Tells whether the kioworker is suspended.
     * @return true if the kioworker is suspended.
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

    /**
     * Force termination
     */
    void kill();

    /**
     * @return true if the worker survived the last mission.
     */
    bool isAlive() const;

    /**
     * Set host for url
     * @param host to connect to.
     * @param port to connect to.
     * @param user to login as
     * @param passwd to login with
     */
    virtual void setHost(const QString &host, quint16 port, const QString &user, const QString &passwd);

    /**
     * Clear host info.
     */
    void resetHost();

    /**
     * Configure worker
     */
    virtual void setConfig(const MetaData &config);

    /**
     * The protocol this worker handles.
     *
     * @return name of protocol handled by this worker, as seen by the user
     */
    QString protocol() const;

    void setProtocol(const QString &protocol);

    /**
     * @return The number of seconds this worker has been idle.
     */
    int idleTime() const;

    /**
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
    void timeout();

Q_SIGNALS:
    void workerDied(KIO::Worker *worker);

private:
    WorkerThread *m_workerThread = nullptr; // only set for in-process workers
    QString m_protocol;
    QString m_workerProtocol;
    QString m_host;
    QString m_user;
    QString m_passwd;
    KIO::ConnectionServer *m_workerConnServer;
    KIO::SimpleJob *m_job = nullptr;
    qint64 m_pid = 0; // only set for out-of-process workers
    quint16 m_port = 0;
    bool m_dead = false;
    QElapsedTimer m_contact_started;
    QElapsedTimer m_idleSince;
    int m_refCount = 1;
};

}

#endif
