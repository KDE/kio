/* This file is part of the KDE libraries
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

#ifndef OPENURLJOBHANDLERINTERFACE_H
#define OPENURLJOBHANDLERINTERFACE_H

#include <QObject>
#include <kiogui_export.h>
#include <KService>
class QString;

namespace KIO {
class OpenUrlJob;
class OpenUrlJobHandlerInterfacePrivate;

/**
 * @class OpenUrlJobHandlerInterface openurljobhandlerinterface.h <KIO/OpenUrlJobHandlerInterface>
 * @brief The OpenUrlJobHandlerInterface class allows OpenUrlJob to
 * prompt the user about which application to use to open URLs that do not
 * have an associated application (via the "Open With" dialog).
 *
 * This extension mechanism for jobs is similar to KIO::JobUiDelegateExtension
 * and UntrustedProgramHandlerInterface.
 *
 * @since 5.71
 */
class KIOGUI_EXPORT OpenUrlJobHandlerInterface : public QObject
{
    Q_OBJECT
protected:
    /**
     * Constructor
     */
    OpenUrlJobHandlerInterface();

    /**
     * Destructor
     */
    ~OpenUrlJobHandlerInterface() override;

public:
    /**
     * Show the "Open With" dialog.
     * @param job the job calling this. Useful to get all its properties
     * @param url the URL to open
     * @param mimeType the mimeType of the URL
     *
     * Implementations of this method must emit either serviceSelected or canceled.
     *
     * The default implementation in this base class simply emits canceled().
     * Any application using KIO::JobUiDelegate (from KIOWidgets) will benefit from an
     * automatically registered subclass which implements this method using KOpenWithDialog.
     */
    virtual void promptUserForApplication(KIO::OpenUrlJob *job, const QUrl &url, const QString &mimeType);

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

private:
    QScopedPointer<OpenUrlJobHandlerInterfacePrivate> d;
};

}

#endif // OPENURLJOBHANDLERINTERFACE_H
