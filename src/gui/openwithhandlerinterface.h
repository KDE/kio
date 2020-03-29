/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef OPENWITHHANDLERINTERFACE_H
#define OPENWITHHANDLERINTERFACE_H

#include <QObject>
#include <kiogui_export.h>
#include <KService>
class QString;

class KJob;

namespace KIO {
class OpenWithHandlerInterfacePrivate;

/**
 * @class OpenWithHandlerInterface openwithhandlerinterface.h <KIO/OpenWithHandlerInterface>
 * @brief The OpenWithHandlerInterface class allows OpenUrlJob to
 * prompt the user about which application to use to open URLs that do not
 * have an associated application (via the "Open With" dialog).
 *
 * This extension mechanism for jobs is similar to KIO::JobUiDelegateExtension
 * and UntrustedProgramHandlerInterface.
 *
 * @since 5.71
 */
class KIOGUI_EXPORT OpenWithHandlerInterface : public QObject
{
    Q_OBJECT
protected:
    /**
     * Constructor
     */
    explicit OpenWithHandlerInterface(QObject *parent = nullptr);

    /**
     * Destructor
     */
    ~OpenWithHandlerInterface() override;

public:
    /**
     * Show the "Open With" dialog.
     * @param job the job calling this. Useful to get all its properties
     * @param urls the URLs to open
     * @param mimeType the MIME type of the URLs, if known. Can be empty otherwise.
     *
     * Implementations of this method must emit either serviceSelected or canceled.
     *
     * The default implementation in this base class simply emits canceled().
     * Any application using KIO::JobUiDelegate (from KIOWidgets) will benefit from an
     * automatically registered subclass which implements this method using KOpenWithDialog.
     */
    virtual void promptUserForApplication(KJob *job, const QList<QUrl> &urls, const QString &mimeType);

Q_SIGNALS:
    /**
     * Emitted by promptUserForApplication() once the user chooses an application.
     * @param service the application chosen by the user
     */
    void serviceSelected(const KService::Ptr &service);

    /**
     * Emitted by promptUserForApplication() if the user canceled the application selection dialog.
     */
    void canceled();

    /**
     * Emitted by promptUserForApplication() if it fully handled it including launching the app.
     * This is a special case for the native Windows open-with dialog.
     */
    void handled();

private:
    QScopedPointer<OpenWithHandlerInterfacePrivate> d;
};

}

#endif // OPENWITHHANDLERINTERFACE_H
