/***************************************************************************
 *   Copyright (C) 2006 by Peter Penz (peter.penz@gmx.at)                  *
 *   Copyright (C) 2007 by Kevin Ottens (ervin@kde.org)                    *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Lesser General Public            *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "kurlnavigatorplacesselector_p.h"

#include <kfileplacesmodel.h>
#include <kurlmimedata.h>

#include <qmimedatabase.h>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QMimeData>

namespace KDEPrivate
{

KUrlNavigatorPlacesSelector::KUrlNavigatorPlacesSelector(QWidget* parent, KFilePlacesModel* placesModel) :
    KUrlNavigatorButtonBase(parent),
    m_selectedItem(-1),
    m_placesModel(placesModel)
{
    setFocusPolicy(Qt::NoFocus);

    m_placesMenu = new QMenu(this);

    updateMenu();

    connect(m_placesModel, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(updateMenu()));
    connect(m_placesModel, SIGNAL(rowsRemoved(QModelIndex,int,int)),
            this, SLOT(updateMenu()));
    connect(m_placesModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
            this, SLOT(updateMenu()));
    connect(m_placesMenu, SIGNAL(triggered(QAction*)),
            this, SLOT(activatePlace(QAction*)));

    setMenu(m_placesMenu);

    setAcceptDrops(true);
}

KUrlNavigatorPlacesSelector::~KUrlNavigatorPlacesSelector()
{
}

void KUrlNavigatorPlacesSelector::updateMenu()
{
    m_placesMenu->clear();

    updateSelection(m_selectedUrl);
    const int rowCount = m_placesModel->rowCount();
    for (int i = 0; i < rowCount; ++i) {
        QModelIndex index = m_placesModel->index(i, 0);
        QAction* action = new QAction(m_placesModel->icon(index),
                                      m_placesModel->text(index),
                                      m_placesMenu);
        m_placesMenu->addAction(action);
        action->setData(i);
        if (i == m_selectedItem) {
            setIcon(m_placesModel->icon(index));
        }
    }

    updateTeardownAction();
}

void KUrlNavigatorPlacesSelector::updateTeardownAction()
{
    const int rowCount = m_placesModel->rowCount();
    if (m_placesMenu->actions().size()==rowCount+2) {
        // remove teardown action
        QAction *action = m_placesMenu->actions().at(rowCount+1);
        m_placesMenu->removeAction(action);
        delete action;

        // remove separator
        action = m_placesMenu->actions().at(rowCount);
        m_placesMenu->removeAction(action);
        delete action;
    }

    const QModelIndex index = m_placesModel->index(m_selectedItem, 0);
    QAction *teardown = m_placesModel->teardownActionForIndex(index);
    if (teardown!=0) {
        teardown->setParent(m_placesMenu);
        teardown->setData("teardownAction");

        m_placesMenu->addSeparator();
        m_placesMenu->addAction(teardown);
    }
}

void KUrlNavigatorPlacesSelector::updateSelection(const QUrl& url)
{
    const QModelIndex index = m_placesModel->closestItem(url);
    if (index.isValid()) {
        m_selectedItem = index.row();
        m_selectedUrl = url;
        setIcon(m_placesModel->icon(index));
    } else {
        m_selectedItem = -1;
        // No bookmark has been found which matches to the given Url. Show
        // a generic folder icon as pixmap for indication:
        setIcon(QIcon::fromTheme("folder"));
    }
    updateTeardownAction();
}

QUrl KUrlNavigatorPlacesSelector::selectedPlaceUrl() const
{
    const QModelIndex index = m_placesModel->index(m_selectedItem, 0);
    return index.isValid() ? m_placesModel->url(index) : QUrl();
}

QString KUrlNavigatorPlacesSelector::selectedPlaceText() const
{
    const QModelIndex index = m_placesModel->index(m_selectedItem, 0);
    return index.isValid() ? m_placesModel->text(index) : QString();
}

QSize KUrlNavigatorPlacesSelector::sizeHint() const
{
    const int height = KUrlNavigatorButtonBase::sizeHint().height();
    return QSize(height, height);
}

void KUrlNavigatorPlacesSelector::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    drawHoverBackground(&painter);

    // draw icon
    const QPixmap pixmap = icon().pixmap(QSize(22, 22).expandedTo(iconSize()), QIcon::Normal);
    const int x = (width() -  pixmap.width()) / 2;
    const int y = (height() - pixmap.height()) / 2;
    painter.drawPixmap(x, y, pixmap);
}

void KUrlNavigatorPlacesSelector::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        setDisplayHintEnabled(DraggedHint, true);
        event->acceptProposedAction();

        update();
    }
}

void KUrlNavigatorPlacesSelector::dragLeaveEvent(QDragLeaveEvent* event)
{
    KUrlNavigatorButtonBase::dragLeaveEvent(event);

    setDisplayHintEnabled(DraggedHint, false);
    update();
}

void KUrlNavigatorPlacesSelector::dropEvent(QDropEvent* event)
{
    setDisplayHintEnabled(DraggedHint, false);
    update();

    QMimeDatabase db;
    const QList<QUrl> urlList = KUrlMimeData::urlsFromMimeData(event->mimeData());
    foreach(const QUrl &url, urlList) {
        QMimeType mimetype = db.mimeTypeForUrl(url);
        if (mimetype.inherits("inode/directory")) {
            m_placesModel->addPlace(url.fileName(), url);
        }
    }
}

void KUrlNavigatorPlacesSelector::activatePlace(QAction* action)
{
    Q_ASSERT(action != 0);
    if (action->data().toString()=="teardownAction") {
        QModelIndex index = m_placesModel->index(m_selectedItem, 0);
        m_placesModel->requestTeardown(index);
        return;
    }

    QModelIndex index = m_placesModel->index(action->data().toInt(), 0);

    m_lastClickedIndex = QPersistentModelIndex();

    if (m_placesModel->setupNeeded(index)) {
        connect(m_placesModel, SIGNAL(setupDone(QModelIndex,bool)),
                this, SLOT(onStorageSetupDone(QModelIndex,bool)));

        m_lastClickedIndex = index;
        m_placesModel->requestSetup(index);
        return;
    } else if (index.isValid()) {
        m_selectedItem = index.row();
        setIcon(m_placesModel->icon(index));
        updateTeardownAction();
        emit placeActivated(m_placesModel->url(index));
    }
}

void KUrlNavigatorPlacesSelector::onStorageSetupDone(const QModelIndex &index, bool success)
{
    if (m_lastClickedIndex == index)  {
        if (success) {
            m_selectedItem = index.row();
            setIcon(m_placesModel->icon(index));
            updateTeardownAction();
            emit placeActivated(m_placesModel->url(index));
        }
        m_lastClickedIndex = QPersistentModelIndex();
    }
}

} // namespace KDEPrivate

#include "moc_kurlnavigatorplacesselector_p.cpp"

