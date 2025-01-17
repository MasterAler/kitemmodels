/*
    Copyright (c) 2010 Stephen Kelly <steveire@gmail.com>

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

#include "kcheckableproxymodel.h"

#include <QItemSelectionModel>

class KCheckableProxyModelPrivate
{
    Q_DECLARE_PUBLIC(KCheckableProxyModel)
    KCheckableProxyModel *q_ptr;

    KCheckableProxyModelPrivate(KCheckableProxyModel *checkableModel)
        : q_ptr(checkableModel)
    {

    }

    QItemSelectionModel *m_itemSelectionModel = nullptr;

    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

};

KCheckableProxyModel::KCheckableProxyModel(QObject *parent)
    : QIdentityProxyModel(parent), d_ptr(new KCheckableProxyModelPrivate(this))
{

}

KCheckableProxyModel::~KCheckableProxyModel()
{ }

void KCheckableProxyModel::setSelectionModel(QItemSelectionModel *itemSelectionModel)
{
    Q_D(KCheckableProxyModel);
    d->m_itemSelectionModel = itemSelectionModel;
    Q_ASSERT(sourceModel() ? d->m_itemSelectionModel->model() == sourceModel() : true);
    connect(itemSelectionModel, &QItemSelectionModel::selectionChanged, [d](const QItemSelection &selected, const QItemSelection &deselected)
    { d->selectionChanged(selected, deselected); });
}

QItemSelectionModel *KCheckableProxyModel::selectionModel() const
{
    Q_D(const KCheckableProxyModel);
    return d->m_itemSelectionModel;
}

Qt::ItemFlags KCheckableProxyModel::flags(const QModelIndex &index) const
{
    if (!index.isValid() || index.column() != 0) {
        return QIdentityProxyModel::flags(index);
    }
    return QIdentityProxyModel::flags(index) | Qt::ItemIsUserCheckable;
}

QVariant KCheckableProxyModel::data(const QModelIndex &index, int role) const
{
    Q_D(const KCheckableProxyModel);

    if (role == Qt::CheckStateRole) {
        if (index.column() != 0) {
            return QVariant();
        }
        if (!d->m_itemSelectionModel) {
            return Qt::Unchecked;
        }

        return d->m_itemSelectionModel->selection().contains(mapToSource(index)) ? Qt::Checked : Qt::Unchecked;
    }
    return QIdentityProxyModel::data(index, role);
}

bool KCheckableProxyModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    Q_D(KCheckableProxyModel);
    if (role == Qt::CheckStateRole) {
        if (index.column() != 0) {
            return false;
        }
        if (!d->m_itemSelectionModel) {
            return false;
        }

        Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());
        const QModelIndex srcIndex = mapToSource(index);
        bool result = select(QItemSelection(srcIndex, srcIndex), state == Qt::Checked ? QItemSelectionModel::Select : QItemSelectionModel::Deselect);
        emit dataChanged(index, index);
        return result;
    }
    return QIdentityProxyModel::setData(index, value, role);
}

void KCheckableProxyModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    QIdentityProxyModel::setSourceModel(sourceModel);
    Q_ASSERT(d_ptr->m_itemSelectionModel ? d_ptr->m_itemSelectionModel->model() == sourceModel : true);
}

void KCheckableProxyModelPrivate::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_Q(KCheckableProxyModel);
    const auto lstSelected = q->mapSelectionFromSource(selected);
    for (const QItemSelectionRange &range : lstSelected) {
        emit q->dataChanged(range.topLeft(), range.bottomRight());
    }
    const auto lstDeselected = q->mapSelectionFromSource(deselected);
    for (const QItemSelectionRange &range : lstDeselected) {
        emit q->dataChanged(range.topLeft(), range.bottomRight());
    }
}

bool KCheckableProxyModel::select(const QItemSelection &selection, QItemSelectionModel::SelectionFlags command)
{
    Q_D(KCheckableProxyModel);
    d->m_itemSelectionModel->select(selection, command);
    return true;
}

