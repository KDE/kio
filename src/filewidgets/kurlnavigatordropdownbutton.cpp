/*****************************************************************************
 * Copyright (C) 2006 by Peter Penz <peter.penz@gmx.at>                      *
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

#include "kurlnavigatordropdownbutton_p.h"
#include "kurlnavigator.h"

#include <QKeyEvent>
#include <QPainter>
#include <QStyleOption>

namespace KDEPrivate
{

KUrlNavigatorDropDownButton::KUrlNavigatorDropDownButton(QWidget* parent) :
    KUrlNavigatorButtonBase(parent)
{
}

KUrlNavigatorDropDownButton::~KUrlNavigatorDropDownButton()
{
}

QSize KUrlNavigatorDropDownButton::sizeHint() const
{
    QSize size = KUrlNavigatorButtonBase::sizeHint();
    size.setWidth(size.height() / 2);
    return size;
}

void KUrlNavigatorDropDownButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    drawHoverBackground(&painter);

    const QColor fgColor = foregroundColor();

    QStyleOption option;
    option.initFrom(this);
    option.rect = QRect(0, 0, width(), height());
    option.palette = palette();
    option.palette.setColor(QPalette::Text, fgColor);
    option.palette.setColor(QPalette::WindowText, fgColor);
    option.palette.setColor(QPalette::ButtonText, fgColor);

    if (layoutDirection() == Qt::LeftToRight) {
        style()->drawPrimitive(QStyle::PE_IndicatorArrowRight, &option, &painter, this);
    } else {
        style()->drawPrimitive(QStyle::PE_IndicatorArrowLeft, &option, &painter, this);
    }
}

void KUrlNavigatorDropDownButton::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Down:
        emit clicked();
        break;
    default:
        KUrlNavigatorButtonBase::keyPressEvent(event);
    }
}

} // namespace KDEPrivate

#include "moc_kurlnavigatordropdownbutton_p.cpp"
