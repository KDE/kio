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

#include "kfilewidgetdocktitlebar_p.h"

#include <QStyle>

using namespace KDEPrivate;

KFileWidgetDockTitleBar::KFileWidgetDockTitleBar(QWidget *parent) : QWidget(parent)
{
}

KFileWidgetDockTitleBar::~KFileWidgetDockTitleBar()
{
}

QSize KFileWidgetDockTitleBar::minimumSizeHint() const
{
    const int border = style()->pixelMetric(QStyle::PM_DockWidgetTitleBarButtonMargin);
    return QSize(border, border);
}

QSize KFileWidgetDockTitleBar::sizeHint() const
{
    return minimumSizeHint();
}

#include "moc_kfilewidgetdocktitlebar_p.cpp"
