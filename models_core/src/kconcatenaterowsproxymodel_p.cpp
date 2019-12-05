#include "kconcatenaterowsproxymodel_p.h"

void KConcatenateRowsProxyModelPrivate::slotRowsAboutToBeInserted(const QModelIndex &, int start, int end)
{
    Q_Q(KConcatenateRowsProxyModel);
    const QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(sender());
    const int rowsPrior = computeRowsPrior(model);
    q->beginInsertRows(QModelIndex(), rowsPrior + start, rowsPrior + end);
}

void KConcatenateRowsProxyModelPrivate::slotRowsInserted(const QModelIndex &, int start, int end)
{
    Q_Q(KConcatenateRowsProxyModel);
    m_rowCount += end - start + 1;
    q->endInsertRows();
}

void KConcatenateRowsProxyModelPrivate::slotRowsAboutToBeRemoved(const QModelIndex &, int start, int end)
{
    Q_Q(KConcatenateRowsProxyModel);
    const QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(sender());
    const int rowsPrior = computeRowsPrior(model);
    q->beginRemoveRows(QModelIndex(), rowsPrior + start, rowsPrior + end);
}

void KConcatenateRowsProxyModelPrivate::slotRowsRemoved(const QModelIndex &, int start, int end)
{
    Q_Q(KConcatenateRowsProxyModel);
    m_rowCount -= end - start + 1;
    q->endRemoveRows();
}

void KConcatenateRowsProxyModelPrivate::slotColumnsAboutToBeInserted(const QModelIndex &parent, int start, int end)
{
    Q_Q(KConcatenateRowsProxyModel);
    if (parent.isValid()) { // we are flat
        return;
    }
    const QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(sender());
    if (m_models.at(0) == model) {
        q->beginInsertColumns(QModelIndex(), start, end);
    }
}

void KConcatenateRowsProxyModelPrivate::slotColumnsInserted(const QModelIndex &parent, int, int)
{
    Q_Q(KConcatenateRowsProxyModel);
    if (parent.isValid()) { // we are flat
        return;
    }
    const QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(sender());
    if (m_models.at(0) == model) {
        q->endInsertColumns();
    }
}

void KConcatenateRowsProxyModelPrivate::slotColumnsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
    Q_Q(KConcatenateRowsProxyModel);
    if (parent.isValid()) { // we are flat
        return;
    }
    const QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(sender());
    if (m_models.at(0) == model) {
        q->beginRemoveColumns(QModelIndex(), start, end);
    }
}

void KConcatenateRowsProxyModelPrivate::slotColumnsRemoved(const QModelIndex &parent, int, int)
{
    Q_Q(KConcatenateRowsProxyModel);
    if (parent.isValid()) { // we are flat
        return;
    }
    const QAbstractItemModel *model = qobject_cast<QAbstractItemModel *>(sender());
    if (m_models.at(0) == model) {
        q->endRemoveColumns();
    }
}

void KConcatenateRowsProxyModelPrivate::slotDataChanged(const QModelIndex &from, const QModelIndex &to, const QVector<int> &roles)
{
    Q_Q(KConcatenateRowsProxyModel);
    if (!from.isValid()) { // QSFPM bug, it emits dataChanged(invalid, invalid) if a cell in a hidden column changes
        return;
    }
    const QModelIndex myFrom = q->mapFromSource(from);
    const QModelIndex myTo = q->mapFromSource(to);
    emit q->dataChanged(myFrom, myTo, roles);
}

void KConcatenateRowsProxyModelPrivate::slotSourceLayoutAboutToBeChanged(const QList<QPersistentModelIndex> &sourceParents, QAbstractItemModel::LayoutChangeHint hint)
{
    Q_Q(KConcatenateRowsProxyModel);
    QList<QPersistentModelIndex> parents;
    parents.reserve(sourceParents.size());
    for (const QPersistentModelIndex &parent : sourceParents) {
        if (!parent.isValid()) {
            parents << QPersistentModelIndex();
            continue;
        }
        const QModelIndex mappedParent = q->mapFromSource(parent);
        Q_ASSERT(mappedParent.isValid());
        parents << mappedParent;
    }

    emit q->layoutAboutToBeChanged(parents, hint);

    const QModelIndexList persistentIndexList = q->persistentIndexList();
    layoutChangePersistentIndexes.reserve(persistentIndexList.size());

    for (const QPersistentModelIndex &proxyPersistentIndex : persistentIndexList) {
        proxyIndexes << proxyPersistentIndex;
        Q_ASSERT(proxyPersistentIndex.isValid());
        const QPersistentModelIndex srcPersistentIndex = q->mapToSource(proxyPersistentIndex);
        Q_ASSERT(srcPersistentIndex.isValid());
        layoutChangePersistentIndexes << srcPersistentIndex;
    }
}

void KConcatenateRowsProxyModelPrivate::slotSourceLayoutChanged(const QList<QPersistentModelIndex> &sourceParents, QAbstractItemModel::LayoutChangeHint hint)
{
    Q_Q(KConcatenateRowsProxyModel);
    for (int i = 0; i < proxyIndexes.size(); ++i) {
        const QModelIndex proxyIdx = proxyIndexes.at(i);
        QModelIndex newProxyIdx = q->mapFromSource(layoutChangePersistentIndexes.at(i));
        q->changePersistentIndex(proxyIdx, newProxyIdx);
    }

    layoutChangePersistentIndexes.clear();
    proxyIndexes.clear();

    QList<QPersistentModelIndex> parents;
    parents.reserve(sourceParents.size());
    for (const QPersistentModelIndex &parent : sourceParents) {
        if (!parent.isValid()) {
            parents << QPersistentModelIndex();
            continue;
        }
        const QModelIndex mappedParent = q->mapFromSource(parent);
        Q_ASSERT(mappedParent.isValid());
        parents << mappedParent;
    }
    emit q->layoutChanged(parents, hint);
}

void KConcatenateRowsProxyModelPrivate::slotModelAboutToBeReset()
{
    Q_Q(KConcatenateRowsProxyModel);
    const QAbstractItemModel *sourceModel = qobject_cast<const QAbstractItemModel *>(sender());
    Q_ASSERT(m_models.contains(const_cast<QAbstractItemModel *>(sourceModel)));
    const int oldRows = sourceModel->rowCount();
    if (oldRows > 0) {
        slotRowsAboutToBeRemoved(QModelIndex(), 0, oldRows - 1);
        slotRowsRemoved(QModelIndex(), 0, oldRows - 1);
    }
    if (m_models.at(0) == sourceModel) {
        q->beginResetModel();
    }
}

void KConcatenateRowsProxyModelPrivate::slotModelReset()
{
    Q_Q(KConcatenateRowsProxyModel);
    const QAbstractItemModel *sourceModel = qobject_cast<const QAbstractItemModel *>(sender());
    Q_ASSERT(m_models.contains(const_cast<QAbstractItemModel *>(sourceModel)));
    if (m_models.at(0) == sourceModel) {
        q->endResetModel();
    }
    const int newRows = sourceModel->rowCount();
    if (newRows > 0) {
        slotRowsAboutToBeInserted(QModelIndex(), 0, newRows - 1);
        slotRowsInserted(QModelIndex(), 0, newRows - 1);
    }
}

int KConcatenateRowsProxyModelPrivate::computeRowsPrior(const QAbstractItemModel *sourceModel) const
{
    int rowsPrior = 0;
    for (const QAbstractItemModel *model : qAsConst(m_models)) {
        if (model == sourceModel) {
            break;
        }
        rowsPrior += model->rowCount();
    }
    return rowsPrior;
}

QAbstractItemModel *KConcatenateRowsProxyModelPrivate::sourceModelForRow(int row, int *sourceRow) const
{
    int rowCount = 0;
    QAbstractItemModel *selection = nullptr;
    for (QAbstractItemModel *model : qAsConst(m_models)) {
        const int subRowCount = model->rowCount();
        if (rowCount + subRowCount > row) {
            selection = model;
            break;
        }
        rowCount += subRowCount;
    }
    *sourceRow = row - rowCount;
    return selection;
}
