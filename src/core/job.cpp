/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000-2009 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "job.h"
#include "job_p.h"

#include <time.h>

#include <KLocalizedString>
#include <KSandbox>
#include <KStringHandler>

#include "kiocoredebug.h"
#include "worker_p.h"
#include <kio/jobuidelegateextension.h>

#if HAVE_QTDBUS
#include <QDBusConnection>
#include <QDBusPendingCallWatcher>

#include "inhibit_interface.h"
#include "portal_inhibit_interface.h"
#include "portal_request_interface.h"
#endif

using namespace KIO;

static constexpr QLatin1String g_portalServiceName{"org.freedesktop.portal.Desktop"};
static constexpr QLatin1String g_portalInhibitObjectPath{"/org/freedesktop/portal/desktop"};

static constexpr QLatin1String g_inhibitServiceName{"org.freedesktop.PowerManagement.Inhibit"};
static constexpr QLatin1String g_inhibitObjectPath{"/org/freedesktop/PowerManagement/Inhibit"};

Job::Job()
    : KCompositeJob(nullptr)
    , d_ptr(new JobPrivate)
{
    d_ptr->q_ptr = this;
    setCapabilities(KJob::Killable | KJob::Suspendable);
}

Job::Job(JobPrivate &dd)
    : KCompositeJob(nullptr)
    , d_ptr(&dd)
{
    d_ptr->q_ptr = this;
    setCapabilities(KJob::Killable | KJob::Suspendable);
}

Job::~Job()
{
    delete d_ptr;
}

JobUiDelegateExtension *Job::uiDelegateExtension() const
{
    Q_D(const Job);
    return d->m_uiDelegateExtension;
}

void Job::setUiDelegateExtension(JobUiDelegateExtension *extension)
{
    Q_D(Job);
    d->m_uiDelegateExtension = extension;
}

bool Job::addSubjob(KJob *jobBase)
{
    // qDebug() << "addSubjob(" << jobBase << ") this=" << this;

    bool ok = KCompositeJob::addSubjob(jobBase);
    KIO::Job *job = qobject_cast<KIO::Job *>(jobBase);
    if (ok && job) {
        // Copy metadata into subjob (e.g. window-id, user-timestamp etc.)
        Q_D(Job);
        job->mergeMetaData(d->m_outgoingMetaData);

        // Forward information from that subjob.
        connect(job, &KJob::speed, this, [this](KJob *job, ulong speed) {
            Q_UNUSED(job);
            emitSpeed(speed);
        });
        job->setProperty("widget", property("widget")); // see KJobWidgets
        job->setProperty("window", property("window")); // see KJobWidgets
        job->setProperty("userTimestamp", property("userTimestamp")); // see KJobWidgets
        job->setUiDelegateExtension(d->m_uiDelegateExtension);
    }
    return ok;
}

bool Job::removeSubjob(KJob *jobBase)
{
    // qDebug() << "removeSubjob(" << jobBase << ") this=" << this << "subjobs=" << subjobs().count();
    return KCompositeJob::removeSubjob(jobBase);
}

static QString url_description_string(const QUrl &url)
{
    return url.scheme() == QLatin1String("data") ? QStringLiteral("data:[...]") : KStringHandler::csqueeze(url.toDisplayString(QUrl::PreferLocalFile), 100);
}

KIO::JobPrivate::~JobPrivate()
{
    uninhibitSuspend();
}

void JobPrivate::doInhibitSuspend()
{
}

void JobPrivate::inhibitSuspend(const QString &reason)
{
#if HAVE_QTDBUS
    if (KSandbox::isInside()) {
        Q_ASSERT(m_portalInhibitionRequest.path().isEmpty());

        org::freedesktop::portal::Inhibit inhibitInterface{g_portalServiceName, g_portalInhibitObjectPath, QDBusConnection::sessionBus()};
        QVariantMap args;
        if (!reason.isEmpty()) {
            args.insert(QStringLiteral("reason"), reason);
        }
        auto call = inhibitInterface.Inhibit(QString() /* TODO window. */, 4 /* Suspend */, args);
        // This is not parented to the job, so we can properly clean up the inhibiton
        // should the job finish before the inhibition has been processed.
        auto *watcher = new QDBusPendingCallWatcher(call);
        QPointer<Job> guard(q_ptr);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher, [this, guard, watcher, reason] {
            QDBusPendingReply<QDBusObjectPath> reply = *watcher;

            if (reply.isError()) {
                qCWarning(KIO_CORE).nospace() << "Failed to inhibit suspend with reason " << reason << ": " << reply.error().message();
            } else {
                const QDBusObjectPath requestPath = reply.value();

                // By the time the inhibition returned, the job was already gone. Uninhibit again.
                if (!guard) {
                    org::freedesktop::portal::Request requestInterface{g_portalServiceName, requestPath.path(), QDBusConnection::sessionBus()};
                    requestInterface.Close();
                } else {
                    m_portalInhibitionRequest = requestPath;
                }
            }

            watcher->deleteLater();
        });
    } else {
        Q_ASSERT(!m_inhibitionCookie);

        QString appName = q_ptr->property("desktopFileName").toString();
        if (appName.isEmpty()) {
            // desktopFileName is in QGuiApplication but we're in KIO Core here.
            appName = QCoreApplication::instance()->property("desktopFileName").toString();
        }
        if (appName.isEmpty()) {
            appName = QCoreApplication::applicationName();
        }

        org::freedesktop::PowerManagement::Inhibit inhibitInterface{g_inhibitServiceName, g_inhibitObjectPath, QDBusConnection::sessionBus()};
        auto call = inhibitInterface.Inhibit(appName, reason);
        auto *watcher = new QDBusPendingCallWatcher(call);
        QPointer<Job> guard(q_ptr);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher, [this, guard, watcher, appName, reason] {
            QDBusPendingReply<uint> reply = *watcher;

            if (reply.isError()) {
                qCWarning(KIO_CORE).nospace() << "Failed to inhibit suspend for " << appName << " with reason " << reason << ": " << reply.error().message();
            } else {
                const uint cookie = reply.value();

                if (!guard) {
                    org::freedesktop::PowerManagement::Inhibit inhibitInterface{g_inhibitServiceName, g_inhibitObjectPath, QDBusConnection::sessionBus()};
                    inhibitInterface.UnInhibit(cookie);
                } else {
                    m_inhibitionCookie = cookie;
                }
            }

            watcher->deleteLater();
        });
    }
#else
    Q_UNUSED(reason)
#endif
}

void JobPrivate::uninhibitSuspend()
{
#if HAVE_QTDBUS
    if (!m_portalInhibitionRequest.path().isEmpty()) {
        org::freedesktop::portal::Request requestInterface{g_portalServiceName, m_portalInhibitionRequest.path(), QDBusConnection::sessionBus()};
        auto call = requestInterface.Close();
        auto *watcher = new QDBusPendingCallWatcher(call, q_ptr);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, q_ptr, [this, watcher] {
            QDBusPendingReply<> reply = *watcher;

            if (reply.isError()) {
                qCWarning(KIO_CORE) << "Failed to uninhibit suspend:" << reply.error().message();
            } else {
                m_portalInhibitionRequest = QDBusObjectPath();
            }

            watcher->deleteLater();
        });
    } else if (m_inhibitionCookie) {
        org::freedesktop::PowerManagement::Inhibit inhibitInterface{g_inhibitServiceName, g_inhibitObjectPath, QDBusConnection::sessionBus()};
        const int cookie = *m_inhibitionCookie;
        auto call = inhibitInterface.UnInhibit(cookie);
        auto *watcher = new QDBusPendingCallWatcher(call, q_ptr);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, q_ptr, [this, watcher, cookie] {
            QDBusPendingReply<> reply = *watcher;

            if (reply.isError()) {
                qCWarning(KIO_CORE).nospace() << "Failed to uninhibit suspend for cookie" << cookie << ": " << reply.error().message();
            } else {
                m_inhibitionCookie.reset();
            }

            watcher->deleteLater();
        });
    }
#endif
}

void JobPrivate::emitMoving(KIO::Job *job, const QUrl &src, const QUrl &dest)
{
    static const QString s_title = i18nc("@title job", "Moving");
    static const QString s_source = i18nc("The source of a file operation", "Source");
    static const QString s_destination = i18nc("The destination of a file operation", "Destination");
    Q_EMIT job->description(job, s_title, qMakePair(s_source, url_description_string(src)), qMakePair(s_destination, url_description_string(dest)));
}

void JobPrivate::emitRenaming(KIO::Job *job, const QUrl &src, const QUrl &dest)
{
    static const QString s_title = i18nc("@title job", "Renaming");
    static const QString s_source = i18nc("The source of a file operation", "Source");
    static const QString s_destination = i18nc("The destination of a file operation", "Destination");
    Q_EMIT job->description(job, s_title, {s_source, url_description_string(src)}, {s_destination, url_description_string(dest)});
}

void JobPrivate::emitCopying(KIO::Job *job, const QUrl &src, const QUrl &dest)
{
    static const QString s_title = i18nc("@title job", "Copying");
    static const QString s_source = i18nc("The source of a file operation", "Source");
    static const QString s_destination = i18nc("The destination of a file operation", "Destination");
    Q_EMIT job->description(job, s_title, qMakePair(s_source, url_description_string(src)), qMakePair(s_destination, url_description_string(dest)));
}

void JobPrivate::emitCreatingDir(KIO::Job *job, const QUrl &dir)
{
    static const QString s_title = i18nc("@title job", "Creating directory");
    static const QString s_directory = i18n("Directory");
    Q_EMIT job->description(job, s_title, qMakePair(s_directory, url_description_string(dir)));
}

void JobPrivate::emitDeleting(KIO::Job *job, const QUrl &url)
{
    static const QString s_title = i18nc("@title job", "Deleting");
    static const QString s_file = i18n("File");
    Q_EMIT job->description(job, s_title, qMakePair(s_file, url_description_string(url)));
}

void JobPrivate::emitStating(KIO::Job *job, const QUrl &url)
{
    static const QString s_title = i18nc("@title job", "Examining");
    static const QString s_file = i18n("File");
    Q_EMIT job->description(job, s_title, qMakePair(s_file, url_description_string(url)));
}

void JobPrivate::emitTransferring(KIO::Job *job, const QUrl &url)
{
    static const QString s_title = i18nc("@title job", "Transferring");
    static const QString s_source = i18nc("The source of a file operation", "Source");
    Q_EMIT job->description(job, s_title, qMakePair(s_source, url_description_string(url)));
}

void JobPrivate::emitMounting(KIO::Job *job, const QString &dev, const QString &point)
{
    Q_EMIT job->description(job, i18nc("@title job", "Mounting"), qMakePair(i18n("Device"), dev), qMakePair(i18n("Mountpoint"), point));
}

void JobPrivate::emitUnmounting(KIO::Job *job, const QString &point)
{
    Q_EMIT job->description(job, i18nc("@title job", "Unmounting"), qMakePair(i18n("Mountpoint"), point));
}

bool Job::doKill()
{
    // kill all subjobs, without triggering their result slot
    for (KJob *job : subjobs()) {
        job->kill(KJob::Quietly);
    }
    clearSubjobs();

    return true;
}

bool Job::doSuspend()
{
    for (KJob *job : subjobs()) {
        if (!job->suspend()) {
            return false;
        }
    }
    d_ptr->uninhibitSuspend();
    return true;
}

bool Job::doResume()
{
    for (KJob *job : subjobs()) {
        if (!job->resume()) {
            return false;
        }
    }
    d_ptr->doInhibitSuspend();
    return true;
}

// Job::errorString is implemented in job_error.cpp

void Job::setParentJob(Job *job)
{
    Q_D(Job);
    Q_ASSERT(d->m_parentJob == nullptr);
    Q_ASSERT(job);
    d->m_parentJob = job;
}

Job *Job::parentJob() const
{
    return d_func()->m_parentJob;
}

MetaData Job::metaData() const
{
    return d_func()->m_incomingMetaData;
}

QString Job::queryMetaData(const QString &key)
{
    return d_func()->m_incomingMetaData.value(key, QString());
}

void Job::setMetaData(const KIO::MetaData &_metaData)
{
    Q_D(Job);
    d->m_outgoingMetaData = _metaData;
}

void Job::addMetaData(const QString &key, const QString &value)
{
    d_func()->m_outgoingMetaData.insert(key, value);
}

void Job::addMetaData(const QMap<QString, QString> &values)
{
    Q_D(Job);
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        d->m_outgoingMetaData.insert(it.key(), it.value());
    }
}

void Job::mergeMetaData(const QMap<QString, QString> &values)
{
    Q_D(Job);
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        // there's probably a faster way
        if (!d->m_outgoingMetaData.contains(it.key())) {
            d->m_outgoingMetaData.insert(it.key(), it.value());
        }
    }
}

MetaData Job::outgoingMetaData() const
{
    return d_func()->m_outgoingMetaData;
}

QByteArray JobPrivate::privilegeOperationData()
{
    PrivilegeOperationStatus status = OperationNotAllowed;

    if (m_parentJob) {
        QByteArray jobData = m_parentJob->d_func()->privilegeOperationData();
        // Copy meta-data from parent job
        m_incomingMetaData.insert(QStringLiteral("TestData"), m_parentJob->queryMetaData(QStringLiteral("TestData")));
        return jobData;
    } else {
        if (m_privilegeExecutionEnabled) {
            status = OperationAllowed;
            switch (m_operationType) {
            case ChangeAttr:
                m_title = i18n("Change Attribute");
                m_message = i18n(
                    "Root privileges are required to change file attributes. "
                    "Do you want to continue?");
                break;
            case Copy:
                m_title = i18n("Copy Files");
                m_message = i18n(
                    "Root privileges are required to complete the copy operation. "
                    "Do you want to continue?");
                break;
            case Delete:
                m_title = i18n("Delete Files");
                m_message = i18n(
                    "Root privileges are required to complete the delete operation. "
                    "However, doing so may damage your system. Do you want to continue?");
                break;
            case MkDir:
                m_title = i18n("Create Folder");
                m_message = i18n(
                    "Root privileges are required to create this folder. "
                    "Do you want to continue?");
                break;
            case Move:
                m_title = i18n("Move Items");
                m_message = i18n(
                    "Root privileges are required to complete the move operation. "
                    "Do you want to continue?");
                break;
            case Rename:
                m_title = i18n("Rename");
                m_message = i18n(
                    "Root privileges are required to complete renaming. "
                    "Do you want to continue?");
                break;
            case Symlink:
                m_title = i18n("Create Symlink");
                m_message = i18n(
                    "Root privileges are required to create a symlink. "
                    "Do you want to continue?");
                break;
            case Transfer:
                m_title = i18n("Transfer data");
                m_message = i18n(
                    "Root privileges are required to complete transferring data. "
                    "Do you want to continue?");
                Q_FALLTHROUGH();
            default:
                break;
            }

            if (m_outgoingMetaData.value(QStringLiteral("UnitTesting")) == QLatin1String("true")) {
                // Set meta-data for the top-level job
                m_incomingMetaData.insert(QStringLiteral("TestData"), QStringLiteral("PrivilegeOperationAllowed"));
            }
        }
    }

    QByteArray parentJobData;
    QDataStream ds(&parentJobData, QIODevice::WriteOnly);
    ds << status << m_title << m_message;
    return parentJobData;
}

//////////////////////////

class KIO::DirectCopyJobPrivate : public KIO::SimpleJobPrivate
{
public:
    DirectCopyJobPrivate(const QUrl &url, int command, const QByteArray &packedArgs)
        : SimpleJobPrivate(url, command, packedArgs)
    {
    }

    /*!
     * \internal
     * Called by the scheduler when a @p worker gets to
     * work on this job.
     * \a worker the worker that starts working on this job
     */
    void start(Worker *worker) override;

    Q_DECLARE_PUBLIC(DirectCopyJob)
};

DirectCopyJob::DirectCopyJob(const QUrl &url, const QByteArray &packedArgs)
    : SimpleJob(*new DirectCopyJobPrivate(url, CMD_COPY, packedArgs))
{
    setUiDelegate(KIO::createDefaultJobUiDelegate());
}

DirectCopyJob::~DirectCopyJob()
{
}

void DirectCopyJobPrivate::start(Worker *worker)
{
    Q_Q(DirectCopyJob);
    q->connect(worker, &WorkerInterface::canResume, q, &DirectCopyJob::slotCanResume);
    SimpleJobPrivate::start(worker);
}

void DirectCopyJob::slotCanResume(KIO::filesize_t offset)
{
    Q_EMIT canResume(this, offset);
}

//////////////////////////

SimpleJob *KIO::file_delete(const QUrl &src, JobFlags flags)
{
    KIO_ARGS << src << qint8(true); // isFile
    SimpleJob *job = SimpleJobPrivate::newJob(src, CMD_DEL, packedArgs, flags);
    if (job->uiDelegateExtension()) {
        job->uiDelegateExtension()->createClipboardUpdater(job, JobUiDelegateExtension::RemoveContent);
    }
    return job;
}

//////////
////

#include "moc_job_base.cpp"
#include "moc_job_p.cpp"
