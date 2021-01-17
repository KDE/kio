/*
    SPDX-FileCopyrightText: 2009 Rahman Duran <rahman.duran@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kurlnavigatormenu_p.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMimeData>

namespace KDEPrivate
{

KUrlNavigatorMenu::KUrlNavigatorMenu(QWidget *parent) :
    QMenu(parent),
    m_initialMousePosition(QCursor::pos()),
    m_mouseMoved(false)
{
    setAcceptDrops(true);
    setMouseTracking(true);
}

KUrlNavigatorMenu::~KUrlNavigatorMenu()
{
}

void KUrlNavigatorMenu::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void KUrlNavigatorMenu::dragMoveEvent(QDragMoveEvent *event)
{
    QMouseEvent mouseEvent(QMouseEvent(QEvent::MouseMove, event->pos(),
                                       Qt::LeftButton, event->mouseButtons(), event->keyboardModifiers()));
    mouseMoveEvent(&mouseEvent);
}

void KUrlNavigatorMenu::dropEvent(QDropEvent *event)
{
    QAction *action = actionAt(event->pos());
    if (action != nullptr) {
        Q_EMIT urlsDropped(action, event);
    }
}

void KUrlNavigatorMenu::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_mouseMoved) {
        QPoint moveDistance = mapToGlobal(event->pos()) - m_initialMousePosition;
        m_mouseMoved = (moveDistance.manhattanLength() >= QApplication::startDragDistance());
    }
    // Don't pass the event to the base class until we consider
    // that the mouse has moved. This prevents menu items from
    // being highlighted too early.
    if (m_mouseMoved) {
        QMenu::mouseMoveEvent(event);
    }
}

void KUrlNavigatorMenu::mouseReleaseEvent(QMouseEvent *event)
{
    Qt::MouseButton btn = event->button();
    // Since menu is opened on mouse press, we may receive
    // the corresponding mouse release event. Let's ignore
    // it unless mouse was moved.
    if (m_mouseMoved || (btn != Qt::LeftButton)) {
        QAction *action = actionAt(event->pos());
        if (action != nullptr) {
            Q_EMIT mouseButtonClicked(action, btn);

            // Prevent QMenu default activation, in case
            // triggered signal is used
            setActiveAction(nullptr);
        }
        QMenu::mouseReleaseEvent(event);
    }
    m_mouseMoved = true;
}

} // namespace KDEPrivate

#include "moc_kurlnavigatormenu_p.cpp"
