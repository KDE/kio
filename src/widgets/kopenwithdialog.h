/* This file is part of the KDE libraries
    Copyright (C) 2000 David Faure <faure@kde.org>

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

#ifndef OPENWITHDIALOG_H
#define OPENWITHDIALOG_H

#include "kiowidgets_export.h"

#include <QDialog>
#include <QUrl>
#include <kservice.h>

class KOpenWithDialogPrivate;

/**
 * @class KOpenWithDialog kopenwithdialog.h <KOpenWithDialog>
 *
 * "Open With" dialog box.
 *
 * @note To let the user choose an application and run it immediately,
 *       use simpler KRun::displayOpenWithDialog().
 *
 * If the Kiosk "shell_access" action is not authorized (see
 * KAuthorized::authorize()), arbitrary commands are not allowed; instead, the
 * user must browse to and choose an executable.
 *
 * @author David Faure <faure@kde.org>
 */
class KIOWIDGETS_EXPORT KOpenWithDialog : public QDialog
{
    Q_OBJECT
public:

    /**
     * Create a dialog that asks for a application to open a given
     * URL(s) with.
     *
     * @param urls   the URLs that should be opened. The list can be empty,
     * if the dialog is used to choose an application but not for some particular URLs.
     * @param parent parent widget
     */
    explicit KOpenWithDialog(const QList<QUrl> &urls, QWidget *parent = nullptr);

    /**
     * Create a dialog that asks for a application to open a given
     * URL(s) with.
     *
     * @param urls   is the URL that should be opened
     * @param text   appears as a label on top of the entry box. Leave empty for default text (since 5.20).
     * @param value  is the initial value of the line
     * @param parent parent widget
     */
    KOpenWithDialog(const QList<QUrl> &urls, const QString &text, const QString &value,
                    QWidget *parent = nullptr);

    /**
     * Create a dialog to select a service for a given mimetype.
     * Note that this dialog doesn't apply to URLs.
     *
     * @param mimeType the mime type we want to choose an application for.
     * @param value  is the initial value of the line
     * @param parent parent widget
     */
    KOpenWithDialog(const QString &mimeType, const QString &value,
                    QWidget *parent = nullptr);

    /**
     * Create a dialog to select an application
     * Note that this dialog doesn't apply to URLs.
     *
     * @param parent parent widget
     */
    KOpenWithDialog(QWidget *parent = nullptr);

    /**
     * Destructor
     */
    ~KOpenWithDialog();

    /**
     * @return the text the user entered
     */
    QString text() const;
    /**
     * Hide the "Do not &close when command exits" Checkbox
     */
    void hideNoCloseOnExit();
    /**
     * Hide the "Run in &terminal" Checkbox
     */
    void hideRunInTerminal();
    /**
     * @return the chosen service in the application tree
     * Can be null, if the user typed some text and didn't select a service.
     */
    KService::Ptr service() const;
    /**
     * Set whether a new .desktop file should be created if the user selects an
     * application for which no corresponding .desktop file can be found.
     *
     * Regardless of this setting a new .desktop file may still be created if
     * the user has chosen to remember the file association.
     *
     * The default is false: no .desktop files are created.
     */
    void setSaveNewApplications(bool b);

public Q_SLOTS: // TODO KDE5: move all those slots to the private class!
    void slotSelected(const QString &_name, const QString &_exec);
    void slotHighlighted(const QString &_name, const QString &_exec);
    void slotTextChanged();
    void slotTerminalToggled(bool);

protected Q_SLOTS:
    /**
     * Reimplemented from QDialog::accept()
     */
    void accept() override;

private:
    bool eventFilter(QObject *object, QEvent *event) override;

    friend class KOpenWithDialogPrivate;
    KOpenWithDialogPrivate *const d;

    Q_DISABLE_COPY(KOpenWithDialog)

    Q_PRIVATE_SLOT(d, void _k_slotDbClick())
    Q_PRIVATE_SLOT(d, void _k_slotFileSelected())
};

#endif
