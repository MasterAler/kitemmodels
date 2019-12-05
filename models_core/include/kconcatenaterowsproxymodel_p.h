#ifndef KCONCATENATEROWSPROXYMODEL_P_H
#define KCONCATENATEROWSPROXYMODEL_P_H

#include "kconcatenaterowsproxymodel.h"
#include <QObject>

class KConcatenateRowsProxyModelPrivate: public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(KConcatenateRowsProxyModel)

    KConcatenateRowsProxyModelPrivate(KConcatenateRowsProxyModel* model)
        : QObject()
        , q_ptr{model}
    {}

    KConcatenateRowsProxyModel * const   q_ptr;
    QList<QAbstractItemModel *>          m_models;
    int m_rowCount = 0; // have to maintain it here since we can't compute during model destruction

    // for layoutAboutToBeChanged/layoutChanged
    QVector<QPersistentModelIndex> layoutChangePersistentIndexes;
    QModelIndexList proxyIndexes;

    int computeRowsPrior(const QAbstractItemModel *sourceModel) const;
    QAbstractItemModel *sourceModelForRow(int row, int *sourceRow) const;

private slots:
    void slotRowsAboutToBeInserted(const QModelIndex &, int start, int end);
    void slotRowsInserted(const QModelIndex &, int start, int end);
    void slotRowsAboutToBeRemoved(const QModelIndex &, int start, int end);
    void slotRowsRemoved(const QModelIndex &, int start, int end);
    void slotColumnsAboutToBeInserted(const QModelIndex &parent, int start, int end);
    void slotColumnsInserted(const QModelIndex &parent, int, int);
    void slotColumnsAboutToBeRemoved(const QModelIndex &parent, int start, int end);
    void slotColumnsRemoved(const QModelIndex &parent, int, int);
    void slotDataChanged(const QModelIndex &from, const QModelIndex &to, const QVector<int> &roles);
    void slotSourceLayoutAboutToBeChanged(const QList<QPersistentModelIndex> &sourceParents, QAbstractItemModel::LayoutChangeHint hint);
    void slotSourceLayoutChanged(const QList<QPersistentModelIndex> &sourceParents, QAbstractItemModel::LayoutChangeHint hint);
    void slotModelAboutToBeReset();
    void slotModelReset();
};
#endif // KCONCATENATEROWSPROXYMODEL_P_H
