/*
    SPDX-FileCopyrightText: 2007 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2019 Méven Car <meven.car@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kdiroperatoriconview_p.h"

#include <QDragEnterEvent>
#include <QMimeData>
#include <QScrollBar>
#include <QApplication>

#include <KIconLoader>
#include <KFileItemDelegate>

KDirOperatorIconView::KDirOperatorIconView(QWidget *parent, QStyleOptionViewItem::Position aDecorationPosition) :
    QListView(parent)
{
    setViewMode(QListView::IconMode);
    setResizeMode(QListView::Adjust);
    setSpacing(0);
    setMovement(QListView::Static);
    setDragDropMode(QListView::DragOnly);
    setVerticalScrollMode(QListView::ScrollPerPixel);
    setHorizontalScrollMode(QListView::ScrollPerPixel);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setWordWrap(true);
    setIconSize(QSize(KIconLoader::SizeSmall, KIconLoader::SizeSmall));

    decorationPosition = aDecorationPosition;

    const QFontMetrics metrics(viewport()->font());
    const int singleStep = metrics.height() * QApplication::wheelScrollLines();

    verticalScrollBar()->setSingleStep(singleStep);
    horizontalScrollBar()->setSingleStep(singleStep);

    updateLayout();
    connect(this, &QListView::iconSizeChanged, this, &KDirOperatorIconView::updateLayout);
}

KDirOperatorIconView::~KDirOperatorIconView()
{
}

void KDirOperatorIconView::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);

    updateLayout();
}

QStyleOptionViewItem KDirOperatorIconView::viewOptions() const
{
    QStyleOptionViewItem viewOptions = QListView::viewOptions();
    viewOptions.showDecorationSelected = true;
    viewOptions.textElideMode = Qt::ElideMiddle;
    viewOptions.decorationPosition = decorationPosition;
    if (viewOptions.decorationPosition == QStyleOptionViewItem::Left) {
        viewOptions.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    } else {
        viewOptions.displayAlignment = Qt::AlignCenter;
    }

    return viewOptions;
}

void KDirOperatorIconView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void KDirOperatorIconView::mousePressEvent(QMouseEvent *event)
{
    if (!indexAt(event->pos()).isValid()) {
        const Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
        if (!(modifiers & Qt::ShiftModifier) && !(modifiers & Qt::ControlModifier)) {
            clearSelection();
        }
    }

    QListView::mousePressEvent(event);
}

void KDirOperatorIconView::wheelEvent(QWheelEvent *event)
{
    QListView::wheelEvent(event);

    // apply the vertical wheel event to the horizontal scrollbar, as
    // the items are aligned from left to right
    if (event->angleDelta().y() != 0) {
        QWheelEvent horizEvent(event->position(),
                               event->globalPosition(),
                               QPoint(event->pixelDelta().y(), 0),
                               QPoint(event->angleDelta().y(), 0),
                               event->buttons(),
                               event->modifiers(),
                               event->phase(),
                               event->inverted(),
                               event->source());
        QApplication::sendEvent(horizontalScrollBar(), &horizEvent);
    }
}

void KDirOperatorIconView::updateLayout() {
    if (decorationPosition == QStyleOptionViewItem::Position::Top) {
        // Icons view
        setFlow(QListView::LeftToRight);
        const QFontMetrics metrics(viewport()->font());

        const int height = iconSize().height() + metrics.height() * 2.5;
        const int minWidth = qMax(height, metrics.height() * 5);

        const int scrollBarWidth = verticalScrollBar()->sizeHint().width();

        // Subtract 1 px to prevent flickering when resizing the window
        // For Oxygen a column is missing after showing the dialog without resizing it,
        // therefore subtract 4 more (scaled) pixels
        const int viewPortWidth = contentsRect().width() - scrollBarWidth - 1 - 4 * devicePixelRatioF();
        const int itemsInRow = qMax(1, viewPortWidth / minWidth);
        const int remainingWidth = viewPortWidth - (minWidth * itemsInRow);
        const int width = minWidth + (remainingWidth / itemsInRow);

        const QSize itemSize(width, height);

        setGridSize(itemSize);
        KFileItemDelegate *delegate = qobject_cast<KFileItemDelegate *>(itemDelegate());
        if (delegate) {
            delegate->setMaximumSize(itemSize);
        }
    } else {
        // compact view
        setFlow(QListView::TopToBottom);
        setGridSize(QSize());
        KFileItemDelegate *delegate = qobject_cast<KFileItemDelegate *>(itemDelegate());
        if (delegate) {
            delegate->setMaximumSize(QSize());
        }
    }
}
void KDirOperatorIconView::setDecorationPosition(QStyleOptionViewItem::Position newDecorationPosition)
{
    decorationPosition = newDecorationPosition;
    updateLayout();
}
