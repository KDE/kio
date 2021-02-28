/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KIO_MIMETYPEFINDERJOB_H
#define KIO_MIMETYPEFINDERJOB_H

#include "kiocore_export.h"
#include <KCompositeJob>
#include <memory>

class QUrl;

namespace KIO
{
class MimeTypeFinderJobPrivate;

/**
 * @class MimeTypeFinderJob MimeTypeFinderjob.h <KIO/MimeTypeFinderJob>
 *
 * @brief MimeTypeFinderJob finds out the MIME type of a URL.
 *
 * @since 5.80
 */
class KIOCORE_EXPORT MimeTypeFinderJob : public KCompositeJob
{
    Q_OBJECT
public:
    /**
     * @brief Creates an MimeTypeFinderJob for a URL.
     * @param url the URL of the file/directory to examine
     */
    explicit MimeTypeFinderJob(const QUrl &url, QObject *parent = nullptr);

    /**
     * Destructor
     *
     * Note that by default jobs auto-delete themselves after emitting result.
     */
    ~MimeTypeFinderJob() override;

    /**
     * Sets whether the job should follow URL redirections.
     * This is enabled by default.
     * @param b whether to follow redirections or not
     */
    void setFollowRedirections(bool b);

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
     * Returns the suggested filename, either set by setSuggestedFileName
     * or returned by the KIO::get job
     */
    QString suggestedFileName() const;

    /**
     * Enable/disable authentication prompt, if the URL requires one.
     * They are enabled by default.
     * This method allows to disable such prompts for jobs that should
     * fail rather than bother the user, if authentication is needed.
     * Example: for starting the associated program (i.e. when OpenUrlJob
     * uses MimeTypeFinderJob), we want auth prompts.
     * But for using a nice icon in a notification, we don't.
     */
    void setAuthenticationPromptEnabled(bool enable);

    /**
     * Returns where authentication prompts are enabled or disabled.
     * @see setAuthenticationPromptEnabled
     */
    bool isAuthenticationPromptEnabled() const;

    /**
     * Starts the job.
     * You must call this, after having called all the needed setters.
     */
    void start() override;

    /**
     * @return the MIME type. Only valid after the result() signal has been emitted.
     */
    QString mimeType() const;

protected:
    bool doKill() override;
    void slotResult(KJob *job) override;

private:
    friend class MimeTypeFinderJobPrivate;
    std::unique_ptr<MimeTypeFinderJobPrivate> d;
};

} // namespace KIO

#endif
