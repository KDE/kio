/*
    SPDX-FileCopyrightText: 2006-2010 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2006 Aaron J. Seigo <aseigo@kde.org>
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer @ kde.org>
    SPDX-FileCopyrightText: 2022 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KCOREURLNAVIGATOR_H
#define KCOREURLNAVIGATOR_H

#include "kiogui_export.h"

#include <QByteArray>
#include <QObject>
#include <QUrl>

#include <memory>

class QMouseEvent;

class KFilePlacesModel;
class KUrlComboBox;

class KCoreUrlNavigatorPrivate;

/**
 * @class KCoreUrlNavigator kcoreurlnavigator.h <KCoreUrlNavigator>
 *
 * @brief Object that helps with keeping track of URLs in file-manager like interfaces.
 *
 * @since 5.93
 */
class KIOGUI_EXPORT KCoreUrlNavigator : public QObject
{
    Q_OBJECT

public:
    KCoreUrlNavigator(const QUrl &url = QUrl(), QObject *parent = nullptr);
    ~KCoreUrlNavigator() override;

    Q_PROPERTY(QUrl currentLocationUrl READ currentLocationUrl WRITE setCurrentLocationUrl NOTIFY currentLocationUrlChanged)

    QUrl currentLocationUrl() const;
    void setCurrentLocationUrl(const QUrl &url);
    Q_SIGNAL void currentLocationUrlChanged();

    /**
     * Is emitted, before the location URL is going to be changed to \a newUrl.
     * The signal KCoreUrlNavigator::urlChanged() will be emitted after the change
     * has been done. Connecting to this signal is useful to save the state
     * of a view with KCoreUrlNavigator::saveLocationState().
     */
    Q_SIGNAL void currentUrlAboutToChange(const QUrl &newUrl);

    /**
     * The amount of locations in the history. The data for each
     * location can be retrieved by KCoreUrlNavigator::locationUrl() and
     * KCoreUrlNavigator::locationState().
     */
    Q_PROPERTY(int historySize READ historySize NOTIFY historySizeChanged)
    int historySize() const;
    Q_SIGNAL void historySizeChanged();

    /**
     * When the URL is changed and the new URL (e.g.\ /home/user1/)
     * is a parent of the previous URL (e.g.\ /home/user1/data/stuff),
     * then this signal is emitted and \p url is set to the child
     * directory of the new URL which is an ancestor of the old URL
     * (in the example paths this would be /home/user1/data/).
     * This signal allows file managers to pre-select the directory
     * that the user is navigating up from.
     * @since 5.95
     */
    Q_SIGNAL void urlSelectionRequested(const QUrl &url);

    /**
     * The history index of the current location, where
     * 0 <= history index < KCoreUrlNavigator::historySize(). 0 is the most
     * recent history entry.
     */
    Q_PROPERTY(int historyIndex READ historyIndex NOTIFY historyIndexChanged)
    int historyIndex() const;
    Q_SIGNAL void historyIndexChanged();

    /**
     * Is emitted, if the history has been changed. Usually
     * the history is changed if a new URL has been selected.
     */
    Q_SIGNAL void historyChanged();

    /**
     * @return URL of the location given by the \a historyIndex. If \a historyIndex
     *         is smaller than 0, the URL of the current location is returned.
     */
    Q_INVOKABLE QUrl locationUrl(int historyIndex = -1) const;

    /**
     * Saves the location state described by \a state for the current location. It is recommended
     * that at least the scroll position of a view is remembered and restored when traversing
     * through the history. Saving the location state should be done when the signal
     * KCoreUrlNavigator::urlAboutToBeChanged() has been emitted. Restoring the location state (see
     * KCoreUrlNavigator::locationState()) should be done when the signal KCoreUrlNavigator::urlChanged()
     * has been emitted.
     *
     * Example:
     * \code
     * urlNavigator->saveLocationState(QPoint(x, y));
     * \endcode
     *
     */
    Q_INVOKABLE void saveLocationState(const QVariant &state);

    /**
     * @return Location state given by \a historyIndex. If \a historyIndex
     *         is smaller than 0, the state of the current location is returned.
     * @see    KCoreUrlNavigator::saveLocationState()
     */
    Q_INVOKABLE QVariant locationState(int historyIndex = -1) const;

    /**
     * Goes back one step in the URL history. The signals
     * KCoreUrlNavigator::urlAboutToBeChanged(), KCoreUrlNavigator::urlChanged() and
     * KCoreUrlNavigator::historyChanged() are emitted if true is returned. False is returned
     * if the beginning of the history has already been reached and hence going back was
     * not possible. The history index (see KCoreUrlNavigator::historyIndex()) is
     * increased by one if the operation was successful.
     */
    Q_INVOKABLE bool goBack();

    /**
     * Goes forward one step in the URL history. The signals
     * KCoreUrlNavigator::urlAboutToBeChanged(), KCoreUrlNavigator::urlChanged() and
     * KCoreUrlNavigator::historyChanged() are emitted if true is returned. False is returned
     * if the end of the history has already been reached and hence going forward
     * was not possible. The history index (see KCoreUrlNavigator::historyIndex()) is
     * decreased by one if the operation was successful.
     */
    Q_INVOKABLE bool goForward();

    /**
     * Goes up one step of the URL path and remembers the old path
     * in the history. The signals KCoreUrlNavigator::urlAboutToBeChanged(),
     * KCoreUrlNavigator::urlChanged() and KCoreUrlNavigator::historyChanged() are
     * emitted if true is returned. False is returned if going up was not
     * possible as the root has been reached.
     */
    Q_INVOKABLE bool goUp();

private:
    friend class KCoreUrlNavigatorPrivate;
    std::unique_ptr<KCoreUrlNavigatorPrivate> const d;
};

#endif
