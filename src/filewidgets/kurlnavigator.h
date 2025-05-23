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

#include <QByteArray>
#include <QUrl>
#include <QWidget>

#include <memory>

class QMouseEvent;

class KFilePlacesModel;
class KUrlComboBox;

class KUrlNavigatorPrivate;

/*!
 * \class KUrlNavigator
 * \inmodule KIOFileWidgets
 *
 * \brief Widget that allows to navigate through the paths of an URL.
 *
 * The URL navigator offers two modes:
 * \list
 * \li Editable: The URL of the location is editable inside an editor.
 *                 By pressing RETURN the URL will get activated.
 * \li Non editable ("breadcrumb view"): The URL of the location is represented by
 *                 a number of buttons, where each button represents a path
 *                 of the URL. By clicking on a button the path will get
 *                 activated. This mode also supports drag and drop of items.
 * \endlist
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
 * \list
 * \li Create an instance providing a places model and an URL.
 * \li Create an instance of QAbstractItemView which shows the content of the URL
 *   given by the URL navigator.
 * \li Connect to the signal KUrlNavigator::urlChanged() and synchronize the content of
 *   QAbstractItemView with the URL given by the URL navigator.
 * \endlist
 *
 * It is recommended, that the application remembers the state of the QAbstractItemView
 * when the URL has been changed. This allows to restore the view state when going back in history.
 * KUrlNavigator offers support for remembering the view state:
 * \list
 * \li The signal urlAboutToBeChanged() will be emitted before the URL change takes places.
 *   This allows the application to store the view state by KUrlNavigator::saveLocationState().
 * \li The signal urlChanged() will be emitted after the URL change took place. This allows
 *   the application to restore the view state by getting the values from
 *   KUrlNavigator::locationState().
 * \endlist
 */
class KIOFILEWIDGETS_EXPORT KUrlNavigator : public QWidget
{
    Q_OBJECT

public:
    /*! \since 4.5 */
    KUrlNavigator(QWidget *parent = nullptr);

    /*!
     * \a placesModel    Model for the places which are selectable inside a
     *                       menu. A place can be a bookmark or a device. If it is 0,
     *                       no places selector is displayed.
     *
     * \a url            URL which is used for the navigation or editing.
     *
     * \a parent         Parent widget.
     */
    KUrlNavigator(KFilePlacesModel *placesModel, const QUrl &url, QWidget *parent);
    ~KUrlNavigator() override;

    /*!
     * Returns URL of the location given by the \a historyIndex. If \a historyIndex
     *         is smaller than 0, the URL of the current location is returned.
     * \since  4.5
     */
    QUrl locationUrl(int historyIndex = -1) const;

    /*!
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
     */
    void saveLocationState(const QByteArray &state);

    /*!
     * Returns Location state given by \a historyIndex. If \a historyIndex
     *         is smaller than 0, the state of the current location is returned.
     * \sa    KUrlNavigator::saveLocationState()
     * \since  4.5
     */
    QByteArray locationState(int historyIndex = -1) const;

    /*!
     * Goes back one step in the URL history. The signals
     * KUrlNavigator::urlAboutToBeChanged(), KUrlNavigator::urlChanged() and
     * KUrlNavigator::historyChanged() are emitted if true is returned. False is returned
     * if the beginning of the history has already been reached and hence going back was
     * not possible. The history index (see KUrlNavigator::historyIndex()) is
     * increased by one if the operation was successful.
     */
    bool goBack();

    /*!
     * Goes forward one step in the URL history. The signals
     * KUrlNavigator::urlAboutToBeChanged(), KUrlNavigator::urlChanged() and
     * KUrlNavigator::historyChanged() are emitted if true is returned. False is returned
     * if the end of the history has already been reached and hence going forward
     * was not possible. The history index (see KUrlNavigator::historyIndex()) is
     * decreased by one if the operation was successful.
     */
    bool goForward();

    /*!
     * Goes up one step of the URL path and remembers the old path
     * in the history. The signals KUrlNavigator::urlAboutToBeChanged(),
     * KUrlNavigator::urlChanged() and KUrlNavigator::historyChanged() are
     * emitted if true is returned. False is returned if going up was not
     * possible as the root has been reached.
     */
    bool goUp();

    // KDE5: Remove the home-property. It is sufficient to invoke
    // KUrlNavigator::setLocationUrl(homeUrl) on application-side.
    /*!
     * Goes to the home URL and remembers the old URL in the history.
     * The signals KUrlNavigator::urlAboutToBeChanged(), KUrlNavigator::urlChanged()
     * and KUrlNavigator::historyChanged() are emitted.
     *
     * \sa setHomeUrl()
     */
    void goHome();

    // KDE5: Remove the home-property. It is sufficient to invoke
    // KUrlNavigator::setLocationUrl(homeUrl) on application-side.
    /*!
     * Sets the home URL used by KUrlNavigator::goHome(). If no
     * home URL is set, the default home path of the user is used.
     */
    void setHomeUrl(const QUrl &url);

    /*!
     *
     */
    QUrl homeUrl() const;

    /*!
     * Allows to edit the URL of the navigation bar if \a editable
     * is true, and sets the focus accordingly.
     * If \a editable is false, each part of
     * the URL is presented by a button for a fast navigation ("breadcrumb view").
     */
    void setUrlEditable(bool editable);

    /*!
     * Returns \c true, if the URL is editable within a line editor.
     *         If false is returned, each part of the URL is presented by a button
     *         for fast navigation ("breadcrumb view").
     */
    bool isUrlEditable() const;

    /*!
     * Shows the full path of the URL even if a place represents a part of the URL.
     * Assuming that a place called "Pictures" uses the URL /home/user/Pictures.
     * An URL like /home/user/Pictures/2008 is shown as [Pictures] > [2008]
     * in the breadcrumb view, if showing the full path is turned off. If
     * showing the full path is turned on, the URL is shown
     * as [/] > [home] > [Pictures] > [2008].
     */
    void setShowFullPath(bool show);

    /*!
     * Returns \c true, if the full path of the URL should be shown in the breadcrumb view.
     * \since 4.2
     */
    bool showFullPath() const;

    /*!
     * Set the URL navigator to the active mode, if \a active
     * is true. The active mode is default. The inactive mode only differs
     * visually from the active mode, no change of the behavior is given.
     *
     * Using the URL navigator in the inactive mode is useful when having split views,
     * where the inactive view is indicated by an inactive URL
     * navigator visually.
     */
    void setActive(bool active);

    /*!
     * Returns \c true, if the URL navigator is in the active mode.
     * \sa setActive()
     */
    bool isActive() const;

    /*!
     * Sets the places selector visible, if \a visible is true.
     * The places selector allows to select the places provided
     * by the places model passed in the constructor. Per default
     * the places selector is visible.
     */
    void setPlacesSelectorVisible(bool visible);

    /*! Returns \c true, if the places selector is visible. */
    bool isPlacesSelectorVisible() const;

    /*!
     * Returns the currently entered, but not accepted URL.
     *         It is possible that the returned URL is not valid.
     */
    QUrl uncommittedUrl() const;

    /*!
     * Returns the amount of locations in the history. The data for each
     *         location can be retrieved by KUrlNavigator::locationUrl() and
     *         KUrlNavigator::locationState().
     */
    int historySize() const;

    /*!
     * Returns the history index of the current location, where
     *          0 <= history index < KUrlNavigator::historySize(). 0 is the most
     *          recent history entry.
     */
    int historyIndex() const;

    /*!
     * Returns the used editor when the navigator is in the edit mode
     * \sa    KUrlNavigator::setUrlEditable()
     */
    KUrlComboBox *editor() const;

    /*!
     * Set the URL schemes that the navigator should allow navigating to.
     *
     * If the passed list is empty, all schemes are supported. Examples for
     * schemes are \c "file" or \c "ftp".
     *
     * \sa QFileDialog::setSupportedSchemes
     * \since 5.103
     */
    void setSupportedSchemes(const QStringList &schemes);

    /*!
     * Returns the URL schemes that the navigator should allow navigating to.
     *
     * If the returned list is empty, all schemes are supported.
     *
     * \sa QFileDialog::supportedSchemes
     * \since 5.103
     */
    QStringList supportedSchemes() const;

    /*!
     * The child widget that received the QDropEvent when dropping on the URL
     * navigator. You can pass this widget to KJobWidgets::setWindow()
     * if you need to show a drop menu with KIO::drop().
     *
     * Returns Child widget that has received the last drop event, or nullptr if
     *         nothing has been dropped yet on the URL navigator.
     * \since 5.37
     * \sa KIO::drop()
     */
    QWidget *dropWidget() const;

    /*!
     * Sets whether to show hidden folders in the subdirectories popup.
     * \since 5.87
     */
    void setShowHiddenFolders(bool showHiddenFolders);

    /*!
     * Returns whether to show hidden folders in the subdirectories popup.
     * \since 5.87
     */
    bool showHiddenFolders() const;

    /*!
     * Sets whether to sort hidden folders in the subdirectories popup last.
     * \since 5.87
     */
    void setSortHiddenFoldersLast(bool sortHiddenFoldersLast);

    /*!
     * Returns whether to sort hidden folders in the subdirectories popup last.
     * \since 5.87
     */
    bool sortHiddenFoldersLast() const;

    /*!
     * Puts \a widget to the right of the breadcrumb.
     *
     * KUrlNavigator takes ownership over \a widget. Any existing badge widget is deleted.
     *
     * \note There is no limit to the size of the badge widget. If your badge widget is taller than other
     * controls in KUrlNavigator, then the whole KUrlNavigator will be resized to accommodate it. Also,
     * KUrlNavigator has fixed minimumWidth of 100, so if your badge widget is too wide, it might be clipped
     * when the space is tight. You might want to call KUrlNavigator::setMinimumWidth() with a larger value
     * in that case.
     * In general, it is recommended to keep the badge widget small and not expanding, to avoid layout issues.
     * \since 6.2
     */
    void setBadgeWidget(QWidget *widget);

    /*!
     * Returns the badge widget set by setBadgeWidget(). If setBadgeWidget() hasn't been called, returns nullptr.
     * \since 6.2
     */
    QWidget *badgeWidget() const;

    /*!
     * Sets the background painting enabled or disabled for the buttons layout.
     *
     * In frameless styles, its recommended to set the background to disabled.
     *
     * Does not affect the input mode.
     * \since 6.14
     */
    void setBackgroundEnabled(bool enabled);

    /*!
     * Returns true if the background of the buttons layout is being painted.
     *
     * Does not represent the input mode background.
     *
     * \since 6.14
     */
    bool isBackgroundEnabled() const;

public Q_SLOTS:
    /*!
     * Sets the location to \a url. The old URL is added to the history.
     * The signals KUrlNavigator::urlAboutToBeChanged(), KUrlNavigator::urlChanged()
     * and KUrlNavigator::historyChanged() are emitted. Use
     * KUrlNavigator::locationUrl() to read the location.
     */
    void setLocationUrl(const QUrl &url);

    /*!
     * Activates the URL navigator (KUrlNavigator::isActive() will return true)
     * and emits the signal KUrlNavigator::activated().
     * \sa setActive()
     */
    void requestActivation();

    // KDE5: Remove and listen for focus-signal instead
    void setFocus();

Q_SIGNALS:
    /*!
     * Is emitted, if the URL navigator has been activated by
     * an user interaction
     * \sa setActive()
     */
    void activated();

    /*!
     * Is emitted, if the location URL has been changed e. g. by
     * the user.
     * \sa setUrl()
     */
    void urlChanged(const QUrl &url);

    /*!
     * Is emitted, before the location URL is going to be changed to \a newUrl.
     * The signal KUrlNavigator::urlChanged() will be emitted after the change
     * has been done. Connecting to this signal is useful to save the state
     * of a view with KUrlNavigator::saveLocationState().
     */
    void urlAboutToBeChanged(const QUrl &newUrl);

    /*!
     * Is emitted, if the editable state for the URL has been changed
     * (see KUrlNavigator::setUrlEditable()).
     */
    void editableStateChanged(bool editable);

    /*!
     * Is emitted, if the history has been changed. Usually
     * the history is changed if a new URL has been selected.
     */
    void historyChanged();

    /*!
     * Is emitted if a dropping has been done above the destination
     * \a destination. The receiver must accept the drop event if
     * the dropped data can be handled.
     */
    void urlsDropped(const QUrl &destination, QDropEvent *event);

    /*!
     * This signal is emitted when the Return or Enter key is pressed.
     */
    void returnPressed();

    /*!
     * Is emitted if the URL \a url should be opened in a new inactive tab because
     * the user clicked on a breadcrumb with the middle mouse button or
     * left-clicked with the ctrl modifier pressed or pressed return with
     * the alt modifier pressed.
     */
    void tabRequested(const QUrl &url);

    /*!
     * Is emitted if the URL \a url should be opened in a new active tab because
     * the user clicked on a breadcrumb with the middle mouse button with
     * the shift modifier pressed or left-clicked with both the ctrl and shift
     * modifiers pressed or pressed return with both the alt and shift modifiers
     * pressed.
     * \since 5.89
     */
    void activeTabRequested(const QUrl &url);

    /*!
     * Is emitted if the URL \a url should be opened in a new window because
     * the user left-clicked on a breadcrumb with the shift modifier pressed
     * or pressed return with the shift modifier pressed.
     * \since 5.89
     */
    void newWindowRequested(const QUrl &url);

    /*!
     * When the URL is changed and the new URL (e.g.\ /home/user1/)
     * is a parent of the previous URL (e.g.\ /home/user1/data/stuff),
     * then this signal is emitted and \a url is set to the child
     * directory of the new URL which is an ancestor of the old URL
     * (in the example paths this would be /home/user1/data/).
     * This signal allows file managers to pre-select the directory
     * that the user is navigating up from.
     * \since 5.37.0
     */
    void urlSelectionRequested(const QUrl &url);

    /*!
     * The internal layout and graphical representation of components has changed,
     * either after an url change or after a switch between editable mode and
     * breadcrumb mode
     * \since 6.11
     */
    void layoutChanged();

protected:
    /*
     * If the Escape key is pressed, the navigation bar should switch
     * to the breadcrumb view.
     */
    void keyPressEvent(QKeyEvent *event) override;

    void keyReleaseEvent(QKeyEvent *event) override;

    /*
     * Paste the clipboard content as URL, if the middle mouse
     * button has been clicked.
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /*
     * Reimplemented to activate on middle mousse button click
     */
    void mousePressEvent(QMouseEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;

    void showEvent(QShowEvent *event) override;

    bool eventFilter(QObject *watched, QEvent *event) override;

    void paintEvent(QPaintEvent *event) override;

private:
    friend class KUrlNavigatorPrivate;
    std::unique_ptr<KUrlNavigatorPrivate> const d;

    Q_DISABLE_COPY(KUrlNavigator)
};

#endif
