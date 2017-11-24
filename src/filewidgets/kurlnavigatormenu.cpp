/*
   Copyright (C) 2009 by Rahman Duran <rahman.duran@gmail.com>

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2 of the License or
   ( at your option ) version 3 or, at the discretion of KDE e.V.
   ( which shall act as a proxy as in section 14 of the GPLv3 ), any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
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
        emit urlsDropped(action, event);
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
            emit mouseButtonClicked(action, btn);
        }
        QMenu::mouseReleaseEvent(event);
    }
    m_mouseMoved = true;
}

} // namespace KDEPrivate

#include "moc_kurlnavigatormenu_p.cpp"
