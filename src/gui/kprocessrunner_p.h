/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KPROCESSRUNNER_P_H
#define KPROCESSRUNNER_P_H

#include "applicationlauncherjob.h"
#include "kiogui_export.h"

#include <KProcess>

#include <KStartupInfo>
#include <QObject>
#include <memory>

namespace KIOGuiPrivate
{
/**
 * @internal DO NOT USE
 */
bool KIOGUI_EXPORT checkStartupNotify(const KService *service, bool *silent_arg, QByteArray *wmclass_arg);
}

/**
 * @internal  (exported for KIO GUI job unit tests)
 * This class runs a KService or a shell command, using QProcess internally.
 * It creates a startup notification and finishes it on success or on error (for the taskbar)
 * It also shows an error message if necessary (e.g. "program not found").
 */
class KIOGUI_EXPORT KProcessRunner : public QObject
{
    Q_OBJECT

public:
    /**
     * Run a KService (application desktop file) to open @p urls.
     * @param service the service to run
     * @param urls the list of URLs, can be empty
     * @param flags various flags
     * @param suggestedFileName see KRun::setSuggestedFileName
     * @param asn Application startup notification id, if any (otherwise "")
     * @param serviceEntryPath the KService entryPath(), passed as an argument
     * because in some cases it could become an empty string, e.g. if an
     * ApplicationLauncherJob is created from a @c KServiceAction, the
     * ApplicationLauncherJob will call KService::setExec() which clears the
     * entryPath() of the KService
     */
    static KProcessRunner *fromApplication(const KService::Ptr &service,
                                           const QString &serviceEntryPath,
                                           const QList<QUrl> &urls,
                                           KIO::ApplicationLauncherJob::RunFlags flags = {},
                                           const QString &suggestedFileName = {},
                                           const QByteArray &asn = {});

    /**
     * Run a shell command
     * @param cmd must be a shell command. No need to append "&" to it.
     * @param desktopName name of the desktop file, if known.
     * @param execName the name of the executable, if known.
     * @param iconName icon for the startup notification
     * @param asn Application startup notification id, if any (otherwise "").
     * @param workingDirectory the working directory for the started process. The default
     *                         (if passing an empty string) is the user's document path.
     *                         This allows a command like "kwrite file.txt" to find file.txt from the right place.
     */
    static KProcessRunner *fromCommand(const QString &cmd,
                                       const QString &desktopName,
                                       const QString &execName,
                                       const QString &iconName,
                                       const QByteArray &asn,
                                       const QString &workingDirectory,
                                       const QProcessEnvironment &environment);

    /**
     * Run an executable with arguments (without invoking a shell, by starting a new process).
     *
     * @note: Starting from 5.92, if an actual executable named @p executable cannot be found
     * in PATH, this will return a nullptr.
     *
     * @param executable the name of (or full path to) the executable, mandatory
     * @param args the arguments to pass to the executable
     * @param desktopName name of the desktop file, if known.
     * @param iconName icon for the startup notification
     * @param asn Application startup notification id, if any (otherwise "").
     * @param workingDirectory the working directory for the started process. The default
     *                         (if passing an empty string) is the user's document path.
     *                         This allows a command like "kwrite file.txt" to find file.txt from the right place.
     */
    static KProcessRunner *fromExecutable(const QString &executable,
                                          const QStringList &args,
                                          const QString &desktopName,
                                          const QString &iconName,
                                          const QByteArray &asn,
                                          const QString &workingDirectory,
                                          const QProcessEnvironment &environment);

    /**
     * Blocks until the process has started. Only exists for KRun via Command/ApplicationLauncherJob, will disappear in KF6.
     */
    virtual bool waitForStarted(int timeout = 30000) = 0;

    ~KProcessRunner() override;

    static int instanceCount(); // for the unittest

Q_SIGNALS:
    /**
     * @brief Emitted on error. In that case, finished() is not emitted.
     * @param errorString the error message
     */
    void error(const QString &errorString);

    /**
     * @brief emitted when the process was successfully started
     * @param pid PID of the process that was started
     */
    void processStarted(qint64 pid);

    /**
     * Notifies about having received the token were waiting for.
     *
     * It only gets emitted when on Wayland.
     *
     * @see m_waitingForXdgToken
     */
    void xdgActivationTokenArrived();

protected:
    KProcessRunner();
    virtual void startProcess() = 0;
    void setPid(qint64 pid);
    void terminateStartupNotification();
    QString name() const;

    std::unique_ptr<KProcess> m_process;
    QString m_executable; // can be a full path
    QString m_desktopName;
    QString m_desktopFilePath;
    QString m_description;
    qint64 m_pid = 0;
    KService::Ptr m_service;
    QString m_serviceEntryPath;
    bool m_waitingForXdgToken = false;
    QList<QUrl> m_urls;
    KStartupInfoId m_startupId;

private:
    void emitDelayedError(const QString &errorMsg);
    void initFromDesktopName(const QString &desktopName,
                             const QString &execName,
                             const QString &iconName,
                             const QByteArray &asn,
                             const QString &workingDirectory,
                             const QProcessEnvironment &environment);
    void init(const KService::Ptr &service, const QString &serviceEntryPath, const QString &userVisibleName, const QString &iconName, const QByteArray &asn);

    Q_DISABLE_COPY(KProcessRunner)
};

class ForkingProcessRunner : public KProcessRunner
{
    Q_OBJECT

public:
    explicit ForkingProcessRunner();

    void startProcess() override;
    bool waitForStarted(int timeout) override;

protected Q_SLOTS:
    void slotProcessExited(int, QProcess::ExitStatus);
    void slotProcessError(QProcess::ProcessError error);
    virtual void slotProcessStarted();
};

#endif
