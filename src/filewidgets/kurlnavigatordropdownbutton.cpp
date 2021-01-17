/*
    SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurlnavigatordropdownbutton_p.h"
#include "kurlnavigator.h"

#include <QKeyEvent>
#include <QPainter>
#include <QStyleOption>

namespace KDEPrivate
{

KUrlNavigatorDropDownButton::KUrlNavigatorDropDownButton(KUrlNavigator *parent) :
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

void KUrlNavigatorDropDownButton::paintEvent(QPaintEvent *event)
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

void KUrlNavigatorDropDownButton::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Down:
        Q_EMIT clicked();
        break;
    default:
        KUrlNavigatorButtonBase::keyPressEvent(event);
    }
}

} // namespace KDEPrivate

#include "moc_kurlnavigatordropdownbutton_p.cpp"
