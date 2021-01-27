/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KFILEPLACESVIEW_H
#define KFILEPLACESVIEW_H

#include "kiofilewidgets_export.h"

#include <QListView>

#include <QUrl>

class QResizeEvent;
class QContextMenuEvent;

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
     * If \a enabled is true (the default), items will automatically resize
     * themselves to fill the view.
     *
     * @since 4.1
     */
    void setAutoResizeItemsEnabled(bool enabled);
    bool isAutoResizeItemsEnabled() const;

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
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                     const QVector<int> &roles) override;

Q_SIGNALS:
    void urlChanged(const QUrl &url);

    /**
     * Is emitted if items are dropped on the place \a dest.
     * The application has to take care itself about performing the
     * corresponding action like copying or moving.
     */
    void urlsDropped(const QUrl &dest, QDropEvent *event, QWidget *parent);

private:
    Q_PRIVATE_SLOT(d, void adaptItemSize())
    Q_PRIVATE_SLOT(d, void _k_placeClicked(const QModelIndex &))
    Q_PRIVATE_SLOT(d, void _k_placeEntered(const QModelIndex &))
    Q_PRIVATE_SLOT(d, void _k_placeLeft(const QModelIndex &))
    Q_PRIVATE_SLOT(d, void _k_storageSetupDone(const QModelIndex &, bool))
    Q_PRIVATE_SLOT(d, void _k_adaptItemsUpdate(qreal))
    Q_PRIVATE_SLOT(d, void _k_itemAppearUpdate(qreal))
    Q_PRIVATE_SLOT(d, void _k_itemDisappearUpdate(qreal))
    Q_PRIVATE_SLOT(d, void _k_enableSmoothItemResizing())
    Q_PRIVATE_SLOT(d, void _k_capacityBarFadeValueChanged())
    Q_PRIVATE_SLOT(d, void _k_triggerDevicePolling())

    class Private;
    Private *const d;
    friend class Private;
};

#endif
