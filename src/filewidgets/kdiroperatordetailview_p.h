/*
    SPDX-FileCopyrightText: 2007 Peter Penz <peter.penz@gmx.at>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KDIROPERATORDETAILVIEW_P_H
#define KDIROPERATORDETAILVIEW_P_H

#include <KDirOperator>
#include <QTreeView>

#include <kfile.h>

class QAbstractItemModel;

/**
 * Default detail view for KDirOperator using
 * custom resizing options and columns.
 */
class KDirOperatorDetailView : public QTreeView
{
    Q_OBJECT

public:
    explicit KDirOperatorDetailView(KDirOperator *dirOperator, QWidget *parent = nullptr);
    ~KDirOperatorDetailView() override;

    /**
     * Displays either Detail, Tree or DetailTree modes.
     */
    virtual bool setViewMode(KFile::FileView viewMode);

protected:
    void initViewItemOption(QStyleOptionViewItem *option) const override;

    bool event(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

private:
    bool m_hideDetailColumns;
    bool m_isEmblemClicked;
    KDirOperator *m_dirOperator;
};

#endif
