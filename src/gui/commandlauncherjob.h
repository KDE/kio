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

/**
 * @class CommandLauncherJob commandlauncherjob.h <KIO/CommandLauncherJob>
 *
 * @brief CommandLauncherJob runs a command and watches it while running.
 *
 * It creates a startup notification and finishes it on success or on error (for the taskbar).
 * It also emits an error message if necessary (e.g. "program not found").
 *
 * The job finishes when the application is successfully started. At that point you can
 * query the PID.
 *
 * For error handling, either connect to the result() signal, or for a simple messagebox on error,
 * you can do
 * @code
 *    job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
 * @endcode
 *
 * @since 5.69
 */
class KIOGUI_EXPORT CommandLauncherJob : public KJob
{
public:
    /**
     * Creates a CommandLauncherJob.
     * @param command the shell command to run
     * The command is given "as is" to the shell, it must already be quoted if necessary.
     * If @p command is instead a filename, consider using the other constructor, even if no args are present.
     * @param parent the parent QObject
     *
     * Please consider also calling setDesktopName() for better startup notification.
     */
    explicit CommandLauncherJob(const QString &command, QObject *parent = nullptr);

    /**
     * Creates a CommandLauncherJob.
     * @param executable the name of the executable
     * @param args the commandline arguments to pass to the executable
     * @param parent the parent QObject
     *
     * Please consider also calling setDesktopName() for better startup notification.
     */
    explicit CommandLauncherJob(const QString &executable, const QStringList &args, QObject *parent = nullptr);

    /**
     * Destructor
     *
     * Note that jobs auto-delete themselves after emitting result
     */
    ~CommandLauncherJob() override;

    /**
     * Sets the command to execute, this will change the command that was set by any of the constructors.
     * @since 5.83
     */
    void setCommand(const QString &command);

    /**
     * Returns the command executed by this job.
     * @since 5.83
     */
    QString command() const;

    /**
     * Sets the name of the executable, used in the startup notification
     * (see KStartupInfoData::setBin()).
     * @param executable executable name, with or without a path
     *
     * Alternatively, use setDesktopName().
     */
    void setExecutable(const QString &executable);

#if KIOGUI_ENABLE_DEPRECATED_SINCE(5, 103)
    /**
     * Sets the icon for the startup notification.
     * @param iconName name of the icon, to be loaded from the current icon theme
     *
     * Alternatively, use setDesktopName().
     *
     * @deprecated since 5.103, use setDesktopName() instead.
     */
    KIOGUI_DEPRECATED_VERSION(5, 103, "Use setDesktopName() instead.")
    void setIcon(const QString &iconName);
#endif

    /**
     * Set the name of the desktop file (e.g.\ "org.kde.dolphin", without the ".desktop" filename extension).
     *
     * This is necessary for startup notification to work.
     */
    void setDesktopName(const QString &desktopName);

    /**
     * Sets the platform-specific startup id of the command launch.
     * @param startupId startup id, if any (otherwise "").
     * For X11, this would be the id for the Startup Notification protocol.
     * For Wayland, this would be the token for the XDG Activation protocol.
     */
    void setStartupId(const QByteArray &startupId);

    /**
     * Sets the working directory from which to run the command.
     * @param workingDirectory path of a local directory
     */
    void setWorkingDirectory(const QString &workingDirectory);

    /**
     * Returns the working directory, which was previously set with @c setWorkingDirectory().
     * @since 5.83
     */
    QString workingDirectory() const;

    /**
     * Can be used to pass environment variables to the child process.
     * @param environment set of environment variables to pass to the child process
     * @see QProcessEnvironment
     * @since 5.82
     */
    void setProcessEnvironment(const QProcessEnvironment &environment);

    /**
     * Starts the job.
     * You must call this, after having called all the necessary setters.
     */
    void start() override;

    /**
     * @return the PID of the application that was started
     *
     * Available after the job emits result().
     */
    qint64 pid() const;

private:
    friend class ::KRunPrivate; // KF6 REMOVE
    friend class ::CommandLauncherJobTest; // KF6 REMOVE
    /**
     * Blocks until the process has started. Only exists for KRun, will disappear in KF6.
     */
    bool waitForStarted();

    friend class CommandLauncherJobPrivate;
    QScopedPointer<CommandLauncherJobPrivate> d;
};

} // namespace KIO

#endif
