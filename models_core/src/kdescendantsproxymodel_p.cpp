#include "kdescendantsproxymodel_p.h"

void KDescendantsProxyModelPrivate::resetInternalData()
{
    m_rowCount = 0;
    m_mapping.clear();
    m_layoutChangePersistentIndexes.clear();
    m_proxyIndexes.clear();
}

void KDescendantsProxyModelPrivate::synchronousMappingRefresh()
{
    m_rowCount = 0;
    m_mapping.clear();
    m_pendingParents.clear();

    m_pendingParents.append(QModelIndex());

    m_relayouting = true;
    while (!m_pendingParents.isEmpty()) {
        processPendingParents();
    }
    m_relayouting = false;
}

void KDescendantsProxyModelPrivate::scheduleProcessPendingParents() const
{
    const_cast<KDescendantsProxyModelPrivate *>(this)->processPendingParents();
}

void KDescendantsProxyModelPrivate::processPendingParents()
{
    Q_Q(KDescendantsProxyModel);
    const QVector<QPersistentModelIndex>::iterator begin = m_pendingParents.begin();
    QVector<QPersistentModelIndex>::iterator it = begin;

    const QVector<QPersistentModelIndex>::iterator end = m_pendingParents.end();

    QVector<QPersistentModelIndex> newPendingParents;

    while (it != end && it != m_pendingParents.end()) {
        const QModelIndex sourceParent = *it;
        if (!sourceParent.isValid() && m_rowCount > 0) {
            // It was removed from the source model before it was inserted.
            it = m_pendingParents.erase(it);
            continue;
        }
        const int rowCount = q->sourceModel()->rowCount(sourceParent);

        Q_ASSERT(rowCount > 0);
        const QPersistentModelIndex sourceIndex = q->sourceModel()->index(rowCount - 1, 0, sourceParent);

        Q_ASSERT(sourceIndex.isValid());

        const QModelIndex proxyParent = q->mapFromSource(sourceParent);

        Q_ASSERT(sourceParent.isValid() == proxyParent.isValid());
        const int proxyEndRow = proxyParent.row() + rowCount;
        const int proxyStartRow = proxyEndRow - rowCount + 1;

        if (!m_relayouting) {
            q->beginInsertRows(QModelIndex(), proxyStartRow, proxyEndRow);
        }

        updateInternalIndexes(proxyStartRow, rowCount);
        m_mapping.insert(sourceIndex, proxyEndRow);
        it = m_pendingParents.erase(it);
        m_rowCount += rowCount;

        if (!m_relayouting) {
            q->endInsertRows();
        }

        for (int sourceRow = 0; sourceRow < rowCount; ++sourceRow) {
            static const int column = 0;
            const QModelIndex child = q->sourceModel()->index(sourceRow, column, sourceParent);
            Q_ASSERT(child.isValid());

            if (q->sourceModel()->hasChildren(child)) {
                Q_ASSERT(q->sourceModel()->rowCount(child) > 0);
                newPendingParents.append(child);
            }
        }
    }
    m_pendingParents += newPendingParents;
    if (!m_pendingParents.isEmpty()) {
        processPendingParents();
    }
//   scheduleProcessPendingParents();
}

void KDescendantsProxyModelPrivate::updateInternalIndexes(int start, int offset)
{
    // TODO: Make KHash2Map support key updates and do this backwards.
    QHash<int, QPersistentModelIndex> updates;
    {
        Mapping::right_iterator it = m_mapping.rightLowerBound(start);
        const Mapping::right_iterator end = m_mapping.rightEnd();

        while (it != end) {
            updates.insert(it.key() + offset, *it);
            ++it;
        }
    }

    {
        QHash<int, QPersistentModelIndex>::const_iterator it = updates.constBegin();
        const QHash<int, QPersistentModelIndex>::const_iterator end = updates.constEnd();

        for (; it != end; ++it) {
            m_mapping.insert(it.value(), it.key());
        }
    }

}

void KDescendantsProxyModelPrivate::sourceRowsAboutToBeInserted(const QModelIndex &parent, int start, int end)
{
    Q_Q(KDescendantsProxyModel);

    if (!q->sourceModel()->hasChildren(parent)) {
        Q_ASSERT(q->sourceModel()->rowCount(parent) == 0);
        // parent was not a parent before.
        return;
    }

    int proxyStart = -1;

    const int rowCount = q->sourceModel()->rowCount(parent);

    if (rowCount > start) {
        const QModelIndex belowStart = q->sourceModel()->index(start, 0, parent);
        proxyStart = q->mapFromSource(belowStart).row();
    } else if (rowCount == 0) {
        proxyStart = q->mapFromSource(parent).row() + 1;
    } else {
        Q_ASSERT(rowCount == start);
        static const int column = 0;
        QModelIndex idx = q->sourceModel()->index(rowCount - 1, column, parent);
        while (q->sourceModel()->hasChildren(idx)) {
            Q_ASSERT(q->sourceModel()->rowCount(idx) > 0);
            idx = q->sourceModel()->index(q->sourceModel()->rowCount(idx) - 1, column, idx);
        }
        // The last item in the list is getting a sibling below it.
        proxyStart = q->mapFromSource(idx).row() + 1;
    }
    const int proxyEnd = proxyStart + (end - start);

    m_insertPair = qMakePair(proxyStart, proxyEnd);
    q->beginInsertRows(QModelIndex(), proxyStart, proxyEnd);
}

void KDescendantsProxyModelPrivate::sourceRowsInserted(const QModelIndex &parent, int start, int end)
{
    Q_Q(KDescendantsProxyModel);

    Q_ASSERT(q->sourceModel()->index(start, 0, parent).isValid());

    const int rowCount = q->sourceModel()->rowCount(parent);
    Q_ASSERT(rowCount > 0);

    const int difference = end - start + 1;

    if (rowCount == difference) {
        // @p parent was not a parent before.
        m_pendingParents.append(parent);
        scheduleProcessPendingParents();
        return;
    }

    const int proxyStart = m_insertPair.first;

    Q_ASSERT(proxyStart >= 0);

    updateInternalIndexes(proxyStart, difference);

    if (rowCount - 1 == end) {
        // The previously last row (the mapped one) is no longer the last.
        // For example,

        // - A            - A           0
        // - - B          - B           1
        // - - C          - C           2
        // - - - D        - D           3
        // - - - E   ->   - E           4
        // - - F          - F           5
        // - - G     ->   - G           6
        // - H            - H           7
        // - I       ->   - I           8

        // As last children, E, F and G have mappings.
        // Consider that 'J' is appended to the children of 'C', below 'E'.

        // - A            - A           0
        // - - B          - B           1
        // - - C          - C           2
        // - - - D        - D           3
        // - - - E   ->   - E           4
        // - - - J        - ???         5
        // - - F          - F           6
        // - - G     ->   - G           7
        // - H            - H           8
        // - I       ->   - I           9

        // The updateInternalIndexes call above will have updated the F and G mappings correctly because proxyStart is 5.
        // That means that E -> 4 was not affected by the updateInternalIndexes call.
        // Now the mapping for E -> 4 needs to be updated so that it's a mapping for J -> 5.

        Q_ASSERT(!m_mapping.isEmpty());
        static const int column = 0;
        const QModelIndex oldIndex = q->sourceModel()->index(rowCount - 1 - difference, column, parent);
        Q_ASSERT(m_mapping.leftContains(oldIndex));

        const QModelIndex newIndex = q->sourceModel()->index(rowCount - 1, column, parent);

        QModelIndex indexAbove = oldIndex;

        if (start > 0) {
            // If we have something like this:
            //
            // - A
            // - - B
            // - - C
            //
            // and we then insert D as a sibling of A below it, we need to remove the mapping for A,
            // and the row number used for D must take into account the descendants of A.

            while (q->sourceModel()->hasChildren(indexAbove)) {
                Q_ASSERT(q->sourceModel()->rowCount(indexAbove) > 0);
                indexAbove = q->sourceModel()->index(q->sourceModel()->rowCount(indexAbove) - 1,  column, indexAbove);
            }
            Q_ASSERT(q->sourceModel()->rowCount(indexAbove) == 0);
        }

        Q_ASSERT(m_mapping.leftContains(indexAbove));

        const int newProxyRow = m_mapping.leftToRight(indexAbove) + difference;

        // oldIndex is E in the source. proxyRow is 4.
        m_mapping.removeLeft(oldIndex);

        // newIndex is J. (proxyRow + difference) is 5.
        m_mapping.insert(newIndex, newProxyRow);
    }

    for (int row = start; row <= end; ++row) {
        static const int column = 0;
        const QModelIndex idx = q->sourceModel()->index(row, column, parent);
        Q_ASSERT(idx.isValid());
        if (q->sourceModel()->hasChildren(idx)) {
            Q_ASSERT(q->sourceModel()->rowCount(idx) > 0);
            m_pendingParents.append(idx);
        }
    }

    m_rowCount += difference;

    q->endInsertRows();
    scheduleProcessPendingParents();
}

void KDescendantsProxyModelPrivate::sourceRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
    Q_Q(KDescendantsProxyModel);

    const int proxyStart = q->mapFromSource(q->sourceModel()->index(start, 0, parent)).row();

    static const int column = 0;
    QModelIndex idx = q->sourceModel()->index(end, column, parent);
    while (q->sourceModel()->hasChildren(idx)) {
        Q_ASSERT(q->sourceModel()->rowCount(idx) > 0);
        idx = q->sourceModel()->index(q->sourceModel()->rowCount(idx) - 1, column, idx);
    }
    const int proxyEnd = q->mapFromSource(idx).row();

    m_removePair = qMakePair(proxyStart, proxyEnd);

    q->beginRemoveRows(QModelIndex(), proxyStart, proxyEnd);
}

static QModelIndex getFirstDeepest(QAbstractItemModel *model, const QModelIndex &parent, int *count)
{
    static const int column = 0;
    Q_ASSERT(model->hasChildren(parent));
    Q_ASSERT(model->rowCount(parent) > 0);
    for (int row = 0; row < model->rowCount(parent); ++row) {
        (*count)++;
        const QModelIndex child = model->index(row, column, parent);
        Q_ASSERT(child.isValid());
        if (model->hasChildren(child)) {
            return getFirstDeepest(model, child, count);
        }
    }
    return model->index(model->rowCount(parent) - 1, column, parent);
}

void KDescendantsProxyModelPrivate::sourceRowsRemoved(const QModelIndex &parent, int start, int end)
{
    Q_Q(KDescendantsProxyModel);
    Q_UNUSED(end)

    const int rowCount = q->sourceModel()->rowCount(parent);

    const int proxyStart = m_removePair.first;
    const int proxyEnd = m_removePair.second;

    const int difference = proxyEnd - proxyStart + 1;
    {
        Mapping::right_iterator it = m_mapping.rightLowerBound(proxyStart);
        const Mapping::right_iterator endIt = m_mapping.rightUpperBound(proxyEnd);

        if (endIt != m_mapping.rightEnd())
            while (it != endIt) {
                it = m_mapping.eraseRight(it);
            }
        else
            while (it != m_mapping.rightUpperBound(proxyEnd)) {
                it = m_mapping.eraseRight(it);
            }
    }

    m_removePair = qMakePair(-1, -1);
    m_rowCount -= difference;
    Q_ASSERT(m_rowCount >= 0);

    updateInternalIndexes(proxyStart, -1 * difference);

    if (rowCount != start || rowCount == 0) {
        q->endRemoveRows();
        return;
    }

    static const int column = 0;
    const QModelIndex newEnd = q->sourceModel()->index(rowCount - 1, column, parent);
    Q_ASSERT(newEnd.isValid());

    if (m_mapping.isEmpty()) {
        m_mapping.insert(newEnd, newEnd.row());
        q->endRemoveRows();
        return;
    }
    if (q->sourceModel()->hasChildren(newEnd)) {
        int count = 0;
        const QModelIndex firstDeepest = getFirstDeepest(q->sourceModel(), newEnd, &count);
        Q_ASSERT(firstDeepest.isValid());
        const int firstDeepestProxy = m_mapping.leftToRight(firstDeepest);

        m_mapping.insert(newEnd, firstDeepestProxy - count);
        q->endRemoveRows();
        return;
    }
    Mapping::right_iterator lowerBound = m_mapping.rightLowerBound(proxyStart);
    if (lowerBound == m_mapping.rightEnd()) {
        int proxyRow = (lowerBound - 1).key();

        for (int row = newEnd.row(); row >= 0; --row) {
            const QModelIndex newEndSibling = q->sourceModel()->index(row, column, parent);
            if (!q->sourceModel()->hasChildren(newEndSibling)) {
                ++proxyRow;
            } else {
                break;
            }
        }
        m_mapping.insert(newEnd, proxyRow);
        q->endRemoveRows();
        return;
    } else if (lowerBound == m_mapping.rightBegin()) {
        int proxyRow = rowCount - 1;
        QModelIndex trackedParent = parent;
        while (trackedParent.isValid()) {
            proxyRow += (trackedParent.row() + 1);
            trackedParent = trackedParent.parent();
        }
        m_mapping.insert(newEnd, proxyRow);
        q->endRemoveRows();
        return;
    }
    const Mapping::right_iterator boundAbove = lowerBound - 1;

    QVector<QModelIndex> targetParents;
    targetParents.push_back(parent);
    {
        QModelIndex target = parent;
        int count = 0;
        while (target.isValid()) {
            if (target == boundAbove.value()) {
                m_mapping.insert(newEnd, count + boundAbove.key() + newEnd.row() + 1);
                q->endRemoveRows();
                return;
            }
            count += (target.row() + 1);
            target = target.parent();
            if (target.isValid()) {
                targetParents.push_back(target);
            }
        }
    }

    QModelIndex boundParent = boundAbove.value().parent();
    QModelIndex prevParent = boundParent;
    Q_ASSERT(boundParent.isValid());
    while (boundParent.isValid()) {
        prevParent = boundParent;
        boundParent = boundParent.parent();

        if (targetParents.contains(prevParent)) {
            break;
        }

        if (!m_mapping.leftContains(prevParent)) {
            break;
        }

        if (m_mapping.leftToRight(prevParent) > boundAbove.key()) {
            break;
        }
    }

    QModelIndex trackedParent = parent;

    int proxyRow = boundAbove.key();

    Q_ASSERT(prevParent.isValid());
    proxyRow -= prevParent.row();
    while (trackedParent != boundParent) {
        proxyRow += (trackedParent.row() + 1);
        trackedParent = trackedParent.parent();
    }
    m_mapping.insert(newEnd, proxyRow + newEnd.row());
    q->endRemoveRows();
}

void KDescendantsProxyModelPrivate::sourceRowsAboutToBeMoved(const QModelIndex &srcParent, int srcStart, int srcEnd, const QModelIndex &destParent, int destStart)
{
    Q_UNUSED(srcParent)
    Q_UNUSED(srcStart)
    Q_UNUSED(srcEnd)
    Q_UNUSED(destParent)
    Q_UNUSED(destStart)
    sourceLayoutAboutToBeChanged();
}

void KDescendantsProxyModelPrivate::sourceRowsMoved(const QModelIndex &srcParent, int srcStart, int srcEnd, const QModelIndex &destParent, int destStart)
{
    Q_UNUSED(srcParent)
    Q_UNUSED(srcStart)
    Q_UNUSED(srcEnd)
    Q_UNUSED(destParent)
    Q_UNUSED(destStart)
    sourceLayoutChanged();
}

void KDescendantsProxyModelPrivate::sourceModelAboutToBeReset()
{
    Q_Q(KDescendantsProxyModel);
    q->beginResetModel();
}

void KDescendantsProxyModelPrivate::sourceModelReset()
{
    Q_Q(KDescendantsProxyModel);
    resetInternalData();
    if (q->sourceModel()->hasChildren()) {
        Q_ASSERT(q->sourceModel()->rowCount() > 0);
        m_pendingParents.append(QModelIndex());
        scheduleProcessPendingParents();
    }
    q->endResetModel();
}

void KDescendantsProxyModelPrivate::sourceLayoutAboutToBeChanged()
{
    Q_Q(KDescendantsProxyModel);

    if (m_ignoreNextLayoutChanged) {
        m_ignoreNextLayoutChanged = false;
        return;
    }

    if (m_mapping.isEmpty()) {
        return;
    }

    emit q->layoutAboutToBeChanged();

    QPersistentModelIndex srcPersistentIndex;
    const auto lst = q->persistentIndexList();
    for (const QPersistentModelIndex &proxyPersistentIndex : lst) {
        m_proxyIndexes << proxyPersistentIndex;
        Q_ASSERT(proxyPersistentIndex.isValid());
        srcPersistentIndex = q->mapToSource(proxyPersistentIndex);
        Q_ASSERT(srcPersistentIndex.isValid());
        m_layoutChangePersistentIndexes << srcPersistentIndex;
    }
}

void KDescendantsProxyModelPrivate::sourceLayoutChanged()
{
    Q_Q(KDescendantsProxyModel);

    if (m_ignoreNextLayoutAboutToBeChanged) {
        m_ignoreNextLayoutAboutToBeChanged = false;
        return;
    }

    if (m_mapping.isEmpty()) {
        return;
    }

    m_rowCount = 0;

    synchronousMappingRefresh();

    for (int i = 0; i < m_proxyIndexes.size(); ++i) {
        q->changePersistentIndex(m_proxyIndexes.at(i), q->mapFromSource(m_layoutChangePersistentIndexes.at(i)));
    }

    m_layoutChangePersistentIndexes.clear();
    m_proxyIndexes.clear();

    emit q->layoutChanged();
}

void KDescendantsProxyModelPrivate::sourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_Q(KDescendantsProxyModel);
    Q_ASSERT(topLeft.model() == q->sourceModel());
    Q_ASSERT(bottomRight.model() == q->sourceModel());

    const int topRow = topLeft.row();
    const int bottomRow = bottomRight.row();

    for (int i = topRow; i <= bottomRow; ++i) {
        const QModelIndex sourceTopLeft = q->sourceModel()->index(i, topLeft.column(), topLeft.parent());
        Q_ASSERT(sourceTopLeft.isValid());
        const QModelIndex proxyTopLeft = q->mapFromSource(sourceTopLeft);
        // TODO. If an index does not have any descendants, then we can emit in blocks of rows.
        // As it is we emit once for each row.
        const QModelIndex sourceBottomRight = q->sourceModel()->index(i, bottomRight.column(), bottomRight.parent());
        const QModelIndex proxyBottomRight = q->mapFromSource(sourceBottomRight);
        Q_ASSERT(proxyTopLeft.isValid());
        Q_ASSERT(proxyBottomRight.isValid());
        emit q->dataChanged(proxyTopLeft, proxyBottomRight);
    }
}

void KDescendantsProxyModelPrivate::sourceModelDestroyed()
{
    resetInternalData();
}
