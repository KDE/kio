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

#include <QObject>
#include <memory>
#include <KStartupInfo>

namespace KIOGuiPrivate {
/**
 * @internal DO NOT USE
 */
bool KIOGUI_EXPORT checkStartupNotify(const KService *service, bool *silent_arg,
                                      QByteArray *wmclass_arg);
}

/**
 * @internal  (exported for KRun, currently)
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
     * @param asn Application startup notification id, if any (otherwise "").

     */
    KProcessRunner(const KService::Ptr &service, const QList<QUrl> &urls,
                   KIO::ApplicationLauncherJob::RunFlags flags = {}, const QString &suggestedFileName = {}, const QByteArray &asn = {});

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
    KProcessRunner(const QString &cmd, const QString &desktopName, const QString &execName, const QString &iconName,
                   const QByteArray &asn = {}, const QString &workingDirectory = {});

    /**
     * @return the PID of the process that was started, on success
     */
    qint64 pid() const;

    bool waitForStarted();

    virtual ~KProcessRunner();

    static int instanceCount(); // for the unittest

Q_SIGNALS:
    /**
     * @brief Emitted on error. In that case, finished() is not emitted.
     * @param errorString the error message
     */
    void error(const QString &errorString);

    /**
     * @brief emitted when the process was successfully started
     */
    void processStarted();

private Q_SLOTS:
    void slotProcessExited(int, QProcess::ExitStatus);
    void slotProcessError(QProcess::ProcessError error);
    void slotProcessStarted();

private:
    void init(const KService::Ptr &service, const QString &userVisibleName,
              const QString &iconName, const QByteArray &asn);
    void startProcess();
    void registerCGroup();
    void terminateStartupNotification();
    void emitDelayedError(const QString &errorMsg);

    std::unique_ptr<KProcess> m_process;
    QString m_executable; // can be a full path
    KStartupInfoId m_startupId;
    QString m_scopeId;
    qint64 m_pid = 0;

    Q_DISABLE_COPY(KProcessRunner)
};

#endif
