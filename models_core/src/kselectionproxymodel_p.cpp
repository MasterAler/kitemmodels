#include "kselectionproxymodel_p.h"

void KSelectionProxyModelPrivate::emitContinuousRanges(const QModelIndex &sourceFirst, const QModelIndex &sourceLast,
        const QModelIndex &proxyFirst, const QModelIndex &proxyLast)
{
    Q_Q(KSelectionProxyModel);

    Q_ASSERT(sourceFirst.model() == q->sourceModel());
    Q_ASSERT(sourceLast.model() == q->sourceModel());
    Q_ASSERT(proxyFirst.model() == q);
    Q_ASSERT(proxyLast.model() == q);

    const int proxyRangeSize = proxyLast.row() - proxyFirst.row();
    const int sourceRangeSize = sourceLast.row() - sourceFirst.row();

    if (proxyRangeSize == sourceRangeSize) {
        emit q->dataChanged(proxyFirst, proxyLast);
        return;
    }

    // TODO: Loop to skip descendant ranges.
//     int lastRow;
//
//     const QModelIndex sourceHalfWay = sourceFirst.sibling(sourceFirst.row() + (sourceRangeSize / 2));
//     const QModelIndex proxyHalfWay = proxyFirst.sibling(proxyFirst.row() + (proxyRangeSize / 2));
//     const QModelIndex mappedSourceHalfway = q->mapToSource(proxyHalfWay);
//
//     const int halfProxyRange = mappedSourceHalfway.row() - proxyFirst.row();
//     const int halfSourceRange = sourceHalfWay.row() - sourceFirst.row();
//
//     if (proxyRangeSize == sourceRangeSize)
//     {
//         emit q->dataChanged(proxyFirst, proxyLast.sibling(proxyFirst.row() + proxyRangeSize, proxyLast.column()));
//         return;
//     }

    emit q->dataChanged(proxyFirst, proxyLast);
}

void KSelectionProxyModelPrivate::sourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_Q(KSelectionProxyModel);

    Q_ASSERT(topLeft.model() == q->sourceModel());
    Q_ASSERT(bottomRight.model() == q->sourceModel());

    const QModelIndex sourceRangeParent = topLeft.parent();
    if (!sourceRangeParent.isValid() && m_startWithChildTrees && !m_rootIndexList.contains(sourceRangeParent)) {
        return;
    }

    const QModelIndex proxyTopLeft = q->mapFromSource(topLeft);
    const QModelIndex proxyBottomRight = q->mapFromSource(bottomRight);

    const QModelIndex proxyRangeParent = proxyTopLeft.parent();

    if (!m_omitChildren && m_omitDescendants && m_startWithChildTrees && m_includeAllSelected) {
        // ChildrenOfExactSelection
        if (proxyTopLeft.isValid()) {
            emitContinuousRanges(topLeft, bottomRight, proxyTopLeft, proxyBottomRight);
        }
        return;
    }

    if ((m_omitChildren && !m_startWithChildTrees && m_includeAllSelected)
            || (!proxyRangeParent.isValid() && !m_startWithChildTrees)) {
        // Exact selection and SubTreeRoots and SubTrees in top level
        // Emit continuous ranges.
        QList<int> changedRows;
        for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
            const QModelIndex index = q->sourceModel()->index(row, topLeft.column(), topLeft.parent());
            const int idx = m_rootIndexList.indexOf(index);
            if (idx != -1) {
                changedRows.append(idx);
            }
        }
        if (changedRows.isEmpty()) {
            return;
        }
        int first = changedRows.first();
        int previous = first;
        QList<int>::const_iterator it = changedRows.constBegin();
        const QList<int>::const_iterator end = changedRows.constEnd();
        for (; it != end; ++it) {
            if (*it == previous + 1) {
                ++previous;
            } else {
                const QModelIndex _top = q->index(first, topLeft.column());
                const QModelIndex _bottom = q->index(previous, bottomRight.column());
                emit q->dataChanged(_top, _bottom);
                previous = first = *it;
            }
        }
        if (first != previous) {
            const QModelIndex _top = q->index(first, topLeft.column());
            const QModelIndex _bottom = q->index(previous, bottomRight.column());
            emit q->dataChanged(_top, _bottom);
        }
        return;
    }
    if (proxyRangeParent.isValid()) {
        if (m_omitChildren && !m_startWithChildTrees && !m_includeAllSelected)
            // SubTreeRoots
        {
            return;
        }
        if (!proxyTopLeft.isValid()) {
            return;
        }
        // SubTrees and SubTreesWithoutRoots
        emit q->dataChanged(proxyTopLeft, proxyBottomRight);
        return;
    }

    if (m_startWithChildTrees && !m_omitChildren && !m_includeAllSelected && !m_omitDescendants) {
        // SubTreesWithoutRoots
        if (proxyTopLeft.isValid()) {
            emit q->dataChanged(proxyTopLeft, proxyBottomRight);
        }
        return;
    }
}

void KSelectionProxyModelPrivate::sourceLayoutAboutToBeChanged()
{
    Q_Q(KSelectionProxyModel);

    if (m_ignoreNextLayoutAboutToBeChanged) {
        m_ignoreNextLayoutAboutToBeChanged = false;
        return;
    }

    if (m_rootIndexList.isEmpty()) {
        return;
    }

    emit q->layoutAboutToBeChanged();

    QItemSelection selection;
    for (const QModelIndex &rootIndex : qAsConst(m_rootIndexList)) {
        // This will be optimized later.
        emit q->rootIndexAboutToBeRemoved(rootIndex);
        selection.append(QItemSelectionRange(rootIndex, rootIndex));
    }

    selection = kNormalizeSelection(selection);
    emit q->rootSelectionAboutToBeRemoved(selection);

    QPersistentModelIndex srcPersistentIndex;
    const auto lst = q->persistentIndexList();
    for (const QPersistentModelIndex &proxyPersistentIndex : lst) {
        m_proxyIndexes << proxyPersistentIndex;
        Q_ASSERT(proxyPersistentIndex.isValid());
        srcPersistentIndex = q->mapToSource(proxyPersistentIndex);
        Q_ASSERT(srcPersistentIndex.isValid());
        m_layoutChangePersistentIndexes << srcPersistentIndex;
    }

    m_rootIndexList.clear();
}

void KSelectionProxyModelPrivate::sourceLayoutChanged()
{
    Q_Q(KSelectionProxyModel);

    if (m_ignoreNextLayoutChanged) {
        m_ignoreNextLayoutChanged = false;
        return;
    }

    if (!m_selectionModel || !m_selectionModel->hasSelection()) {
        return;
    }

    // Handling this signal is slow.
    // The problem is that anything can happen between emissions of layoutAboutToBeChanged and layoutChanged.
    // We can't assume anything is the same about the structure anymore. items have been sorted, items which
    // were parents before are now not, items which were not parents before now are, items which used to be the
    // first child are now the Nth child and items which used to be the Nth child are now the first child.
    // We effectively can't update our mapping because we don't have enough information to update everything.
    // The only way we would have is if we take a persistent index of the entire source model
    // on sourceLayoutAboutToBeChanged and then examine it here. That would be far too expensive.
    // Instead we just have to clear the entire mapping and recreate it.

    m_rootIndexList.clear();
    m_mappedFirstChildren.clear();
    m_mappedParents.clear();
    m_parentIds.clear();

    m_resetting = true;
    m_layoutChanging = true;
    selectionChanged(m_selectionModel->selection(), QItemSelection());
    m_resetting = false;
    m_layoutChanging = false;

    for (int i = 0; i < m_proxyIndexes.size(); ++i) {
        q->changePersistentIndex(m_proxyIndexes.at(i), q->mapFromSource(m_layoutChangePersistentIndexes.at(i)));
    }

    m_layoutChangePersistentIndexes.clear();
    m_proxyIndexes.clear();

    emit q->layoutChanged();
}

void KSelectionProxyModelPrivate::resetInternalData()
{
    m_rootIndexList.clear();
    m_layoutChangePersistentIndexes.clear();
    m_proxyIndexes.clear();
    m_mappedParents.clear();
    m_parentIds.clear();
    m_mappedFirstChildren.clear();
    m_voidPointerFactory.clear();
}

void KSelectionProxyModelPrivate::sourceModelDestroyed()
{
    // There is very little we can do here.
    resetInternalData();
    m_resetting = false;
    m_sourceModelResetting = false;
}

void KSelectionProxyModelPrivate::sourceModelAboutToBeReset()
{
    Q_Q(KSelectionProxyModel);

    // We might be resetting as a result of the selection source model resetting.
    // If so we don't want to emit
    // sourceModelAboutToBeReset
    // sourceModelAboutToBeReset
    // sourceModelReset
    // sourceModelReset
    // So we ensure that we just emit one.
    if (m_resetting) {

        // If both the source model and the selection source model are reset,
        // We want to begin our reset before the first one is reset and end
        // it after the second one is reset.
        m_doubleResetting = true;
        return;
    }

    q->beginResetModel();
    m_resetting = true;
    m_sourceModelResetting = true;
}

void KSelectionProxyModelPrivate::sourceModelReset()
{
    Q_Q(KSelectionProxyModel);

    if (m_doubleResetting) {
        m_doubleResetting = false;
        return;
    }

    resetInternalData();
    m_sourceModelResetting = false;
    m_resetting = false;
    selectionChanged(m_selectionModel->selection(), QItemSelection());
    q->endResetModel();
}

int KSelectionProxyModelPrivate::getProxyInitialRow(const QModelIndex &parent) const
{
    Q_ASSERT(m_rootIndexList.contains(parent));

    // The difficulty here is that parent and parent.parent() might both be in the m_rootIndexList.

    // - A
    // - B
    // - - C
    // - - D
    // - - - E

    // Consider that B and D are selected. The proxy model is:

    // - C
    // - D
    // - E

    // Then D gets a new child at 0. In that case we require adding F between D and E.

    // Consider instead that D gets removed. Then @p parent will be B.

    Q_Q(const KSelectionProxyModel);

    Q_ASSERT(parent.model() == q->sourceModel());

    int parentPosition = m_rootIndexList.indexOf(parent);

    QModelIndex parentAbove;

    // If parentPosition == 0, then parent.parent() is not also in the model. (ordering is preserved)
    while (parentPosition > 0) {
        parentPosition--;

        parentAbove = m_rootIndexList.at(parentPosition);
        Q_ASSERT(parentAbove.isValid());

        int rows = q->sourceModel()->rowCount(parentAbove);
        if (rows > 0) {
            QModelIndex sourceIndexAbove = q->sourceModel()->index(rows - 1, 0, parentAbove);
            Q_ASSERT(sourceIndexAbove.isValid());
            QModelIndex proxyChildAbove = mapFromSource(sourceIndexAbove);
            Q_ASSERT(proxyChildAbove.isValid());
            return proxyChildAbove.row() + 1;
        }
    }
    return 0;
}

void KSelectionProxyModelPrivate::updateFirstChildMapping(const QModelIndex &parent, int offset)
{
    Q_Q(KSelectionProxyModel);

    Q_ASSERT(parent.isValid() ? parent.model() == q->sourceModel() : true);

    static const int column = 0;
    static const int row = 0;

    const QPersistentModelIndex srcIndex = q->sourceModel()->index(row, column, parent);

    const QPersistentModelIndex previousFirstChild = q->sourceModel()->index(offset, column, parent);

    SourceIndexProxyRowMapping::left_iterator it = m_mappedFirstChildren.findLeft(previousFirstChild);
    if (it == m_mappedFirstChildren.leftEnd()) {
        return;
    }

    Q_ASSERT(srcIndex.isValid());
    const int proxyRow = it.value();
    Q_ASSERT(proxyRow >= 0);

    m_mappedFirstChildren.eraseLeft(it);

    // The proxy row in the mapping has already been updated by the offset in updateInternalTopIndexes
    // so we restore it by applying the reverse.
    m_mappedFirstChildren.insert(srcIndex, proxyRow - offset);
}

QPair< int, int > KSelectionProxyModelPrivate::beginInsertRows(const QModelIndex &parent, int start, int end) const
{
    const QModelIndex proxyParent = mapFromSource(parent);

    if (!proxyParent.isValid()) {
        if (!m_startWithChildTrees) {
            return qMakePair(-1, -1);
        }

        if (!m_rootIndexList.contains(parent)) {
            return qMakePair(-1, -1);
        }
    }

    if (!m_startWithChildTrees) {
        // SubTrees
        if (proxyParent.isValid()) {
            return qMakePair(start, end);
        }
        return qMakePair(-1, -1);
    }

    if (!m_includeAllSelected && proxyParent.isValid()) {
        // SubTreesWithoutRoots deeper than topLevel
        return qMakePair(start, end);
    }

    if (!m_rootIndexList.contains(parent)) {
        return qMakePair(-1, -1);
    }

    const int proxyStartRow = getProxyInitialRow(parent) + start;
    return qMakePair(proxyStartRow, proxyStartRow + (end - start));
}

void KSelectionProxyModelPrivate::sourceRowsAboutToBeInserted(const QModelIndex &parent, int start, int end)
{
    Q_Q(KSelectionProxyModel);

    Q_ASSERT(parent.isValid() ? parent.model() == q->sourceModel() : true);

    if (!m_selectionModel || !m_selectionModel->hasSelection()) {
        return;
    }

    if (m_omitChildren)
        // ExactSelection and SubTreeRoots
    {
        return;
    }

    // topLevel insertions can be ignored because topLevel items would need to be selected to affect the proxy.
    if (!parent.isValid()) {
        return;
    }

    QPair<int, int> pair = beginInsertRows(parent, start, end);
    if (pair.first == -1) {
        return;
    }

    const QModelIndex proxyParent = m_startWithChildTrees ? QModelIndex() : mapFromSource(parent);

    m_rowsInserted = true;
    q->beginInsertRows(proxyParent, pair.first, pair.second);
}

void KSelectionProxyModelPrivate::endInsertRows(const QModelIndex &parent, int start, int end)
{
    Q_Q(const KSelectionProxyModel);
    const QModelIndex proxyParent = mapFromSource(parent);
    const int offset = end - start + 1;

    const bool isNewParent = (q->sourceModel()->rowCount(parent) == offset);

    if (m_startWithChildTrees && m_rootIndexList.contains(parent)) {
        const int proxyInitialRow = getProxyInitialRow(parent);
        Q_ASSERT(proxyInitialRow >= 0);
        const int proxyStartRow = proxyInitialRow + start;

        updateInternalTopIndexes(proxyStartRow, offset);
        if (isNewParent) {
            createFirstChildMapping(parent, proxyStartRow);
        } else if (start == 0)
            // We already have a first child mapping, but what we have mapped is not the first child anymore
            // so we need to update it.
        {
            updateFirstChildMapping(parent, end + 1);
        }
    } else {
        Q_ASSERT(proxyParent.isValid());
        if (!isNewParent) {
            updateInternalIndexes(proxyParent, start, offset);
        } else {
            createParentMappings(parent.parent(), parent.row(), parent.row());
        }
    }
    createParentMappings(parent, start, end);
}

void KSelectionProxyModelPrivate::sourceRowsInserted(const QModelIndex &parent, int start, int end)
{
    Q_Q(KSelectionProxyModel);

    Q_ASSERT(parent.isValid() ? parent.model() == q->sourceModel() : true);

    if (!m_rowsInserted) {
        return;
    }
    m_rowsInserted = false;
    endInsertRows(parent, start, end);
    q->endInsertRows();
    for (const PendingSelectionChange &pendingChange : qAsConst(m_pendingSelectionChanges)) {
        selectionChanged(pendingChange.selected, pendingChange.deselected);
    }
    m_pendingSelectionChanges.clear();
}

static bool rootWillBeRemovedFrom(const QModelIndex &ancestor, int start, int end,
                               const QModelIndex &root)
{
  Q_ASSERT(root.isValid());

  auto parent = root;
  while (parent.isValid()) {
      auto prev = parent;
      parent = parent.parent();
      if (parent == ancestor) {
          return (prev.row() <= end && prev.row() >= start);
      }
  }
  return false;
}

bool KSelectionProxyModelPrivate::rootWillBeRemoved(const QItemSelection &selection,
                               const QModelIndex &root)
{
    Q_ASSERT(root.isValid());

    for (auto& r : selection) {
        if (m_includeAllSelected) {
            if (r.parent() == root.parent() && root.row() <= r.bottom() && root.row() >= r.top()) {
                return true;
            }
        } else {
            if (rootWillBeRemovedFrom(r.parent(), r.top(), r.bottom(), root)) {
                return true;
            }
        }
    }
    return false;
}

QPair<int, int> KSelectionProxyModelPrivate::beginRemoveRows(const QModelIndex &parent, int start, int end) const
{
    Q_Q(const KSelectionProxyModel);

    if (!m_includeAllSelected && !m_omitChildren) {
        // SubTrees and SubTreesWithoutRoots
        const QModelIndex proxyParent = mapParentFromSource(parent);
        if (proxyParent.isValid()) {
            return qMakePair(start, end);
        }
    }

    if (m_startWithChildTrees && m_rootIndexList.contains(parent)) {
        const int proxyStartRow = getProxyInitialRow(parent) + start;
        const int proxyEndRow = proxyStartRow + (end - start);
        return qMakePair(proxyStartRow, proxyEndRow);
    }

    QList<QPersistentModelIndex>::const_iterator rootIt = m_rootIndexList.constBegin();
    const QList<QPersistentModelIndex>::const_iterator rootEnd = m_rootIndexList.constEnd();
    int proxyStartRemove = 0;

    for (; rootIt != rootEnd; ++rootIt) {
        if (rootWillBeRemovedFrom(parent, start, end, *rootIt)) {
            break;
        } else {
            if (m_startWithChildTrees) {
                proxyStartRemove += q->sourceModel()->rowCount(*rootIt);
            } else {
                ++proxyStartRemove;
            }
        }
    }

    if (rootIt == rootEnd) {
        return qMakePair(-1, -1);
    }

    int proxyEndRemove = proxyStartRemove;

    for (; rootIt != rootEnd; ++rootIt) {
        if (!rootWillBeRemovedFrom(parent, start, end, *rootIt)) {
            break;
        }
        if (m_startWithChildTrees) {
            proxyEndRemove += q->sourceModel()->rowCount(*rootIt);
        } else {
            ++proxyEndRemove;
        }
    }

    --proxyEndRemove;
    if (proxyEndRemove >= proxyStartRemove) {
        return qMakePair(proxyStartRemove, proxyEndRemove);
    }
    return qMakePair(-1, -1);
}

void KSelectionProxyModelPrivate::sourceRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
    Q_Q(KSelectionProxyModel);

    Q_ASSERT(parent.isValid() ? parent.model() == q->sourceModel() : true);

    if (!m_selectionModel || !m_selectionModel->hasSelection()) {
        return;
    }

    QPair<int, int> pair = beginRemoveRows(parent, start, end);
    if (pair.first == -1) {
        return;
    }

    const QModelIndex proxyParent = mapParentFromSource(parent);

    m_rowsRemoved = true;
    m_proxyRemoveRows = pair;
    m_recreateFirstChildMappingOnRemoval = m_mappedFirstChildren.leftContains(q->sourceModel()->index(start, 0, parent));
    q->beginRemoveRows(proxyParent, pair.first, pair.second);
}

void KSelectionProxyModelPrivate::endRemoveRows(const QModelIndex &sourceParent, int proxyStart, int proxyEnd)
{
    const QModelIndex proxyParent = mapParentFromSource(sourceParent);

    // We need to make sure to remove entries from the mappings before updating internal indexes.

    // - A
    // - - B
    // - C
    // - - D

    // If A and C are selected, B and D are in the proxy. B maps to row 0 and D maps to row 1.
    // If B is then deleted leaving only D in the proxy, D needs to be updated to be a mapping
    // to row 0 instead of row 1. If that is done before removing the mapping for B, then the mapping
    // for D would overwrite the mapping for B and then the code for removing mappings would incorrectly
    // remove D.
    // So we first remove B and then update D.

    {
        SourceProxyIndexMapping::right_iterator it = m_mappedParents.rightBegin();

        while (it != m_mappedParents.rightEnd()) {
            if (!it.value().isValid()) {
                m_parentIds.removeRight(it.key());
                it = m_mappedParents.eraseRight(it);
            } else {
                ++it;
            }
        }
    }

    {
        // Depending on what is selected at the time, a single removal in the source could invalidate
        // many mapped first child items at once.

        // - A
        // - B
        // - - C
        // - - D
        // - - - E
        // - - - F
        // - - - - G
        // - - - - H

        // If D and F are selected, the proxy contains E, F, G, H. If B is then deleted E to H will
        // be removed, including both first child mappings at E and G.

        if (!proxyParent.isValid()) {
            removeFirstChildMappings(proxyStart, proxyEnd);
        }
    }

    if (proxyParent.isValid()) {
        updateInternalIndexes(proxyParent, proxyEnd + 1, -1 * (proxyEnd - proxyStart + 1));
    } else {
        updateInternalTopIndexes(proxyEnd + 1, -1 * (proxyEnd - proxyStart + 1));
    }

    QList<QPersistentModelIndex>::iterator rootIt = m_rootIndexList.begin();
    while (rootIt != m_rootIndexList.end()) {
        if (!rootIt->isValid()) {
            rootIt = m_rootIndexList.erase(rootIt);
        } else {
            ++rootIt;
        }
    }
}

void KSelectionProxyModelPrivate::sourceRowsRemoved(const QModelIndex &parent, int start, int end)
{
    Q_Q(KSelectionProxyModel);
    Q_UNUSED(end)
    Q_UNUSED(start)

    Q_ASSERT(parent.isValid() ? parent.model() == q->sourceModel() : true);

    if (!m_selectionModel) {
        return;
    }

    if (!m_rowsRemoved) {
        return;
    }
    m_rowsRemoved = false;

    Q_ASSERT(m_proxyRemoveRows.first >= 0);
    Q_ASSERT(m_proxyRemoveRows.second >= 0);
    endRemoveRows(parent, m_proxyRemoveRows.first, m_proxyRemoveRows.second);
    if (m_recreateFirstChildMappingOnRemoval && q->sourceModel()->hasChildren(parent))
        // The private endRemoveRows call might remove the first child mapping for parent, so
        // we create it again in that case.
    {
        createFirstChildMapping(parent, m_proxyRemoveRows.first);
    }
    m_recreateFirstChildMappingOnRemoval = false;

    m_proxyRemoveRows = qMakePair(-1, -1);
    q->endRemoveRows();
}

void KSelectionProxyModelPrivate::sourceRowsAboutToBeMoved(const QModelIndex &srcParent, int srcStart, int srcEnd, const QModelIndex &destParent, int destRow)
{
    Q_UNUSED(srcParent)
    Q_UNUSED(srcStart)
    Q_UNUSED(srcEnd)
    Q_UNUSED(destParent)
    Q_UNUSED(destRow)
    // we cheat and just act like the layout changed; this might benefit from some
    // optimization
    sourceLayoutAboutToBeChanged();
}

void KSelectionProxyModelPrivate::sourceRowsMoved(const QModelIndex &srcParent, int srcStart, int srcEnd, const QModelIndex &destParent, int destRow)
{
    Q_UNUSED(srcParent)
    Q_UNUSED(srcStart)
    Q_UNUSED(srcEnd)
    Q_UNUSED(destParent)
    Q_UNUSED(destRow)
    // we cheat and just act like the layout changed; this might benefit from some
    // optimization
    sourceLayoutChanged();
}

QModelIndex KSelectionProxyModelPrivate::mapParentToSource(const QModelIndex &proxyParent) const
{
    return m_mappedParents.rightToLeft(proxyParent);
}

QModelIndex KSelectionProxyModelPrivate::mapParentFromSource(const QModelIndex &sourceParent) const
{
    return m_mappedParents.leftToRight(sourceParent);
}

static bool indexIsValid(bool startWithChildTrees, int row, const QList<QPersistentModelIndex> &rootIndexList, const SourceIndexProxyRowMapping &mappedFirstChildren)
{
    if (!startWithChildTrees) {
        Q_ASSERT(rootIndexList.size() > row);
    } else {

        Q_ASSERT(!mappedFirstChildren.isEmpty());

        SourceIndexProxyRowMapping::right_const_iterator result = mappedFirstChildren.rightUpperBound(row) - 1;

        Q_ASSERT(result != mappedFirstChildren.rightEnd());
        const int proxyFirstRow = result.key();
        const QModelIndex sourceFirstChild = result.value();
        Q_ASSERT(proxyFirstRow >= 0);
        Q_ASSERT(sourceFirstChild.isValid());
        Q_ASSERT(sourceFirstChild.parent().isValid());
        Q_ASSERT(row <= proxyFirstRow + sourceFirstChild.model()->rowCount(sourceFirstChild.parent()));
    }
    return true;
}

QModelIndex KSelectionProxyModelPrivate::createTopLevelIndex(int row, int column) const
{
    Q_Q(const KSelectionProxyModel);

    Q_ASSERT(indexIsValid(m_startWithChildTrees, row, m_rootIndexList, m_mappedFirstChildren));
    return q->createIndex(row, column);
}

QModelIndex KSelectionProxyModelPrivate::mapTopLevelFromSource(const QModelIndex &sourceIndex) const
{
    Q_Q(const KSelectionProxyModel);

    const QModelIndex sourceParent = sourceIndex.parent();
    const int row = m_rootIndexList.indexOf(sourceIndex);
    if (row == -1) {
        return QModelIndex();
    }

    if (!m_startWithChildTrees) {
        Q_ASSERT(m_rootIndexList.size() > row);
        return q->createIndex(row, sourceIndex.column());
    }
    if (!m_rootIndexList.contains(sourceParent)) {
        return QModelIndex();
    }

    const QModelIndex firstChild = q->sourceModel()->index(0, 0, sourceParent);
    const int firstProxyRow = m_mappedFirstChildren.leftToRight(firstChild);

    return q->createIndex(firstProxyRow + sourceIndex.row(), sourceIndex.column());
}

QModelIndex KSelectionProxyModelPrivate::mapFromSource(const QModelIndex &sourceIndex) const
{
    Q_Q(const KSelectionProxyModel);

    const QModelIndex maybeMapped = mapParentFromSource(sourceIndex);
    if (maybeMapped.isValid()) {
//     Q_ASSERT((!d->m_startWithChildTrees && d->m_rootIndexList.contains(maybeMapped)) ? maybeMapped.row() < 0 : true );
        return maybeMapped;
    }
    const QModelIndex sourceParent = sourceIndex.parent();

    const QModelIndex proxyParent = mapParentFromSource(sourceParent);
    if (proxyParent.isValid()) {
        void *const parentId = m_parentIds.rightToLeft(proxyParent);
        static const int column = 0;
        return q->createIndex(sourceIndex.row(), column, parentId);
    }

    const QModelIndex firstChild = q->sourceModel()->index(0, 0, sourceParent);

    if (m_mappedFirstChildren.leftContains(firstChild)) {
        const int firstProxyRow = m_mappedFirstChildren.leftToRight(firstChild);
        return q->createIndex(firstProxyRow + sourceIndex.row(), sourceIndex.column());
    }
    return mapTopLevelFromSource(sourceIndex);
}

int KSelectionProxyModelPrivate::topLevelRowCount() const
{
    Q_Q(const KSelectionProxyModel);

    if (!m_startWithChildTrees) {
        return m_rootIndexList.size();
    }

    if (m_mappedFirstChildren.isEmpty()) {
        return 0;
    }

    const SourceIndexProxyRowMapping::right_const_iterator result = m_mappedFirstChildren.rightConstEnd() - 1;

    const int proxyFirstRow = result.key();
    const QModelIndex sourceFirstChild = result.value();
    Q_ASSERT(sourceFirstChild.isValid());
    const QModelIndex sourceParent = sourceFirstChild.parent();
    Q_ASSERT(sourceParent.isValid());
    return q->sourceModel()->rowCount(sourceParent) + proxyFirstRow;
}

bool KSelectionProxyModelPrivate::ensureMappable(const QModelIndex &parent) const
{
    Q_Q(const KSelectionProxyModel);

    if (isFlat()) {
        return true;
    }

    if (parentIsMappable(parent)) {
        return true;
    }

    QModelIndex ancestor = parent.parent();
    QModelIndexList ancestorList;
    while (ancestor.isValid()) {
        if (parentIsMappable(ancestor)) {
            break;
        } else {
            ancestorList.prepend(ancestor);
        }

        ancestor = ancestor.parent();
    }

    if (!ancestor.isValid())
        // @p parent is not a descendant of m_rootIndexList.
    {
        return false;
    }

    // sourceIndex can be mapped to the proxy. We just need to create mappings for its ancestors first.
    for (int i = 0; i < ancestorList.size(); ++i) {
        const QModelIndex existingAncestor = mapParentFromSource(ancestor);
        Q_ASSERT(existingAncestor.isValid());

        void *const ansId = m_parentIds.rightToLeft(existingAncestor);
        const QModelIndex newSourceParent = ancestorList.at(i);
        const QModelIndex newProxyParent = q->createIndex(newSourceParent.row(), newSourceParent.column(), ansId);

        void *const newId = m_voidPointerFactory.createPointer();
        m_parentIds.insert(newId, newProxyParent);
        m_mappedParents.insert(QPersistentModelIndex(newSourceParent), newProxyParent);
        ancestor = newSourceParent;
    }
    return true;
}

void KSelectionProxyModelPrivate::updateInternalTopIndexes(int start, int offset)
{
    updateInternalIndexes(QModelIndex(), start, offset);

    QHash<QPersistentModelIndex, int> updates;
    {
        SourceIndexProxyRowMapping::right_iterator it = m_mappedFirstChildren.rightLowerBound(start);
        const SourceIndexProxyRowMapping::right_iterator end = m_mappedFirstChildren.rightEnd();

        for (; it != end; ++it) {
            updates.insert(*it, it.key() + offset);
        }
    }
    {
        QHash<QPersistentModelIndex, int>::const_iterator it = updates.constBegin();
        const QHash<QPersistentModelIndex, int>::const_iterator end = updates.constEnd();

        for (; it != end; ++it) {
            m_mappedFirstChildren.insert(it.key(), it.value());
        }
    }
}

void KSelectionProxyModelPrivate::updateInternalIndexes(const QModelIndex &parent, int start, int offset)
{
    Q_Q(KSelectionProxyModel);

    Q_ASSERT(start + offset >= 0);
    Q_ASSERT(parent.isValid() ? parent.model() == q : true);

    if (isFlat()) {
        return;
    }

    SourceProxyIndexMapping::left_iterator mappedParentIt = m_mappedParents.leftBegin();

    QHash<void *, QModelIndex> updatedParentIds;
    QHash<QPersistentModelIndex, QModelIndex> updatedParents;

    for (; mappedParentIt != m_mappedParents.leftEnd(); ++mappedParentIt) {
        const QModelIndex proxyIndex = mappedParentIt.value();
        Q_ASSERT(proxyIndex.isValid());

        if (proxyIndex.row() < start) {
            continue;
        }

        const QModelIndex proxyParent = proxyIndex.parent();

        if (parent.isValid()) {
            if (proxyParent != parent) {
                continue;
            }
        } else {
            if (proxyParent.isValid()) {
                continue;
            }
        }
        Q_ASSERT(m_parentIds.rightContains(proxyIndex));
        void *const key = m_parentIds.rightToLeft(proxyIndex);

        const QModelIndex newIndex = q->createIndex(proxyIndex.row() + offset, proxyIndex.column(), proxyIndex.internalPointer());

        Q_ASSERT(newIndex.isValid());

        updatedParentIds.insert(key, newIndex);
        updatedParents.insert(mappedParentIt.key(), newIndex);
    }

    {
        QHash<QPersistentModelIndex, QModelIndex>::const_iterator it = updatedParents.constBegin();
        const QHash<QPersistentModelIndex, QModelIndex>::const_iterator end = updatedParents.constEnd();
        for (; it != end; ++it) {
            m_mappedParents.insert(it.key(), it.value());
        }
    }

    {
        QHash<void *, QModelIndex>::const_iterator it = updatedParentIds.constBegin();
        const QHash<void *, QModelIndex>::const_iterator end = updatedParentIds.constEnd();
        for (; it != end; ++it) {
            m_parentIds.insert(it.key(), it.value());
        }
    }
}

bool KSelectionProxyModelPrivate::parentAlreadyMapped(const QModelIndex &parent) const
{
    Q_Q(const KSelectionProxyModel);
    Q_UNUSED(q) // except in Q_ASSERT
    Q_ASSERT(parent.model() == q->sourceModel());
    return m_mappedParents.leftContains(parent);
}

bool KSelectionProxyModelPrivate::firstChildAlreadyMapped(const QModelIndex &firstChild) const
{
    Q_Q(const KSelectionProxyModel);
    Q_UNUSED(q) // except in Q_ASSERT
    Q_ASSERT(firstChild.model() == q->sourceModel());
    return m_mappedFirstChildren.leftContains(firstChild);
}

void KSelectionProxyModelPrivate::createFirstChildMapping(const QModelIndex &parent, int proxyRow) const
{
    Q_Q(const KSelectionProxyModel);

    Q_ASSERT(parent.isValid() ? parent.model() == q->sourceModel() : true);

    static const int column = 0;
    static const int row = 0;

    const QPersistentModelIndex srcIndex = q->sourceModel()->index(row, column, parent);

    if (firstChildAlreadyMapped(srcIndex)) {
        return;
    }

    Q_ASSERT(srcIndex.isValid());
    m_mappedFirstChildren.insert(srcIndex, proxyRow);
}

void KSelectionProxyModelPrivate::createParentMappings(const QModelIndex &parent, int start, int end) const
{
    if (isFlat()) {
        return;
    }

    Q_Q(const KSelectionProxyModel);

    Q_ASSERT(parent.isValid() ? parent.model() == q->sourceModel() : true);

    static const int column = 0;

    for (int row = start; row <= end; ++row) {
        const QModelIndex srcIndex = q->sourceModel()->index(row, column, parent);
        Q_ASSERT(srcIndex.isValid());
        if (!q->sourceModel()->hasChildren(srcIndex) || parentAlreadyMapped(srcIndex)) {
            continue;
        }

        const QModelIndex proxyIndex = mapFromSource(srcIndex);
        if (!proxyIndex.isValid()) {
            return;    // If one of them is not mapped, its siblings won't be either
        }

        void *const newId = m_voidPointerFactory.createPointer();
        m_parentIds.insert(newId, proxyIndex);
        Q_ASSERT(srcIndex.isValid());
        m_mappedParents.insert(QPersistentModelIndex(srcIndex), proxyIndex);
    }
}

void KSelectionProxyModelPrivate::removeFirstChildMappings(int start, int end)
{
    SourceIndexProxyRowMapping::right_iterator it = m_mappedFirstChildren.rightLowerBound(start);
    const SourceIndexProxyRowMapping::right_iterator endIt = m_mappedFirstChildren.rightUpperBound(end);
    while (it != endIt) {
        it = m_mappedFirstChildren.eraseRight(it);
    }
}

void KSelectionProxyModelPrivate::removeParentMappings(const QModelIndex &parent, int start, int end)
{
    Q_Q(KSelectionProxyModel);

    Q_ASSERT(parent.isValid() ? parent.model() == q : true);

    SourceProxyIndexMapping::right_iterator it = m_mappedParents.rightBegin();
    SourceProxyIndexMapping::right_iterator endIt = m_mappedParents.rightEnd();

    const bool flatList = isFlat();

    while (it != endIt) {
        if (it.key().row() >= start && it.key().row() <= end) {
            const QModelIndex sourceParent = it.value();
            const QModelIndex proxyGrandParent = mapParentFromSource(sourceParent.parent());
            if (proxyGrandParent == parent) {
                if (!flatList)
                    // Due to recursive calls, we could have several iterators on the container
                    // when erase is called. That's safe according to the QHash::iterator docs though.
                {
                    removeParentMappings(it.key(), 0, q->sourceModel()->rowCount(it.value()) - 1);
                }

                m_parentIds.removeRight(it.key());
                it = m_mappedParents.eraseRight(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }
}

QModelIndex KSelectionProxyModelPrivate::mapTopLevelToSource(int row, int column) const
{
    if (!m_startWithChildTrees) {
        const QModelIndex idx = m_rootIndexList.at(row);
        return idx.sibling(idx.row(), column);
    }

    if (m_mappedFirstChildren.isEmpty()) {
        return QModelIndex();
    }

    SourceIndexProxyRowMapping::right_iterator result = m_mappedFirstChildren.rightUpperBound(row) - 1;

    Q_ASSERT(result != m_mappedFirstChildren.rightEnd());

    const int proxyFirstRow = result.key();
    const QModelIndex sourceFirstChild = result.value();
    Q_ASSERT(sourceFirstChild.isValid());
    return sourceFirstChild.sibling(row - proxyFirstRow, column);
}

void KSelectionProxyModelPrivate::removeSelectionFromProxy(const QItemSelection &selection)
{
    Q_Q(KSelectionProxyModel);
    if (selection.isEmpty()) {
        return;
    }

    QList<QPersistentModelIndex>::iterator rootIt = m_rootIndexList.begin();
    const QList<QPersistentModelIndex>::iterator rootEnd = m_rootIndexList.end();
    int proxyStartRemove = 0;

    for (; rootIt != rootEnd; ++rootIt) {
        if (rootWillBeRemoved(selection, *rootIt)) {
            break;
        } else {
            if (m_startWithChildTrees) {
                auto rc = q->sourceModel()->rowCount(*rootIt);
                proxyStartRemove += rc;
            } else {
                ++proxyStartRemove;
            }
        }
    }

    if (rootIt == rootEnd) {
        return;
    }

    int proxyEndRemove = proxyStartRemove;

    QList<QPersistentModelIndex>::iterator rootRemoveStart = rootIt;

    for (; rootIt != rootEnd; ++rootIt) {
        if (!rootWillBeRemoved(selection, *rootIt)) {
            break;
        }
        q->rootIndexAboutToBeRemoved(*rootIt);
        if (m_startWithChildTrees) {
            auto rc = q->sourceModel()->rowCount(*rootIt);
            proxyEndRemove += rc;
        } else {
            ++proxyEndRemove;
        }
    }

    --proxyEndRemove;
    if (proxyEndRemove >= proxyStartRemove) {
        q->beginRemoveRows(QModelIndex(), proxyStartRemove, proxyEndRemove);

        rootIt = m_rootIndexList.erase(rootRemoveStart, rootIt);

        removeParentMappings(QModelIndex(), proxyStartRemove, proxyEndRemove);
        if (m_startWithChildTrees) {
            removeFirstChildMappings(proxyStartRemove, proxyEndRemove);
        }
        updateInternalTopIndexes(proxyEndRemove + 1, -1 * (proxyEndRemove - proxyStartRemove + 1));

        q->endRemoveRows();
    } else {
        rootIt = m_rootIndexList.erase(rootRemoveStart, rootIt);
    }
    if (rootIt != rootEnd) {
        removeSelectionFromProxy(selection);
    }
}

void KSelectionProxyModelPrivate::selectionChanged(const QItemSelection &_selected, const QItemSelection &_deselected)
{
    Q_Q(KSelectionProxyModel);

    if (!q->sourceModel() || (_selected.isEmpty() && _deselected.isEmpty())) {
        return;
    }

    if (m_sourceModelResetting) {
        return;
    }

    if (m_rowsInserted || m_rowsRemoved) {
        m_pendingSelectionChanges.append(PendingSelectionChange(_selected, _deselected));
        return;
    }

    // Any deselected indexes in the m_rootIndexList are removed. Then, any
    // indexes in the selected range which are not a descendant of one of the already selected indexes
    // are inserted into the model.
    //
    // All ranges from the selection model need to be split into individual rows. Ranges which are contiguous in
    // the selection model may not be contiguous in the source model if there's a sort filter proxy model in the chain.
    //
    // Some descendants of deselected indexes may still be selected. The ranges in m_selectionModel->selection()
    // are examined. If any of the ranges are descendants of one of the
    // indexes in deselected, they are added to the ranges to be inserted into the model.
    //
    // The new indexes are inserted in sorted order.

    const QItemSelection selected = kNormalizeSelection(m_indexMapper->mapSelectionRightToLeft(_selected));
    const QItemSelection deselected = kNormalizeSelection(m_indexMapper->mapSelectionRightToLeft(_deselected));

#if QT_VERSION < 0x040800
    // The QItemSelectionModel sometimes doesn't remove deselected items from its selection
    // Fixed in Qt 4.8 : http://qt.gitorious.org/qt/qt/merge_requests/2403
    QItemSelection reportedSelection = m_selectionModel->selection();
    reportedSelection.merge(deselected, QItemSelectionModel::Deselect);
    QItemSelection fullSelection = m_indexMapper->mapSelectionRightToLeft(reportedSelection);
#else
    QItemSelection fullSelection = m_indexMapper->mapSelectionRightToLeft(m_selectionModel->selection());
#endif

    fullSelection = kNormalizeSelection(fullSelection);

    QItemSelection newRootRanges;
    QItemSelection removedRootRanges;
    if (!m_includeAllSelected) {
        newRootRanges = getRootRanges(selected);

        QItemSelection existingSelection = fullSelection;
        // What was selected before the selection was made.
        existingSelection.merge(selected, QItemSelectionModel::Deselect);

        // This is similar to m_rootRanges, but that m_rootRanges at this point still contains the roots
        // of deselected and existingRootRanges does not.

        const QItemSelection existingRootRanges = getRootRanges(existingSelection);
        {
            QMutableListIterator<QItemSelectionRange> i(newRootRanges);
            while (i.hasNext()) {
                const QItemSelectionRange range = i.next();
                const QModelIndex topLeft = range.topLeft();
                if (isDescendantOf(existingRootRanges, topLeft)) {
                    i.remove();
                }
            }
        }

        QItemSelection exposedSelection;
        {
            QItemSelection deselectedRootRanges = getRootRanges(deselected);
            QListIterator<QItemSelectionRange> i(deselectedRootRanges);
            while (i.hasNext()) {
                const QItemSelectionRange range = i.next();
                const QModelIndex topLeft = range.topLeft();
                // Consider this:
                //
                // - A
                // - - B
                // - - - C
                // - - - - D
                //
                // B and D were selected, then B was deselected and C was selected in one go.
                if (!isDescendantOf(existingRootRanges, topLeft)) {
                    // B is topLeft and fullSelection contains D.
                    // B is not a descendant of D.

                    // range is not a descendant of the selection, but maybe the selection is a descendant of range.
                    // no need to check selected here. That's already in newRootRanges.
                    // existingRootRanges and newRootRanges do not overlap.
                    for (const QItemSelectionRange &selectedRange : existingRootRanges) {
                        const QModelIndex selectedRangeTopLeft = selectedRange.topLeft();
                        // existingSelection (and selectedRangeTopLeft) is D.
                        // D is a descendant of B, so when B was removed, D might have been exposed as a root.
                        if (isDescendantOf(range, selectedRangeTopLeft)
                                // But D is also a descendant of part of the new selection C, which is already set to be a new root
                                // so D would not be added to exposedSelection because C is in newRootRanges.
                                && !isDescendantOf(newRootRanges, selectedRangeTopLeft)) {
                            exposedSelection.append(selectedRange);
                        }
                    }
                    removedRootRanges << range;
                }
            }
        }

        QItemSelection obscuredRanges;
        {
            QListIterator<QItemSelectionRange> i(existingRootRanges);
            while (i.hasNext()) {
                const QItemSelectionRange range = i.next();
                if (isDescendantOf(newRootRanges, range.topLeft())) {
                    obscuredRanges << range;
                }
            }
        }
        removedRootRanges << getRootRanges(obscuredRanges);
        newRootRanges << getRootRanges(exposedSelection);

        removedRootRanges = kNormalizeSelection(removedRootRanges);
        newRootRanges = kNormalizeSelection(newRootRanges);
    } else {
        removedRootRanges = deselected;
        newRootRanges = selected;
    }

    removeSelectionFromProxy(removedRootRanges);

    if (!m_selectionModel->hasSelection()) {
        Q_ASSERT(m_rootIndexList.isEmpty());
        Q_ASSERT(m_mappedFirstChildren.isEmpty());
        Q_ASSERT(m_mappedParents.isEmpty());
        Q_ASSERT(m_parentIds.isEmpty());
    }

    insertSelectionIntoProxy(newRootRanges);
}

int KSelectionProxyModelPrivate::getTargetRow(int rootListRow)
{
    Q_Q(KSelectionProxyModel);
    if (!m_startWithChildTrees) {
        return rootListRow;
    }

    --rootListRow;
    while (rootListRow >= 0) {
        const QModelIndex idx = m_rootIndexList.at(rootListRow);
        Q_ASSERT(idx.isValid());
        const int rowCount = q->sourceModel()->rowCount(idx);
        if (rowCount > 0) {
            static const int column = 0;
            const QModelIndex srcIdx = q->sourceModel()->index(rowCount - 1, column, idx);
            const QModelIndex proxyLastChild = mapFromSource(srcIdx);
            return proxyLastChild.row() + 1;
        }
        --rootListRow;
    }
    return 0;
}

void KSelectionProxyModelPrivate::insertSelectionIntoProxy(const QItemSelection &selection)
{
    Q_Q(KSelectionProxyModel);

    if (selection.isEmpty()) {
        return;
    }

    const auto lst = selection.indexes();
    for (const QModelIndex &newIndex : lst) {
        Q_ASSERT(newIndex.model() == q->sourceModel());
        if (newIndex.column() > 0) {
            continue;
        }
        if (m_startWithChildTrees) {
            const int rootListRow = getRootListRow(m_rootIndexList, newIndex);
            Q_ASSERT(q->sourceModel() == newIndex.model());
            const int rowCount = q->sourceModel()->rowCount(newIndex);
            const int startRow = getTargetRow(rootListRow);

            if (rowCount == 0) {
                // Even if the newindex doesn't have any children to put into the model yet,
                // We still need to make sure its future children are inserted into the model.
                m_rootIndexList.insert(rootListRow, newIndex);
                if (!m_resetting || m_layoutChanging) {
                    emit q->rootIndexAdded(newIndex);
                }
                continue;
            }
            if (!m_resetting) {
                q->beginInsertRows(QModelIndex(), startRow, startRow + rowCount - 1);
            }
            Q_ASSERT(newIndex.isValid());
            m_rootIndexList.insert(rootListRow, newIndex);
            if (!m_resetting || m_layoutChanging) {
                emit q->rootIndexAdded(newIndex);
            }

            int _start = 0;
            for (int i = 0; i < rootListRow; ++i) {
                _start += q->sourceModel()->rowCount(m_rootIndexList.at(i));
            }

            updateInternalTopIndexes(_start, rowCount);
            createFirstChildMapping(newIndex, _start);
            createParentMappings(newIndex, 0, rowCount - 1);

            if (!m_resetting) {
                q->endInsertRows();
            }

        } else {
            const int row = getRootListRow(m_rootIndexList, newIndex);
            if (!m_resetting) {
                q->beginInsertRows(QModelIndex(), row, row);
            }

            Q_ASSERT(newIndex.isValid());
            m_rootIndexList.insert(row, newIndex);

            if (!m_resetting || m_layoutChanging) {
                emit q->rootIndexAdded(newIndex);
            }
            Q_ASSERT(m_rootIndexList.size() > row);
            updateInternalIndexes(QModelIndex(), row, 1);
            createParentMappings(newIndex.parent(), newIndex.row(), newIndex.row());

            if (!m_resetting) {
                q->endInsertRows();
            }
        }
    }
    q->rootSelectionAdded(selection);
}
