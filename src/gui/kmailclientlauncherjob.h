/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KMAILCLIENTLAUNCHERJOB_H
#define KMAILCLIENTLAUNCHERJOB_H

#include <KIO/CommandLauncherJob>
#include <memory>

class KMailClientLauncherJobPrivate;

/**
 * @class KMailClientLauncherJob kmailclientlauncherjob.h <KMailClientLauncherJob>
 *
 * @brief KMailClientLauncherJob starts a terminal application,
 * either for the user to use interactively, or to execute a command.
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
 * @since 5.85
 */
class KIOGUI_EXPORT KMailClientLauncherJob : public KJob
{
    Q_OBJECT
public:
    /**
     * Creates a KMailClientLauncherJob.
     * @param parent the parent QObject
     */
    explicit KMailClientLauncherJob(QObject *parent = nullptr);

    /**
     * Destructor
     *
     * Note that jobs auto-delete themselves after emitting result
     */
    ~KMailClientLauncherJob() override;

    /**
     * Sets the address of the To recipient(s) for the email
     * @param to Each entry can use the format "someone@example.com" or "John Doe <someone@example.com>"
     */
    void setTo(const QStringList &to);
    /**
     * Sets the address of the CC recipient(s) for the email
     * @param cc Each entry can use the format "someone@example.com" or "John Doe <someone@example.com>"
     */
    void setCc(const QStringList &cc);
    /**
     * Sets the subject for the email
     * @param subject Email subject
     */
    void setSubject(const QString &subject);
    /**
     * Sets the body for the email
     * @param body Email body
     */
    void setBody(const QString &body);
    /**
     * Sets attachments for the email
     * @param urls URLs of the attachments for the email.
     * Remember to use QUrl::fromLocalFile() to construct those URLs from local file paths.
     */
    void setAttachments(const QList<QUrl> &urls);

    /**
     * Sets the startup notification id of the command launch.
     * @param startupId startup notification id, if any (otherwise "").
     */
    void setStartupId(const QByteArray &startupId);

    /**
     * Starts the job.
     * You must call this, after having called all the necessary setters.
     */
    void start() override;

private:
    friend class KMailClientLauncherJobTest;
    QUrl mailToUrl() const; // for the unittest
    QString thunderbirdCommandLine(const QString &exec) const; // for the unittest

    void emitDelayedResult();

    friend class KMailClientLauncherJobPrivate;
    std::unique_ptr<KMailClientLauncherJobPrivate> d;
};

#endif // KMAILCLIENTLAUNCHERJOB_H
