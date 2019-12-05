/*
    Copyright (c) 2009 Stephen Kelly <steveire@gmail.com>
    Copyright (c) 2016 Ableton AG <info@ableton.com>
        Author Stephen Kelly <stephen.kelly@ableton.com>

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

#include "kselectionproxymodel.h"
#include "kselectionproxymodel_p.h"

KSelectionProxyModel::KSelectionProxyModel(QItemSelectionModel *selectionModel, QObject *parent)
    : QAbstractProxyModel(parent), d_ptr(new KSelectionProxyModelPrivate(this))
{
    setSelectionModel(selectionModel);
}

KSelectionProxyModel::KSelectionProxyModel()
    : QAbstractProxyModel(nullptr), d_ptr(new KSelectionProxyModelPrivate(this))
{
}

KSelectionProxyModel::~KSelectionProxyModel()
{ }

void KSelectionProxyModel::setFilterBehavior(FilterBehavior behavior)
{
    Q_D(KSelectionProxyModel);

    Q_ASSERT(behavior != InvalidBehavior);
    if (behavior == InvalidBehavior) {
        return;
    }
    if (d->m_filterBehavior != behavior) {
        beginResetModel();

        d->m_filterBehavior = behavior;

        switch (behavior) {
        case InvalidBehavior: {
            Q_ASSERT(!"InvalidBehavior can't be used here");
            return;
        }
        case SubTrees: {
            d->m_omitChildren = false;
            d->m_omitDescendants = false;
            d->m_startWithChildTrees = false;
            d->m_includeAllSelected = false;
            break;
        }
        case SubTreeRoots: {
            d->m_omitChildren = true;
            d->m_startWithChildTrees = false;
            d->m_includeAllSelected = false;
            break;
        }
        case SubTreesWithoutRoots: {
            d->m_omitChildren = false;
            d->m_omitDescendants = false;
            d->m_startWithChildTrees = true;
            d->m_includeAllSelected = false;
            break;
        }
        case ExactSelection: {
            d->m_omitChildren = true;
            d->m_startWithChildTrees = false;
            d->m_includeAllSelected = true;
            break;
        }
        case ChildrenOfExactSelection: {
            d->m_omitChildren = false;
            d->m_omitDescendants = true;
            d->m_startWithChildTrees = true;
            d->m_includeAllSelected = true;
            break;
        }
        }
        emit filterBehaviorChanged();
        d->resetInternalData();
        if (d->m_selectionModel) {
            d->selectionChanged(d->m_selectionModel->selection(), QItemSelection());
        }

        endResetModel();
    }
}

KSelectionProxyModel::FilterBehavior KSelectionProxyModel::filterBehavior() const
{
    Q_D(const KSelectionProxyModel);
    return d->m_filterBehavior;
}

void KSelectionProxyModel::setSourceModel(QAbstractItemModel *_sourceModel)
{
    Q_D(KSelectionProxyModel);

    Q_ASSERT(_sourceModel != this);

    if (_sourceModel == sourceModel()) {
        return;
    }

    beginResetModel();
    d->m_resetting = true;

    static const char *const modelSignals[] = {
        SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)),
        SIGNAL(rowsInserted(QModelIndex,int,int)),
        SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
        SIGNAL(rowsRemoved(QModelIndex,int,int)),
        SIGNAL(rowsAboutToBeMoved(QModelIndex,int,int,QModelIndex,int)),
        SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
        SIGNAL(modelAboutToBeReset()),
        SIGNAL(modelReset()),
        SIGNAL(dataChanged(QModelIndex,QModelIndex)),
        SIGNAL(layoutAboutToBeChanged()),
        SIGNAL(layoutChanged()),
        SIGNAL(destroyed())
    };
    static const char *const proxySlots[] = {
        SLOT(sourceRowsAboutToBeInserted(QModelIndex,int,int)),
        SLOT(sourceRowsInserted(QModelIndex,int,int)),
        SLOT(sourceRowsAboutToBeRemoved(QModelIndex,int,int)),
        SLOT(sourceRowsRemoved(QModelIndex,int,int)),
        SLOT(sourceRowsAboutToBeMoved(QModelIndex,int,int,QModelIndex,int)),
        SLOT(sourceRowsMoved(QModelIndex,int,int,QModelIndex,int)),
        SLOT(sourceModelAboutToBeReset()),
        SLOT(sourceModelReset()),
        SLOT(sourceDataChanged(QModelIndex,QModelIndex)),
        SLOT(sourceLayoutAboutToBeChanged()),
        SLOT(sourceLayoutChanged()),
        SLOT(sourceModelDestroyed())
    };

    if (sourceModel()) {
        for (int i = 0; i < int(sizeof modelSignals / sizeof * modelSignals); ++i) {
            disconnect(sourceModel(), modelSignals[i], d, proxySlots[i]);
        }
    }

    // Must be called before QAbstractProxyModel::setSourceModel because it emit some signals.
    d->resetInternalData();
    QAbstractProxyModel::setSourceModel(_sourceModel);
    if (_sourceModel) {
        if (d->m_selectionModel) {
            delete d->m_indexMapper;
            d->m_indexMapper = new KModelIndexProxyMapper(_sourceModel, d->m_selectionModel->model(), this);
            if (d->m_selectionModel->hasSelection()) {
                d->selectionChanged(d->m_selectionModel->selection(), QItemSelection());
            }
        }

        for (int i = 0; i < int(sizeof modelSignals / sizeof * modelSignals); ++i) {
            connect(_sourceModel, modelSignals[i], d, proxySlots[i]);
        }
    }

    d->m_resetting = false;
    endResetModel();
}

QModelIndex KSelectionProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    Q_D(const KSelectionProxyModel);

    if (!proxyIndex.isValid() || !sourceModel() || d->m_rootIndexList.isEmpty()) {
        return QModelIndex();
    }

    Q_ASSERT(proxyIndex.model() == this);

    if (proxyIndex.internalPointer() == nullptr) {
        return d->mapTopLevelToSource(proxyIndex.row(), proxyIndex.column());
    }

    const QModelIndex proxyParent = d->parentForId(proxyIndex.internalPointer());
    Q_ASSERT(proxyParent.isValid());
    const QModelIndex sourceParent = d->mapParentToSource(proxyParent);
    Q_ASSERT(sourceParent.isValid());
    return sourceModel()->index(proxyIndex.row(), proxyIndex.column(), sourceParent);
}

QModelIndex KSelectionProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    Q_D(const KSelectionProxyModel);

    if (!sourceModel() || !sourceIndex.isValid() || d->m_rootIndexList.isEmpty()) {
        return QModelIndex();
    }

    Q_ASSERT(sourceIndex.model() == sourceModel());

    if (!sourceIndex.isValid()) {
        return QModelIndex();
    }

    if (!d->ensureMappable(sourceIndex)) {
        return QModelIndex();
    }

    return d->mapFromSource(sourceIndex);
}

int KSelectionProxyModel::rowCount(const QModelIndex &index) const
{
    Q_D(const KSelectionProxyModel);

    if (!sourceModel() || index.column() > 0 || d->m_rootIndexList.isEmpty()) {
        return 0;
    }

    Q_ASSERT(index.isValid() ? index.model() == this : true);
    if (!index.isValid()) {
        return d->topLevelRowCount();
    }

    // index is valid
    if (d->isFlat()) {
        return 0;
    }

    QModelIndex sourceParent = d->mapParentToSource(index);

    if (!sourceParent.isValid() && sourceModel()->hasChildren(sourceParent)) {
        sourceParent = mapToSource(index.parent());
        d->createParentMappings(sourceParent, 0, sourceModel()->rowCount(sourceParent) - 1);
        sourceParent = d->mapParentToSource(index);
    }

    if (!sourceParent.isValid()) {
        return 0;
    }

    return sourceModel()->rowCount(sourceParent);
}

QModelIndex KSelectionProxyModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_D(const KSelectionProxyModel);

    if (!sourceModel() || d->m_rootIndexList.isEmpty() || !hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    Q_ASSERT(parent.isValid() ? parent.model() == this : true);

    if (!parent.isValid()) {
        return d->createTopLevelIndex(row, column);
    }

    void *const parentId = d->parentId(parent);
    Q_ASSERT(parentId);
    return createIndex(row, column, parentId);
}

QModelIndex KSelectionProxyModel::parent(const QModelIndex &index) const
{
    Q_D(const KSelectionProxyModel);

    if (!sourceModel() || !index.isValid() || d->m_rootIndexList.isEmpty()) {
        return QModelIndex();
    }

    Q_ASSERT(index.model() == this);

    return d->parentForId(index.internalPointer());
}

Qt::ItemFlags KSelectionProxyModel::flags(const QModelIndex &index) const
{
    if (!index.isValid() || !sourceModel()) {
        return QAbstractProxyModel::flags(index);
    }

    Q_ASSERT(index.model() == this);

    const QModelIndex srcIndex = mapToSource(index);
    Q_ASSERT(srcIndex.isValid());
    return sourceModel()->flags(srcIndex);
}

QVariant KSelectionProxyModel::data(const QModelIndex &index, int role) const
{
    if (!sourceModel()) {
        return QVariant();
    }

    if (index.isValid()) {
        Q_ASSERT(index.model() == this);
        const QModelIndex idx = mapToSource(index);
        return idx.data(role);
    }
    return sourceModel()->data(index, role);
}

QVariant KSelectionProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (!sourceModel()) {
        return QVariant();
    }
    return sourceModel()->headerData(section, orientation, role);
}

QMimeData *KSelectionProxyModel::mimeData(const QModelIndexList &indexes) const
{
    if (!sourceModel()) {
        return QAbstractProxyModel::mimeData(indexes);
    }
    QModelIndexList sourceIndexes;
    for (const QModelIndex &index : indexes) {
        sourceIndexes << mapToSource(index);
    }
    return sourceModel()->mimeData(sourceIndexes);
}

QStringList KSelectionProxyModel::mimeTypes() const
{
    if (!sourceModel()) {
        return QAbstractProxyModel::mimeTypes();
    }
    return sourceModel()->mimeTypes();
}

Qt::DropActions KSelectionProxyModel::supportedDropActions() const
{
    if (!sourceModel()) {
        return QAbstractProxyModel::supportedDropActions();
    }
    return sourceModel()->supportedDropActions();
}

bool KSelectionProxyModel::hasChildren(const QModelIndex &parent) const
{
    Q_D(const KSelectionProxyModel);

    if (d->m_rootIndexList.isEmpty() || !sourceModel()) {
        return false;
    }

    if (parent.isValid()) {
        Q_ASSERT(parent.model() == this);
        if (d->isFlat()) {
            return false;
        }
        return sourceModel()->hasChildren(mapToSource(parent));
    }

    if (!d->m_startWithChildTrees) {
        return true;
    }

    return !d->m_mappedFirstChildren.isEmpty();
}

int KSelectionProxyModel::columnCount(const QModelIndex &index) const
{
    if (!sourceModel() || index.column() > 0) {
        return 0;
    }

    return sourceModel()->columnCount(mapToSource(index));
}

QItemSelectionModel *KSelectionProxyModel::selectionModel() const
{
    Q_D(const KSelectionProxyModel);
    return d->m_selectionModel;
}

void KSelectionProxyModel::setSelectionModel(QItemSelectionModel *itemSelectionModel)
{
    Q_D(KSelectionProxyModel);
    if (d->m_selectionModel != itemSelectionModel) {
        if (d->m_selectionModel) {
            disconnect(d->m_selectionModel, SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                       d, SLOT(selectionChanged(QItemSelection,QItemSelection)));
        }

        d->m_selectionModel = itemSelectionModel;
        emit selectionModelChanged();

        if (d->m_selectionModel) {
            connect(d->m_selectionModel, SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                    d, SLOT(selectionChanged(QItemSelection,QItemSelection)));

            auto handleSelectionModelModel = [ &, d] {
                beginResetModel();
                if (d->selectionModelModelAboutToBeResetConnection)
                {
                    disconnect(d->selectionModelModelAboutToBeResetConnection);
                }
                if (d->selectionModelModelResetConnection)
                {
                    disconnect(d->selectionModelModelResetConnection);
                }
                if (d->m_selectionModel->model())
                {
                    d->selectionModelModelAboutToBeResetConnection = connect(
                        d->m_selectionModel->model(),
                        SIGNAL(modelAboutToBeReset()), d, SLOT(sourceModelAboutToBeReset()));
                    d->selectionModelModelResetConnection = connect(
                        d->m_selectionModel->model(),
                        SIGNAL(modelReset()), d, SLOT(sourceModelReset()));
                    d->m_rootIndexList.clear();
                    delete d->m_indexMapper;
                    d->m_indexMapper = new KModelIndexProxyMapper(sourceModel(), d->m_selectionModel->model(), this);
                }
                endResetModel();
            };
            connect(d->m_selectionModel.data(), &QItemSelectionModel::modelChanged,
                    this, handleSelectionModelModel);
            handleSelectionModelModel();
        }

        if (sourceModel()) {
            delete d->m_indexMapper;
            d->m_indexMapper = new KModelIndexProxyMapper(sourceModel(), d->m_selectionModel->model(), this);
            if (d->m_selectionModel->hasSelection()) {
                d->selectionChanged(d->m_selectionModel->selection(), QItemSelection());
            }
        }
    }
}

bool KSelectionProxyModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    Q_D(const KSelectionProxyModel);
    if (!sourceModel() || d->m_rootIndexList.isEmpty()) {
        return false;
    }

    if ((row == -1) && (column == -1)) {
        return sourceModel()->dropMimeData(data, action, -1, -1, mapToSource(parent));
    }

    int source_destination_row = -1;
    int source_destination_column = -1;
    QModelIndex source_parent;

    if (row == rowCount(parent)) {
        source_parent = mapToSource(parent);
        source_destination_row = sourceModel()->rowCount(source_parent);
    } else {
        const QModelIndex proxy_index = index(row, column, parent);
        const QModelIndex source_index = mapToSource(proxy_index);
        source_destination_row = source_index.row();
        source_destination_column = source_index.column();
        source_parent = source_index.parent();
    }
    return sourceModel()->dropMimeData(data, action, source_destination_row,
                                       source_destination_column, source_parent);
}

QList<QPersistentModelIndex> KSelectionProxyModel::sourceRootIndexes() const
{
    Q_D(const KSelectionProxyModel);
    return d->m_rootIndexList;
}

QModelIndexList KSelectionProxyModel::match(const QModelIndex &start, int role, const QVariant &value, int hits, Qt::MatchFlags flags) const
{
    if (role < Qt::UserRole) {
        return QAbstractProxyModel::match(start, role, value, hits, flags);
    }

    QModelIndexList list;
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

QItemSelection KSelectionProxyModel::mapSelectionFromSource(const QItemSelection &selection) const
{
    Q_D(const KSelectionProxyModel);
    if (!d->m_startWithChildTrees && d->m_includeAllSelected) {
        // QAbstractProxyModel::mapSelectionFromSource puts invalid ranges in the result
        // without checking. We can't have that.
        QItemSelection proxySelection;
        for (const QItemSelectionRange &range : selection) {
            QModelIndex proxyTopLeft = mapFromSource(range.topLeft());
            if (!proxyTopLeft.isValid()) {
                continue;
            }
            QModelIndex proxyBottomRight = mapFromSource(range.bottomRight());
            Q_ASSERT(proxyBottomRight.isValid());
            proxySelection.append(QItemSelectionRange(proxyTopLeft, proxyBottomRight));
        }
        return proxySelection;
    }

    QItemSelection proxySelection;
    QItemSelection::const_iterator it = selection.constBegin();
    const QItemSelection::const_iterator end = selection.constEnd();
    for (; it != end; ++it) {
        const QModelIndex proxyTopLeft = mapFromSource(it->topLeft());
        if (!proxyTopLeft.isValid()) {
            continue;
        }

        if (it->height() == 1 && it->width() == 1) {
            proxySelection.append(QItemSelectionRange(proxyTopLeft, proxyTopLeft));
        } else {
            proxySelection.append(QItemSelectionRange(proxyTopLeft, d->mapFromSource(it->bottomRight())));
        }
    }
    return proxySelection;
}

QItemSelection KSelectionProxyModel::mapSelectionToSource(const QItemSelection &selection) const
{
    Q_D(const KSelectionProxyModel);

    if (selection.isEmpty()) {
        return selection;
    }

    if (!d->m_startWithChildTrees && d->m_includeAllSelected) {
        // QAbstractProxyModel::mapSelectionFromSource puts invalid ranges in the result
        // without checking. We can't have that.
        QItemSelection sourceSelection;
        for (const QItemSelectionRange &range : selection) {
            QModelIndex sourceTopLeft = mapToSource(range.topLeft());
            Q_ASSERT(sourceTopLeft.isValid());

            QModelIndex sourceBottomRight = mapToSource(range.bottomRight());
            Q_ASSERT(sourceBottomRight.isValid());
            sourceSelection.append(QItemSelectionRange(sourceTopLeft, sourceBottomRight));
        }
        return sourceSelection;
    }

    QItemSelection sourceSelection;
    QItemSelection extraSelection;
    QItemSelection::const_iterator it = selection.constBegin();
    const QItemSelection::const_iterator end = selection.constEnd();
    for (; it != end; ++it) {
        const QModelIndex sourceTopLeft = mapToSource(it->topLeft());
        if (it->height() == 1 && it->width() == 1) {
            sourceSelection.append(QItemSelectionRange(sourceTopLeft, sourceTopLeft));
        } else if (it->parent().isValid()) {
            sourceSelection.append(QItemSelectionRange(sourceTopLeft, mapToSource(it->bottomRight())));
        } else {
            // A contiguous selection in the proxy might not be contiguous in the source if it
            // is at the top level of the proxy.
            if (d->m_startWithChildTrees) {
                const QModelIndex sourceParent = mapFromSource(sourceTopLeft);
                Q_ASSERT(sourceParent.isValid());
                const int rowCount = sourceModel()->rowCount(sourceParent);
                if (rowCount < it->bottom()) {
                    Q_ASSERT(sourceTopLeft.isValid());
                    Q_ASSERT(it->bottomRight().isValid());
                    const QModelIndex sourceBottomRight = mapToSource(it->bottomRight());
                    Q_ASSERT(sourceBottomRight.isValid());
                    sourceSelection.append(QItemSelectionRange(sourceTopLeft, sourceBottomRight));
                    continue;
                }
                // Store the contiguous part...
                const QModelIndex sourceBottomRight = sourceModel()->index(rowCount - 1, it->right(), sourceParent);
                Q_ASSERT(sourceTopLeft.isValid());
                Q_ASSERT(sourceBottomRight.isValid());
                sourceSelection.append(QItemSelectionRange(sourceTopLeft, sourceBottomRight));
                // ... and the rest will be processed later.
                extraSelection.append(QItemSelectionRange(createIndex(it->top() - rowCount, it->right()), it->bottomRight()));
            } else {
                QItemSelection topSelection;
                const QModelIndex idx = createIndex(it->top(), it->right());
                const QModelIndex sourceIdx = mapToSource(idx);
                Q_ASSERT(sourceIdx.isValid());
                topSelection.append(QItemSelectionRange(sourceTopLeft, sourceIdx));
                for (int i = it->top() + 1; i <= it->bottom(); ++i) {
                    const QModelIndex left = mapToSource(createIndex(i, 0));
                    const QModelIndex right = mapToSource(createIndex(i, it->right()));
                    Q_ASSERT(left.isValid());
                    Q_ASSERT(right.isValid());
                    topSelection.append(QItemSelectionRange(left, right));
                }
                sourceSelection += kNormalizeSelection(topSelection);
            }
        }
    }
    sourceSelection << mapSelectionToSource(extraSelection);
    return sourceSelection;
}
