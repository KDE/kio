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

class KRunPrivate; // KF6 REMOVE
class ApplicationLauncherJobTest; // KF6 REMOVE

namespace KIO {

class ApplicationLauncherJobPrivate;

/**
 * @brief ApplicationLauncherJob runs an application and watches it while running.
 *
 * It creates a startup notification and finishes it on success or on error (for the taskbar).
 * It also emits an error message if necessary (e.g. "program not found").
 *
 * Note that this class doesn't support warning the user if a desktop file or a binary
 * does not have the executable bit set and offering to make it so. Therefore file managers
 * should use KRun::runApplication rather than using ApplicationLauncherJob directly.
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
 *    auto *delegate = new KDialogJobUiDelegate;
 *    delegate->setAutoErrorHandlingEnabled(true);
 *    job->setUiDelegate(delegate);
 * @endcode
 *
 * @since 5.69
 */
class KIOGUI_EXPORT ApplicationLauncherJob : public KJob
{
public:
    /**
     * @brief Creates a ApplicationLauncherJob
     * @param service the service (application desktop file) to run
     * @param parent the parent QObject
     */
    explicit ApplicationLauncherJob(const KService::Ptr &service, QObject *parent = nullptr);

    /**
     * @brief Creates a ApplicationLauncherJob
     * @param serviceAction the service action to run
     * @param parent the parent QObject
     */
    explicit ApplicationLauncherJob(const KServiceAction &serviceAction, QObject *parent = nullptr);

    /**
     * Destructor.
     * Note that jobs auto-delete themselves after emitting result.
     * Deleting/killing the job will not stop the started application.
     */
    ~ApplicationLauncherJob() override;

    /**
     * @brief setUrls specifies the URLs to be passed to the application
     * @param urls list of files (local or remote) to open
     *
     * Note that when passing multiple URLs to an application that doesn't support opening
     * multiple files, the application will be launched once for each URL.
     */
    void setUrls(const QList<QUrl> &urls);

    enum RunFlag {
        DeleteTemporaryFiles = 0x1, ///< the URLs passed to the service will be deleted when it exits (if the URLs are local files)
    };
    Q_DECLARE_FLAGS(RunFlags, RunFlag)

    /**
     * @brief setRunFlags specifies various flags
     * @param runFlags the flags to be set. For instance, whether the URLs are temporary files that should be deleted after execution.
     */
    void setRunFlags(RunFlags runFlags);

    /**
     * Sets the file name to use in the case of downloading the file to a tempfile
     * in order to give to a non-url-aware application. Some apps rely on the extension
     * to determine the mimetype of the file. Usually the file name comes from the URL,
     * but in the case of the HTTP Content-Disposition header, we need to override the
     * file name.
     * @param suggestedFileName the file name
     */
    void setSuggestedFileName(const QString &suggestedFileName);

    /**
     * @brief setStartupId sets the startupId of the new application
     * @param startupId Application startup notification id, if any (otherwise "").
     */
    void setStartupId(const QByteArray &startupId);

    /**
     * @brief start starts the job. You must call this, after all the setters.
     */
    void start() override;

    /**
     * @return the PID of the application that was started.
     * Convenience method for pids().at(0). You should only use this when specifying zero or one URL,
     * or when you are sure that the application supports opening multiple files. Otherwise use pids().
     * Available after the job emits result().
     */
    qint64 pid() const;

    /**
     * @return the PIDs of the applications that were started.
     * Available after the job emits result().
     */
    QVector<qint64> pids() const;

private:
    friend class ::KRunPrivate; // KF6 REMOVE
    friend class ::ApplicationLauncherJobTest; // KF6 REMOVE
    /**
     * Blocks until the process has started. Only exists for KRun, will disappear in KF6.
     */
    bool waitForStarted();

    friend class ApplicationLauncherJobPrivate;
    QScopedPointer<ApplicationLauncherJobPrivate> d;
};

} // namespace KIO

#endif
