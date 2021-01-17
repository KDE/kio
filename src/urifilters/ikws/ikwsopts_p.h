/*
    SPDX-FileCopyrightText: 2009 Nick Shaforostoff <shaforostoff@kde.ru>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef IKWSOPTS_P_H
#define IKWSOPTS_P_H

#include <QAbstractTableModel>
#include <QSet>

class SearchProvider;

class ProvidersModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum {Name, Shortcuts, Preferred, ColumnCount};
    explicit ProvidersModel(QObject *parent = nullptr) : QAbstractTableModel(parent)
    {
    }

    ~ProvidersModel();

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        Q_UNUSED(parent);
        return ColumnCount;
    }

    void setProviders(const QList<SearchProvider *> &, const QStringList &);
    void setFavoriteProviders(const QStringList &);
    void addProvider(SearchProvider *p);
    void deleteProvider(SearchProvider *p);
    void changeProvider(SearchProvider *p);
    QStringList favoriteEngines() const;
    QList<SearchProvider *> providers() const
    {
        return m_providers;
    }

    ///Creates new ProvidersListModel which directly uses data of this model.
    QAbstractListModel *createListModel();

Q_SIGNALS:
    void dataModified();

private:
    QSet<QString> m_favoriteEngines;
    QList<SearchProvider *> m_providers;
};

/**
 * A model for kcombobox of default search engine.
 * It is created via ProvidersModel::createListModel() and uses createListModel's data directly,
 * just forwarding all the signals
 */
class ProvidersListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum {ShortNameRole = Qt::UserRole};

private:
    explicit ProvidersListModel(QList<SearchProvider *> &providers, QObject *parent = nullptr);

public:
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public Q_SLOTS:
    void emitDataChanged(const QModelIndex &start, const QModelIndex &end)
    {
        Q_EMIT dataChanged(index(start.row(), 0), index(end.row(), 0));
    }

    void emitRowsAboutToBeInserted(const QModelIndex &, int start, int end)
    {
        beginInsertRows(QModelIndex(), start, end);
    }

    void emitRowsAboutToBeRemoved(const QModelIndex &, int start, int end)
    {
        beginRemoveRows(QModelIndex(), start, end);
    }

    void emitRowsInserted(const QModelIndex &, int, int)
    {
        endInsertRows();
    }

    void emitRowsRemoved(const QModelIndex &, int, int)
    {
        endRemoveRows();
    }

private:
    QList<SearchProvider *> &m_providers;

    friend class ProvidersModel;
};

#endif
