/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef SHIFTALTERNATIVE_P_H
#define SHIFTALTERNATIVE_P_H

#include <QAction>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QObject>
#include <QString>

namespace FileWidgetsPrivate
{

/*
 * Tells the user what holding the Shift key down does to an action. The action shows a second text
 * while Shift is held, and carries a tooltip saying so. Both last as long as this object, so it is
 * meant to be parented to the action itself.
 *
 * The menu the action belongs to is watched for the Shift key, which it receives while it is open
 * because a menu holds the keyboard grab.
 *
 * An application shows the tooltip of a menu entry if it wants to, KXmlGui offers KToolTipHelper
 * for that.
 */
class ShiftAlternative : public QObject
{
public:
    ShiftAlternative(QAction *action, QMenu *menu, const QString &shiftText, const QString &toolTip)
        : QObject(action)
        , m_action(action)
        , m_text(action->text())
        , m_shiftText(shiftText)
    {
        m_action->setToolTip(toolTip);
        // The menu can be opened with Shift already held down.
        updateText(qGuiApp->keyboardModifiers() & Qt::ShiftModifier);
        menu->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Shift) {
                updateText(event->type() == QEvent::KeyPress);
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void updateText(bool shiftPressed)
    {
        m_action->setText(shiftPressed ? m_shiftText : m_text);
    }

    QAction *const m_action;
    const QString m_text;
    const QString m_shiftText;
};

} // namespace FileWidgetsPrivate

#endif // SHIFTALTERNATIVE_P_H
