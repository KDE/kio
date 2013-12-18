/*****************************************************************************
 * Copyright (C) 2006-2010 by Peter Penz <peter.penz@gmx.at>                 *
 * Copyright (C) 2006 by Aaron J. Seigo <aseigo@kde.org>                     *
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

#include "kurlnavigatorbuttonbase_p.h"

#include <klocalizedstring.h>

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QStyle>
#include <QStyleOptionFocusRect>

namespace KDEPrivate
{

KUrlNavigatorButtonBase::KUrlNavigatorButtonBase(QWidget* parent) :
    QPushButton(parent),
    m_active(true),
    m_displayHint(0)
{
    setFocusPolicy(Qt::TabFocus);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    setMinimumHeight(parent->minimumHeight());

    connect(this, SIGNAL(pressed()), parent, SLOT(requestActivation()));
}

KUrlNavigatorButtonBase::~KUrlNavigatorButtonBase()
{
}

void KUrlNavigatorButtonBase::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        update();
    }
}

bool KUrlNavigatorButtonBase::isActive() const
{
    return m_active;
}

void KUrlNavigatorButtonBase::setDisplayHintEnabled(DisplayHint hint,
                                       bool enable)
{
    if (enable) {
        m_displayHint = m_displayHint | hint;
    } else {
        m_displayHint = m_displayHint & ~hint;
    }
    update();
}

bool KUrlNavigatorButtonBase::isDisplayHintEnabled(DisplayHint hint) const
{
    return (m_displayHint & hint) > 0;
}

void KUrlNavigatorButtonBase::focusInEvent(QFocusEvent *event)
{
    setDisplayHintEnabled(EnteredHint, true);
    QPushButton::focusInEvent(event);
}

void KUrlNavigatorButtonBase::focusOutEvent(QFocusEvent *event)
{
    setDisplayHintEnabled(EnteredHint, false);
    QPushButton::focusOutEvent(event);
}

void KUrlNavigatorButtonBase::enterEvent(QEvent* event)
{
    QPushButton::enterEvent(event);
    setDisplayHintEnabled(EnteredHint, true);
    update();
}

void KUrlNavigatorButtonBase::leaveEvent(QEvent* event)
{
    QPushButton::leaveEvent(event);
    setDisplayHintEnabled(EnteredHint, false);
    update();
}

void KUrlNavigatorButtonBase::drawHoverBackground(QPainter* painter)
{
    const bool isHighlighted = isDisplayHintEnabled(EnteredHint) ||
                               isDisplayHintEnabled(DraggedHint) ||
                               isDisplayHintEnabled(PopupActiveHint);

    QColor backgroundColor = isHighlighted ? palette().color(QPalette::Highlight) : Qt::transparent;
    if (!m_active && isHighlighted) {
        backgroundColor.setAlpha(128);
    }

    if (backgroundColor != Qt::transparent) {
        // TODO: the backgroundColor should be applied to the style
        QStyleOptionViewItemV4 option;
        option.initFrom(this);
        option.state = QStyle::State_Enabled | QStyle::State_MouseOver;
        option.viewItemPosition = QStyleOptionViewItemV4::OnlyOne;
        style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, this);
    }
}

QColor KUrlNavigatorButtonBase::foregroundColor() const
{
    const bool isHighlighted = isDisplayHintEnabled(EnteredHint) ||
                               isDisplayHintEnabled(DraggedHint) ||
                               isDisplayHintEnabled(PopupActiveHint);

    QColor foregroundColor = palette().color(foregroundRole());

    int alpha = m_active ? 255 : 128;
    if (!m_active && !isHighlighted) {
        alpha -= alpha / 4;
    }
    foregroundColor.setAlpha(alpha);

    return foregroundColor;
}

void KUrlNavigatorButtonBase::activate()
{
    setActive(true);
}

} // namespace KDEPrivate

#include "moc_kurlnavigatorbuttonbase_p.cpp"
