#ifndef KRECURSIVEFILTERPROXYMODEL_P_H
#define KRECURSIVEFILTERPROXYMODEL_P_H

#include "krecursivefilterproxymodel.h"

#include <QObject>
#include <QMetaMethod>

// Maintainability note:
// This class invokes some Q_PRIVATE_SLOTs in QSortFilterProxyModel which are
// private API and could be renamed or removed at any time.
// If they are renamed, the invocations can be updated with an #if (QT_VERSION(...))
// If they are removed, then layout{AboutToBe}Changed Q_SIGNALS should be used when the source model
// gets new rows or has rowsremoved or moved. The Q_PRIVATE_SLOT invocation is an optimization
// because layout{AboutToBe}Changed is expensive and causes the entire mapping of the tree in QSFPM
// to be cleared, even if only a part of it is dirty.
// Stephen Kelly, 30 April 2010.

// All this is temporary anyway, the long term solution is support in QSFPM: https://codereview.qt-project.org/151000

class KRecursiveFilterProxyModelPrivate: public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(KRecursiveFilterProxyModel)
    KRecursiveFilterProxyModel *q_ptr;

    KRecursiveFilterProxyModelPrivate(KRecursiveFilterProxyModel *model)
        : QObject()
        , q_ptr(model)
        , completeInsert(false)
    {
        qRegisterMetaType<QModelIndex>("QModelIndex");
    }

    inline QMetaMethod findMethod(const char *signature) const
    {
        Q_Q(const KRecursiveFilterProxyModel);
        const int idx = q->metaObject()->indexOfMethod(signature);
        Q_ASSERT(idx != -1);
        return q->metaObject()->method(idx);
    }

    // Convenience methods for invoking the QSFPM Q_SLOTS. Those slots must be invoked with invokeMethod
    // because they are Q_PRIVATE_SLOTs
    inline void invokeDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles = QVector<int>())
    {
        Q_Q(KRecursiveFilterProxyModel);
        // required for Qt 5.5 and upwards, see commit f96baeb75fc in qtbase
        static const QMetaMethod m = findMethod("_q_sourceDataChanged(QModelIndex,QModelIndex,QVector<int>)");
        bool success = m.invoke(q, Qt::DirectConnection,
                Q_ARG(QModelIndex, topLeft),
                Q_ARG(QModelIndex, bottomRight),
                Q_ARG(QVector<int>, roles));
        Q_UNUSED(success);
        Q_ASSERT(success);
    }

    inline void invokeRowsInserted(const QModelIndex &source_parent, int start, int end)
    {
        Q_Q(KRecursiveFilterProxyModel);
        static const QMetaMethod m = findMethod("_q_sourceRowsInserted(QModelIndex,int,int)");
        bool success = m.invoke(q, Qt::DirectConnection,
                                Q_ARG(QModelIndex, source_parent),
                                Q_ARG(int, start),
                                Q_ARG(int, end));
        Q_UNUSED(success);
        Q_ASSERT(success);
    }

    inline void invokeRowsAboutToBeInserted(const QModelIndex &source_parent, int start, int end)
    {
        Q_Q(KRecursiveFilterProxyModel);
        static const QMetaMethod m = findMethod("_q_sourceRowsAboutToBeInserted(QModelIndex,int,int)");
        bool success = m.invoke(q, Qt::DirectConnection,
                                Q_ARG(QModelIndex, source_parent),
                                Q_ARG(int, start),
                                Q_ARG(int, end));
        Q_UNUSED(success);
        Q_ASSERT(success);
    }

    inline void invokeRowsRemoved(const QModelIndex &source_parent, int start, int end)
    {
        Q_Q(KRecursiveFilterProxyModel);
        static const QMetaMethod m = findMethod("_q_sourceRowsRemoved(QModelIndex,int,int)");
        bool success = m.invoke(q, Qt::DirectConnection,
                                Q_ARG(QModelIndex, source_parent),
                                Q_ARG(int, start),
                                Q_ARG(int, end));
        Q_UNUSED(success);
        Q_ASSERT(success);
    }

    inline void invokeRowsAboutToBeRemoved(const QModelIndex &source_parent, int start, int end)
    {
        Q_Q(KRecursiveFilterProxyModel);
        static const QMetaMethod m = findMethod("_q_sourceRowsAboutToBeRemoved(QModelIndex,int,int)");
        bool success = m.invoke(q, Qt::DirectConnection,
                                Q_ARG(QModelIndex, source_parent),
                                Q_ARG(int, start),
                                Q_ARG(int, end));
        Q_UNUSED(success);
        Q_ASSERT(success);
    }

    /**
    Force QSortFilterProxyModel to re-evaluate whether to hide or show index and its parents.
    */
    void refreshAscendantMapping(const QModelIndex &index);

    QModelIndex lastFilteredOutAscendant(const QModelIndex &index);

    bool completeInsert;
    QModelIndex lastHiddenAscendantForInsert;

private slots:
    void sourceDataChanged(const QModelIndex &source_top_left, const QModelIndex &source_bottom_right, const QVector<int> &roles = QVector<int>());
    void sourceRowsAboutToBeInserted(const QModelIndex &source_parent, int start, int end);
    void sourceRowsInserted(const QModelIndex &source_parent, int start, int end);
    void sourceRowsAboutToBeRemoved(const QModelIndex &source_parent, int start, int end);
    void sourceRowsRemoved(const QModelIndex &source_parent, int start, int end);
};
#endif // KRECURSIVEFILTERPROXYMODEL_P_H
