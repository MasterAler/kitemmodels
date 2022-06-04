/*
    Copyright (c) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
    Authors: David Faure <david.faure@kdab.com>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU Library General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This library is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
    License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to the
    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.
*/

#include "kconcatenaterowsproxymodel.h"
#include "kconcatenaterowsproxymodel_p.h"


KConcatenateRowsProxyModel::KConcatenateRowsProxyModel(QObject *parent)
    : QAbstractItemModel(parent),
      d_ptr(new KConcatenateRowsProxyModelPrivate(this))
{ }

KConcatenateRowsProxyModel::~KConcatenateRowsProxyModel()
{ }

QModelIndex KConcatenateRowsProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    Q_D(const KConcatenateRowsProxyModel);
    const QAbstractItemModel *sourceModel = sourceIndex.model();
    if (!sourceModel) {
        return {};
    }
    int rowsPrior = d->computeRowsPrior(sourceModel);
    return createIndex(rowsPrior + sourceIndex.row(), sourceIndex.column());
}

QModelIndex KConcatenateRowsProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    Q_D(const KConcatenateRowsProxyModel);
    if (!proxyIndex.isValid()) {
        return QModelIndex();
    }
    const int row = proxyIndex.row();
    int sourceRow;
    QAbstractItemModel *sourceModel = d->sourceModelForRow(row, &sourceRow);
    if (!sourceModel) {
        return QModelIndex();
    }
    return sourceModel->index(sourceRow, proxyIndex.column());
}

QVariant KConcatenateRowsProxyModel::data(const QModelIndex &index, int role) const
{
    const QModelIndex sourceIndex = mapToSource(index);
    if (!sourceIndex.isValid()) {
        return QVariant();
    }
    return sourceIndex.model()->data(sourceIndex, role);
}

bool KConcatenateRowsProxyModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    const QModelIndex sourceIndex = mapToSource(index);
    if (!sourceIndex.isValid()) {
        return false;
    }
    QAbstractItemModel *sourceModel = const_cast<QAbstractItemModel *>(sourceIndex.model());
    return sourceModel->setData(sourceIndex, value, role);
}

QMap<int, QVariant> KConcatenateRowsProxyModel::itemData(const QModelIndex &proxyIndex) const
{
    const QModelIndex sourceIndex = mapToSource(proxyIndex);
    if (!sourceIndex.isValid()) {
        return {};
    }
    return sourceIndex.model()->itemData(sourceIndex);
}

Qt::ItemFlags KConcatenateRowsProxyModel::flags(const QModelIndex &index) const
{
    const QModelIndex sourceIndex = mapToSource(index);
    return sourceIndex.isValid() ? sourceIndex.model()->flags(sourceIndex) : Qt::ItemFlags();
}

QVariant KConcatenateRowsProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_D(const KConcatenateRowsProxyModel);
    if (d->m_models.isEmpty()) {
        return QVariant();
    }
    if (orientation == Qt::Horizontal) {
        return d->m_models.at(0)->headerData(section, orientation, role);
    } else {
        int sourceRow;
        QAbstractItemModel *sourceModel = d->sourceModelForRow(section, &sourceRow);
        if (!sourceModel) {
            return QVariant();
        }
        return sourceModel->headerData(sourceRow, orientation, role);
    }
}

int KConcatenateRowsProxyModel::columnCount(const QModelIndex &parent) const
{
    Q_D(const KConcatenateRowsProxyModel);
    if (d->m_models.isEmpty()) {
        return 0;
    }
    if (parent.isValid()) {
        return 0; // flat model;
    }
    return d->m_models.at(0)->columnCount(QModelIndex());
}

QHash<int, QByteArray> KConcatenateRowsProxyModel::roleNames() const
{
    Q_D(const KConcatenateRowsProxyModel);
    if (d->m_models.isEmpty()) {
        return {};
    }
    return d->m_models.at(0)->roleNames();
}

QModelIndex KConcatenateRowsProxyModel::index(int row, int column, const QModelIndex &parent) const
{
    if(row < 0) {
        return {};
    }
    if(column < 0) {
        return {};
    }
    int sourceRow;

    Q_D(const KConcatenateRowsProxyModel);
    QAbstractItemModel *sourceModel = d->sourceModelForRow(row, &sourceRow);
    if (!sourceModel) {
        return QModelIndex();
    }
    return mapFromSource(sourceModel->index(sourceRow, column, parent));
}

QModelIndex KConcatenateRowsProxyModel::parent(const QModelIndex &) const
{
    return QModelIndex(); // we are flat, no hierarchy
}

int KConcatenateRowsProxyModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;    // flat model
    }

    Q_D(const KConcatenateRowsProxyModel);
    return d->m_rowCount;
}

void KConcatenateRowsProxyModel::addSourceModel(QAbstractItemModel* sourceModel)
{
    Q_D(KConcatenateRowsProxyModel);
    Q_ASSERT(sourceModel);
    Q_ASSERT(!d->m_models.contains(sourceModel));

    connect(sourceModel, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), d, SLOT(slotDataChanged(QModelIndex,QModelIndex,QVector<int>)));
    connect(sourceModel, SIGNAL(rowsInserted(QModelIndex,int,int)), d, SLOT(slotRowsInserted(QModelIndex,int,int)));
    connect(sourceModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), d, SLOT(slotRowsRemoved(QModelIndex,int,int)));
    connect(sourceModel, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)), d, SLOT(slotRowsAboutToBeInserted(QModelIndex,int,int)));
    connect(sourceModel, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)), d, SLOT(slotRowsAboutToBeRemoved(QModelIndex,int,int)));

    connect(sourceModel, SIGNAL(columnsInserted(QModelIndex,int,int)), d, SLOT(slotColumnsInserted(QModelIndex,int,int)));
    connect(sourceModel, SIGNAL(columnsRemoved(QModelIndex,int,int)), d, SLOT(slotColumnsRemoved(QModelIndex,int,int)));
    connect(sourceModel, SIGNAL(columnsAboutToBeInserted(QModelIndex,int,int)), d, SLOT(slotColumnsAboutToBeInserted(QModelIndex,int,int)));
    connect(sourceModel, SIGNAL(columnsAboutToBeRemoved(QModelIndex,int,int)), d, SLOT(slotColumnsAboutToBeRemoved(QModelIndex,int,int)));

    connect(sourceModel, SIGNAL(layoutAboutToBeChanged(QList<QPersistentModelIndex>,QAbstractItemModel::LayoutChangeHint)),
            d, SLOT(slotSourceLayoutAboutToBeChanged(QList<QPersistentModelIndex>,QAbstractItemModel::LayoutChangeHint)));
    connect(sourceModel, SIGNAL(layoutChanged(QList<QPersistentModelIndex>,QAbstractItemModel::LayoutChangeHint)),
            d, SLOT(slotSourceLayoutChanged(QList<QPersistentModelIndex>,QAbstractItemModel::LayoutChangeHint)));
    connect(sourceModel, SIGNAL(modelAboutToBeReset()), d, SLOT(slotModelAboutToBeReset()));
    connect(sourceModel, SIGNAL(modelReset()), d, SLOT(slotModelReset()));

    const int newRows = sourceModel->rowCount();
    if (newRows > 0) {
        beginInsertRows(QModelIndex(), d->m_rowCount, d->m_rowCount + newRows - 1);
    }

    d->m_rowCount += newRows;
    d->m_models.append(sourceModel);
    if (newRows > 0) {
        endInsertRows();
    }
}

QList<QAbstractItemModel*> KConcatenateRowsProxyModel::sources() const
{
    Q_D(const KConcatenateRowsProxyModel);
    return d->m_models;
}

void KConcatenateRowsProxyModel::removeSourceModel(QAbstractItemModel *sourceModel)
{
    Q_D(KConcatenateRowsProxyModel);
    Q_ASSERT(d->m_models.contains(sourceModel));
    disconnect(sourceModel, nullptr, this, nullptr);

    const int rowsRemoved = sourceModel->rowCount();
    const int rowsPrior = d->computeRowsPrior(sourceModel);   // location of removed section

    if (rowsRemoved > 0) {
        beginRemoveRows(QModelIndex(), rowsPrior, rowsPrior + rowsRemoved - 1);
    }
    d->m_models.removeOne(sourceModel);
    d->m_rowCount -= rowsRemoved;
    if (rowsRemoved > 0) {
        endRemoveRows();
    }
}
