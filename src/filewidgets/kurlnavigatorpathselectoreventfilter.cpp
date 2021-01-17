/*
    SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kurlnavigatorpathselectoreventfilter_p.h"

#include <QEvent>
#include <QMenu>
#include <QMouseEvent>

using namespace KDEPrivate;

KUrlNavigatorPathSelectorEventFilter::KUrlNavigatorPathSelectorEventFilter(QObject *parent)
    : QObject(parent)
{

}

KUrlNavigatorPathSelectorEventFilter::~KUrlNavigatorPathSelectorEventFilter()
{

}

bool KUrlNavigatorPathSelectorEventFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::MiddleButton) {
            if (QMenu *menu = qobject_cast<QMenu *>(watched)) {
                if (QAction *action = menu->activeAction()) {
                    const QUrl url(action->data().toString());
                    if (url.isValid()) {
                        menu->close();

                        Q_EMIT tabRequested(url);
                        return true;
                    }
                }
            }
        }
    }

    return QObject::eventFilter(watched, event);
}

#include "moc_kurlnavigatorpathselectoreventfilter_p.cpp"
