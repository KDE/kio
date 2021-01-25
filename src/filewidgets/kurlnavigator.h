/*
    SPDX-FileCopyrightText: 2006-2010 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2006 Aaron J. Seigo <aseigo@kde.org>
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer @ kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KURLNAVIGATOR_H
#define KURLNAVIGATOR_H

#include "kiofilewidgets_export.h"

#include <QUrl>
#include <QWidget>
#include <QByteArray>

#include <memory>

class QMouseEvent;

class KFilePlacesModel;
class KUrlComboBox;

class KUrlNavigatorPrivate;

/**
 * @class KUrlNavigator kurlnavigator.h <KUrlNavigator>
 *
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
    KUrlNavigator(QWidget *parent = nullptr);

    /**
     * @param placesModel    Model for the places which are selectable inside a
     *                       menu. A place can be a bookmark or a device. If it is 0,
     *                       no places selector is displayed.
     * @param url            URL which is used for the navigation or editing.
     * @param parent         Parent widget.
     */
    KUrlNavigator(KFilePlacesModel *placesModel, const QUrl &url, QWidget *parent);
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
    void saveLocationState(const QByteArray &state);

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
    void setHomeUrl(const QUrl &url);

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
    KUrlComboBox *editor() const;

    /**
     * If an application supports only some special protocols, they can be set
     * with \a protocols .
     */
    // TODO KF6 rename to setSupportedSchemes to match KDirOperator and KFileWidget
    void setCustomProtocols(const QStringList &protocols);

    /**
     * @return The custom protocols if they are set, QStringList() otherwise.
     */
    QStringList customProtocols() const;

    /**
     * The child widget that received the QDropEvent when dropping on the URL
     * navigator. You can pass this widget to KJobWidgets::setWindow()
     * if you need to show a drop menu with KIO::drop().
     * @return Child widget that has received the last drop event, or nullptr if
     *         nothing has been dropped yet on the URL navigator.
     * @since 5.37
     * @see KIO::drop()
     */
    QWidget *dropWidget() const;

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(4, 5)
    /**
     * @return     The current URL of the location.
     * @deprecated Since 4.5, use KUrlNavigator::locationUrl() instead.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(4, 5, "Use KUrlNavigator::locationUrl(int)")
    const QUrl &url() const;
#endif

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(4, 5)
    /**
     * @return The portion of the current URL up to the path part given
     * by \a index. Assuming that the current URL is /home/peter/Documents/Music,
     * then the following URLs are returned for an index:
     * - index <= 0: /home
     * - index is 1: /home/peter
     * - index is 2: /home/peter/Documents
     * - index >= 3: /home/peter/Documents/Music
     * @deprecated Since 4.5. It should not be necessary for a client of KUrlNavigator to query this information.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(4, 5, "Do not use")
    QUrl url(int index) const;
#endif

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(4, 5)
    /**
     * @return URL for the history element with the index \a historyIndex.
     *         The history index 0 represents the most recent URL.
     * @since 4.3
     * @deprecated Since 4.5, use KUrlNavigator::locationUrl(historyIndex) instead.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(4, 5, "Use KUrlNavigator::locationUrl(int)")
    QUrl historyUrl(int historyIndex) const;
#endif

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(4, 5)
    /**
     * @return The saved root URL for the current URL (see KUrlNavigator::saveRootUrl()).
     * @deprecated Since 4.5, use KUrlNavigator::locationState() instead.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(4, 5, "Use KUrlNavigator::locationState(int)")
    const QUrl &savedRootUrl() const;
#endif

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(4, 5)
    /**
     * @return The saved contents position of the upper left corner
     *         for the current URL.
     * @deprecated Since 4.5, use KUrlNavigator::locationState() instead.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(4, 5, "Use KUrlNavigator::locationState(int)")
    QPoint savedPosition() const;
#endif

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(4, 5)
    /** @deprecated Since 4.5, use setHomeUrl(const QUrl& url) instead. */
    KIOFILEWIDGETS_DEPRECATED_VERSION(4, 5, "Use KUrlNavigator::setHomeUrl(const QUrl&)")
    void setHomeUrl(const QString &homeUrl);
#endif

public Q_SLOTS:
    /**
     * Sets the location to \a url. The old URL is added to the history.
     * The signals KUrlNavigator::urlAboutToBeChanged(), KUrlNavigator::urlChanged()
     * and KUrlNavigator::historyChanged() are emitted. Use
     * KUrlNavigator::locationUrl() to read the location.
     * @since 4.5
     */
    void setLocationUrl(const QUrl &url);

    /**
     * Activates the URL navigator (KUrlNavigator::isActive() will return true)
     * and emits the signal KUrlNavigator::activated().
     * @see KUrlNavigator::setActive()
     */
    void requestActivation();

#if !defined(K_DOXYGEN)
    // KDE5: Remove and listen for focus-signal instead
    void setFocus();
#endif

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(4, 5)
    /**
     * Sets the location to \a url.
     * @deprecated Since 4.5, use KUrlNavigator::setLocationUrl(url).
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(4, 5, "Use KUrlNavigator::setLocationUrl(const QUrl&))")
    void setUrl(const QUrl &url);
#endif

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(4, 5)
    /**
     * Saves the used root URL of the content for the current history element.
     * @deprecated Since 4.5, use KUrlNavigator::saveLocationState() instead.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(4, 5, "Use KUrlNavigator::saveLocationState(const QByteArray &)")
    void saveRootUrl(const QUrl &url);
#endif

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(4, 5)
    /**
     * Saves the coordinates of the contents for the current history element.
     * @deprecated Since 4.5, use KUrlNavigator::saveLocationState() instead.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(4, 5, "Use KUrlNavigator::saveLocationState(const QByteArray &)")
    void savePosition(int x, int y);
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
    void urlChanged(const QUrl &url);

    /**
     * Is emitted, before the location URL is going to be changed to \a newUrl.
     * The signal KUrlNavigator::urlChanged() will be emitted after the change
     * has been done. Connecting to this signal is useful to save the state
     * of a view with KUrlNavigator::saveLocationState().
     * @since 4.5
     */
    void urlAboutToBeChanged(const QUrl &newUrl);

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
    void urlsDropped(const QUrl &destination, QDropEvent *event);

    /**
     * This signal is emitted when the Return or Enter key is pressed.
     */
    void returnPressed();

    /**
     * Is emitted if the URL \a url should be opened in a new tab because
     * the user clicked on a breadcrumb with the middle mouse button.
     * @since 4.5
     */
    void tabRequested(const QUrl &url);

    /**
     * When the URL is changed and the new URL (e.g. /home/user1/)
     * is a parent of the previous URL (e.g. /home/user1/data/stuff),
     * then this signal is emitted and \a url is set to the child
     * directory of the new URL which is an ancestor of the old URL
     * (in the example paths this would be /home/user1/data/).
     * This signal allows file managers to pre-select the directory
     * that the user is navigating up from.
     * @since 5.37.0
     */
    void urlSelectionRequested(const QUrl &url);

protected:
#if !defined(K_DOXYGEN)
    /**
     * If the Escape key is pressed, the navigation bar should switch
     * to the breadcrumb view.
     * @see QWidget::keyPressEvent()
     */
    void keyPressEvent(QKeyEvent *event) override;

    /**
     * Reimplemented for internal purposes.
     */
    void keyReleaseEvent(QKeyEvent *event) override;

    /**
     * Paste the clipboard content as URL, if the middle mouse
     * button has been clicked.
     * @see QWidget::mouseReleaseEvent()
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /**
     * Reimplemented to activate on middle mousse button click
     */
    void mousePressEvent(QMouseEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;

    bool eventFilter(QObject *watched, QEvent *event) override;
#endif

private:
    friend class KUrlNavigatorPrivate;
    std::unique_ptr<KUrlNavigatorPrivate> const d;

    Q_DISABLE_COPY(KUrlNavigator)
};

#endif
