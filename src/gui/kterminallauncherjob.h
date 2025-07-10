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

/*!
 * \class KTerminalLauncherJob
 * \inmodule KIOGui
 *
 * \brief KTerminalLauncherJob starts a terminal application,
 * either for the user to use interactively, or to execute a command.
 *
 * It creates a startup notification and finishes it on success or on error (for the taskbar).
 * It also emits an error message if necessary (e.g. "program not found").
 *
 * The job finishes when the application is successfully started.
 * For error handling, either connect to the result() signal, or for a simple messagebox on error,
 * you can do
 * \code
 *    job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
 * \endcode
 *
 * \since 5.83
 */
class KIOGUI_EXPORT KTerminalLauncherJob : public KJob
{
    Q_OBJECT
public:
    /*!
     * Creates a KTerminalLauncherJob.
     *
     * \a command the command to execute in a terminal, can be empty.
     *
     * \a parent the parent QObject
     */
    explicit KTerminalLauncherJob(const QString &command, QObject *parent = nullptr);

    /*!
     * Destructor
     *
     * Note that jobs auto-delete themselves after emitting result
     */
    ~KTerminalLauncherJob() override;

    /*!
     * Sets the working directory from which to run the command.
     *
     * \a workingDirectory path of a local directory
     */
    void setWorkingDirectory(const QString &workingDirectory);

    /*!
     * Sets the platform-specific startup id of the command launch.
     *
     * \a startupId startup id, if any (otherwise "").
     *
     * For X11, this would be the id for the Startup Notification protocol.
     *
     * For Wayland, this would be the token for the XDG Activation protocol.
     */
    void setStartupId(const QByteArray &startupId);

    /*!
     * Can be used to pass environment variables to the child process.
     *
     * \a environment set of environment variables to pass to the child process
     *
     * \sa QProcessEnvironment
     */
    void setProcessEnvironment(const QProcessEnvironment &environment);

    /*!
     * Checks whether the command to launch a terminal can be constructed and sets it.
     *
     * The start() function calls this internally, so you only need to call prepare()
     * directly if you want to validate the command separately from actually starting
     * the terminal.
     *
     * This is useful if you want to avoid calling start() just to catch an error,
     * for example to conditionally show or hide a GUI element depending on whether
     * launching a terminal is likely to succeed.
     *
     * \return true if a launch command could be constructed, false otherwise.
     * \sa start()
     *
     * \since 6.17
     */
    bool prepare();

    /*!
     * Starts the job.
     * You must call this, after having called all the necessary setters.
     */
    void start() override;

private:
    friend class KTerminalLauncherJobTest;
    void determineFullCommand(bool fallbackToKonsoleService = true); // for the unittest
    QString fullCommand() const; // for the unittest

    KIOGUI_NO_EXPORT void emitDelayedResult();

    friend class KTerminalLauncherJobPrivate;
    std::unique_ptr<KTerminalLauncherJobPrivate> d;
};

#endif // KTERMINALLAUNCHERJOB_H
