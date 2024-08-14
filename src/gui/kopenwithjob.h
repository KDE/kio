#pragma once

#include <KCompositeJob>

#include <QUrl>

#include <QDBusUnixFileDescriptor>
#include <QWindow>

#include <KService>

#include "kiogui_export.h"

class KIOGUI_EXPORT KOpenWithJob : public KCompositeJob
{
    Q_OBJECT

public:
    KOpenWithJob();
    ~KOpenWithJob();

    void start() override;

    /**
     * Specifies the URLs to be passed to the application.
     * @param urls list of files (local or remote) to open
     *
     * Note that when passing multiple URLs to an application that doesn't support opening
     * multiple files, the application will be launched once for each URL.
     */
    void setUrls(const QList<QUrl> &urls);

    void setMimeType(const QString &mimeType);

    /**
     * Sets the platform-specific startup id of the application launch.
     * @param startupId startup id, if any (otherwise "").
     * For X11, this would be the id for the Startup Notification protocol.
     * For Wayland, this would be the token for the XDG Activation protocol.
     */
    void setStartupId(const QByteArray &startupId);

protected:
    void slotResult(KJob *job) override;

private:
    void slotGotWindow();
    void slotGotActivationToken();

    void showOpenWithDialog();
    void useXdgPortal();

    QList<QUrl> m_urls;

    QWindow *m_window = nullptr;
    QString m_portalWindowHandle;
    QString m_activationToken;
    QString m_mimeTypeName;
};
