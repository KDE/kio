/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KIO_OPENURLJOB_H
#define KIO_OPENURLJOB_H

#include "kiogui_export.h"
#include "applicationlauncherjob.h"
#include <KCompositeJob>
#include <QScopedPointer>

class QUrl;

namespace KIO {

class OpenUrlJobPrivate;

/**
 * @class OpenUrlJob openurljob.h <KIO/OpenUrlJob>
 *
 * @brief OpenUrlJob finds out the right way to "open" a URL.
 * This includes finding out its MIME type, and then the associated application,
 * or running desktop files, executables, etc.
 * It also honours the "use this webbrowser for all http(s) URLs" setting.
 *
 * For the "Open With" dialog functionality to work, make sure to set
 * KIO::JobUiDelegate as the delegate for this job (in widgets applications).
 * @code
 *    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
 * @endcode
 *
 * @since 5.71
 */
class KIOGUI_EXPORT OpenUrlJob : public KCompositeJob
{
    Q_OBJECT
public:
    /**
     * @brief Creates an OpenUrlJob in order to open a URL.
     * @param url the URL of the file/directory to open
     */
    explicit OpenUrlJob(const QUrl &url, QObject *parent = nullptr);

    /**
     * @brief Creates an OpenUrlJob for the case where the MIME type is already known.
     * @param url the URL of the file/directory to open
     * @param mimeType the type of file/directory. See QMimeType.
     */
    explicit OpenUrlJob(const QUrl &url, const QString &mimeType, QObject *parent = nullptr);

    /**
     * Destructor
     *
     * Note that by default jobs auto-delete themselves after emitting result.
     */
    ~OpenUrlJob() override;

    /**
     * Specifies that the URL passed to the application will be deleted when it exits (if the URL is a local file)
     */
    void setDeleteTemporaryFile(bool b);

    /**
     * Sets the file name to use in the case of downloading the file to a tempfile,
     * in order to give it to a non-URL-aware application.
     * Some apps rely on the extension to determine the MIME type of the file.
     * Usually the file name comes from the URL, but in the case of the
     * HTTP Content-Disposition header, we need to override the file name.
     * @param suggestedFileName the file name
     */
    void setSuggestedFileName(const QString &suggestedFileName);

    /**
     * Sets the startup notification id of the application launch.
     * @param startupId startup notification id, if any (otherwise "").
     */
    void setStartupId(const QByteArray &startupId);

    /**
     * Set this to true if this class should allow the user to run executables.
     * Unlike KF5's KRun, this setting is OFF by default here for security reasons.
     * File managers can enable this, but e.g. web browsers, mail clients etc. shouldn't.
     */
    void setRunExecutables(bool allow);

    /**
     * Set this to @c true if this class should show a dialog to ask the user about how
     * to handle various types of executable files; note that executing/running remote
     * files is disallowed as that is not secure (in the case of remote shell scripts
     * and .desktop files, they are always opened as text in the default application):
     *  - For native binaries: whether to execute or cancel
     *  - For .exe files: whether to execute or cancel, ("execute" on Linux in this
     *    context means running the file with the default application (e.g. WINE))
     *  - For executable shell scripts: whether to execute the file or open it as
     *    text in the default application; note that if the file doesn't have the
     *    execute bit, it'll always be opened as text
     *  - For .desktop files: whether to run the file or open it as text in the default
     *    application; note that if the .desktop file is located in a non-standard
     *    location (on Linux standard locations are /usr/share/applications or
     *    ~/.local/share/applications) and does not have the execute bit, another dialog
     *    (see UntrustedProgramHandlerInterface) will be launched to ask the user whether
     *    they trust running the application (the one the .desktop file launches based on
     *    the Exec line)
     *
     * Note that the dialog, ExecutableFileOpenDialog (from KIOWidgets), provides an option
     * to remember the last value used and not ask again, if that is set, then the dialog will
     * not be shown.
     *
     * When set to @c true this will take precedence over setRunExecutables (the latter can be
     * used to allow running executables without first asking the user for confirmation).
     *
     * @since 5.73
     */
    void setShowOpenOrExecuteDialog(bool b);

    /**
     * Sets whether the external webbrowser setting should be honoured.
     * This is enabled by default.
     * This should only be disabled in webbrowser applications.
     * @param b whether to let the external browser handle the URL or not
     */
    void setEnableExternalBrowser(bool b);

    /**
     * Sets whether the job should follow URL redirections.
     * This is enabled by default.
     * @param b whether to follow redirections or not.
     */
    void setFollowRedirections(bool b);

    /**
     * Starts the job.
     * You must call this, after having called all the needed setters.
     * This is a GUI job, never use exec(), it would block user interaction.
     */
    void start() override;

Q_SIGNALS:
    /**
     * Emitted when the MIME type is determined.
     * This can be used for special cases like webbrowsers
     * who want to embed the URL in some cases, rather than starting a different
     * application. In that case they can kill the job.
     */
    void mimeTypeFound(const QString &mimeType);

protected:
    bool doKill() override;

private:
    void slotResult(KJob *job) override;

    friend class OpenUrlJobPrivate;
    QScopedPointer<OpenUrlJobPrivate> d;
};

} // namespace KIO

#endif // OPENURLJOB_H
