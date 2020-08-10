/*
    SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

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
