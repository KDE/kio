/*
 * SPDX-FileCopyrightText: 2023 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */

#ifndef KIO_MIMETYPEEDITORLAUNCHERJOB_H
#define KIO_MIMETYPEEDITORLAUNCHERJOB_H

#include "kiogui_export.h"
#include <KJob>
#include <memory>

class QWindow;

namespace KIO
{
class MimeTypeEditorLauncherJobPrivate;

/**
 * @class MimeTypeEditorLauncherJob mimetypeeditorlauncherjob.h <KIO/MimeTypeEditorLauncherJob>
 *
 * @brief MimeTypeEditorLauncherJob starts the editor for a given mime type.
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
 * @since 6.0
 */
class KIOGUI_EXPORT MimeTypeEditorLauncherJob : public KJob
{
    Q_OBJECT
public:
    /**
     * Creates a MimeTypeEditorLauncherJob.
     * @param mimeType the MIME type to edit, e.g. "text/plain"
     * @param parent the parent QObject
     */
    explicit MimeTypeEditorLauncherJob(const QString &mimeType, QObject *parent = nullptr);

    /**
     * Destructor
     *
     * Note that jobs auto-delete themselves after emitting result
     */
    ~MimeTypeEditorLauncherJob() override;

    /**
     * TODO
     */
    void setParentWindow(QWindow *parentWindow);

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

    static bool isSupported();

private:
    friend class MimeTypeEditorLauncherJobPrivate;
    std::unique_ptr<MimeTypeEditorLauncherJobPrivate> d;
};

} // namespace KIO

#endif // KIO_MIMETYPEEDITORLAUNCHERJOB_H
