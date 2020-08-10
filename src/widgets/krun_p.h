/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998, 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
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
#include <KStartupInfo>

namespace KIO {
    class ApplicationLauncherJob;
    class CommandLauncherJob;
}

/**
 * @internal
 */
class Q_DECL_HIDDEN KRunPrivate
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
    bool runExternalBrowser(const QString &_exec);
    static qint64 runCommandLauncherJob(KIO::CommandLauncherJob *job, QWidget *widget);

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
