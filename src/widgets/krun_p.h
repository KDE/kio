/* This file is part of the KDE project
   Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
   Copyright (C) 2006 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KRUN_P_H
#define KRUN_P_H

#include <memory>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QEventLoopLocker>
#include <QProcess>
#include <KService>
class KProcess;

#include "executablefileopendialog_p.h"
#include "kstartupinfo.h"

/**
 * @internal
 * This class runs a KService or a shell command, using QProcess internally.
 * It creates a startup notification and finishes it on success or on error (for the taskbar)
 * It also shows an error message if necessary (e.g. "program not found").
 */
class KProcessRunner : public QObject
{
    Q_OBJECT

public:
    /**
     * Run a KService (application desktop file) to open @p urls.
     * @param service the service to run
     * @param urls the list of URLs, can be empty
     * @param windowId the identifier of window of the app that invoked this class.
     * @param flags various flags
     * @param suggestedFileName see KRun::setSuggestedFileName
     * @param asn Application startup notification id, if any (otherwise "").

     */
    KProcessRunner(const KService::Ptr &service, const QList<QUrl> &urls, WId windowId,
                   KRun::RunFlags flags = {}, const QString &suggestedFileName = {}, const QByteArray &asn = {});

    /**
     * Run a shell command
     * @param cmd must be a shell command. No need to append "&" to it.
     * @param execName the name of the executable, if known. This improves startup notification,
     * as well as honoring various flags coming from the desktop file for this executable, if there's one.
     * @param iconName icon for the startup notification
     * @param windowId the identifier of window of the app that invoked this class.
     * @param asn Application startup notification id, if any (otherwise "").
     * @param workingDirectory the working directory for the started process. The default
     *                         (if passing an empty string) is the user's document path.
     *                         This allows a command like "kwrite file.txt" to find file.txt from the right place.
     */
    KProcessRunner(const QString &cmd, const QString &execName, const QString &iconName,
                   WId windowId, const QByteArray &asn = {}, const QString &workingDirectory = {});

    virtual ~KProcessRunner();

    /**
     * @return the PID of the process that was started, on success
     */
    qint64 pid() const;

Q_SIGNALS:
    /**
     * @brief Emitted on error
     * @param errorString the error message
     */
    void error(const QString &errorString);

private Q_SLOTS:
    void slotProcessExited(int, QProcess::ExitStatus);

private:
    void init(const KService::Ptr &service, const QString &bin, const QString &userVisibleName,
              const QString &iconName, WId windowId, const QByteArray &asn);
    void startProcess();
    void terminateStartupNotification();
    void emitDelayedError(const QString &errorMsg);

    std::unique_ptr<KProcess> m_process;
    const QString m_executable; // can be a full path
    KStartupInfoId m_startupId;
    qint64 m_pid = 0;

    Q_DISABLE_COPY(KProcessRunner)
};

/**
 * @internal
 */
class Q_DECL_HIDDEN KRun::KRunPrivate
{
public:
    KRunPrivate(KRun *parent);

    void init(const QUrl &url, QWidget *window,
              bool showProgressInfo, const QByteArray &asn);

    // This helper method makes debugging easier: a single breakpoint for all
    // the code paths that start the timer - at least from KRun itself.
    void startTimer();

#ifdef Q_OS_WIN
    static bool displayNativeOpenWithDialog(const QList<QUrl> &lst, QWidget *window, bool tempFiles,
                                            const QString &suggestedFileName, const QByteArray &asn);
#endif
    bool runExecutable(const QString &_exec);

    void showPrompt();
    /**
     * Check whether we need to show a prompt(before executing a script or desktop file)
     */
    bool isPromptNeeded();
    ExecutableFileOpenDialog::Mode promptMode();
    void onDialogFinished(int result, bool isDontAskAgainSet);

    KRun * const q;
    bool m_showingDialog;
    bool m_runExecutables;
    bool m_followRedirections;

    // Don't exit the app while a KRun is running.
    QEventLoopLocker m_eventLoopLocker;

    QString m_preferredService;
    QString m_externalBrowser;
    QString m_localPath;
    QString m_suggestedFileName;
    QPointer <QWidget> m_window;
    QByteArray m_asn;
    QUrl m_strURL;
    bool m_bFault;
    bool m_bAutoDelete;
    bool m_bProgressInfo;
    bool m_bFinished;
    KIO::Job *m_job;
    QTimer *m_timer;

    /**
     * Used to indicate that the next action is to scan the file.
     * This action is invoked from slotTimeout.
     */
    bool m_bScanFile;
    bool m_bIsDirectory;

    /**
     * Used to indicate that the next action is to initialize.
     * This action is invoked from slotTimeout
     */
    bool m_bInit;

    /**
     * Used to indicate that the next action is to check whether we need to
     * show a prompt(before executing a script or desktop file).
     * This action is invoked from slotTimeout.
     */
    bool m_bCheckPrompt;

    bool m_externalBrowserEnabled;
};

#endif  // KRUN_P_H
