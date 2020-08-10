/*
    SPDX-FileCopyrightText: 2008 Peter Penz <peter.penz@gmx.at>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef DEFAULTVIEWADAPTER_H
#define DEFAULTVIEWADAPTER_H

#include "kiofilewidgets_export.h"
#include "kabstractviewadapter.h"

class QAbstractItemView;

namespace KIO
{
/**
 * Implementation of the view adapter for the default case when
 * an instance of QAbstractItemView is used as view.
 */
class KIOFILEWIDGETS_EXPORT DefaultViewAdapter : public KAbstractViewAdapter
{
    Q_OBJECT
public:
    DefaultViewAdapter(QAbstractItemView *view, QObject *parent);
    QAbstractItemModel *model() const override;
    QSize iconSize() const override;
    QPalette palette() const override;
    QRect visibleArea() const override;
    QRect visualRect(const QModelIndex &index) const override;
    void connect(Signal signal, QObject *receiver, const char *slot) override;

private:
    QAbstractItemView *m_view;
};
}

#endif
