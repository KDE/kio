/*
    SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurlnavigatortogglebutton_p.h"

#include <KLocalizedString>

#include <QPainter>
#include <QPaintEvent>
#include <QStyle>

namespace KDEPrivate
{

KUrlNavigatorToggleButton::KUrlNavigatorToggleButton(KUrlNavigator *parent) :
    KUrlNavigatorButtonBase(parent)
{
    setCheckable(true);
    connect(this, &QAbstractButton::toggled,
            this, &KUrlNavigatorToggleButton::updateToolTip);
    connect(this, &QAbstractButton::clicked,
            this, &KUrlNavigatorToggleButton::updateCursor);
    m_pixmap = QIcon::fromTheme(QStringLiteral("dialog-ok")).pixmap(QSize(22, 22).expandedTo(iconSize()));

#ifndef QT_NO_ACCESSIBILITY
    setAccessibleName(i18n("Edit mode"));
#endif

    updateToolTip();
}

KUrlNavigatorToggleButton::~KUrlNavigatorToggleButton()
{
}

QSize KUrlNavigatorToggleButton::sizeHint() const
{
    QSize size = KUrlNavigatorButtonBase::sizeHint();
    size.setWidth(m_pixmap.width() + 4);
    return size;
}

void KUrlNavigatorToggleButton::enterEvent(QEvent *event)
{
    KUrlNavigatorButtonBase::enterEvent(event);
    updateCursor();
}

void KUrlNavigatorToggleButton::leaveEvent(QEvent *event)
{
    KUrlNavigatorButtonBase::leaveEvent(event);
    setCursor(Qt::ArrowCursor);
}

void KUrlNavigatorToggleButton::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setClipRect(event->rect());

    const int buttonWidth = width();
    const int buttonHeight = height();
    if (isChecked()) {
        drawHoverBackground(&painter);
        style()->drawItemPixmap(&painter, rect(), Qt::AlignCenter, m_pixmap);
    } else if (isDisplayHintEnabled(EnteredHint)) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(foregroundRole()));

        const int verticalGap = 4;
        const int caretWidth = 2;
        const int x = (layoutDirection() == Qt::LeftToRight) ? 0 : buttonWidth - caretWidth;
        painter.drawRect(x, verticalGap, caretWidth, buttonHeight - 2 * verticalGap);
    }
}

void KUrlNavigatorToggleButton::updateToolTip()
{
    if (isChecked()) {
        setToolTip(i18n("Click for Location Navigation"));
    } else {
        setToolTip(i18n("Click to Edit Location"));
    }
}

void KUrlNavigatorToggleButton::updateCursor()
{
    setCursor(isChecked() ? Qt::ArrowCursor : Qt::IBeamCursor);
}

} // namespace KDEPrivate

#include "moc_kurlnavigatortogglebutton_p.cpp"
