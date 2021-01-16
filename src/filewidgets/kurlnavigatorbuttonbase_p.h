/*
    SPDX-FileCopyrightText: 2006-2010 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2006 Aaron J. Seigo <aseigo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KURLNAVIGATORBUTTONBASE_P_H
#define KURLNAVIGATORBUTTONBASE_P_H

#include <QColor>
#include <QPushButton>

class QUrl;
class QEvent;

class KUrlNavigator;

namespace KDEPrivate
{

/**
 * @brief Base class for buttons of the URL navigator.
 *
 * Buttons of the URL navigator offer an active/inactive
 * state and custom display hints.
 */
class KUrlNavigatorButtonBase : public QPushButton
{
    Q_OBJECT

public:
    explicit KUrlNavigatorButtonBase(KUrlNavigator *parent);
    virtual ~KUrlNavigatorButtonBase();

    /**
     * When having several URL navigator instances, it is important
     * to provide a visual difference to indicate which URL navigator
     * is active (usecase: split view in Dolphin). The activation state
     * is independent from the focus or hover state.
     * Per default the URL navigator button is marked as active.
     */
    void setActive(bool active);
    bool isActive() const;

protected:
    enum DisplayHint {
        EnteredHint = 1,
        DraggedHint = 2,
        PopupActiveHint = 4,
    };

    enum { BorderWidth = 2 };

    void setDisplayHintEnabled(DisplayHint hint, bool enable);
    bool isDisplayHintEnabled(DisplayHint hint) const;

    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;

    void drawHoverBackground(QPainter *painter);

    /** Returns the foreground color by respecting the current display hint. */
    QColor foregroundColor() const;

private Q_SLOTS:
    /** Invokes setActive(true). */
    void activate();

private:
    bool m_active;
    int m_displayHint;
};

} // namespace KDEPrivate

#endif
