/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2007 Pino Toscano <pino@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef OPENWITHDIALOG_P_H
#define OPENWITHDIALOG_P_H

#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QTreeView>

class KApplicationModelPrivate;

/**
 * @internal
 */
class KApplicationModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit KApplicationModel(QObject *parent = nullptr);
    virtual ~KApplicationModel();
    bool canFetchMore(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    void fetchMore(const QModelIndex &parent) override;
//        Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QString entryPathFor(const QModelIndex &index) const;
    QString execFor(const QModelIndex &index) const;
    bool isDirectory(const QModelIndex &index) const;
    void fetchAll(const QModelIndex &parent);

private:
    friend class KApplicationModelPrivate;
    KApplicationModelPrivate *const d;

    Q_DISABLE_COPY(KApplicationModel)
};

/**
 * @internal
 */
class QTreeViewProxyFilter : public QSortFilterProxyModel
{
     Q_OBJECT

public:
    explicit QTreeViewProxyFilter(QObject *parent = nullptr);
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
};

class KApplicationViewPrivate;

/**
 * @internal
 */
class KApplicationView : public QTreeView
{
    Q_OBJECT

public:
    explicit KApplicationView(QWidget *parent = nullptr);
    ~KApplicationView();

    void setModels(KApplicationModel *model, QSortFilterProxyModel *proxyModel);
    QSortFilterProxyModel* proxyModel();

    bool isDirSel() const;

Q_SIGNALS:
    void selected(const QString &_name, const QString &_exec);
    void highlighted(const QString &_name, const QString &_exec);

protected Q_SLOTS:
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

private Q_SLOTS:
    void slotSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

private:
    friend class KApplicationViewPrivate;
    KApplicationViewPrivate *const d;

    Q_DISABLE_COPY(KApplicationView)
};

#endif
