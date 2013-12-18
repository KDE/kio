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

#include <QKeyEvent>
#include <QMimeData>

namespace KDEPrivate
{

KUrlNavigatorMenu::KUrlNavigatorMenu(QWidget* parent) :
    QMenu(parent)
{
    setAcceptDrops(true);
}

KUrlNavigatorMenu::~KUrlNavigatorMenu()
{
}

void KUrlNavigatorMenu::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void KUrlNavigatorMenu::dragMoveEvent(QDragMoveEvent* event)
{
      QMouseEvent mouseEvent(QMouseEvent(QEvent::MouseMove, event->pos(),
          Qt::LeftButton, event->mouseButtons(), event->keyboardModifiers()));
      mouseMoveEvent(&mouseEvent);
}

void KUrlNavigatorMenu::dropEvent(QDropEvent* event)
{
    QAction* action = actionAt(event->pos());
    if (action != 0) {
        emit urlsDropped(action, event);
    }
}

void KUrlNavigatorMenu::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MidButton) {
        QAction* action = actionAt(event->pos());
        if (action != 0) {
            emit middleMouseButtonClicked(action);
        }
    }
    QMenu::mouseReleaseEvent(event);
}

} // namespace KDEPrivate

#include "moc_kurlnavigatormenu_p.cpp"
