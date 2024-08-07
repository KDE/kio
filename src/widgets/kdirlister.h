/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1999 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001, 2002, 2004-2006 Michael Brade <brade@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef kdirlister_h
#define kdirlister_h

#include "kiowidgets_export.h"
#include <kcoredirlister.h>

class QWidget;
class KDirListerPrivate;

/*!
 * \class KDirLister
 * \inmodule KIOWidgets
 *
 * \brief Subclass of KCoreDirLister which uses QWidgets to show error messages
 * and to associate jobs with windows.
 */
class KIOWIDGETS_EXPORT KDirLister : public KCoreDirLister
{
    Q_OBJECT

public:
    /*!
     * Create a directory lister.
     */
    explicit KDirLister(QObject *parent = nullptr);

    /*!
     * Destroy the directory lister.
     */
    ~KDirLister() override;

    /*!
     * Check whether auto error handling is enabled.
     * If enabled, it will show an error dialog to the user when an
     * error occurs. It is turned on by default.
     *
     * Returns \c true if auto error handling is enabled, \c false otherwise
     * \sa setAutoErrorHandlingEnabled()
     */
    bool autoErrorHandlingEnabled() const; // KF6 remove, already provided by KCoreDirLister

    /*!
     * Pass the main window this object is associated with
     * this is used for caching authentication data
     *
     * \a window the window to associate with, \c nullptr to disassociate
     */
    void setMainWindow(QWidget *window);

    /*!
     * Returns the main window associated with this object, or \c nullptr if there is none
     */
    QWidget *mainWindow();

protected:
    void jobStarted(KIO::ListJob *) override;

private:
    friend class KDirListerPrivate;
    std::unique_ptr<KDirListerPrivate> d;
};

#endif
