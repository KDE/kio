/*
    SPDX-FileCopyrightText: 2006-2010 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2006 Aaron J. Seigo <aseigo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurlnavigatorbuttonbase_p.h"

#include <KLocalizedString>
#include <KUrlNavigator>

#include <QStyle>
#include <QStyleOptionViewItem>

namespace KDEPrivate
{

KUrlNavigatorButtonBase::KUrlNavigatorButtonBase(KUrlNavigator *parent) :
    QPushButton(parent),
    m_active(true),
    m_displayHint(0)
{
    setFocusPolicy(Qt::TabFocus);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    setMinimumHeight(parent->minimumHeight());
    setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(this, &KUrlNavigatorButtonBase::pressed, parent, &KUrlNavigator::requestActivation);
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

void KUrlNavigatorButtonBase::enterEvent(QEvent *event)
{
    QPushButton::enterEvent(event);
    setDisplayHintEnabled(EnteredHint, true);
    update();
}

void KUrlNavigatorButtonBase::leaveEvent(QEvent *event)
{
    QPushButton::leaveEvent(event);
    setDisplayHintEnabled(EnteredHint, false);
    update();
}

void KUrlNavigatorButtonBase::drawHoverBackground(QPainter *painter)
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
        QStyleOptionViewItem option;
        option.initFrom(this);
        option.state = QStyle::State_Enabled | QStyle::State_MouseOver;
        option.viewItemPosition = QStyleOptionViewItem::OnlyOne;
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
