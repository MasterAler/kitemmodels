#ifndef KDESCENDANTSPROXYMODEL_P_H
#define KDESCENDANTSPROXYMODEL_P_H

#include "kdescendantsproxymodel.h"

#include <QStringList>
#include "kbihash_p.h"

typedef KHash2Map<QPersistentModelIndex, int> Mapping;

// Yeah, it should not be a QObject, bu it made
// refactoring quicker, not too much of a downgrade, it's a GUI
// class after all
class KDescendantsProxyModelPrivate: public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(KDescendantsProxyModel)

    KDescendantsProxyModelPrivate(KDescendantsProxyModel *qq)
        : QObject(),
          q_ptr(qq),
          m_rowCount(0),
          m_ignoreNextLayoutAboutToBeChanged(false),
          m_ignoreNextLayoutChanged(false),
          m_relayouting(false),
          m_displayAncestorData(false),
          m_ancestorSeparator(QStringLiteral(" / "))
    { }

    KDescendantsProxyModel *const q_ptr;

    mutable QVector<QPersistentModelIndex> m_pendingParents;

    Mapping m_mapping;
    int m_rowCount;
    QPair<int, int> m_removePair;
    QPair<int, int> m_insertPair;

    bool m_ignoreNextLayoutAboutToBeChanged;
    bool m_ignoreNextLayoutChanged;
    bool m_relayouting;

    bool m_displayAncestorData;
    QString m_ancestorSeparator;

    QList<QPersistentModelIndex> m_layoutChangePersistentIndexes;
    QModelIndexList m_proxyIndexes;

    void scheduleProcessPendingParents() const;
    void processPendingParents();

    void synchronousMappingRefresh();

    void updateInternalIndexes(int start, int offset);

    void resetInternalData();

private slots:
    void sourceRowsAboutToBeInserted(const QModelIndex &, int, int);
    void sourceRowsInserted(const QModelIndex &, int, int);
    void sourceRowsAboutToBeRemoved(const QModelIndex &, int, int);
    void sourceRowsRemoved(const QModelIndex &, int, int);
    void sourceRowsAboutToBeMoved(const QModelIndex &, int, int, const QModelIndex &, int);
    void sourceRowsMoved(const QModelIndex &, int, int, const QModelIndex &, int);
    void sourceModelAboutToBeReset();
    void sourceModelReset();
    void sourceLayoutAboutToBeChanged();
    void sourceLayoutChanged();
    void sourceDataChanged(const QModelIndex &, const QModelIndex &);
    void sourceModelDestroyed();
};

#endif // KDESCENDANTSPROXYMODEL_P_H
