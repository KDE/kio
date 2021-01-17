/*
    SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurlnavigatorplacesselector_p.h"

#include <kfileplacesmodel.h>
#include <KUrlMimeData>

#include <QMimeDatabase>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QMimeData>

namespace KDEPrivate
{

KUrlNavigatorPlacesSelector::KUrlNavigatorPlacesSelector(KUrlNavigator *parent, KFilePlacesModel *placesModel) :
    KUrlNavigatorButtonBase(parent),
    m_selectedItem(-1),
    m_placesModel(placesModel)
{
    setFocusPolicy(Qt::NoFocus);

    m_placesMenu = new QMenu(this);
    m_placesMenu->installEventFilter(this);

    updateMenu();

    connect(m_placesModel, &KFilePlacesModel::reloaded,
            this, &KUrlNavigatorPlacesSelector::updateMenu);
    connect(m_placesMenu, &QMenu::triggered,
            this, &KUrlNavigatorPlacesSelector::activatePlace);

    setMenu(m_placesMenu);

    setAcceptDrops(true);
}

KUrlNavigatorPlacesSelector::~KUrlNavigatorPlacesSelector()
{
}

void KUrlNavigatorPlacesSelector::updateMenu()
{
    m_placesMenu->clear();

    // Submenus have to be deleted explicitly (QTBUG-11070)
    for(QObject *obj : QObjectList(m_placesMenu->children())) {
        delete qobject_cast<QMenu*>(obj); // Noop for nullptr
    }

    updateSelection(m_selectedUrl);

    QString previousGroup;
    QMenu *subMenu = nullptr;

    const int rowCount = m_placesModel->rowCount();
    for (int i = 0; i < rowCount; ++i) {
        QModelIndex index = m_placesModel->index(i, 0);
        if (m_placesModel->isHidden(index)) {
            continue;
        }

        QAction *placeAction = new QAction(m_placesModel->icon(index),
                                      m_placesModel->text(index),
                                      m_placesMenu);
        placeAction->setData(i);

        const QString &groupName = index.data(KFilePlacesModel::GroupRole).toString();
        if (previousGroup.isEmpty()) { // Skip first group heading.
            previousGroup = groupName;
        }

        // Put all subsequent categories into a submenu.
        if (previousGroup != groupName) {
            QAction *subMenuAction = new QAction(groupName, m_placesMenu);
            subMenu = new QMenu(m_placesMenu);
            subMenu->installEventFilter(this);
            subMenuAction->setMenu(subMenu);

            m_placesMenu->addAction(subMenuAction);

            previousGroup = groupName;
        }

        if (subMenu) {
            subMenu->addAction(placeAction);
        } else {
            m_placesMenu->addAction(placeAction);
        }

        if (i == m_selectedItem) {
            setIcon(m_placesModel->icon(index));
        }
    }

    updateTeardownAction();
}

void KUrlNavigatorPlacesSelector::updateTeardownAction()
{
    const QString teardownActionId = QStringLiteral("teardownAction");

    const auto actions = m_placesMenu->actions();
    for (QAction *action : actions) {
        if (action->data() == teardownActionId) {
            delete action;
        }
    }

    const QModelIndex index = m_placesModel->index(m_selectedItem, 0);
    QAction *teardown = m_placesModel->teardownActionForIndex(index);
    if (teardown) {
        QAction *separator = m_placesMenu->addSeparator();
        separator->setData(teardownActionId);

        teardown->setParent(m_placesMenu);
        teardown->setData(teardownActionId);
        m_placesMenu->addAction(teardown);
    }
}

void KUrlNavigatorPlacesSelector::updateSelection(const QUrl &url)
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
        setIcon(QIcon::fromTheme(QStringLiteral("folder")));
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

void KUrlNavigatorPlacesSelector::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    drawHoverBackground(&painter);

    // draw icon
    const QPixmap pixmap = icon().pixmap(QSize(22, 22).expandedTo(iconSize()), QIcon::Normal);
    style()->drawItemPixmap(&painter, rect(), Qt::AlignCenter, pixmap);
}

void KUrlNavigatorPlacesSelector::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        setDisplayHintEnabled(DraggedHint, true);
        event->acceptProposedAction();

        update();
    }
}

void KUrlNavigatorPlacesSelector::dragLeaveEvent(QDragLeaveEvent *event)
{
    KUrlNavigatorButtonBase::dragLeaveEvent(event);

    setDisplayHintEnabled(DraggedHint, false);
    update();
}

void KUrlNavigatorPlacesSelector::dropEvent(QDropEvent *event)
{
    setDisplayHintEnabled(DraggedHint, false);
    update();

    QMimeDatabase db;
    const QList<QUrl> urlList = KUrlMimeData::urlsFromMimeData(event->mimeData());
    for (const QUrl &url : urlList) {
        QMimeType mimetype = db.mimeTypeForUrl(url);
        if (mimetype.inherits(QStringLiteral("inode/directory"))) {
            m_placesModel->addPlace(url.fileName(), url);
        }
    }
}

void KUrlNavigatorPlacesSelector::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton && geometry().contains(event->pos())) {
        Q_EMIT tabRequested(KFilePlacesModel::convertedUrl(m_placesModel->url(m_placesModel->index(m_selectedItem, 0))));
        event->accept();
        return;
    }

    KUrlNavigatorButtonBase::mouseReleaseEvent(event);
}

void KUrlNavigatorPlacesSelector::activatePlace(QAction *action)
{
    Q_ASSERT(action != nullptr);
    if (action->data().toString() == QLatin1String("teardownAction")) {
        QModelIndex index = m_placesModel->index(m_selectedItem, 0);
        m_placesModel->requestTeardown(index);
        return;
    }

    QModelIndex index = m_placesModel->index(action->data().toInt(), 0);

    m_lastClickedIndex = QPersistentModelIndex();

    if (m_placesModel->setupNeeded(index)) {
        connect(m_placesModel, &KFilePlacesModel::setupDone,
                this, &KUrlNavigatorPlacesSelector::onStorageSetupDone);

        m_lastClickedIndex = index;
        m_placesModel->requestSetup(index);
        return;
    } else if (index.isValid()) {
        m_selectedItem = index.row();
        setIcon(m_placesModel->icon(index));
        updateTeardownAction();
        Q_EMIT placeActivated(KFilePlacesModel::convertedUrl(m_placesModel->url(index)));
    }
}

void KUrlNavigatorPlacesSelector::onStorageSetupDone(const QModelIndex &index, bool success)
{
    if (m_lastClickedIndex == index)  {
        if (success) {
            m_selectedItem = index.row();
            setIcon(m_placesModel->icon(index));
            updateTeardownAction();
            Q_EMIT placeActivated(KFilePlacesModel::convertedUrl(m_placesModel->url(index)));
        }
        m_lastClickedIndex = QPersistentModelIndex();
    }
}

bool KUrlNavigatorPlacesSelector::eventFilter(QObject *watched, QEvent *event)
{
    if (auto *menu = qobject_cast<QMenu *>(watched)) {
        if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::MiddleButton) {
                if (QAction *action = menu->activeAction()) {
                    m_placesMenu->close(); // always close top menu

                    QModelIndex index = m_placesModel->index(action->data().toInt(), 0);
                    Q_EMIT tabRequested(KFilePlacesModel::convertedUrl(m_placesModel->url(index)));
                    return true;
                }
            }
        }
    }

    return KUrlNavigatorButtonBase::eventFilter(watched, event);
}

} // namespace KDEPrivate

#include "moc_kurlnavigatorplacesselector_p.cpp"

