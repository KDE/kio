/* This file is part of the KDE libraries
    Copyright (C) 2001,2002,2003 Carsten Pfeiffer <pfeiffer@kde.org>
    Copyright (C) 2007 Kevin Ottens <ervin@kde.org>

    library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation, version 2.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KFILEPLACEEDITDIALOG_H
#define KFILEPLACEEDITDIALOG_H

// Not exported anymore, only used internally.
//#include <kio/kiofilewidgets_export.h>

#include <QDialog>
#include <QUrl>
#include <kiconloader.h>

class QCheckBox;
class QDialogButtonBox;
class QLineEdit;
class KIconButton;
class KUrlRequester;

/**
 * A dialog that allows editing entries of a KUrlBar ( KUrlBarItem).
 * The dialog offers to configure a given url, label and icon.
 * See the class-method getInformation() for easy usage.
 *
 * @author Carsten Pfeiffer <pfeiffer@kde.org>
 */
class KFilePlaceEditDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * A convenience method to show the dialog and retrieve all the
     * properties via the given parameters. The parameters are used to
     * initialize the dialog and then return the user-configured values.
     *
     * @p allowGlobal if you set this to true, the dialog will have a checkbox
     *                for the user to decide if he wants the entry to be
     *                available globally or just for the current application.
     * @p url the url of the item
     * @p label a short, translated description of the item
     * @p icon an icon for the item
     * @p appLocal tells whether the item should be local for this application
     *             or be available globally
     * @p iconSize determines the size of the icon that is shown/selectable
     * @p parent the parent-widget for the dialog
     *
     * If you leave the icon empty, the default icon for the given url will be
     * used (KMimeType::pixmapForUrl()).
     */
    static bool getInformation( bool allowGlobal, QUrl& url,
                                QString& label, QString& icon,
                                bool isAddingNewPlace,
                                bool& appLocal, int iconSize,
                                QWidget *parent = 0 );

    /**
     * Constructs a KFilePlaceEditDialog.
     *
     * @p allowGlobal if you set this to true, the dialog will have a checkbox
     *                for the user to decide if he wants the entry to be
     *                available globally or just for the current application.
     * @p url the url of the item
     * @p label a short, translated description of the item
     * @p icon an icon for the item
     * @p appLocal tells whether the item should be local for this application
     *             or be available globally
     * @p iconSize determines the size of the icon that is shown/selectable
     * @p parent the parent-widget for the dialog
     *
     * If you leave the icon empty, the default icon for the given url will be
     * used (KMimeType::pixmapForUrl()).
     */
    KFilePlaceEditDialog(bool allowGlobal, const QUrl& url,
                         const QString& label, const QString &icon,
                         bool isAddingNewPlace,
                         bool appLocal = true,
                         int iconSize = KIconLoader::SizeMedium,
                         QWidget *parent = 0);
    /**
     * Destroys the dialog.
     */
    ~KFilePlaceEditDialog();

    /**
     * @returns the configured url
     */
    QUrl url() const;

    /**
     * @returns the configured label
     */
    QString label() const;

    /**
     * @returns the configured icon
     */
    const QString &icon() const;

    /**
     * @returns whether the item should be local to the application or global.
     * If allowGlobal was set to false in the constructor, this will always
     * return true.
     */
    bool applicationLocal() const;

public Q_SLOTS:
    void urlChanged(const QString & );

private:
    /**
     * The KUrlRequester used for editing the url
     */
    KUrlRequester * m_urlEdit;
    /**
     * The QLineEdit used for editing the label
     */
    QLineEdit     * m_labelEdit;
    /**
     * The KIconButton to configure the icon
     */
    KIconButton   * m_iconButton;
    /**
     * The QCheckBox to modify the local/global setting
     */
    QCheckBox     * m_appLocal;

    QDialogButtonBox *m_buttonBox;
};


#endif // KURLBAR_H
