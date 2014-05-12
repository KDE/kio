/*
 * Copyright (c) 2009 Nick Shaforostoff <shaforostoff@kde.ru>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef IKWSOPTS_P_H
#define IKWSOPTS_P_H

#include <QAbstractTableModel>

class SearchProvider;

class ProvidersModel: public QAbstractTableModel
{
    Q_OBJECT
public:
    enum {Name,Shortcuts,Preferred,ColumnCount};
    explicit ProvidersModel(QObject* parent = 0): QAbstractTableModel(parent){}
    ~ProvidersModel();

    Qt::ItemFlags flags(const QModelIndex& index) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    bool setData (const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const{Q_UNUSED(parent); return ColumnCount;}

    void setProviders(const QList<SearchProvider*>&, const QStringList&);
    void setFavoriteProviders(const QStringList&);
    void addProvider(SearchProvider* p);
    void deleteProvider(SearchProvider* p);
    void changeProvider(SearchProvider* p);
    QStringList favoriteEngines() const;
    QList<SearchProvider*> providers() const{ return m_providers;}

    ///Creates new ProvidersListModel which directly uses data of this model.
    QAbstractListModel* createListModel();

Q_SIGNALS:
    void dataModified();

private:
    QSet<QString> m_favoriteEngines;
    QList<SearchProvider*> m_providers;
};

/**
 * A model for kcombobox of default search engine.
 * It is created via ProvidersModel::createListModel() and uses createListModel's data directly,
 * just forwarding all the signals
 */
class ProvidersListModel: public QAbstractListModel
{
    Q_OBJECT
public:
    enum{ShortNameRole = Qt::UserRole};

private:
    explicit ProvidersListModel(QList<SearchProvider*>& providers, QObject* parent = 0) ;

public:
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const;

public Q_SLOTS:
    void emitDataChanged(const QModelIndex& start, const QModelIndex& end){emit dataChanged(index(start.row(),0),index(end.row(),0));}
    void emitRowsAboutToBeInserted(const QModelIndex&, int start, int end){beginInsertRows(QModelIndex(),start,end);}
    void emitRowsAboutToBeRemoved(const QModelIndex&, int start, int end){beginRemoveRows(QModelIndex(),start,end);}
    void emitRowsInserted(const QModelIndex&, int , int){endInsertRows();}
    void emitRowsRemoved(const QModelIndex& , int , int){endRemoveRows();}

private:
    QList<SearchProvider*>& m_providers;

    friend class ProvidersModel;
};

#endif
