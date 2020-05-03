/*
    This file is part of the KDE libraries
    Copyright (c) 2020 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
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
 * This includes finding out its mimetype, and then the associated application,
 * or running desktop files, executables, etc.
 * It also honours the "use this webbrowser for all http(s) URLs" setting.
 * @since 5.71
 */
class KIOGUI_EXPORT OpenUrlJob : public KCompositeJob
{
    Q_OBJECT
public:
    /**
     * @brief Creates a OpenUrlJob in order to open a URL.
     * @param url the URL of the file/directory to open
     */
    explicit OpenUrlJob(const QUrl &url, QObject *parent = nullptr);

    /**
     * @brief Creates a OpenUrlJob for the case where the mimeType is already known
     * @param url the URL of the file/directory to open
     * @param mimeType the type of file/directory. See QMimeType.
     */
    explicit OpenUrlJob(const QUrl &url, const QString &mimeType, QObject *parent = nullptr);

    /**
     * Destructor
     *
     * Note that jobs auto-delete themselves after emitting result.
     */
    ~OpenUrlJob() override;

    /**
     * Specifies that the URL passed to the application will be deleted when it exits (if the URL is a local file)
     */
    void setDeleteTemporaryFile(bool b);

    /**
     * Sets the file name to use in the case of downloading the file to a tempfile
     * in order to give to a non-URL-aware application.
     * Some apps rely on the extension to determine the mimetype of the file.
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
     * Sets whether the external webbrowser setting should be honoured.
     * This is enabled by default.
     * This should only be disabled in webbrowser applications.
     * @param b whether to enable the external browser or not.
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
     * You must call this, after having done all the setters.
     * This is a GUI job, never use exec(), it would block user interaction.
     */
    void start() override;

Q_SIGNALS:
    /**
     * Emitted when the mimeType was determined.
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
