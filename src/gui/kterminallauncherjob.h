/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KTERMINALLAUNCHERJOB_H
#define KTERMINALLAUNCHERJOB_H

#include <KIO/CommandLauncherJob>
#include <memory>

class KTerminalLauncherJobPrivate;

/**
 * @class KTerminalLauncherJob kterminallauncherjob.h <KTerminalLauncherJob>
 *
 * @brief KTerminalLauncherJob starts a terminal application,
 * either for the user to use interactively, or to execute a command.
 *
 * It creates a startup notification and finishes it on success or on error (for the taskbar).
 * It also emits an error message if necessary (e.g. "program not found").
 *
 * The job finishes when the application is successfully started.
 * For error handling, either connect to the result() signal, or for a simple messagebox on error,
 * you can do
 * @code
 *    job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
 * @endcode
 *
 * @since 5.83
 */
class KIOGUI_EXPORT KTerminalLauncherJob : public KJob
{
    Q_OBJECT
public:
    /**
     * Creates a KTerminalLauncherJob.
     * @param command the command to execute in a terminal, can be empty.
     * @param parent the parent QObject
     */
    explicit KTerminalLauncherJob(const QString &command, QObject *parent = nullptr);

    /**
     * Destructor
     *
     * Note that jobs auto-delete themselves after emitting result
     */
    ~KTerminalLauncherJob() override;

    /**
     * Sets the working directory from which to run the command.
     * @param workingDirectory path of a local directory
     */
    void setWorkingDirectory(const QString &workingDirectory);

    /**
     * Sets the platform-specific startup id of the command launch.
     * @param startupId startup id, if any (otherwise "").
     * For X11, this would be the id for the Startup Notification protocol.
     * For Wayland, this would be the token for the XDG Activation protocol.
     */
    void setStartupId(const QByteArray &startupId);

    /**
     * Can be used to pass environment variables to the child process.
     * @param environment set of environment variables to pass to the child process
     * @see QProcessEnvironment
     */
    void setProcessEnvironment(const QProcessEnvironment &environment);

    /**
     * Starts the job.
     * You must call this, after having called all the necessary setters.
     */
    void start() override;

private:
    friend class KTerminalLauncherJobTest;
    void determineFullCommand(bool fallbackToKonsoleService = true); // for the unittest
    QString fullCommand() const; // for the unittest

    void emitDelayedResult();

    friend class KTerminalLauncherJobPrivate;
    std::unique_ptr<KTerminalLauncherJobPrivate> d;
};

#endif // KTERMINALLAUNCHERJOB_H
