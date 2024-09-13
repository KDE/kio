/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KIO_COMMANDLAUNCHERJOB_H
#define KIO_COMMANDLAUNCHERJOB_H

#include "kiogui_export.h"
#include <KJob>

class KRunPrivate; // KF6 REMOVE
class CommandLauncherJobTest; // KF6 REMOVE

class QProcessEnvironment;
namespace KIO
{
class CommandLauncherJobPrivate;

/*!
 * \class KIO::CommandLauncherJob
 * \inmodule KIOGui
 * \inheaderfile KIO/CommandLauncherJob
 *
 * \brief CommandLauncherJob runs a command and watches it while running.
 *
 * It creates a startup notification and finishes it on success or on error (for the taskbar).
 * It also emits a "program not found" error message if the requested command did not exist.
 *
 * The job finishes when the command is successfully started; at that point you can
 * query the PID with pid(). Note that no other errors are handled automatically after the
 * command starts running. As far as CommandLauncherJob is concerned, if the command was
 * launched, the result is a success. If you need to query the command for its exit status
 * or error text later, it is recommended to use QProcess instead.
 *
 * For error handling, either connect to the result() signal, or for a simple messagebox on error,
 * you can do
 * \code
 *    job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
 * \endcode
 *
 * \since KIO 5.69
 */
class KIOGUI_EXPORT CommandLauncherJob : public KJob
{
public:
    /*!
     * Creates a CommandLauncherJob.
     *
     * \a command the shell command to run
     *
     * The command is given "as is" to the shell, it must already be quoted if necessary.
     * If \a command is instead a filename, consider using the other constructor, even if no args are present.
     *
     * \a parent the parent QObject
     *
     * Please consider also calling setDesktopName() for better startup notification.
     */
    explicit CommandLauncherJob(const QString &command, QObject *parent = nullptr);

    /*!
     * Creates a CommandLauncherJob.
     *
     * \a executable the name of the executable
     *
     * \a args the commandline arguments to pass to the executable
     *
     * \a parent the parent QObject
     *
     * Please consider also calling setDesktopName() for better startup notification.
     */
    explicit CommandLauncherJob(const QString &executable, const QStringList &args, QObject *parent = nullptr);

    /*!
     * Destructor
     *
     * Note that jobs auto-delete themselves after emitting result
     */
    ~CommandLauncherJob() override;

    /*!
     * Sets the command to execute, this will change the command that was set by any of the constructors.
     * \since KIO 5.83
     */
    void setCommand(const QString &command);

    /*!
     * Returns the command executed by this job.
     * \since KIO 5.83
     */
    QString command() const;

    /*!
     * Sets the name of the executable, used in the startup notification
     * (see KStartupInfoData::setBin()).
     *
     * \a executable executable name, with or without a path
     *
     * Alternatively, use setDesktopName().
     */
    void setExecutable(const QString &executable);

    /*!
     * Set the name of the desktop file (e.g.\ "org.kde.dolphin", without the ".desktop" filename extension).
     *
     * This is necessary for startup notification to work.
     */
    void setDesktopName(const QString &desktopName);

    /*!
     * Sets the platform-specific startup id of the command launch.
     *
     * \a startupId startup id, if any (otherwise "").
     *
     * For X11, this would be the id for the Startup Notification protocol.
     * For Wayland, this would be the token for the XDG Activation protocol.
     */
    void setStartupId(const QByteArray &startupId);

    /*!
     * Sets the working directory from which to run the command.
     *
     * \a workingDirectory path of a local directory
     */
    void setWorkingDirectory(const QString &workingDirectory);

    /*!
     * Returns the working directory, which was previously set with @c setWorkingDirectory().
     * \since KIO 5.83
     */
    QString workingDirectory() const;

    /*!
     * Can be used to pass environment variables to the child process.
     *
     * \a environment set of environment variables to pass to the child process
     *
     * \sa QProcessEnvironment
     * \since KIO 5.82
     */
    void setProcessEnvironment(const QProcessEnvironment &environment);

    /*!
     * Starts the job.
     * You must call this, after having called all the necessary setters.
     */
    void start() override;

    /*!
     * Returns the PID of the command that was started
     *
     * Available after the job emits result().
     */
    qint64 pid() const;

private:
    friend class ::KRunPrivate; // KF6 REMOVE
    friend class ::CommandLauncherJobTest; // KF6 REMOVE
    /*!
     * Blocks until the process has started. Only exists for KRun, will disappear in KF6.
     */
    bool waitForStarted();

    friend class CommandLauncherJobPrivate;
    QScopedPointer<CommandLauncherJobPrivate> d;
};

} // namespace KIO

#endif
