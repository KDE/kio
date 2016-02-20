/*******************************************************************************
 *   Copyright (C) 2008 by Peter Penz <peter.penz@gmx.at>                      *
 *                                                                             *
 *   This library is free software; you can redistribute it and/or             *
 *   modify it under the terms of the GNU Library General Public               *
 *   License as published by the Free Software Foundation; either              *
 *   version 2 of the License, or (at your option) any later version.          *
 *                                                                             *
 *   This library is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 *   Library General Public License for more details.                          *
 *                                                                             *
 *   You should have received a copy of the GNU Library General Public License *
 *   along with this library; see the file COPYING.LIB.  If not, write to      *
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,      *
 *   Boston, MA 02110-1301, USA.                                               *
 *******************************************************************************/

#ifndef DEFAULTVIEWADAPTER_H
#define DEFAULTVIEWADAPTER_H

#include "kiofilewidgets_export.h"
#include "kabstractviewadapter.h"

class QAbstractItemView;

namespace KIO
{
/**
 * Implementation of the view adapter for the default case when
 * an instance of QAbstractItemView is used as view.
 */
class KIOFILEWIDGETS_EXPORT DefaultViewAdapter : public KAbstractViewAdapter
{
    Q_OBJECT
public:
    DefaultViewAdapter(QAbstractItemView *view, QObject *parent);
    QAbstractItemModel *model() const Q_DECL_OVERRIDE;
    QSize iconSize() const Q_DECL_OVERRIDE;
    QPalette palette() const Q_DECL_OVERRIDE;
    QRect visibleArea() const Q_DECL_OVERRIDE;
    QRect visualRect(const QModelIndex &index) const Q_DECL_OVERRIDE;
    void connect(Signal signal, QObject *receiver, const char *slot) Q_DECL_OVERRIDE;

private:
    QAbstractItemView *m_view;
};
}

#endif
