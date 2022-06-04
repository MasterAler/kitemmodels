/*
    Copyright (c) 2009 Stephen Kelly <steveire@gmail.com>

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

#include "krecursivefilterproxymodel.h"
#include "krecursivefilterproxymodel_p.h"

KRecursiveFilterProxyModel::KRecursiveFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent), d_ptr(new KRecursiveFilterProxyModelPrivate(this))
{
    setDynamicSortFilter(true);
}

KRecursiveFilterProxyModel::~KRecursiveFilterProxyModel()
{ }

bool KRecursiveFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    // TODO: Implement some caching so that if one match is found on the first pass, we can return early results
    // when the subtrees are checked by QSFPM.
    if (acceptRow(sourceRow, sourceParent)) {
        return true;
    }

    QModelIndex source_index = sourceModel()->index(sourceRow, 0, sourceParent);
    Q_ASSERT(source_index.isValid());
    bool accepted = false;

    const int numChildren = sourceModel()->rowCount(source_index);
    for (int row = 0, rows = numChildren; row < rows; ++row) {
        if (filterAcceptsRow(row, source_index)) {
            accepted = true;
            break;
        }
    }

    return accepted;
}

QModelIndexList KRecursiveFilterProxyModel::match(const QModelIndex &start, int role, const QVariant &value, int hits, Qt::MatchFlags flags) const
{
    if (role < Qt::UserRole) {
        return QSortFilterProxyModel::match(start, role, value, hits, flags);
    }

    QModelIndexList list;
    if (!sourceModel())
        return list;

    QModelIndex proxyIndex;
    const auto lst = sourceModel()->match(mapToSource(start), role, value, hits, flags);
    for (const QModelIndex &idx : lst) {
        proxyIndex = mapFromSource(idx);
        if (proxyIndex.isValid()) {
            list << proxyIndex;
        }
    }

    return list;
}

bool KRecursiveFilterProxyModel::acceptRow(int sourceRow, const QModelIndex &sourceParent) const
{
    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

void KRecursiveFilterProxyModel::setSourceModel(QAbstractItemModel *model)
{
    Q_D(KRecursiveFilterProxyModel);
    // Standard disconnect of the previous source model, if present
    if (sourceModel()) {
        disconnect(sourceModel(), SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
                   d, SLOT(sourceDataChanged(QModelIndex,QModelIndex,QVector<int>)));

        disconnect(sourceModel(), SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)),
                   d, SLOT(sourceRowsAboutToBeInserted(QModelIndex,int,int)));

        disconnect(sourceModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                   d, SLOT(sourceRowsInserted(QModelIndex,int,int)));

        disconnect(sourceModel(), SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
                   d, SLOT(sourceRowsAboutToBeRemoved(QModelIndex,int,int)));

        disconnect(sourceModel(), SIGNAL(rowsRemoved(QModelIndex,int,int)),
                   d, SLOT(sourceRowsRemoved(QModelIndex,int,int)));
    }

    QSortFilterProxyModel::setSourceModel(model);

    // Disconnect in the QSortFilterProxyModel. These methods will be invoked manually
    // in invokeDataChanged, invokeRowsInserted etc.
    //
    // The reason for that is that when the source model adds new rows for example, the new rows
    // May not match the filter, but maybe their child items do match.
    //
    // Source model before insert:
    //
    // - A
    // - B
    // - - C
    // - - D
    // - - - E
    // - - - F
    // - - - G
    // - H
    // - I
    //
    // If the A F and L (which doesn't exist in the source model yet) match the filter
    // the proxy will be:
    //
    // - A
    // - B
    // - - D
    // - - - F
    //
    // New rows are inserted in the source model below H:
    //
    // - A
    // - B
    // - - C
    // - - D
    // - - - E
    // - - - F
    // - - - G
    // - H
    // - - J
    // - - K
    // - - - L
    // - I
    //
    // As L matches the filter, it should be part of the KRecursiveFilterProxyModel.
    //
    // - A
    // - B
    // - - D
    // - - - F
    // - H
    // - - K
    // - - - L
    //
    // when the QSortFilterProxyModel gets a notification about new rows in H, it only checks
    // J and K to see if they match, ignoring L, and therefore not adding it to the proxy.
    // To work around that, we make sure that the QSFPM slot which handles that change in
    // the source model (_q_sourceRowsAboutToBeInserted) does not get called directly.
    // Instead we connect the sourceModel signal to our own slot in *this (sourceRowsAboutToBeInserted)
    // Inside that method, the entire new subtree is queried (J, K *and* L) to see if there is a match,
    // then the relevant Q_SLOTS in QSFPM are invoked.
    // In the example above, we need to tell the QSFPM that H should be queried again to see if
    // it matches the filter. It did not before, because L did not exist before. Now it does. That is
    // achieved by telling the QSFPM that the data changed for H, which causes it to requery this class
    // to see if H matches the filter (which it now does as L now exists).
    // That is done in sourceRowsInserted.

    if (!model) {
        return;
    }

    disconnect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
               this, SLOT(_q_sourceDataChanged(QModelIndex,QModelIndex,QVector<int>)));

    disconnect(model, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)),
               this, SLOT(_q_sourceRowsAboutToBeInserted(QModelIndex,int,int)));

    disconnect(model, SIGNAL(rowsInserted(QModelIndex,int,int)),
               this, SLOT(_q_sourceRowsInserted(QModelIndex,int,int)));

    disconnect(model, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
               this, SLOT(_q_sourceRowsAboutToBeRemoved(QModelIndex,int,int)));

    disconnect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
               this, SLOT(_q_sourceRowsRemoved(QModelIndex,int,int)));

    // Slots for manual invoking of QSortFilterProxyModel methods.
    connect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
            d, SLOT(sourceDataChanged(QModelIndex,QModelIndex,QVector<int>)));

    connect(model, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)),
            d, SLOT(sourceRowsAboutToBeInserted(QModelIndex,int,int)));

    connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)),
            d, SLOT(sourceRowsInserted(QModelIndex,int,int)));

    connect(model, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
            d, SLOT(sourceRowsAboutToBeRemoved(QModelIndex,int,int)));

    connect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
            d, SLOT(sourceRowsRemoved(QModelIndex,int,int)));

}
