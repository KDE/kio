/*****************************************************************************
 * Copyright (C) 2018 Kai Uwe Broulik <kde@privat.broulik.de>                *
 *                                                                           *
 * This library is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU Library General Public               *
 * License as published by the Free Software Foundation; either              *
 * version 2 of the License, or (at your option) any later version.          *
 *                                                                           *
 * This library is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 * Library General Public License for more details.                          *
 *                                                                           *
 * You should have received a copy of the GNU Library General Public License *
 * along with this library; see the file COPYING.LIB.  If not, write to      *
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,      *
 * Boston, MA 02110-1301, USA.                                               *
 *****************************************************************************/

#ifndef KFILEWIDGETDOCKTITLEBAR_P_H
#define KFILEWIDGETDOCKTITLEBAR_P_H

#include <QWidget>

namespace KDEPrivate
{

/**
 * @brief An empty title bar for the Places dock widget
 */
class KFileWidgetDockTitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit KFileWidgetDockTitleBar(QWidget *parent);
    ~KFileWidgetDockTitleBar() Q_DECL_OVERRIDE;

    QSize minimumSizeHint() const Q_DECL_OVERRIDE;
    QSize sizeHint() const Q_DECL_OVERRIDE;
};

} // namespace KDEPrivate

#endif // KFILEWIDGETDOCKTITLEBAR_P_H
