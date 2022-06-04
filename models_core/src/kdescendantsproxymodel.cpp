/*
    Copyright (c) 2009 Stephen Kelly <steveire@gmail.com>
    Copyright (C) 2010 Klarälvdalens Datakonsult AB,
        a KDAB Group company, info@kdab.net,
        author Stephen Kelly <stephen@kdab.com>

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

#include "kdescendantsproxymodel.h"
#include "kdescendantsproxymodel_p.h"


KDescendantsProxyModel::KDescendantsProxyModel(QObject *parent)
    : QAbstractProxyModel(parent)
    , d_ptr(new KDescendantsProxyModelPrivate(this))
{ }

KDescendantsProxyModel::~KDescendantsProxyModel()
{ }

QModelIndexList KDescendantsProxyModel::match(const QModelIndex &start, int role, const QVariant &value, int hits, Qt::MatchFlags flags) const
{
    return QAbstractProxyModel::match(start, role, value, hits, flags);
}

namespace {
    // we only work on DisplayRole for now
    static const QVector<int> changedRoles = {Qt::DisplayRole};
}

void KDescendantsProxyModel::setDisplayAncestorData(bool display)
{
    Q_D(KDescendantsProxyModel);
    bool displayChanged = (display != d->m_displayAncestorData);
    d->m_displayAncestorData = display;
    if (displayChanged) {
        emit displayAncestorDataChanged();
        // send out big hammer. Everything needs to be updated.
        emit dataChanged(index(0,0),index(rowCount()-1,columnCount()-1),  changedRoles);
    }
}

bool KDescendantsProxyModel::displayAncestorData() const
{
    Q_D(const KDescendantsProxyModel);
    return d->m_displayAncestorData;
}

void KDescendantsProxyModel::setAncestorSeparator(const QString &separator)
{
    Q_D(KDescendantsProxyModel);
    bool separatorChanged = (separator != d->m_ancestorSeparator);
    d->m_ancestorSeparator = separator;
    if (separatorChanged) {
        emit ancestorSeparatorChanged();
        if (d->m_displayAncestorData) {
            // send out big hammer. Everything needs to be updated.
            emit dataChanged(index(0,0),index(rowCount()-1,columnCount()-1),  changedRoles);
        }
    }
}

QString KDescendantsProxyModel::ancestorSeparator() const
{
    Q_D(const KDescendantsProxyModel);
    return d->m_ancestorSeparator;
}

void KDescendantsProxyModel::setSourceModel(QAbstractItemModel *_sourceModel)
{
    Q_D(KDescendantsProxyModel);

    beginResetModel();

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

    QAbstractProxyModel::setSourceModel(_sourceModel);

    if (_sourceModel) {
        for (int i = 0; i < int(sizeof modelSignals / sizeof * modelSignals); ++i) {
            connect(_sourceModel, modelSignals[i], d, proxySlots[i]);
        }
    }

    resetInternalData();
    if (_sourceModel && _sourceModel->hasChildren()) {
        d->synchronousMappingRefresh();
    }

    endResetModel();
    emit sourceModelChanged();
}

QModelIndex KDescendantsProxyModel::parent(const QModelIndex &index) const
{
    Q_UNUSED(index)
    return QModelIndex();
}

bool KDescendantsProxyModel::hasChildren(const QModelIndex &parent) const
{
    Q_D(const KDescendantsProxyModel);
    return !(d->m_mapping.isEmpty() || parent.isValid());
}

int KDescendantsProxyModel::rowCount(const QModelIndex &parent) const
{
    Q_D(const KDescendantsProxyModel);
    if (d->m_pendingParents.contains(parent) || parent.isValid() || !sourceModel()) {
        return 0;
    }

    if (d->m_mapping.isEmpty() && sourceModel()->hasChildren()) {
        Q_ASSERT(sourceModel()->rowCount() > 0);
        const_cast<KDescendantsProxyModelPrivate *>(d)->synchronousMappingRefresh();
    }
    return d->m_rowCount;
}

QModelIndex KDescendantsProxyModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return QModelIndex();
    }

    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    return createIndex(row, column);
}

QModelIndex KDescendantsProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    Q_D(const KDescendantsProxyModel);
    if (d->m_mapping.isEmpty() || !proxyIndex.isValid() || !sourceModel()) {
        return QModelIndex();
    }

    const Mapping::right_const_iterator result = d->m_mapping.rightLowerBound(proxyIndex.row());
    Q_ASSERT(result != d->m_mapping.rightEnd());

    const int proxyLastRow = result.key();
    const QModelIndex sourceLastChild = result.value();
    Q_ASSERT(sourceLastChild.isValid());

    // proxyLastRow is greater than proxyIndex.row().
    // sourceLastChild is vertically below the result we're looking for
    // and not necessarily in the correct parent.
    // We travel up through its parent hierarchy until we are in the
    // right parent, then return the correct sibling.

    // Source:           Proxy:    Row
    // - A               - A       - 0
    // - B               - B       - 1
    // - C               - C       - 2
    // - D               - D       - 3
    // - - E             - E       - 4
    // - - F             - F       - 5
    // - - G             - G       - 6
    // - - H             - H       - 7
    // - - I             - I       - 8
    // - - - J           - J       - 9
    // - - - K           - K       - 10
    // - - - L           - L       - 11
    // - - M             - M       - 12
    // - - N             - N       - 13
    // - O               - O       - 14

    // Note that L, N and O are lastChildIndexes, and therefore have a mapping. If we
    // are trying to map G from the proxy to the source, We at this point have an iterator
    // pointing to (L -> 11). The proxy row of G is 6. (proxyIndex.row() == 6). We seek the
    // sourceIndex which is vertically above L by the distance proxyLastRow - proxyIndex.row().
    // In this case the verticalDistance is 5.

    int verticalDistance = proxyLastRow - proxyIndex.row();

    // We traverse the ancestors of L, until we can index the desired row in the source.

    QModelIndex ancestor = sourceLastChild;
    while (ancestor.isValid()) {
        const int ancestorRow = ancestor.row();
        if (verticalDistance <= ancestorRow) {
            return ancestor.sibling(ancestorRow - verticalDistance, proxyIndex.column());
        }
        verticalDistance -= (ancestorRow + 1);
        ancestor = ancestor.parent();
    }
    Q_ASSERT(!"Didn't find target row.");
    return QModelIndex();
}

QModelIndex KDescendantsProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    Q_D(const KDescendantsProxyModel);

    if (!sourceModel()) {
        return QModelIndex();
    }

    if (d->m_mapping.isEmpty()) {
        return QModelIndex();
    }

    {
        // TODO: Consider a parent Mapping to speed this up.

        Mapping::right_const_iterator it = d->m_mapping.rightConstBegin();
        const Mapping::right_const_iterator end = d->m_mapping.rightConstEnd();
        const QModelIndex sourceParent = sourceIndex.parent();
        Mapping::right_const_iterator result = end;

        for (; it != end; ++it) {
            QModelIndex index = it.value();
            bool found_block = false;
            while (index.isValid()) {
                const QModelIndex ancestor = index.parent();
                if (ancestor == sourceParent && index.row() >= sourceIndex.row()) {
                    found_block = true;
                    if (result == end || it.key() < result.key()) {
                        result = it;
                        break; // Leave the while loop. index is still valid.
                    }
                }
                index = ancestor;
            }
            if (found_block && !index.isValid())
                // Looked through the ascendants of it.key() without finding sourceParent.
                // That means we've already got the result we need.
            {
                break;
            }
        }
        Q_ASSERT(result != end);
        const QModelIndex sourceLastChild = result.value();
        int proxyRow = result.key();
        QModelIndex index = sourceLastChild;
        while (index.isValid()) {
            const QModelIndex ancestor = index.parent();
            if (ancestor == sourceParent) {
                return createIndex(proxyRow - (index.row() - sourceIndex.row()), sourceIndex.column());
            }
            proxyRow -= (index.row() + 1);
            index = ancestor;
        }
        Q_ASSERT(!"Didn't find valid proxy mapping.");
        return QModelIndex();
    }

}

int KDescendantsProxyModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid() /* || rowCount(parent) == 0 */ || !sourceModel()) {
        return 0;
    }

    return sourceModel()->columnCount();
}

QVariant KDescendantsProxyModel::data(const QModelIndex &index, int role) const
{
    Q_D(const KDescendantsProxyModel);

    if (!sourceModel()) {
        return QVariant();
    }

    if (!index.isValid()) {
        return sourceModel()->data(index, role);
    }

    QModelIndex sourceIndex = mapToSource(index);

    if ((d->m_displayAncestorData) && (role == Qt::DisplayRole)) {
        if (!sourceIndex.isValid()) {
            return QVariant();
        }
        QString displayData = sourceIndex.data().toString();
        sourceIndex = sourceIndex.parent();
        while (sourceIndex.isValid()) {
            displayData.prepend(d->m_ancestorSeparator);
            displayData.prepend(sourceIndex.data().toString());
            sourceIndex = sourceIndex.parent();
        }
        return displayData;
    } else {
        return sourceIndex.data(role);
    }
}

QVariant KDescendantsProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (!sourceModel() || columnCount() <= section) {
        return QVariant();
    }

    return QAbstractProxyModel::headerData(section, orientation, role);
}

Qt::ItemFlags KDescendantsProxyModel::flags(const QModelIndex &index) const
{
    if (!index.isValid() || !sourceModel()) {
        return QAbstractProxyModel::flags(index);
    }

    const QModelIndex srcIndex = mapToSource(index);
    Q_ASSERT(srcIndex.isValid());
    return sourceModel()->flags(srcIndex);
}

QMimeData *KDescendantsProxyModel::mimeData(const QModelIndexList &indexes) const
{
    if (!sourceModel()) {
        return QAbstractProxyModel::mimeData(indexes);
    }
    Q_ASSERT(sourceModel());
    QModelIndexList sourceIndexes;
    for (const QModelIndex &index : indexes) {
        sourceIndexes << mapToSource(index);
    }
    return sourceModel()->mimeData(sourceIndexes);
}

QStringList KDescendantsProxyModel::mimeTypes() const
{
    if (!sourceModel()) {
        return QAbstractProxyModel::mimeTypes();
    }
    Q_ASSERT(sourceModel());
    return sourceModel()->mimeTypes();
}

Qt::DropActions KDescendantsProxyModel::supportedDropActions() const
{
    if (!sourceModel()) {
        return QAbstractProxyModel::supportedDropActions();
    }
    return sourceModel()->supportedDropActions();
}
