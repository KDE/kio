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
