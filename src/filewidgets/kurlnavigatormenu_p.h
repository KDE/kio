/*
    SPDX-FileCopyrightText: 2009 Rahman Duran <rahman.duran@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KURLNAVIGATORMENU_P_H
#define KURLNAVIGATORMENU_P_H

#include <QMenu>

namespace KDEPrivate
{

/**
 * @brief Provides drop-down menus for the URL navigator.
 *
 * The implementation extends QMenu with drag & drop support.
 *
 * @internal
 */
class KUrlNavigatorMenu : public QMenu
{
    Q_OBJECT

public:
    explicit KUrlNavigatorMenu(QWidget *parent);
    ~KUrlNavigatorMenu() override;

Q_SIGNALS:
    /**
     * Is emitted when drop event occurs.
     */
    void urlsDropped(QAction *action, QDropEvent *event);

    /**
     * Is emitted, if the action \p action has been clicked.
     */
    void mouseButtonClicked(QAction *action, Qt::MouseButton button);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    const QPoint m_initialMousePosition;
    bool m_mouseMoved;
};

} // namespace KDEPrivate

#endif
