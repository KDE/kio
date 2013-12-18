/*****************************************************************************
 * Copyright (C) 2006-2010 by Peter Penz <peter.penz@gmx.at>                 *
 * Copyright (C) 2006 by Aaron J. Seigo <aseigo@kde.org>                     *
 * Copyright (C) 2007 by Kevin Ottens <ervin@kde.org>                        *
 * Copyright (C) 2007 by Urs Wolfer <uwolfer @ kde.org>                      *
 *                                                                           *
 * This library is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU Library General Public               *
 * License as published by the Free Software Foundation; either              *
 * version 2 of the License, or (at your option) any later version.          *
 *                                                                           *
 * This library is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 * Library General Public License for more details.                          *
 *                                                                           *
 * You should have received a copy of the GNU Library General Public License *
 * along with this library; see the file COPYING.LIB.  If not, write to      *
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,      *
 * Boston, MA 02110-1301, USA.                                               *
 *****************************************************************************/

#ifndef KURLNAVIGATOR_H
#define KURLNAVIGATOR_H

#include <kio/kiofilewidgets_export.h>

#include <QUrl>
#include <QWidget>
#include <QtCore/QByteArray>

class KFilePlacesModel;
class KUrlComboBox;
class QMouseEvent;

/**
 * @brief Widget that allows to navigate through the paths of an URL.
 *
 * The URL navigator offers two modes:
 * - Editable:     The URL of the location is editable inside an editor.
 *                 By pressing RETURN the URL will get activated.
 * - Non editable ("breadcrumb view"): The URL of the location is represented by
 *                 a number of buttons, where each button represents a path
 *                 of the URL. By clicking on a button the path will get
 *                 activated. This mode also supports drag and drop of items.
 *
 * The mode can be changed by clicking on the empty area of the URL navigator.
 * It is recommended that the application remembers the setting
 * or allows to configure the default mode (see KUrlNavigator::setUrlEditable()).
 *
 * The URL navigator remembers the URL history during navigation and allows to go
 * back and forward within this history.
 *
 * In the non editable mode ("breadcrumb view") it can be configured whether
 * the full path should be shown. It is recommended that the application
 * remembers the setting or allows to configure the default mode (see
 * KUrlNavigator::setShowFullPath()).
 *
 * The typical usage of the KUrlNavigator is:
 * - Create an instance providing a places model and an URL.
 * - Create an instance of QAbstractItemView which shows the content of the URL
 *   given by the URL navigator.
 * - Connect to the signal KUrlNavigator::urlChanged() and synchronize the content of
 *   QAbstractItemView with the URL given by the URL navigator.
 *
 * It is recommended, that the application remembers the state of the QAbstractItemView
 * when the URL has been changed. This allows to restore the view state when going back in history.
 * KUrlNavigator offers support for remembering the view state:
 * - The signal urlAboutToBeChanged() will be emitted before the URL change takes places.
 *   This allows the application to store the view state by KUrlNavigator::saveLocationState().
 * - The signal urlChanged() will be emitted after the URL change took place. This allows
 *   the application to restore the view state by getting the values from
 *   KUrlNavigator::locationState().
 */
class KIOFILEWIDGETS_EXPORT KUrlNavigator : public QWidget
{
    Q_OBJECT

public:
    /** @since 4.5 */
    KUrlNavigator(QWidget* parent = 0);

    /**
     * @param placesModel    Model for the places which are selectable inside a
     *                       menu. A place can be a bookmark or a device. If it is 0,
     *                       no places selector is displayed.
     * @param url            URL which is used for the navigation or editing.
     * @param parent         Parent widget.
     */
    KUrlNavigator(KFilePlacesModel* placesModel, const QUrl& url, QWidget* parent);
    virtual ~KUrlNavigator();

    /**
     * @return URL of the location given by the \a historyIndex. If \a historyIndex
     *         is smaller than 0, the URL of the current location is returned.
     * @since  4.5
     */
    QUrl locationUrl(int historyIndex = -1) const;

    /**
     * Saves the location state described by \a state for the current location. It is recommended
     * that at least the scroll position of a view is remembered and restored when traversing
     * through the history. Saving the location state should be done when the signal
     * KUrlNavigator::urlAboutToBeChanged() has been emitted. Restoring the location state (see
     * KUrlNavigator::locationState()) should be done when the signal KUrlNavigator::urlChanged()
     * has been emitted.
     *
     * Example:
     * \code
     * QByteArray state;
     * QDataStream data(&state, QIODevice::WriteOnly);
     * data << QPoint(x, y);
     * data << ...;
     * ...
     * urlNavigator->saveLocationState(state);
     * \endcode
     *
     * @since 4.5
     */
    void saveLocationState(const QByteArray& state);

    /**
     * @return Location state given by \a historyIndex. If \a historyIndex
     *         is smaller than 0, the state of the current location is returned.
     * @see    KUrlNavigator::saveLocationState()
     * @since  4.5
     */
    QByteArray locationState(int historyIndex = -1) const;

    /**
     * Goes back one step in the URL history. The signals
     * KUrlNavigator::urlAboutToBeChanged(), KUrlNavigator::urlChanged() and
     * KUrlNavigator::historyChanged() are emitted if true is returned. False is returned
     * if the beginning of the history has already been reached and hence going back was
     * not possible. The history index (see KUrlNavigator::historyIndex()) is
     * increased by one if the operation was successful.
     */
    bool goBack();

    /**
     * Goes forward one step in the URL history. The signals
     * KUrlNavigator::urlAboutToBeChanged(), KUrlNavigator::urlChanged() and
     * KUrlNavigator::historyChanged() are emitted if true is returned. False is returned
     * if the end of the history has already been reached and hence going forward
     * was not possible. The history index (see KUrlNavigator::historyIndex()) is
     * decreased by one if the operation was successful.
     */
    bool goForward();

    /**
     * Goes up one step of the URL path and remembers the old path
     * in the history. The signals KUrlNavigator::urlAboutToBeChanged(),
     * KUrlNavigator::urlChanged() and KUrlNavigator::historyChanged() are
     * emitted if true is returned. False is returned if going up was not
     * possible as the root has been reached.
     */
    bool goUp();

    /**
     * Goes to the home URL and remembers the old URL in the history.
     * The signals KUrlNavigator::urlAboutToBeChanged(), KUrlNavigator::urlChanged()
     * and KUrlNavigator::historyChanged() are emitted.
     *
     * @see KUrlNavigator::setHomeUrl()
     */
    // KDE5: Remove the home-property. It is sufficient to invoke
    // KUrlNavigator::setLocationUrl(homeUrl) on application-side.
    void goHome();

    /**
     * Sets the home URL used by KUrlNavigator::goHome(). If no
     * home URL is set, the default home path of the user is used.
     * @since 4.5
     */
    // KDE5: Remove the home-property. It is sufficient to invoke
    // KUrlNavigator::setLocationUrl(homeUrl) on application-side.
    void setHomeUrl(const QUrl& url);

    QUrl homeUrl() const;

    /**
     * Allows to edit the URL of the navigation bar if \a editable
     * is true, and sets the focus accordingly.
     * If \a editable is false, each part of
     * the URL is presented by a button for a fast navigation ("breadcrumb view").
     */
    void setUrlEditable(bool editable);

    /**
     * @return True, if the URL is editable within a line editor.
     *         If false is returned, each part of the URL is presented by a button
     *         for fast navigation ("breadcrumb view").
     */
    bool isUrlEditable() const;

    /**
     * Shows the full path of the URL even if a place represents a part of the URL.
     * Assuming that a place called "Pictures" uses the URL /home/user/Pictures.
     * An URL like /home/user/Pictures/2008 is shown as [Pictures] > [2008]
     * in the breadcrumb view, if showing the full path is turned off. If
     * showing the full path is turned on, the URL is shown
     * as [/] > [home] > [Pictures] > [2008].
     * @since 4.2
     */
    void setShowFullPath(bool show);

    /**
     * @return True, if the full path of the URL should be shown in the breadcrumb view.
     * @since  4.2
     */
    bool showFullPath() const;

    /**
     * Set the URL navigator to the active mode, if \a active
     * is true. The active mode is default. The inactive mode only differs
     * visually from the active mode, no change of the behavior is given.
     *
     * Using the URL navigator in the inactive mode is useful when having split views,
     * where the inactive view is indicated by an inactive URL
     * navigator visually.
     */
    void setActive(bool active);

    /**
     * @return True, if the URL navigator is in the active mode.
     * @see    KUrlNavigator::setActive()
     */
    bool isActive() const;

    /**
     * Sets the places selector visible, if \a visible is true.
     * The places selector allows to select the places provided
     * by the places model passed in the constructor. Per default
     * the places selector is visible.
     */
    void setPlacesSelectorVisible(bool visible);

    /** @return True, if the places selector is visible. */
    bool isPlacesSelectorVisible() const;

    /**
     * @return The currently entered, but not accepted URL.
     *         It is possible that the returned URL is not valid.
     */
    QUrl uncommittedUrl() const;

    /**
     * @return The amount of locations in the history. The data for each
     *         location can be retrieved by KUrlNavigator::locationUrl() and
     *         KUrlNavigator::locationState().
     */
    int historySize() const;

    /**
     * @return  The history index of the current location, where
     *          0 <= history index < KUrlNavigator::historySize(). 0 is the most
     *          recent history entry.
     */
    int historyIndex() const;

    /**
     * @return The used editor when the navigator is in the edit mode
     * @see    KUrlNavigator::setUrlEditable()
     */
    KUrlComboBox* editor() const;

    /**
     * If an application supports only some special protocols, they can be set
     * with \a protocols .
     */
    // KDE5: Think about removing the custom-protocols-property. It had been used
    // only by one application currently which uses a different approach now.
    void setCustomProtocols(const QStringList& protocols);

    /**
     * @return The custom protocols if they are set, QStringList() otherwise.
     */
    QStringList customProtocols() const;

#if !defined(KDE_NO_DEPRECATED) && !defined(DOXYGEN_SHOULD_SKIP_THIS)
    /**
     * @return     The current URL of the location.
     * @deprecated Use KUrlNavigator::locationUrl() instead.
     */
    KIOFILEWIDGETS_DEPRECATED const QUrl& url() const;

    /**
     * @return The portion of the current URL up to the path part given
     * by \a index. Assuming that the current URL is /home/peter/Documents/Music,
     * then the following URLs are returned for an index:
     * - index <= 0: /home
     * - index is 1: /home/peter
     * - index is 2: /home/peter/Documents
     * - index >= 3: /home/peter/Documents/Music
     * @deprecated It should not be necessary for a client of KUrlNavigator to query this information.
     */
    KIOFILEWIDGETS_DEPRECATED QUrl url(int index) const;

    /**
     * @return URL for the history element with the index \a historyIndex.
     *         The history index 0 represents the most recent URL.
     * @since 4.3
     * @deprecated Use KUrlNavigator::locationUrl(historyIndex) instead.
     */
    KIOFILEWIDGETS_DEPRECATED QUrl historyUrl(int historyIndex) const;

    /**
     * @return The saved root URL for the current URL (see KUrlNavigator::saveRootUrl()).
     * @deprecated Use KUrlNavigator::locationState() instead.
     */
    KIOFILEWIDGETS_DEPRECATED const QUrl& savedRootUrl() const;

    /**
     * @return The saved contents position of the upper left corner
     *         for the current URL.
     * @deprecated Use KUrlNavigator::locationState() instead.
     */
    KIOFILEWIDGETS_DEPRECATED QPoint savedPosition() const;

    /** @deprecated Use setHomeUrl(const QUrl& url) instead. */
    KIOFILEWIDGETS_DEPRECATED void setHomeUrl(const QString& homeUrl);
#endif

public Q_SLOTS:
    /**
     * Sets the location to \a url. The old URL is added to the history.
     * The signals KUrlNavigator::urlAboutToBeChanged(), KUrlNavigator::urlChanged()
     * and KUrlNavigator::historyChanged() are emitted. Use
     * KUrlNavigator::locationUrl() to read the location.
     * @since 4.5
     */
    void setLocationUrl(const QUrl& url);

    /**
     * Activates the URL navigator (KUrlNavigator::isActive() will return true)
     * and emits the signal KUrlNavigator::activated().
     * @see KUrlNavigator::setActive()
     */
    void requestActivation();

#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
    // KDE5: Remove and listen for focus-signal instead
    void setFocus();
#endif

#if !defined(KDE_NO_DEPRECATED) && !defined(DOXYGEN_SHOULD_SKIP_THIS)
    /**
     * Sets the location to \a url.
     * @deprecated Use KUrlNavigator::setLocationUrl(url).
     */
    KIOFILEWIDGETS_DEPRECATED void setUrl(const QUrl& url);

    /**
     * Saves the used root URL of the content for the current history element.
     * @deprecated Use KUrlNavigator::saveLocationState() instead.
     */
    KIOFILEWIDGETS_DEPRECATED void saveRootUrl(const QUrl& url);

    /**
     * Saves the coordinates of the contents for the current history element.
     * @deprecated Use KUrlNavigator::saveLocationState() instead.
     */
    KIOFILEWIDGETS_DEPRECATED void savePosition(int x, int y);
#endif

Q_SIGNALS:
    /**
     * Is emitted, if the URL navigator has been activated by
     * an user interaction
     * @see KUrlNavigator::setActive()
     */
    void activated();

    /**
     * Is emitted, if the location URL has been changed e. g. by
     * the user.
     * @see KUrlNavigator::setUrl()
     */
    void urlChanged(const QUrl& url);

    /**
     * Is emitted, before the location URL is going to be changed to \a newUrl.
     * The signal KUrlNavigator::urlChanged() will be emitted after the change
     * has been done. Connecting to this signal is useful to save the state
     * of a view with KUrlNavigator::saveLocationState().
     * @since 4.5
     */
    void urlAboutToBeChanged(const QUrl& newUrl);

    /**
     * Is emitted, if the editable state for the URL has been changed
     * (see KUrlNavigator::setUrlEditable()).
     */
    void editableStateChanged(bool editable);

    /**
     * Is emitted, if the history has been changed. Usually
     * the history is changed if a new URL has been selected.
     */
    void historyChanged();

    /**
     * Is emitted if a dropping has been done above the destination
     * \a destination. The receiver must accept the drop event if
     * the dropped data can be handled.
     * @since 4.2
     */
    void urlsDropped(const QUrl& destination, QDropEvent* event);

    /**
     * This signal is emitted when the Return or Enter key is pressed.
     */
    void returnPressed();

    /**
     * Is emitted if the URL \a url should be opened in a new tab because
     * the user clicked on a breadcrumb with the middle mouse button.
     * @since 4.5
     */
    void tabRequested(const QUrl& url);

protected:
#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
    /**
     * If the Escape key is pressed, the navigation bar should switch
     * to the breadcrumb view.
     * @see QWidget::keyPressEvent()
     */
    virtual void keyPressEvent(QKeyEvent* event);

    /**
     * Reimplemented for internal purposes.
     */
    virtual void keyReleaseEvent(QKeyEvent* event);

    /**
     * Paste the clipboard content as URL, if the middle mouse
     * button has been clicked.
     * @see QWidget::mouseReleaseEvent()
     */
    virtual void mouseReleaseEvent(QMouseEvent* event);

    virtual void resizeEvent(QResizeEvent* event);

    virtual void wheelEvent(QWheelEvent* event);

    virtual bool eventFilter(QObject* watched, QEvent* event);
#endif

private:
    Q_PRIVATE_SLOT(d, void slotReturnPressed())
    Q_PRIVATE_SLOT(d, void slotProtocolChanged(const QString& protocol))
    Q_PRIVATE_SLOT(d, void switchView())
    Q_PRIVATE_SLOT(d, void dropUrls(const QUrl& destination, QDropEvent*))
    Q_PRIVATE_SLOT(d, void slotNavigatorButtonClicked(const QUrl& url, Qt::MouseButton button))
    Q_PRIVATE_SLOT(d, void openContextMenu())
    Q_PRIVATE_SLOT(d, void openPathSelectorMenu())
    Q_PRIVATE_SLOT(d, void updateButtonVisibility())
    Q_PRIVATE_SLOT(d, void switchToBreadcrumbMode())
    Q_PRIVATE_SLOT(d, void slotPathBoxChanged(const QString& text))
    Q_PRIVATE_SLOT(d, void updateContent())

private:
    class Private;
    Private* const d;

    Q_DISABLE_COPY(KUrlNavigator)
};

#endif
