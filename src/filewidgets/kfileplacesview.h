/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2022 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KFILEPLACESVIEW_H
#define KFILEPLACESVIEW_H

#include "kiofilewidgets_export.h"

#include <QListView>
#include <QUrl>

#include <functional>
#include <memory>

class QResizeEvent;
class QContextMenuEvent;

class KFilePlacesViewPrivate;

/**
 * @class KFilePlacesView kfileplacesview.h <KFilePlacesView>
 *
 * This class allows to display a KFilePlacesModel.
 */
class KIOFILEWIDGETS_EXPORT KFilePlacesView : public QListView
{
    Q_OBJECT
public:
    explicit KFilePlacesView(QWidget *parent = nullptr);
    ~KFilePlacesView() override;

    /**
     * The teardown function signature. Custom teardown logic
     * may be provided via the setTeardownFunction method.
     * @since 5.91
     */
    using TeardownFunction = std::function<void(const QModelIndex &)>;

    /**
     * Whether hidden places, if any, are currently shown.
     * @since 5.91
     */
    bool allPlacesShown() const;

    /**
     * If \a enabled is true, it is allowed dropping items
     * above a place for e. g. copy or move operations. The application
     * has to take care itself to perform the operation
     * (see KFilePlacesView::urlsDropped()). If
     * \a enabled is false, it is only possible adding items
     * as additional place. Per default dropping on a place is
     * disabled.
     */
    void setDropOnPlaceEnabled(bool enabled);
    bool isDropOnPlaceEnabled() const;

    /**
     * If \a delay (in ms) is greater than zero, the place will
     * automatically be activated if an item is dragged over
     * and held on top of a place for at least that duraton.
     *
     * @param delay Delay in ms, default is zero.
     * @since 5.92
     */
    void setDragAutoActivationDelay(int delay);
    int dragAutoActivationDelay() const;

    /**
     * If \a enabled is true (the default), items will automatically resize
     * themselves to fill the view.
     *
     * @since 4.1
     */
    void setAutoResizeItemsEnabled(bool enabled);
    bool isAutoResizeItemsEnabled() const;

    /**
     * Sets a custom function that will be called when teardown of
     * a device (e.g.\ unmounting a drive) is requested.
     * @since 5.91
     */
    void setTeardownFunction(TeardownFunction teardownFunc);

public Q_SLOTS:
    void setUrl(const QUrl &url);
    void setShowAll(bool showAll);

    // TODO KF6: make it a public method, not a slot
    QSize sizeHint() const override; // clazy:exclude=const-signal-or-slot

    void setModel(QAbstractItemModel *model) override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;
    void mousePressEvent(QMouseEvent *event) override;

protected Q_SLOTS:
    void rowsInserted(const QModelIndex &parent, int start, int end) override;
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) override;

Q_SIGNALS:
    /**
     * Emitted when an item in the places view is clicked on with left mouse
     * button with no modifier keys pressed.
     *
     * If a storage device needs to be mounted first, this signal is emitted once
     * mounting has completed successfully.
     *
     * @param url The URL of the place
     * @since 5.91
     */
    void placeActivated(const QUrl &url);

    /**
     * Emitted when the URL \a url should be opened in a new inactive tab because
     * the user clicked on a place with the middle mouse button or
     * left-clicked with the Ctrl modifier pressed or selected "Open in New Tab"
     * from the context menu.
     *
     * If a storage device needs to be mounted first, this signal is emitted once
     * mounting has completed successfully.
     * @since 5.91
     */
    void tabRequested(const QUrl &url);

    /**
     * Emitted when the URL \a url should be opened in a new active tab because
     * the user clicked on a place with the middle mouse button with
     * the Shift modifier pressed or left-clicked with both the Ctrl and Shift
     * modifiers pressed.

     * If a storage device needs to be mounted first, this signal is emitted once
     * mounting has completed successfully.
     * @since 5.91
     */
    void activeTabRequested(const QUrl &url);

    /**
     * Emitted when the URL \a url should be opened in a new window because
     * the user left-clicked on a place with Shift modifier pressed or selected
     * "Open in New Window" from the context menu.
     *
     * If a storage device needs to be mounted first, this signal is emitted once
     * mounting has completed successfully.
     * @since 5.91
     */
    void newWindowRequested(const QUrl &url);

    /**
     * Emitted just before the context menu opens. This can be used to add additional
     * application actions to the menu.
     * @param index The model index of the place whose menu is about to open.
     * @param menu The menu that will be opened.
     * @since 5.91
     */
    void contextMenuAboutToShow(const QModelIndex &index, QMenu *menu);

    /**
     * Emitted when allPlacesShown changes
     * @since 5.91
     */
    void allPlacesShownChanged(bool allPlacesShown);

    void urlChanged(const QUrl &url);

    /**
     * Is emitted if items are dropped on the place \a dest.
     * The application has to take care itself about performing the
     * corresponding action like copying or moving.
     */
    void urlsDropped(const QUrl &dest, QDropEvent *event, QWidget *parent);

private:
    friend class KFilePlacesViewPrivate;
    std::unique_ptr<KFilePlacesViewPrivate> const d;
};

#endif
