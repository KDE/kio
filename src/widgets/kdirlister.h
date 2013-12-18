/* This file is part of the KDE project
   Copyright (C) 1999 David Faure <faure@kde.org>
                 2001, 2002, 2004-2006 Michael Brade <brade@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef kdirlister_h
#define kdirlister_h

#include <kcoredirlister.h>
#include <kio/kiowidgets_export.h>
class QWidget;

/**
 * Subclass of KCoreDirLister which uses QWidgets to show error messages
 * and to associate jobs with windows.
 */
class KIOWIDGETS_EXPORT KDirLister : public KCoreDirLister
{
    Q_OBJECT
    Q_PROPERTY(bool autoErrorHandlingEnabled READ autoErrorHandlingEnabled)

public:
    /**
     * Create a directory lister.
     */
    KDirLister(QObject* parent = 0);

    /**
     * Destroy the directory lister.
     */
    virtual ~KDirLister();

    /**
     * Check whether auto error handling is enabled.
     * If enabled, it will show an error dialog to the user when an
     * error occurs. It is turned on by default.
     * @return true if auto error handling is enabled, false otherwise
     * @see setAutoErrorHandlingEnabled()
     */
    bool autoErrorHandlingEnabled() const;

    /**
     * Enable or disable auto error handling is enabled.
     * If enabled, it will show an error dialog to the user when an
     * error occurs. It is turned on by default.
     * @param enable true to enable auto error handling, false to disable
     * @param parent the parent widget for the error dialogs, can be 0 for
     *               top-level
     * @see autoErrorHandlingEnabled()
     */
    void setAutoErrorHandlingEnabled(bool enable, QWidget *parent);

    /**
     * Pass the main window this object is associated with
     * this is used for caching authentication data
     * @param window the window to associate with, 0 to disassociate
     */
    void setMainWindow(QWidget *window);

    /**
     * Returns the main window associated with this object.
     * @return the associated main window, or 0 if there is none
     */
    QWidget *mainWindow();

protected:
    /**
     * Reimplemented to customize error handling
     * @reimp
     */
    virtual void handleError(KIO::Job *);
    /**
     * Reimplemented to customize error handling
     * @reimp
     */
    virtual void handleErrorMessage(const QString &message);

    /**
     * Reimplemented to associate a window with new jobs
     * @reimp
     */
    virtual void jobStarted(KIO::ListJob *);

private:
    class Private;
    Private* const d;
    friend class Private;
};

#endif

