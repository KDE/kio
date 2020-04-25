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

#ifndef UNTRUSTEDPROGRAMHANDLERINTERFACE_H
#define UNTRUSTEDPROGRAMHANDLERINTERFACE_H

#include <QObject>
#include <kiocore_export.h>
class KJob;
class QString;

namespace KIO {

/**
 * @brief The UntrustedProgramHandlerInterface class allows ApplicationLauncherJob to
 * prompt the user about an untrusted executable or desktop file.
 * This extension mechanism for jobs is similar to KIO::JobUiDelegateExtension.
 *
 * The class also provides helper methods to set the execute bit so that the program
 * can be started.
 * @since 5.70
 */
class KIOCORE_EXPORT UntrustedProgramHandlerInterface : public QObject
{
    Q_OBJECT
protected:
    /**
     * Constructor
     */
    UntrustedProgramHandlerInterface();

    /**
     * Destructor
     */
    virtual ~UntrustedProgramHandlerInterface();

public:
    /**
     * Show a warning to the user about the program not being trusted for execution.
     * This could be an executable which is not a script and without the execute bit.
     * Or it could be a desktop file outside the standard locations, without the execute bit.
     * @param job the job calling this. Useful to get the associated window.
     * @param programName the full path to the executable or desktop file
     *
     * If this function emits result(true), the caller should then call
     * either setExecuteBit or makeServiceFileExecutable; those helper methods
     * are provided by this class.
     *
     * The default implementation in this base class simply emits result(false).
     * Any application linking to KIOWidgets will benefit from an automatically registered
     * subclass which implements this method using QtWidgets.
     */
    virtual void showUntrustedProgramWarning(KJob *job, const QString &programName);

    /**
     * Helper function that attempts to make a desktop file executable.
     * In addition to the execute bit, this includes fixing its first line to ensure that
     * it says #!/usr/bin/env xdg-open.
     * @param fileName the full path to the file
     * @param errorString output parameter so the method can return an error message
     * @return true on success, false on error
     */
    bool makeServiceFileExecutable(const QString &fileName, QString &errorString);

    /**
     * Helper function that attempts to set execute bit for given file.
     * @param fileName the full path to the file
     * @param errorString output parameter so the method can return an error message
    * @return true on success, false on error
     */
    bool setExecuteBit(const QString &fileName, QString &errorString);

Q_SIGNALS:
    /**
     * Implementations of this interface must emit result in showUntrustedProgramWarning.
     * @param confirmed true if the user confirms running this program, false on cancel
     */
    void result(bool confirmed);

private:
    class Private;
    Private *const d;
};

}

#endif // UNTRUSTEDPROGRAMHANDLERINTERFACE_H
