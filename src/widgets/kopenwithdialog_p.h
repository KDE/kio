/* This file is part of the KDE libraries
    Copyright (C) 2000 David Faure <faure@kde.org>
    Copyright (C) 2007 Pino Toscano <pino@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef OPENWITHDIALOG_P_H
#define OPENWITHDIALOG_P_H

#include <QtCore/QAbstractItemModel>
#include <QTreeView>

class KApplicationModelPrivate;

/**
 * @internal
 */
class KApplicationModel : public QAbstractItemModel
{
    Q_OBJECT

    public:
        KApplicationModel(QObject *parent = 0);
        virtual ~KApplicationModel();
        virtual bool canFetchMore(const QModelIndex &parent) const;
        virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
        virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
        virtual void fetchMore(const QModelIndex &parent);
//        virtual Qt::ItemFlags flags(const QModelIndex &index) const;
        virtual bool hasChildren(const QModelIndex &parent = QModelIndex()) const;
        virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
        virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
        virtual QModelIndex parent(const QModelIndex &index) const;
        virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;

        QString entryPathFor(const QModelIndex &index) const;
        QString execFor(const QModelIndex &index) const;
        bool isDirectory(const QModelIndex &index) const;

    private:
        friend class KApplicationModelPrivate;
        KApplicationModelPrivate *const d;

        Q_DISABLE_COPY(KApplicationModel)
};

class KApplicationViewPrivate;

/**
 * @internal
 */
class KApplicationView : public QTreeView
{
    Q_OBJECT

    public:
        KApplicationView(QWidget *parent = 0);
        ~KApplicationView();

        virtual void setModel(QAbstractItemModel *model);

        bool isDirSel() const;

    Q_SIGNALS:
        void selected(const QString &_name, const QString &_exec);
        void highlighted(const QString &_name, const QString &_exec);

    protected Q_SLOTS:
        virtual void currentChanged(const QModelIndex &current, const QModelIndex &previous);

    private Q_SLOTS:
        void slotSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

    private:
        friend class KApplicationViewPrivate;
        KApplicationViewPrivate *const d;

        Q_DISABLE_COPY(KApplicationView)
};

#endif
