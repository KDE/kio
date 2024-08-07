/*
    SPDX-FileCopyrightText: 2007 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2019 Méven Car <meven.car@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KDIROPERATORICONVIEW_P_H
#define KDIROPERATORICONVIEW_P_H

#include <KDirOperator>
#include <QListView>

/*!
 * Default icon view for KDirOperator using
 * custom view options.
 *
 * \internal
 */
class KDirOperatorIconView : public QListView
{
    Q_OBJECT
public:
    KDirOperatorIconView(KDirOperator *dirOperator,
                         QWidget *parent = nullptr,
                         QStyleOptionViewItem::Position decorationPosition = QStyleOptionViewItem::Position::Top);
    ~KDirOperatorIconView() override;
    void setDecorationPosition(QStyleOptionViewItem::Position decorationPosition);

protected:
    void initViewItemOption(QStyleOptionViewItem *option) const override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

protected
    Q_SLOT : void updateLayout();

private:
    QStyleOptionViewItem::Position decorationPosition;
    bool m_isEmblemClicked;
    KDirOperator *m_dirOperator;
};

#endif // KDIROPERATORICONVIEW_P_H
