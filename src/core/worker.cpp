/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "worker_p.h"

#include <qplatformdefs.h>
#include <stdio.h>

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <QPluginLoader>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <KLibexec>
#include <KLocalizedString>

#include "commands_p.h"
#include "connection_p.h"
#include "connectionserver.h"
#include "dataprotocol_p.h"
#include "kioglobal_p.h"
#include <config-kiocore.h> // KDE_INSTALL_FULL_LIBEXECDIR_KF
#include <kprotocolinfo.h>

#include "kiocoredebug.h"
#include "workerbase.h"
#include "workerfactory.h"
#include "workerthread_p.h"

using namespace KIO;

static constexpr int s_workerConnectionTimeoutMin = 2;

// Without debug info we consider it an error if the worker doesn't connect
// within 10 seconds.
// With debug info we give the worker an hour so that developers have a chance
// to debug their worker.
#ifdef NDEBUG
static constexpr int s_workerConnectionTimeoutMax = 10;
#else
static constexpr int s_workerConnectionTimeoutMax = 3600;
#endif

void Worker::accept()
{
    m_workerConnServer->setNextPendingConnection(m_connection);
    m_workerConnServer->deleteLater();
    m_workerConnServer = nullptr;

    connect(m_connection, &Connection::readyRead, this, &Worker::gotInput);
}

void Worker::timeout()
{
    if (m_dead) { // already dead? then workerDied was emitted and we are done
        return;
    }
    if (m_connection->isConnected()) {
        return;
    }

    /*qDebug() << "worker failed to connect to application pid=" << m_pid
                 << " protocol=" << m_protocol;*/
    if (m_pid && KIOPrivate::isProcessAlive(m_pid)) {
        int delta_t = m_contact_started.elapsed() / 1000;
        // qDebug() << "worker is slow... pid=" << m_pid << " t=" << delta_t;
        if (delta_t < s_workerConnectionTimeoutMax) {
            QTimer::singleShot(1000 * s_workerConnectionTimeoutMin, this, &Worker::timeout);
            return;
        }
    }
    // qDebug() << "Houston, we lost our worker, pid=" << m_pid;
    m_connection->close();
    m_dead = true;
    QString arg = m_protocol;
    if (!m_host.isEmpty()) {
        arg += QLatin1String("://") + m_host;
    }
    // qDebug() << "worker failed to connect pid =" << m_pid << arg;

    ref();
    // Tell the job about the problem.
    Q_EMIT error(ERR_WORKER_DIED, arg);
    // Tell the scheduler about the problem.
    Q_EMIT workerDied(this);
    // After the above signal we're dead!!
    deref();
}

Worker::Worker(const QString &protocol, QObject *parent)
    : WorkerInterface(parent)
    , m_protocol(protocol)
    , m_workerProtocol(protocol)
    , m_workerConnServer(new KIO::ConnectionServer)
{
    m_contact_started.start();
    m_workerConnServer->setParent(this);
    m_workerConnServer->listenForRemote();
    if (!m_workerConnServer->isListening()) {
        qCWarning(KIO_CORE) << "KIO Connection server not listening, could not connect";
    }
    m_connection = new Connection(Connection::Type::Application, this);
    connect(m_workerConnServer, &ConnectionServer::newConnection, this, &Worker::accept);
}

Worker::~Worker()
{
    // qDebug() << "destructing worker object pid =" << m_pid;
    delete m_workerConnServer;
}

QString Worker::protocol() const
{
    return m_protocol;
}

void Worker::setProtocol(const QString &protocol)
{
    m_protocol = protocol;
}

QString Worker::workerProtocol() const
{
    return m_workerProtocol;
}

QString Worker::host() const
{
    return m_host;
}

quint16 Worker::port() const
{
    return m_port;
}

QString Worker::user() const
{
    return m_user;
}

QString Worker::passwd() const
{
    return m_passwd;
}

void Worker::setIdle()
{
    m_idleSince.start();
}

void Worker::ref()
{
    m_refCount++;
}

void Worker::deref()
{
    m_refCount--;
    if (!m_refCount) {
        aboutToDelete();
        if (m_workerThread) {
            // When on a thread, delete in a thread to prevent deadlocks between the main thread and the worker thread.
            // This most notably can happen when the worker thread uses QDBus, because traffic will generally be routed
            // through the main loop.
            // Generally speaking we'd want to avoid waiting in the main thread anyway, the worker stopping isn't really
            // useful for anything but delaying deletion.
            // https://bugs.kde.org/show_bug.cgi?id=468673
            WorkerThread *workerThread = nullptr;
            std::swap(workerThread, m_workerThread);
            workerThread->setParent(nullptr);
            connect(workerThread, &QThread::finished, workerThread, &QThread::deleteLater);
            workerThread->quit();
        }
        delete this; // yes it reads funny, but it's too late for a deleteLater() here, no event loop anymore
    }
}

void Worker::aboutToDelete()
{
    m_connection->disconnect(this);
    this->disconnect();
}

void Worker::setWorkerThread(WorkerThread *thread)
{
    m_workerThread = thread;
}

int Worker::idleTime() const
{
    if (!m_idleSince.isValid()) {
        return 0;
    }
    return m_idleSince.elapsed() / 1000;
}

void Worker::setPID(qint64 pid)
{
    m_pid = pid;
}

qint64 Worker::worker_pid() const
{
    return m_pid;
}

void Worker::setJob(KIO::SimpleJob *job)
{
    m_job = job;
}

KIO::SimpleJob *Worker::job() const
{
    return m_job;
}

bool Worker::isAlive() const
{
    return !m_dead;
}

void Worker::suspend()
{
    m_connection->suspend();
}

void Worker::resume()
{
    m_connection->resume();
}

bool Worker::suspended()
{
    return m_connection->suspended();
}

void Worker::send(int cmd, const QByteArray &arr)
{
    m_connection->send(cmd, arr);
}

void Worker::gotInput()
{
    if (m_dead) { // already dead? then workerDied was emitted and we are done
        return;
    }
    ref();
    if (!dispatch()) {
        m_connection->close();
        m_dead = true;
        QString arg = m_protocol;
        if (!m_host.isEmpty()) {
            arg += QLatin1String("://") + m_host;
        }
        // qDebug() << "worker died pid =" << m_pid << arg;
        // Tell the job about the problem.
        Q_EMIT error(ERR_WORKER_DIED, arg);
        // Tell the scheduler about the problem.
        Q_EMIT workerDied(this);
    }
    deref();
    // Here we might be dead!!
}

void Worker::kill()
{
    m_dead = true; // OO can be such simple.
    if (m_pid) {
        qCDebug(KIO_CORE) << "killing worker process pid" << m_pid << "(" << m_protocol + QLatin1String("://") + m_host << ")";
        KIOPrivate::sendTerminateSignal(m_pid);
        m_pid = 0;
    } else if (m_workerThread) {
        qCDebug(KIO_CORE) << "aborting worker thread for " << m_protocol + QLatin1String("://") + m_host;
        m_workerThread->abort();
    }
    deref();
}

void Worker::setHost(const QString &host, quint16 port, const QString &user, const QString &passwd)
{
    m_host = host;
    m_port = port;
    m_user = user;
    m_passwd = passwd;

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << m_host << m_port << m_user << m_passwd;
    m_connection->send(CMD_HOST, data);
}

void Worker::resetHost()
{
    m_host = QStringLiteral("<reset>");
}

void Worker::setConfig(const MetaData &config)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << config;
    m_connection->send(CMD_CONFIG, data);
}

/**
 * @returns true if the worker should not be created because it would insecurely ask users for a password.
 *          false is returned when the worker is either safe because only the root user can write to it, or if this kio binary is already not secure.
 */
bool isWorkerSecurityCompromised(const QString &workerPath, const QString &protocolName, int &error, QString &error_text)
{
#ifdef Q_OS_WIN
    return false; // This security check is not (yet?) implemented on Windows.
#endif
    auto onlyRootHasWriteAccess = [](const QString &filePath) {
        QFileInfo file(filePath);
        return file.ownerId() == 0 && (file.groupId() == 0 || !file.permission(QFileDevice::WriteGroup)) && !file.permission(QFileDevice::WriteOther);
    };
    if (onlyRootHasWriteAccess(workerPath)) {
        return false;
    }

    // The worker can be modified by non-privileged processes! If it ever asks for elevated privileges, this could lead to a privilege escalation!
    // We will only let this slide if we are e.g. in a development environment. In a development environment the binaries are not system-installed,
    // so this KIO library itself would also be writable by non-privileged processes. We check if this KIO library is safe from unprivileged tampering.
    // If it is not, the security is already compromised anyway, so we ignore that the security of the worker binary is compromised as well.
    std::optional<bool> kioCoreSecurityCompromised;

    QDir folderOfKioBinary{KLibexec::path(QString{})};
    const QFileInfoList kioBinariesAndSymlinks = folderOfKioBinary.entryInfoList({QLatin1String{"*KIOCore.so*"}}, QDir::Files);
    for (const QFileInfo &kioFile : kioBinariesAndSymlinks) {
        if (onlyRootHasWriteAccess(kioFile.absoluteFilePath())) {
            kioCoreSecurityCompromised = false;
            break; // As long as there is at least one library which appears to be secure, we assume that the whole execution is supposed to be secure.
        } else {
            kioCoreSecurityCompromised = true;
            // We have found a library that is compromised. We continue searching in case this library was only placed here to circumvent this security check.
        }
    }
    const auto adminWorkerSecurityWarning{i18nc("@info %2 is a path",
                                                "The security of the KIO worker for protocol ’%1’, which typically asks for elevated permissions, "
                                                "can not be guaranteed because users other than root have permission to modify it at %2.",
                                                protocolName,
                                                workerPath)};
    if (!kioCoreSecurityCompromised.has_value() || !kioCoreSecurityCompromised.value()) {
        error_text = adminWorkerSecurityWarning;
        error = KIO::ERR_CANNOT_CREATE_WORKER;
        return true;
    }
    // Both KIO as well as the worker can be written to by non-root objects, so there is no protection against these binaries being compromised.
    // Notwithstanding, we let everything continue as normal because we assume this is a development environment.
    qCInfo(KIO_CORE) << adminWorkerSecurityWarning;
    return false;
}

// TODO KF6: return std::unique_ptr
Worker *Worker::createWorker(const QString &protocol, const QUrl &url, int &error, QString &error_text)
{
    Q_UNUSED(url)
    // qDebug() << "createWorker" << protocol << "for" << url;
    // Firstly take into account all special workers
    if (protocol == QLatin1String("data")) {
        return new DataProtocol();
    }

    const QString _name = KProtocolInfo::exec(protocol);
    if (_name.isEmpty()) {
        error_text = i18n("Unknown protocol '%1'.", protocol);
        error = KIO::ERR_CANNOT_CREATE_WORKER;
        return nullptr;
    }

    // find the KIO worker using QPluginLoader; kioworker would do this
    // anyway, but if it doesn't exist, we want to be able to return
    // a useful error message immediately
    QPluginLoader loader(_name);
    const QString lib_path = loader.fileName();
    if (lib_path.isEmpty()) {
        error_text = i18n("Can not find a KIO worker for protocol '%1'.", protocol);
        error = KIO::ERR_CANNOT_CREATE_WORKER;
        return nullptr;
    }

    // The "admin" worker will ask for elevated permissions.
    // Make sure no malware hides behind the "admin" protocol.
    if (protocol == QLatin1String("admin") && isWorkerSecurityCompromised(lib_path, protocol, error, error_text)) {
        return nullptr;
    }

    auto *worker = new Worker(protocol);
    const QUrl workerAddress = worker->m_workerConnServer->address();
    if (workerAddress.isEmpty()) {
        error_text = i18n("Can not create a socket for launching a KIO worker for protocol '%1'.", protocol);
        error = KIO::ERR_CANNOT_CREATE_WORKER;
        delete worker;
        return nullptr;
    }

    // Threads are enabled by default, set KIO_ENABLE_WORKER_THREADS=0 to disable them
    const auto useThreads = []() {
        return qgetenv("KIO_ENABLE_WORKER_THREADS") != "0";
    };
    static bool bUseThreads = useThreads();

    // Threads have performance benefits, but degrade robustness
    // (a worker crashing kills the app). So let's only enable the feature for kio_file, for now.
    if (protocol == QLatin1String("admin") || (bUseThreads && protocol == QLatin1String("file"))) {
        auto *factory = qobject_cast<WorkerFactory *>(loader.instance());
        if (factory) {
            auto *thread = new WorkerThread(worker, factory, workerAddress.toString().toLocal8Bit());
            thread->start();
            worker->setWorkerThread(thread);
            return worker;
        } else {
            qCWarning(KIO_CORE) << lib_path << "doesn't implement WorkerFactory?";
        }
    }

    const QStringList args = QStringList{lib_path, protocol, QString(), workerAddress.toString()};
    // qDebug() << "kioworker" << ", " << lib_path << ", " << protocol << ", " << QString() << ", " << workerAddress;

    // search paths
    QStringList searchPaths = KLibexec::kdeFrameworksPaths(QStringLiteral("libexec/kf6"));
    searchPaths.append(QFile::decodeName(KDE_INSTALL_FULL_LIBEXECDIR_KF)); // look at our installation location
    QString kioworkerExecutable = QStandardPaths::findExecutable(QStringLiteral("kioworker"), searchPaths);
    if (kioworkerExecutable.isEmpty()) {
        // Fallback to PATH. On win32 we install to bin/ which tests outside
        // KIO cannot not find at the time ctest is run because it
        // isn't the same as applicationDirPath().
        kioworkerExecutable = QStandardPaths::findExecutable(QStringLiteral("kioworker"));
    }
    if (kioworkerExecutable.isEmpty()) {
        error_text = i18n("Can not find 'kioworker' executable at '%1'", searchPaths.join(QLatin1String(", ")));
        error = KIO::ERR_CANNOT_CREATE_WORKER;
        delete worker;
        return nullptr;
    }

    qint64 pid = 0;
    QProcess process;
    process.setProgram(kioworkerExecutable);
    process.setArguments(args);
#ifdef Q_OS_UNIX
    process.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors);
#endif
    process.startDetached(&pid);
    worker->setPID(pid);

    return worker;
}

#include "moc_worker_p.cpp"
