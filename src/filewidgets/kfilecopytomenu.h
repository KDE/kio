/*
    SPDX-FileCopyrightText: 2008, 2015 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KFILECOPYTOMENU_H
#define KFILECOPYTOMENU_H

#include <QObject>
#include <QUrl>

#include <kiofilewidgets_export.h>

#include <memory>

class QMenu;
class KFileCopyToMenuPrivate;

/*!
 * \class KFileCopyToMenu
 * \inmodule KIOFileWidgets
 *
 * \brief This class adds "Copy To" and "Move To" submenus to a popupmenu.
 */
class KIOFILEWIDGETS_EXPORT KFileCopyToMenu : public QObject
{
    Q_OBJECT
public:
    /*!
     * Creates a KFileCopyToMenu instance
     * Note that this instance (and the widget) must stay alive for at least as
     * long as the popupmenu; it has the slots for the actions created by addActionsTo.
     *
     * \a parentWidget parent widget for the file dialog and message boxes.
     * The parentWidget also serves as a parent for this object.
     */
    explicit KFileCopyToMenu(QWidget *parentWidget);

    ~KFileCopyToMenu() override;

    /*!
     * Sets the URLs which the actions apply to.
     */
    void setUrls(const QList<QUrl> &urls);

    /*!
     * If setReadOnly(true) is called, the "Move To" submenu will not appear.
     */
    void setReadOnly(bool ro);

    /*!
     * Generate the actions and submenus, and adds them to the \a menu.
     * All actions are created as children of the menu.
     */
    void addActionsTo(QMenu *menu) const;

    /*!
     * Enables or disables automatic error handling with message boxes.
     * When called with true, a messagebox is shown in case of an error during a copy or move.
     * When called with false, the application should connect to the error signal instead.
     * Auto error handling is disabled by default.
     */
    void setAutoErrorHandlingEnabled(bool b);

Q_SIGNALS:
    /*!
     * Emitted when the copy or move job fails.
     *
     * \a errorCode the KIO job error code
     *
     * \a message the error message to show the user
     */
    void error(int errorCode, const QString &message);

private:
    std::unique_ptr<KFileCopyToMenuPrivate> const d;
};

#endif
