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

#ifndef KURLNAVIGATORDROPDOWNBUTTON_P_H
#define KURLNAVIGATORDROPDOWNBUTTON_P_H

#include "kurlnavigatorbuttonbase_p.h"

namespace KDEPrivate
{

/**
 * @brief Button of the URL navigator which offers a drop down menu
 *        of hidden paths.
 *
 * The button will only be shown if the width of the URL navigator is
 * too small to show the whole path.
 */
class KUrlNavigatorDropDownButton : public KUrlNavigatorButtonBase
{
    Q_OBJECT

public:
    explicit KUrlNavigatorDropDownButton(QWidget* parent);
    virtual ~KUrlNavigatorDropDownButton();

    /** @see QWidget::sizeHint() */
    virtual QSize sizeHint() const;

protected:
    virtual void keyPressEvent(QKeyEvent* event);
    virtual void paintEvent(QPaintEvent* event);
};

} // namespace KDEPrivate

#endif
