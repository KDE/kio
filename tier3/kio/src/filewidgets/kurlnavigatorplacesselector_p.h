/***************************************************************************
 *   Copyright (C) 2006 by Peter Penz (peter.penz@gmx.at)                  *
 *   Copyright (C) 2007 by Kevin Ottens (ervin@kde.org)                    *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Lesser General Public            *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#ifndef KURLNAVIGATORPLACESSELECTOR_P_H
#define KURLNAVIGATORPLACESSELECTOR_P_H

#include "kurlnavigatorbuttonbase_p.h"
#include <QUrl>

#include <QtCore/QPersistentModelIndex>

class KFilePlacesModel;
class QMenu;

namespace KDEPrivate
{

/**
 * @brief Allows to select a bookmark from a popup menu.
 *
 * The icon from the current selected bookmark is shown
 * inside the bookmark selector.
 *
 * @see KUrlNavigator
 * @internal
 */
class KUrlNavigatorPlacesSelector : public KUrlNavigatorButtonBase
{
    Q_OBJECT

public:
    /**
     * @param parent Parent widget where the bookmark selector
     *               is embedded into.
     */
    KUrlNavigatorPlacesSelector(QWidget* parent, KFilePlacesModel* placesModel);

    virtual ~KUrlNavigatorPlacesSelector();

    /**
     * Updates the selection dependent from the given URL \a url. The
     * URL must not match exactly to one of the available bookmarks:
     * The bookmark which is equal to the URL or at least is a parent URL
     * is selected. If there are more than one possible parent URL candidates,
     * the bookmark which covers the bigger range of the URL is selected.
     */
    void updateSelection(const QUrl& url);

    /** Returns the selected bookmark. */
    QUrl selectedPlaceUrl() const;
    /** Returns the selected bookmark. */
    QString selectedPlaceText() const;

    /** @see QWidget::sizeHint() */
    virtual QSize sizeHint() const;

Q_SIGNALS:
    /**
     * Is send when a bookmark has been activated by the user.
     * @param url URL of the selected place.
     */
    void placeActivated(const QUrl& url);

protected:
    /**
     * Draws the icon of the selected Url as content of the Url
     * selector.
     */
    virtual void paintEvent(QPaintEvent* event);

    virtual void dragEnterEvent(QDragEnterEvent* event);
    virtual void dragLeaveEvent(QDragLeaveEvent* event);
    virtual void dropEvent(QDropEvent* event);

private Q_SLOTS:
    /**
     * Updates the selected index and the icon to the bookmark
     * which is indicated by the triggered action \a action.
     */
    void activatePlace(QAction* action);

    void updateMenu();
    void updateTeardownAction();

    void onStorageSetupDone(const QModelIndex &index, bool success);

private:
    int m_selectedItem;
    QPersistentModelIndex m_lastClickedIndex;
    QMenu* m_placesMenu;
    KFilePlacesModel* m_placesModel;
    QUrl m_selectedUrl;
};

} // namespace KDEPrivate

#endif
