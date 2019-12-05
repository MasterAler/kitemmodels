#include "krecursivefilterproxymodel_p.h"

void KRecursiveFilterProxyModelPrivate::sourceDataChanged(const QModelIndex &source_top_left, const QModelIndex &source_bottom_right, const QVector<int> &roles)
{
    QModelIndex source_parent = source_top_left.parent();
    Q_ASSERT(source_bottom_right.parent() == source_parent); // don't know how to handle different parents in this code...

    // Tell the world.
    invokeDataChanged(source_top_left, source_bottom_right, roles);

    // We can't find out if the change really matters to us or not, for a lack of a dataAboutToBeChanged signal (or a cache).
    // TODO: add a set of roles that we care for, so we can at least ignore the rest.

    // Even if we knew the visibility was just toggled, we also can't find out what
    // was the last filtered out ascendant (on show, like sourceRowsAboutToBeInserted does)
    // or the last to-be-filtered-out ascendant (on hide, like sourceRowsRemoved does)
    // So we have to refresh all parents.
    QModelIndex sourceParent = source_parent;
    while (sourceParent.isValid()) {
        invokeDataChanged(sourceParent, sourceParent, roles);
        sourceParent = sourceParent.parent();
    }
}

QModelIndex KRecursiveFilterProxyModelPrivate::lastFilteredOutAscendant(const QModelIndex &idx)
{
    Q_Q(KRecursiveFilterProxyModel);
    QModelIndex last = idx;
    QModelIndex index = idx.parent();
    while (index.isValid() && !q->filterAcceptsRow(index.row(), index.parent())) {
        last = index;
        index = index.parent();
    }
    return last;
}

void KRecursiveFilterProxyModelPrivate::sourceRowsAboutToBeInserted(const QModelIndex &source_parent, int start, int end)
{
    Q_Q(KRecursiveFilterProxyModel);

    if (!source_parent.isValid() || q->filterAcceptsRow(source_parent.row(), source_parent.parent())) {
        // If the parent is already in the model (directly or indirectly), we can just pass on the signal.
        invokeRowsAboutToBeInserted(source_parent, start, end);
        completeInsert = true;
    } else {
        // OK, so parent is not in the model.
        // Maybe the grand parent neither.. Go up until the first one that is.
        lastHiddenAscendantForInsert = lastFilteredOutAscendant(source_parent);
    }
}

void KRecursiveFilterProxyModelPrivate::sourceRowsInserted(const QModelIndex &source_parent, int start, int end)
{
    Q_Q(KRecursiveFilterProxyModel);

    if (completeInsert) {
        // If the parent is already in the model, we can just pass on the signal.
        completeInsert = false;
        invokeRowsInserted(source_parent, start, end);
        return;
    }

    bool requireRow = false;
    for (int row = start; row <= end; ++row) {
        if (q->filterAcceptsRow(row, source_parent)) {
            requireRow = true;
            break;
        }
    }

    if (!requireRow) {
        // The new rows doesn't have any descendants that match the filter. Filter them out.
        return;
    }

    // Make QSFPM realize that lastHiddenAscendantForInsert should be shown now
    invokeDataChanged(lastHiddenAscendantForInsert, lastHiddenAscendantForInsert);
}

void KRecursiveFilterProxyModelPrivate::sourceRowsAboutToBeRemoved(const QModelIndex &source_parent, int start, int end)
{
    invokeRowsAboutToBeRemoved(source_parent, start, end);
}

void KRecursiveFilterProxyModelPrivate::sourceRowsRemoved(const QModelIndex &source_parent, int start, int end)
{
    Q_Q(KRecursiveFilterProxyModel);

    invokeRowsRemoved(source_parent, start, end);

    // Find out if removing this visible row means that some ascendant
    // row can now be hidden.
    // We go up until we find a row that should still be visible
    // and then make QSFPM re-evaluate the last one we saw before that, to hide it.

    QModelIndex toHide;
    QModelIndex sourceAscendant = source_parent;
    while (sourceAscendant.isValid()) {
        if (q->filterAcceptsRow(sourceAscendant.row(), sourceAscendant.parent())) {
            break;
        }
        toHide = sourceAscendant;
        sourceAscendant = sourceAscendant.parent();
    }
    if (toHide.isValid()) {
        invokeDataChanged(toHide, toHide);
    }
}
