/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KEMAILCLIENTLAUNCHERJOB_H
#define KEMAILCLIENTLAUNCHERJOB_H

#include "kiogui_export.h"
#include <KJob>
#include <memory>

class KEMailClientLauncherJobPrivate;

/**
 * @class KEMailClientLauncherJob kemailclientlauncherjob.h <KEMailClientLauncherJob>
 *
 * @brief KEMailClientLauncherJob starts a mail client in order to compose a new mail.
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
 * @since 5.87
 */
class KIOGUI_EXPORT KEMailClientLauncherJob : public KJob
{
    Q_OBJECT
public:
    /**
     * Creates a KEMailClientLauncherJob.
     * @param parent the parent QObject
     */
    explicit KEMailClientLauncherJob(QObject *parent = nullptr);

    /**
     * Destructor
     *
     * Note that jobs auto-delete themselves after emitting result
     */
    ~KEMailClientLauncherJob() override;

    /**
     * Sets the email address(es) that will be used in the To field for the email
     * @param to recipients; each entry can use the format "someone@example.com" or "John Doe <someone@example.com>"
     */
    void setTo(const QStringList &to);
    /**
     * Sets the email address(es) that will be used in the CC field for the email
     * @param cc recipients; each entry can use the format "someone@example.com" or "John Doe <someone@example.com>"
     */
    void setCc(const QStringList &cc);
    /**
     * Sets the subject for the email
     * @param subject the email subject
     */
    void setSubject(const QString &subject);
    /**
     * Sets the body for the email
     * @param body the email body
     */
    void setBody(const QString &body);
    /**
     * Sets attachments for the email
     * @param urls URLs of the attachments for the email
     * Remember to use QUrl::fromLocalFile() to construct those URLs from local file paths.
     */
    void setAttachments(const QList<QUrl> &urls);

    /**
     * Sets the platform-specific startup id of the mail client launch.
     * @param startupId startup id, if any (otherwise "").
     * For X11, this would be the id for the Startup Notification protocol.
     * For Wayland, this would be the token for the XDG Activation protocol.
     */
    void setStartupId(const QByteArray &startupId);

    /**
     * Starts the job.
     * You must call this, after having called all the necessary setters.
     */
    void start() override;

private:
    friend class KEMailClientLauncherJobTest;
    QUrl mailToUrl() const; // for the unittest
    QStringList thunderbirdArguments() const; // for the unittest

    void emitDelayedResult();

    friend class KEMailClientLauncherJobPrivate;
    std::unique_ptr<KEMailClientLauncherJobPrivate> d;
};

#endif // KEMAILCLIENTLAUNCHERJOB_H
