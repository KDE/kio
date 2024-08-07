/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2001, 2002, 2003 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KFILEPLACEEDITDIALOG_H
#define KFILEPLACEEDITDIALOG_H

#include "kiofilewidgets_export.h"

#include <KIconLoader>
#include <QDialog>
#include <QUrl>

class QCheckBox;
class QDialogButtonBox;
class QLineEdit;
class KIconButton;
class KUrlRequester;

/*!
 * \class KFilePlaceEditDialog
 * \inmodule KIOFileWidgets
 *
 * \brief A dialog that allows editing entries of a KFilePlacesModel.
 *
 * The dialog offers to configure a given url, label and icon.
 * See the class-method getInformation() for easy usage.
 *
 * \since 5.53
 */
class KIOFILEWIDGETS_EXPORT KFilePlaceEditDialog : public QDialog
{
    Q_OBJECT

public:
    /*!
     * A convenience method to show the dialog and retrieve all the
     * properties via the given parameters. The parameters are used to
     * initialize the dialog and then return the user-configured values.
     *
     * \a allowGlobal if you set this to true, the dialog will have a checkbox
     *                for the user to decide if he wants the entry to be
     *                available globally or just for the current application.
     *
     * \a url the url of the item
     *
     * \a label a short, translated description of the item
     *
     * \a icon an icon for the item
     *
     * \a appLocal tells whether the item should be local for this application
     *             or be available globally
     *
     * \a iconSize determines the size of the icon that is shown/selectable
     *
     * \a parent the parent-widget for the dialog
     *
     * If you leave the icon empty, the default icon for the given url will be
     * used (KMimeType::pixmapForUrl()).
     */
    static bool
    getInformation(bool allowGlobal, QUrl &url, QString &label, QString &icon, bool isAddingNewPlace, bool &appLocal, int iconSize, QWidget *parent = nullptr);

    /*!
     * Constructs a KFilePlaceEditDialog.
     *
     * \a allowGlobal if you set this to true, the dialog will have a checkbox
     *                for the user to decide if he wants the entry to be
     *                available globally or just for the current application.
     *
     * \a url the url of the item
     *
     * \a label a short, translated description of the item
     *
     * \a icon an icon for the item
     *
     * \a appLocal tells whether the item should be local for this application
     *             or be available globally
     *
     * \a iconSize determines the size of the icon that is shown/selectable
     *
     * \a parent the parent-widget for the dialog
     *
     * If you leave the icon empty, the default icon for the given url will be
     * used (KMimeType::pixmapForUrl()).
     */
    KFilePlaceEditDialog(bool allowGlobal,
                         const QUrl &url,
                         const QString &label,
                         const QString &icon,
                         bool isAddingNewPlace,
                         bool appLocal = true,
                         int iconSize = KIconLoader::SizeMedium,
                         QWidget *parent = nullptr);

    ~KFilePlaceEditDialog() override;

    /*!
     * Returns the configured url
     */
    QUrl url() const;

    /*!
     * Returns the configured label
     */
    QString label() const;

    /*!
     * Returns the configured icon
     */
    QString icon() const;

    /*!
     * Returns whether the item should be local to the application or global.
     * If allowGlobal was set to false in the constructor, this will always
     * return true.
     */
    bool applicationLocal() const;

public Q_SLOTS:
    /*!
     *
     */
    void urlChanged(const QString &);

private:
    /*
     * The KUrlRequester used for editing the url
     */
    KUrlRequester *m_urlEdit;
    /*
     * The QLineEdit used for editing the label
     */
    QLineEdit *m_labelEdit;
    /*
     * The KIconButton to configure the icon
     */
    KIconButton *m_iconButton;
    /*
     * The QCheckBox to modify the local/global setting
     */
    QCheckBox *m_appLocal;

    QDialogButtonBox *m_buttonBox;
};

#endif // KFILEPLACEEDITDIALOG_H
