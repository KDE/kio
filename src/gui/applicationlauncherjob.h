/*
    This file is part of the KDE libraries
    Copyright (c) 2020 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KIO_APPLICATIONLAUNCHERJOB_H
#define KIO_APPLICATIONLAUNCHERJOB_H

#include "kiogui_export.h"
#include <KJob>
#include <KService>
#include <QUrl>

class KRun; // KF6 REMOVE
class ApplicationLauncherJobTest; // KF6 REMOVE

namespace KIO {

class ApplicationLauncherJobPrivate;

/**
 * @class ApplicationLauncherJob applicationlauncherjob.h <KIO/ApplicationLauncherJob>
 *
 * @brief ApplicationLauncherJob runs an application and watches it while running.
 *
 * It creates a startup notification and finishes it on success or on error (for the taskbar).
 * It also emits an error message if necessary (e.g. "program not found").
 *
 * When passing multiple URLs to an application that doesn't support opening
 * multiple files, the application will be launched once for each URL.
 *
 * The job finishes when the application is successfully started. At that point you can
 * query the PID(s).
 *
 * For error handling, either connect to the result() signal, or for a simple messagebox on error,
 * you can do
 * @code
 *    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
 * @endcode
 * Using JobUiDelegate (which is widgets based) also enables the feature of asking the user
 * in case the executable or desktop file isn't marked as executable. Otherwise the job will
 * just refuse executing those files.
 *
 * @since 5.69
 */
class KIOGUI_EXPORT ApplicationLauncherJob : public KJob
{
public:
    /**
     * Creates an ApplicationLauncherJob.
     * @param service the service (application desktop file) to run
     * @param parent the parent QObject
     */
    explicit ApplicationLauncherJob(const KService::Ptr &service, QObject *parent = nullptr);

    /**
     * Creates an ApplicationLauncherJob.
     * @param serviceAction the service action to run
     * @param parent the parent QObject
     */
    explicit ApplicationLauncherJob(const KServiceAction &serviceAction, QObject *parent = nullptr);

    /**
     * Destructor
     *
     * Note that jobs auto-delete themselves after emitting result.
     * Deleting/killing the job will not stop the started application.
     */
    ~ApplicationLauncherJob() override;

    /**
     * Specifies the URLs to be passed to the application.
     * @param urls list of files (local or remote) to open
     *
     * Note that when passing multiple URLs to an application that doesn't support opening
     * multiple files, the application will be launched once for each URL.
     */
    void setUrls(const QList<QUrl> &urls);

    /**
     * @see RunFlag
     */
    enum RunFlag {
        DeleteTemporaryFiles = 0x1, ///< the URLs passed to the service will be deleted when it exits (if the URLs are local files)
    };
    /**
     * Stores a combination of #RunFlag values.
     */
    Q_DECLARE_FLAGS(RunFlags, RunFlag)

    /**
     * Specifies various flags.
     * @param runFlags the flags to be set. For instance, whether the URLs are temporary files that should be deleted after execution.
     */
    void setRunFlags(RunFlags runFlags);

    /**
     * Sets the file name to use in the case of downloading the file to a tempfile
     * in order to give to a non-URL-aware application.
     * Some apps rely on the extension to determine the mimetype of the file.
     * Usually the file name comes from the URL, but in the case of the
     * HTTP Content-Disposition header, we need to override the file name.
     * @param suggestedFileName the file name
     */
    void setSuggestedFileName(const QString &suggestedFileName);

    /**
     * Sets the startup notification id of the application launch.
     * @param startupId startup notification id, if any (otherwise "").
     */
    void setStartupId(const QByteArray &startupId);

    /**
     * Starts the job.
     * You must call this, after having done all the setters.
     */
    void start() override;

    /**
     * @return the PID of the application that was started
     *
     * Convenience method for pids().at(0). You should only use this when specifying zero or one URL,
     * or when you are sure that the application supports opening multiple files. Otherwise use pids().
     * Available after the job emits result().
     */
    qint64 pid() const;

    /**
     * @return the PIDs of the applications that were started
     *
     * Available after the job emits result().
     */
    QVector<qint64> pids() const;

private:
    friend class ::KRun; // KF6 REMOVE
    friend class ::ApplicationLauncherJobTest; // KF6 REMOVE
    /**
     * Blocks until the process has started. Only exists for KRun, will disappear in KF6.
     */
    bool waitForStarted();
    void emitUnauthorizedError();
    void proceedAfterSecurityChecks();

    friend class ApplicationLauncherJobPrivate;
    QScopedPointer<ApplicationLauncherJobPrivate> d;
};

} // namespace KIO

#endif
