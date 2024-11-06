/*
    SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KURLNAVIGATORDROPDOWNBUTTON_P_H
#define KURLNAVIGATORDROPDOWNBUTTON_P_H

#include "kurlnavigatorbuttonbase_p.h"

class KUrlNavigator;

namespace KDEPrivate
{
/**
 * @brief Button of the URL navigator which offers a drop down menu
 *        of the hidden portion of the path.
 *
 * The button will only be shown if the width of the URL navigator is
 * too small to show the whole path or if some part of the path is
 * expected to be a known location like "home".
 */
class KUrlNavigatorDropDownButton : public KUrlNavigatorButtonBase
{
    Q_OBJECT

public:
    explicit KUrlNavigatorDropDownButton(KUrlNavigator *parent);
    ~KUrlNavigatorDropDownButton() override;

    /** @see QWidget::sizeHint() */
    QSize sizeHint() const override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
};

} // namespace KDEPrivate

#endif
