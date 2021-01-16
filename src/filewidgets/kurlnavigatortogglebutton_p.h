/*
    SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KURLNAVIGATORTOGGLEBUTTON_P_H
#define KURLNAVIGATORTOGGLEBUTTON_P_H

#include "kurlnavigatorbuttonbase_p.h"
#include <QPixmap>

namespace KDEPrivate
{

/**
 * @brief Represents the button of the URL navigator to switch to
 *        the editable mode.
 *
 * A cursor is shown when hovering the button.
 */
class KUrlNavigatorToggleButton : public KUrlNavigatorButtonBase
{
    Q_OBJECT

public:
    explicit KUrlNavigatorToggleButton(KUrlNavigator *parent);
    ~KUrlNavigatorToggleButton() override;

    /** @see QWidget::sizeHint() */
    QSize sizeHint() const override;

protected:
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private Q_SLOTS:
    void updateToolTip();
    void updateCursor();

private:
    QPixmap m_pixmap;
};

} // namespace KDEPrivate

#endif
