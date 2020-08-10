/*
    SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KFILEWIDGETDOCKTITLEBAR_P_H
#define KFILEWIDGETDOCKTITLEBAR_P_H

#include <QWidget>

namespace KDEPrivate
{

/**
 * @brief An empty title bar for the Places dock widget
 */
class KFileWidgetDockTitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit KFileWidgetDockTitleBar(QWidget *parent);
    ~KFileWidgetDockTitleBar() override;

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
};

} // namespace KDEPrivate

#endif // KFILEWIDGETDOCKTITLEBAR_P_H
