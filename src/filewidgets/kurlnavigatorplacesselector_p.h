/*
    SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KURLNAVIGATORPLACESSELECTOR_P_H
#define KURLNAVIGATORPLACESSELECTOR_P_H

#include "kurlnavigatorbuttonbase_p.h"
#include <QUrl>

#include <QPersistentModelIndex>

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
    KUrlNavigatorPlacesSelector(KUrlNavigator *parent, KFilePlacesModel *placesModel);

    ~KUrlNavigatorPlacesSelector() override;

    /**
     * Updates the selection dependent from the given URL \a url. The
     * URL must not match exactly to one of the available bookmarks:
     * The bookmark which is equal to the URL or at least is a parent URL
     * is selected. If there are more than one possible parent URL candidates,
     * the bookmark which covers the bigger range of the URL is selected.
     */
    void updateSelection(const QUrl &url);

    /** Returns the selected bookmark. */
    QUrl selectedPlaceUrl() const;
    /** Returns the selected bookmark. */
    QString selectedPlaceText() const;

    /** @see QWidget::sizeHint() */
    QSize sizeHint() const override;

Q_SIGNALS:
    /**
     * Is send when a bookmark has been activated by the user.
     * @param url URL of the selected place.
     */
    void placeActivated(const QUrl &url);

    /**
     * Is sent when a bookmark was middle clicked by the user
     * and thus should be opened in a new tab.
     */
    void tabRequested(const QUrl &url);

protected:
    /**
     * Draws the icon of the selected Url as content of the Url
     * selector.
     */
    void paintEvent(QPaintEvent *event) override;

    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    /**
     * Updates the selected index and the icon to the bookmark
     * which is indicated by the triggered action \a action.
     */
    void activatePlace(QAction *action);

    void updateMenu();
    void updateTeardownAction();

    void onStorageSetupDone(const QModelIndex &index, bool success);

private:
    int m_selectedItem;
    QPersistentModelIndex m_lastClickedIndex;
    QMenu *m_placesMenu;
    KFilePlacesModel *m_placesModel;
    QUrl m_selectedUrl;
};

} // namespace KDEPrivate

#endif
