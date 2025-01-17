/*
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

#ifndef BREADCRUMBNAVIGATION_WIDGET_H
#define BREADCRUMBNAVIGATION_WIDGET_H

#include <QWidget>
#include <QItemSelection>
#include <QLabel>
#include <kselectionproxymodel.h>

#include "klinkitemselectionmodel.h"

class CurrentItemLabel : public QLabel
{
    Q_OBJECT
public:
    CurrentItemLabel(QAbstractItemModel *model, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

private Q_SLOTS:
    void rowsInserted(const QModelIndex &parent, int start, int end);
    void rowsRemoved(const QModelIndex &parent, int start, int end);
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void modelReset();

private:
    QAbstractItemModel *m_model;
};

class KBreadcrumbNavigationProxyModel : public KSelectionProxyModel
{
    Q_OBJECT
public:
    KBreadcrumbNavigationProxyModel(QItemSelectionModel *selectionModel, QObject *parent = nullptr);

    void setShowHiddenAscendantData(bool showHiddenAscendantData);
    bool showHiddenAscendantData() const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    bool m_showHiddenAscendantData;

};

class KNavigatingProxyModel : public KSelectionProxyModel
{
    Q_OBJECT
public:
    KNavigatingProxyModel(QItemSelectionModel *selectionModel, QObject *parent = nullptr);

    void setSourceModel(QAbstractItemModel *sourceModel) override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private Q_SLOTS:
    void modelReset();
    void updateNavigation();
    void navigationSelectionChanged(const QItemSelection &, const QItemSelection &);

private:

private:
    using KSelectionProxyModel::setFilterBehavior;

    QItemSelectionModel *m_selectionModel;

};

class KForwardingItemSelectionModel : public QItemSelectionModel
{
    Q_OBJECT
public:
    enum Direction {
        Forward,
        Reverse
    };
    KForwardingItemSelectionModel(QAbstractItemModel *model, QItemSelectionModel *selectionModel, QObject *parent = nullptr);
    KForwardingItemSelectionModel(QAbstractItemModel *model, QItemSelectionModel *selectionModel, Direction direction, QObject *parent = nullptr);

    void select(const QModelIndex &index, SelectionFlags command) override;
    void select(const QItemSelection &selection, SelectionFlags command) override;

private Q_SLOTS:
    void navigationSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

private:
    QItemSelectionModel *m_selectionModel;
    Direction m_direction;
};

class BreadcrumbNavigationWidget : public QWidget
{
    Q_OBJECT
public:
    BreadcrumbNavigationWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

};

#endif

