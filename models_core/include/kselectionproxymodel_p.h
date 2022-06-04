#ifndef KSELECTIONPROXYMODEL_P_H
#define KSELECTIONPROXYMODEL_P_H

#include "kselectionproxymodel.h"

#include <QObject>
#include <QStringList>
#include <QPointer>
#include <QItemSelectionRange>

#include "kmodelindexproxymapper.h"
#include "kbihash_p.h"
#include "kvoidpointerfactory_p.h"

typedef KBiHash<QPersistentModelIndex, QModelIndex> SourceProxyIndexMapping;
typedef KBiHash<void *, QModelIndex> ParentMapping;
typedef KHash2Map<QPersistentModelIndex, int> SourceIndexProxyRowMapping;

/**
  Return true if @p idx is a descendant of one of the indexes in @p list.
  Note that this returns false if @p list contains @p idx.
*/
template<typename ModelIndex>
bool isDescendantOf(const QList<ModelIndex> &list, const QModelIndex &idx)
{
    if (!idx.isValid()) {
        return false;
    }

    if (list.contains(idx)) {
        return false;
    }

    QModelIndex parent = idx.parent();
    while (parent.isValid()) {
        if (list.contains(parent)) {
            return true;
        }
        parent = parent.parent();
    }
    return false;
}

static bool isDescendantOf(const QItemSelection &selection, const QModelIndex &descendant)
{
    if (!descendant.isValid()) {
        return false;
    }

    if (selection.contains(descendant)) {
        return false;
    }

    QModelIndex parent = descendant.parent();
    while (parent.isValid()) {
        if (selection.contains(parent)) {
            return true;
        }

        parent = parent.parent();
    }
    return false;
}

static bool isDescendantOf(const QItemSelectionRange &range, const QModelIndex &descendant)
{
    if (!descendant.isValid()) {
        return false;
    }

    if (range.contains(descendant)) {
        return false;
    }

    QModelIndex parent = descendant.parent();
    while (parent.isValid()) {
        if (range.contains(parent)) {
            return true;
        }

        parent = parent.parent();
    }
    return false;
}

static int _getRootListRow(const QList<QModelIndexList> &rootAncestors, const QModelIndex &index)
{
    QModelIndex commonParent = index;
    QModelIndex youngestAncestor;

    int firstCommonParent = -1;
    int bestParentRow = -1;
    while (commonParent.isValid()) {
        youngestAncestor = commonParent;
        commonParent = commonParent.parent();

        for (int i = 0; i < rootAncestors.size(); ++i) {
            const QModelIndexList ancestorList = rootAncestors.at(i);

            const int parentRow = ancestorList.indexOf(commonParent);

            if (parentRow < 0) {
                continue;
            }

            if (parentRow > bestParentRow) {
                firstCommonParent = i;
                bestParentRow = parentRow;
            }
        }

        if (firstCommonParent >= 0) {
            break;
        }
    }

    // If @p list is non-empty, the invalid QModelIndex() will at least be found in ancestorList.
    Q_ASSERT(firstCommonParent >= 0);

    const QModelIndexList firstAnsList = rootAncestors.at(firstCommonParent);

    const QModelIndex eldestSibling = firstAnsList.value(bestParentRow + 1);

    if (eldestSibling.isValid()) {
        // firstCommonParent is a sibling of one of the ancestors of @p index.
        // It is the first index to share a common parent with one of the ancestors of @p index.
        if (eldestSibling.row() >= youngestAncestor.row()) {
            return firstCommonParent;
        }
    }

    int siblingOffset = 1;

    // The same commonParent might be common to several root indexes.
    // If this is the last in the list, it's the only match. We instruct the model
    // to insert the new index after it ( + siblingOffset).
    if (rootAncestors.size() <= firstCommonParent + siblingOffset) {
        return firstCommonParent + siblingOffset;
    }

    // A
    // - B
    //   - C
    //   - D
    //   - E
    // F
    //
    // F is selected, then C then D. When inserting D into the model, the commonParent is B (the parent of C).
    // The next existing sibling of B is F (in the proxy model). bestParentRow will then refer to an index on
    // the level of a child of F (which doesn't exist - Boom!). If it doesn't exist, then we've already found
    // the place to insert D
    QModelIndexList ansList = rootAncestors.at(firstCommonParent + siblingOffset);
    if (ansList.size() <= bestParentRow) {
        return firstCommonParent + siblingOffset;
    }

    QModelIndex nextParent = ansList.at(bestParentRow);
    while (nextParent == commonParent) {
        if (ansList.size() < bestParentRow + 1)
            // If the list is longer, it means that at the end of it is a descendant of the new index.
            // We insert the ancestors items first in that case.
        {
            break;
        }

        const QModelIndex nextSibling = ansList.value(bestParentRow + 1);

        if (!nextSibling.isValid()) {
            continue;
        }

        if (youngestAncestor.row() <= nextSibling.row()) {
            break;
        }

        siblingOffset++;

        if (rootAncestors.size() <= firstCommonParent + siblingOffset) {
            break;
        }

        ansList = rootAncestors.at(firstCommonParent + siblingOffset);

        // In the scenario above, E is selected after D, causing this loop to be entered,
        // and requiring a similar result if the next sibling in the proxy model does not have children.
        if (ansList.size() <= bestParentRow) {
            break;
        }

        nextParent = ansList.at(bestParentRow);
    }

    return firstCommonParent + siblingOffset;
}

/**
  Determines the correct location to insert @p index into @p list.
*/
template<typename ModelIndex>
static int getRootListRow(const QList<ModelIndex> &list, const QModelIndex &index)
{
    if (!index.isValid()) {
        return -1;
    }

    if (list.isEmpty()) {
        return 0;
    }

    // What's going on?
    // Consider a tree like
    //
    // A
    // - B
    // - - C
    // - - - D
    // - E
    // - F
    // - - G
    // - - - H
    // - I
    // - - J
    // - K
    //
    // If D, E and J are already selected, and H is newly selected, we need to put H between E and J in the proxy model.
    // To figure that out, we create a list for each already selected index of its ancestors. Then,
    // we climb the ancestors of H until we reach an index with siblings which have a descendant
    // selected (F above has siblings B, E and I which have descendants which are already selected).
    // Those child indexes are traversed to find the right sibling to put F beside.
    //
    // i.e., new items are inserted in the expected location.

    QList<QModelIndexList> rootAncestors;
    for (const QModelIndex &root : list) {
        QModelIndexList ancestors;
        ancestors << root;
        QModelIndex parent = root.parent();
        while (parent.isValid()) {
            ancestors.prepend(parent);
            parent = parent.parent();
        }
        ancestors.prepend(QModelIndex());
        rootAncestors << ancestors;
    }
    return _getRootListRow(rootAncestors, index);
}

/**
  Returns a selection in which no descendants of selected indexes are also themselves selected.
  For example,
  @code
    A
    - B
    C
    D
  @endcode
  If A, B and D are selected in @p selection, the returned selection contains only A and D.
*/
static QItemSelection getRootRanges(const QItemSelection &_selection)
{
    QItemSelection rootSelection;
    QItemSelection selection = _selection;
    QList<QItemSelectionRange>::iterator it = selection.begin();
    while (it != selection.end()) {
        if (!it->topLeft().parent().isValid()) {
            rootSelection.append(*it);
            it = selection.erase(it);
        } else {
            ++it;
        }
    }

    it = selection.begin();
    const QList<QItemSelectionRange>::iterator end = selection.end();
    while (it != end) {
        const QItemSelectionRange range = *it;
        it = selection.erase(it);

        if (isDescendantOf(rootSelection, range.topLeft()) || isDescendantOf(selection, range.topLeft())) {
            continue;
        }

        rootSelection << range;
    }
    return rootSelection;
}

/**
 */
struct RangeLessThan {
    bool operator()(const QItemSelectionRange &left, const QItemSelectionRange &right) const
    {
        if (right.model() == left.model()) {
            // parent has to be calculated, so we only do so once.
            const QModelIndex topLeftParent = left.parent();
            const QModelIndex otherTopLeftParent = right.parent();
            if (topLeftParent == otherTopLeftParent) {
                if (right.top() == left.top()) {
                    if (right.left() == left.left()) {
                        if (right.bottom() == left.bottom()) {
                            return left.right() < right.right();
                        }
                        return left.bottom() < right.bottom();
                    }
                    return left.left() < right.left();
                }
                return left.top() < right.top();
            }
            return topLeftParent < otherTopLeftParent;
        }
        return left.model() < right.model();
    }
};

static QItemSelection stableNormalizeSelection(const QItemSelection &selection)
{
    if (selection.size() <= 1) {
        return selection;
    }

    QItemSelection::const_iterator it = selection.begin();
    const QItemSelection::const_iterator end = selection.end();

    Q_ASSERT(it != end);
    QItemSelection::const_iterator scout = it + 1;

    QItemSelection result;
    while (scout != end) {
        Q_ASSERT(it != end);
        int bottom = it->bottom();
        while (scout != end && it->parent() == scout->parent() && bottom + 1 == scout->top()) {
            bottom = scout->bottom();
            ++scout;
        }
        if (bottom != it->bottom()) {
            const QModelIndex topLeft = it->topLeft();
            result << QItemSelectionRange(topLeft, topLeft.sibling(bottom, it->right()));
        }
        Q_ASSERT(it != scout);
        if (scout == end) {
            break;
        }
        if (it + 1 == scout) {
            result << *it;
        }
        it = scout;
        ++scout;
        if (scout == end) {
            result << *it;
        }
    }
    return result;
}

static QItemSelection kNormalizeSelection(QItemSelection selection)
{
    if (selection.size() <= 1) {
        return selection;
    }

    // KSelectionProxyModel has a strong assumption that
    // the views always select rows, so usually the
    // @p selection here contains ranges that span all
    // columns. However, if a QSortFilterProxyModel
    // is used too, it splits up the complete ranges into
    // one index per range. That confuses the data structures
    // held by this proxy (which keeps track of indexes in the
    // first column). As this proxy already assumes that if the
    // zeroth column is selected, then its entire row is selected,
    // we can safely remove the ranges which do not include
    // column 0 without a loss of functionality.
    QItemSelection::iterator i = selection.begin();
    while (i != selection.end()) {
        if (i->left() > 0) {
            i = selection.erase(i);
        } else {
            ++i;
        }
    }

    RangeLessThan lt;
    std::sort(selection.begin(), selection.end(), lt);
    return stableNormalizeSelection(selection);
}

class KSelectionProxyModelPrivate: public QObject
{
    KSelectionProxyModelPrivate(KSelectionProxyModel *model)
        : QObject(),
          q_ptr(model),
          m_indexMapper(nullptr),
          m_startWithChildTrees(false),
          m_omitChildren(false),
          m_omitDescendants(false),
          m_includeAllSelected(false),
          m_rowsInserted(false),
          m_rowsRemoved(false),
          m_recreateFirstChildMappingOnRemoval(false),
          m_rowsMoved(false),
          m_resetting(false),
          m_sourceModelResetting(false),
          m_doubleResetting(false),
          m_layoutChanging(false),
          m_ignoreNextLayoutAboutToBeChanged(false),
          m_ignoreNextLayoutChanged(false),
          m_selectionModel(nullptr),
          m_filterBehavior(KSelectionProxyModel::InvalidBehavior)
    { }

    Q_OBJECT
    Q_DECLARE_PUBLIC(KSelectionProxyModel)
    KSelectionProxyModel *const q_ptr;

    // A unique id is generated for each parent. It is used for the internalPointer of its children in the proxy
    // This is used to store a unique id for QModelIndexes in the proxy which have children.
    // If an index newly gets children it is added to this hash. If its last child is removed it is removed from this map.
    // If this map contains an index, that index hasChildren(). This hash is populated when new rows are inserted in the
    // source model, or a new selection is made.
    mutable ParentMapping m_parentIds;
    // This mapping maps indexes with children in the source to indexes with children in the proxy.
    // The order of indexes in this list is not relevant.
    mutable SourceProxyIndexMapping m_mappedParents;

    KVoidPointerFactory<> m_voidPointerFactory;

    /**
      Keeping Persistent indexes from this model in this model breaks in certain situations
      such as after source insert, but before calling endInsertRows in this model. In such a state,
      the persistent indexes are not updated, but the methods assume they are already uptodate.

      Instead of using persistentindexes for proxy indexes in m_mappedParents, we maintain them ourselves with this method.

      m_mappedParents and m_parentIds are affected.

      @p parent and @p start refer to the proxy model. Any rows >= @p start will be updated.
      @p offset is the amount that affected indexes will be changed.
    */
    void updateInternalIndexes(const QModelIndex &parent, int start, int offset);

    /**
     * Updates stored indexes in the proxy. Any proxy row >= @p start is changed by @p offset.
     *
     * This is only called to update indexes in the top level of the proxy. Most commonly that is
     *
     * m_mappedParents, m_parentIds and m_mappedFirstChildren are affected.
     */
    void updateInternalTopIndexes(int start, int offset);

    void updateFirstChildMapping(const QModelIndex &parent, int offset);

    bool isFlat() const
    {
        return m_omitChildren || (m_omitDescendants && m_startWithChildTrees);
    }

    /**
     * Tries to ensure that @p parent is a mapped parent in the proxy.
     * Returns true if parent is mappable in the model, and false otherwise.
     */
    bool ensureMappable(const QModelIndex &parent) const;
    bool parentIsMappable(const QModelIndex &parent) const
    {
        return parentAlreadyMapped(parent) || m_rootIndexList.contains(parent);
    }

    /**
     * Maps @p parent to source if it is already mapped, and otherwise returns an invalid QModelIndex.
     */
    QModelIndex mapFromSource(const QModelIndex &parent) const;

    /**
      Creates mappings in m_parentIds and m_mappedParents between the source and the proxy.

      This is not recursive
    */
    void createParentMappings(const QModelIndex &parent, int start, int end) const;
    void createFirstChildMapping(const QModelIndex &parent, int proxyRow) const;
    bool firstChildAlreadyMapped(const QModelIndex &firstChild) const;
    bool parentAlreadyMapped(const QModelIndex &parent) const;
    void removeFirstChildMappings(int start, int end);
    void removeParentMappings(const QModelIndex &parent, int start, int end);

    /**
      Given a QModelIndex in the proxy, return the corresponding QModelIndex in the source.

      This method works only if the index has children in the proxy model which already has a mapping from the source.

      This means that if the proxy is a flat list, this method will always return QModelIndex(). Additionally, it means that m_mappedParents is not populated automatically and must be populated manually.

      No new mapping is created by this method.
    */
    QModelIndex mapParentToSource(const QModelIndex &proxyParent) const;

    /**
      Given a QModelIndex in the source model, return the corresponding QModelIndex in the proxy.

      This method works only if the index has children in the proxy model which already has a mapping from the source.

      No new mapping is created by this method.
    */
    QModelIndex mapParentFromSource(const QModelIndex &sourceParent) const;

    QModelIndex mapTopLevelToSource(int row, int column) const;
    QModelIndex mapTopLevelFromSource(const QModelIndex &sourceIndex) const;
    QModelIndex createTopLevelIndex(int row, int column) const;
    int topLevelRowCount() const;

    void *parentId(const QModelIndex &proxyParent) const
    {
        return m_parentIds.rightToLeft(proxyParent);
    }
    QModelIndex parentForId(void *id) const
    {
        return m_parentIds.leftToRight(id);
    }

    // Only populated if m_startWithChildTrees.

    mutable SourceIndexProxyRowMapping m_mappedFirstChildren;

    // Source list is the selection in the source model.
    QList<QPersistentModelIndex> m_rootIndexList;

    KModelIndexProxyMapper *m_indexMapper;

    QPair<int, int> beginRemoveRows(const QModelIndex &parent, int start, int end) const;
    QPair<int, int> beginInsertRows(const QModelIndex &parent, int start, int end) const;
    void endRemoveRows(const QModelIndex &sourceParent, int proxyStart, int proxyEnd);
    void endInsertRows(const QModelIndex &parent, int start, int end);

private slots:
    void sourceRowsAboutToBeInserted(const QModelIndex &parent, int start, int end);
    void sourceRowsInserted(const QModelIndex &parent, int start, int end);
    void sourceRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end);
    void sourceRowsRemoved(const QModelIndex &parent, int start, int end);
    void sourceRowsAboutToBeMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destParent, int destRow);
    void sourceRowsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destParent, int destRow);
    void sourceModelAboutToBeReset();
    void sourceModelReset();
    void sourceLayoutAboutToBeChanged();
    void sourceLayoutChanged();
    void sourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void sourceModelDestroyed();

private:
    void emitContinuousRanges(const QModelIndex &sourceFirst, const QModelIndex &sourceLast,
                              const QModelIndex &proxyFirst, const QModelIndex &proxyLast);

    void removeSelectionFromProxy(const QItemSelection &selection);


    void resetInternalData();

    bool rootWillBeRemoved(const QItemSelection &selection,
                           const QModelIndex &root);

    /**
      When items are inserted or removed in the m_startWithChildTrees configuration,
      this method helps find the startRow for use emitting the signals from the proxy.
    */
    int getProxyInitialRow(const QModelIndex &parent) const;

    /**
      If m_startWithChildTrees is true, this method returns the row in the proxy model to insert newIndex
      items.

      This is a special case because the items above rootListRow in the list are not in the model, but
      their children are. Those children must be counted.

      If m_startWithChildTrees is false, this method returns @p rootListRow.
    */
    int getTargetRow(int rootListRow);

    /**
      Inserts the indexes in @p list into the proxy model.
    */
    void insertSelectionIntoProxy(const QItemSelection &selection);

    bool m_startWithChildTrees;
    bool m_omitChildren;
    bool m_omitDescendants;
    bool m_includeAllSelected;
    bool m_rowsInserted;
    bool m_rowsRemoved;
    bool m_recreateFirstChildMappingOnRemoval;
    QPair<int, int> m_proxyRemoveRows;
    bool m_rowsMoved;
    bool m_resetting;
    bool m_sourceModelResetting;
    bool m_doubleResetting;
    bool m_layoutChanging;
    bool m_ignoreNextLayoutAboutToBeChanged;
    bool m_ignoreNextLayoutChanged;
    QPointer<QItemSelectionModel> m_selectionModel;

    KSelectionProxyModel::FilterBehavior m_filterBehavior;

    QList<QPersistentModelIndex> m_layoutChangePersistentIndexes;
    QModelIndexList m_proxyIndexes;

    struct PendingSelectionChange {
        PendingSelectionChange() {}
        PendingSelectionChange(const QItemSelection &selected_, const QItemSelection &deselected_)
            : selected(selected_), deselected(deselected_)
        {

        }
        QItemSelection selected;
        QItemSelection deselected;
    };
    QVector<PendingSelectionChange> m_pendingSelectionChanges;
    QMetaObject::Connection selectionModelModelAboutToBeResetConnection;
    QMetaObject::Connection selectionModelModelResetConnection;
};

#endif // KSELECTIONPROXYMODEL_P_H
