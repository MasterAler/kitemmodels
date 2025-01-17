/*
    Copyright (c) 2015 Stephen Kelly <steveire@gmail.com>
    Copyright (c) 2015 David Faure <faure@kde.org>
    Copyright (c) 2016 Ableton AG <info@ableton.com>
        Author Stephen Kelly <stephen.kelly@ableton.com>

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

#include <QTest>
#include <QStringListModel>
#include <QIdentityProxyModel>
#include <QSignalSpy>
#include <kselectionproxymodel.h>

#include "modeltest.h"
#include "dynamictreemodel.h"

#include "test_model_helpers.h"
using namespace TestModelHelpers;

#include "TestSuite.h"

class KSelectionProxyModelTest : public TestSuite
{
  Q_OBJECT
public:
  KSelectionProxyModelTest(QObject* parent = nullptr)
    : TestSuite(parent),
      days({
          QStringLiteral("Monday"),
          QStringLiteral("Tuesday"),
          QStringLiteral("Wednesday"),
          QStringLiteral("Thursday")
      })
  {
  }

private Q_SLOTS:
  void columnCountShouldBeStable();
  void selectOnSourceReset();
  void selectionMapping();
  void removeRows_data();
  void removeRows();

  void selectionModelModelChange();
  void deselection_data();
  void deselection();

private:
  const QStringList days;
};

void KSelectionProxyModelTest::columnCountShouldBeStable()
{
    // Given a KSelectionProxy on top of a stringlist model
    QStringListModel strings(days);
    QItemSelectionModel selectionModel(&strings);
    KSelectionProxyModel proxy(&selectionModel);
    proxy.setSourceModel(&strings);

    QSignalSpy rowATBISpy(&proxy, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)));
    QSignalSpy rowInsertedSpy(&proxy, SIGNAL(rowsInserted(QModelIndex,int,int)));

    // No selection => the proxy should have 0 rows, 1 column
    // (if it had 0 columns, it would have to emit column insertions, too much trouble)
    QCOMPARE(proxy.rowCount(), 0);
    QCOMPARE(proxy.columnCount(), 1);
    QCOMPARE(rowATBISpy.count(), 0);
    QCOMPARE(rowInsertedSpy.count(), 0);

    // Select second entry -> the proxy should have 1 rows, 1 column
    selectionModel.select(QItemSelection(strings.index(1, 0), strings.index(1, 0)), QItemSelectionModel::Select);
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.columnCount(), 1);
    QCOMPARE(rowSpyToText(rowATBISpy), QStringLiteral("0,0"));
    QCOMPARE(rowSpyToText(rowInsertedSpy), QStringLiteral("0,0"));
}

void KSelectionProxyModelTest::selectOnSourceReset()
{
  QStringListModel strings(days);
  QItemSelectionModel selectionModel(&strings);

  connect(&strings, &QAbstractItemModel::modelReset, [&] {
    selectionModel.select(QItemSelection(strings.index(0, 0), strings.index(2, 0)),
                          QItemSelectionModel::Select);
  });

  KSelectionProxyModel proxy(&selectionModel);
  proxy.setSourceModel(&strings);

  selectionModel.select(QItemSelection(strings.index(0, 0), strings.index(2, 0)),
                        QItemSelectionModel::Select);

  QCOMPARE(proxy.rowCount(), 3);
  for (int i = 0; i < 3; ++i)
    QCOMPARE(proxy.index(i, 0).data().toString(), days.at(i));

  QStringList numbers = { QStringLiteral("One"), QStringLiteral("Two"), QStringLiteral("Three"), QStringLiteral("Four") };
  strings.setStringList(numbers);

  QCOMPARE(proxy.rowCount(), 3);
  for (int i = 0; i < 3; ++i)
    QCOMPARE(proxy.index(i, 0).data().toString(), numbers.at(i));

  QVERIFY(selectionModel.selection().contains(strings.index(0, 0)));
  QVERIFY(selectionModel.selection().contains(strings.index(1, 0)));
  QVERIFY(selectionModel.selection().contains(strings.index(2, 0)));
}

void KSelectionProxyModelTest::selectionModelModelChange()
{
  QStringListModel strings(days);
  QItemSelectionModel selectionModel(&strings);

  QIdentityProxyModel identity;
  identity.setSourceModel(&strings);

  KSelectionProxyModel proxy(&selectionModel);
  proxy.setSourceModel(&identity);
  selectionModel.select(strings.index(0, 0), QItemSelectionModel::Select);

  QCOMPARE(proxy.rowCount(), 1);
  QCOMPARE(proxy.index(0, 0).data().toString(), QStringLiteral("Monday"));


  QStringListModel strings2({
    QStringLiteral("Today"),
    QStringLiteral("Tomorrow")
  });

  QSignalSpy resetSpy(&proxy, &QAbstractItemModel::modelReset);

  selectionModel.setModel(&strings2);

  QCOMPARE(resetSpy.size(), 1);
  QCOMPARE(proxy.rowCount(), 0);

  proxy.setSourceModel(&strings2);
  selectionModel.select(strings2.index(0, 0), QItemSelectionModel::Select);

  QCOMPARE(proxy.rowCount(), 1);
  QCOMPARE(proxy.index(0, 0).data().toString(), QStringLiteral("Today"));

  QSignalSpy spy(&proxy, SIGNAL(modelReset()));

  QStringList numbers = { QStringLiteral("One"), QStringLiteral("Two"), QStringLiteral("Three"), QStringLiteral("Four") };
  strings.setStringList(numbers);

  QCOMPARE(spy.count(), 0);

  strings2.setStringList(numbers);

  QCOMPARE(spy.count(), 1);

  QCOMPARE(proxy.rowCount(), 0);
  QVERIFY(!selectionModel.hasSelection());

  selectionModel.select(strings2.index(0, 0), QItemSelectionModel::Select);

  QCOMPARE(proxy.rowCount(), 1);
  QCOMPARE(proxy.index(0, 0).data().toString(), numbers.at(0));
}

void KSelectionProxyModelTest::deselection_data()
{
    QTest::addColumn<int>("kspm_mode");
    QTest::addColumn<QStringList>("selection");
    QTest::addColumn<int>("expectedRowCountBefore");
    QTest::addColumn<int>("spyCount");
    QTest::addColumn<QStringList>("toDeselect");
    QTest::addColumn<int>("expectedRowCountAfter");

    auto testNumber = 1;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::SubTreesWithoutRoots)
            << QStringList{"2"} << 2
            << 1
            << QStringList{"2"} << 0;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::SubTreesWithoutRoots)
            << QStringList{"3", "9"} << 4
            << 1
            << QStringList{"3"} << 2;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::SubTreesWithoutRoots)
            << QStringList{"3", "9"} << 4
            << 1
            << QStringList{"3", "9"} << 0;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::SubTreesWithoutRoots)
            << QStringList{"3", "9"} << 4
            << 1
            << QStringList{"9"} << 2;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::SubTreesWithoutRoots)
            << QStringList{"3", "9", "11", "15"} << 6
            << 1
            << QStringList{"9"} << 7;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::SubTreesWithoutRoots)
            << QStringList{"3", "9", "11", "15"} << 6
            << 1
            << QStringList{"9", "15"} << 5;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::SubTreesWithoutRoots)
            << QStringList{"3", "9", "11", "15"} << 6
            << 1
            << QStringList{"3", "9", "15"} << 3;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::SubTreesWithoutRoots)
            << QStringList{"3", "9", "11", "15"} << 6
            << 1
            << QStringList{"3", "9", "11", "15"} << 0;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::SubTreesWithoutRoots)
            << QStringList{"3", "9", "11", "15"} << 6
            << 0
            << QStringList{"11"} << 6;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::SubTreesWithoutRoots)
            << QStringList{"3", "9", "11", "15"} << 6
            << 2
            << QStringList{"3", "15"} << 2;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::ExactSelection)
            << QStringList{"3", "9", "11", "15"} << 4
            << 1
            << QStringList{"11"} << 3;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::ExactSelection)
            << QStringList{"3", "9", "11", "15"} << 4
            << 2
            << QStringList{"3", "11"} << 2;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::ExactSelection)
            << QStringList{"3", "9", "11", "15"} << 4
            << 1
            << QStringList{"3", "9", "11"} << 1;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::ChildrenOfExactSelection)
            << QStringList{"3"} << 2
            << 1
            << QStringList{"3"} << 0;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::ChildrenOfExactSelection)
            << QStringList{"3", "9", "11", "15"} << 9
            << 1
            << QStringList{"3", "9", "11"} << 2;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::ChildrenOfExactSelection)
            << QStringList{"3", "9", "11", "15"} << 9
            << 1
            << QStringList{"9"} << 7;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::ChildrenOfExactSelection)
            << QStringList{"3", "9", "11", "15"} << 9
            << 2
            << QStringList{"3", "11"} << 4;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::ChildrenOfExactSelection)
            << QStringList{"4", "6", "9", "15"} << 7
            << 1
            << QStringList{"4"} << 5;
    ++testNumber;

    QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
            << static_cast<int>(KSelectionProxyModel::ChildrenOfExactSelection)
            << QStringList{"6", "7"} << 1
            << 0
            << QStringList{"7"} << 1;
    ++testNumber;
}

void KSelectionProxyModelTest::deselection()
{
    QFETCH(int, kspm_mode);
    QFETCH(QStringList, selection);
    QFETCH(int, expectedRowCountBefore);
    QFETCH(int, spyCount);
    QFETCH(QStringList, toDeselect);
    QFETCH(int, expectedRowCountAfter);

    DynamicTreeModel tree;
    new ModelTest(&tree, &tree);
    ModelResetCommand resetCommand(&tree);
    resetCommand.setInitialTree(
                " - 1"
                " - - 2"
                " - - - 3"
                " - - - - 4"
                " - - - - - 5"
                " - - - - - 6"
                " - - - - - - 7"
                " - - - - 8"
                " - - - 9"
                " - - - - 10"
                " - - - - 11"
                " - - - - - 12"
                " - - - - - 13"
                " - - - - - 14"
                " - - 15"
                " - - - 16"
                " - - - 17"
                );
    resetCommand.doCommand();

    QItemSelectionModel selectionModel(&tree);

    KSelectionProxyModel proxy(&selectionModel);
    new ModelTest(&proxy, &proxy);
    proxy.setFilterBehavior(static_cast<KSelectionProxyModel::FilterBehavior>(kspm_mode));
    proxy.setSourceModel(&tree);

    QSignalSpy beforeSpy(&proxy, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)));
    QSignalSpy afterSpy(&proxy, SIGNAL(rowsRemoved(QModelIndex,int,int)));

    QItemSelection sel;
    for (auto item : selection) {
        QModelIndexList idxs = tree.match(tree.index(0, 0), Qt::DisplayRole, item, 1, Qt::MatchRecursive);
        QCOMPARE(idxs.size(), 1);
        sel << QItemSelectionRange(idxs.at(0), idxs.at(0));
    }
    selectionModel.select(sel, QItemSelectionModel::Select);

    QCOMPARE(proxy.rowCount(), expectedRowCountBefore);

    QItemSelection desel;
    for (auto item : toDeselect) {
        QModelIndexList idxs = tree.match(tree.index(0, 0), Qt::DisplayRole, item, 1, Qt::MatchRecursive);
        QCOMPARE(idxs.size(), 1);
        desel << QItemSelectionRange(idxs.at(0), idxs.at(0));
    }
    selectionModel.select(desel, QItemSelectionModel::Deselect);

    QCOMPARE(beforeSpy.count(), spyCount);
    QCOMPARE(afterSpy.count(), spyCount);

    QCOMPARE(proxy.rowCount(), expectedRowCountAfter);
}

void KSelectionProxyModelTest::removeRows_data()
{
  QTest::addColumn<int>("kspm_mode");
  QTest::addColumn<bool>("connectSelectionModelFirst");
  QTest::addColumn<bool>("emulateSingleSelectionMode");
  QTest::addColumn<QStringList>("selection");
  QTest::addColumn<int>("expectedRowCountBefore");
  QTest::addColumn<int>("spyCount");
  QTest::addColumn<QStringList>("toRemove");
  QTest::addColumn<int>("expectedRowCountAfter");

  // Because the KSelectionProxyModel has two dependencies - a QItemSelectionModel
  // and a QAbstractItemModel, whichever one signals first determines the internal
  // code path is used to perform removal.  That order is determined by order
  // of signal slot connections, and these tests use connectSelectionModelFirst
  // to test both.

  // When using a QAbstractItemView, the SelectionMode can determine how the
  // selection changes when a selected row is removed.  When the row is
  // AboutToBeRemoved, the view might change the selection to a row which is
  // not to be removed.  This means that depending on signal-slot connection
  // order, the KSelectionProxyModel::sourceRowsAboutToBeRemoved method
  // might be executed, but then the selection can be changed before
  // executing the KSelectionProxyModel::sourceRowsRemoved method.  These tests
  // are run with and without similar emulated behavior.

  auto testNumber = 1;

  for (auto emulateSingleSelectionMode: {true, false})
  {
      for (auto kspm_mode: {KSelectionProxyModel::SubTreesWithoutRoots, KSelectionProxyModel::ChildrenOfExactSelection})
      {
          for (auto connectSelectionModelFirst: {true, false})
          {
              QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                      << static_cast<int>(kspm_mode) << connectSelectionModelFirst << emulateSingleSelectionMode
                      << QStringList{"2"} << 2
                      << 1
                      << QStringList{"2", "2"} << (emulateSingleSelectionMode ? 2 : 0);
              ++testNumber;

              QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                      << static_cast<int>(kspm_mode) << connectSelectionModelFirst << emulateSingleSelectionMode
                      << QStringList{"2"} << 2
                      << (kspm_mode == KSelectionProxyModel::ChildrenOfExactSelection ? 0 : 1)
                      << QStringList{"4", "4"} << 2;
              ++testNumber;

              QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                      << static_cast<int>(kspm_mode) << connectSelectionModelFirst << emulateSingleSelectionMode
                      << QStringList{"2"} << 2
                      << (kspm_mode == KSelectionProxyModel::ChildrenOfExactSelection ? 0 : 1)
                      << QStringList{"5", "5"} << 2;
              ++testNumber;

              QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                      << static_cast<int>(kspm_mode) << connectSelectionModelFirst << emulateSingleSelectionMode
                      << QStringList{"2"} << 2
                      << (kspm_mode == KSelectionProxyModel::ChildrenOfExactSelection ? 0 : 1)
                      << QStringList{"6", "6"} << 2;
              ++testNumber;

              QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                      << static_cast<int>(kspm_mode) << connectSelectionModelFirst << emulateSingleSelectionMode
                      << QStringList{"2"} << 2
                      << (kspm_mode == KSelectionProxyModel::ChildrenOfExactSelection ? 0 : 1)
                      << QStringList{"7", "7"} << 2;
              ++testNumber;

              QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                      << static_cast<int>(kspm_mode) << connectSelectionModelFirst << emulateSingleSelectionMode
                      << QStringList{"2"} << 2
                      << 1
                      << QStringList{"1", "1"} << 0;
              ++testNumber;

              QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                      << static_cast<int>(kspm_mode) << connectSelectionModelFirst << emulateSingleSelectionMode
                      << QStringList{"2"} << 2
                      << 1
                      << QStringList{"9", "9"} << 1;
              ++testNumber;

              QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                      << static_cast<int>(kspm_mode) << connectSelectionModelFirst << emulateSingleSelectionMode
                      << QStringList{"2"} << 2
                      << 0
                      << QStringList{"15", "15"} << 2;
              ++testNumber;

              QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                      << static_cast<int>(kspm_mode) << connectSelectionModelFirst << emulateSingleSelectionMode
                      << QStringList{"5"} << 0
                      << 0
                      << QStringList{"5", "5"} << (emulateSingleSelectionMode ? 1 : 0);
              ++testNumber;

              QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                      << static_cast<int>(kspm_mode) << connectSelectionModelFirst << emulateSingleSelectionMode
                      << QStringList{"5"} << 0
                      << 0
                      << QStringList{"4", "4"} << 0;
              ++testNumber;

              QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                      << static_cast<int>(kspm_mode) << connectSelectionModelFirst << emulateSingleSelectionMode
                      << QStringList{"5"} << 0
                      << 0
                      << QStringList{"3", "3"} << 0;
              ++testNumber;

              QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                      << static_cast<int>(kspm_mode) << connectSelectionModelFirst << emulateSingleSelectionMode
                      << QStringList{"5"} << 0
                      << 0
                      << QStringList{"2", "2"} << 0;
              ++testNumber;

              QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                      << static_cast<int>(kspm_mode) << connectSelectionModelFirst << emulateSingleSelectionMode
                      << QStringList{"6"} << 1
                      << 1
                      << QStringList{"4", "4"} << 0;
              ++testNumber;
          }
      }
  }

  for (auto connectSelectionModelFirst: {true, false})
  {
      for (auto kspm_mode: {KSelectionProxyModel::SubTreesWithoutRoots, KSelectionProxyModel::ChildrenOfExactSelection})
      {

          QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                  << static_cast<int>(kspm_mode) << connectSelectionModelFirst << false
                  << QStringList{"3", "15"} << 4
                  << 1
                  << QStringList{"3", "3"} << 2;
          ++testNumber;

          QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                  << static_cast<int>(kspm_mode) << connectSelectionModelFirst << false
                  << QStringList{"4", "15"} << 4
                  << 1
                  << QStringList{"2", "2"} << 2;
          ++testNumber;

          QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                  << static_cast<int>(kspm_mode) << connectSelectionModelFirst << false
                  << QStringList{"4", "11"} << 5
                  << 1
                  << QStringList{"2", "2"} << 0;
          ++testNumber;

          QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                  << static_cast<int>(kspm_mode) << connectSelectionModelFirst << false
                  << QStringList{"4", "11"} << 5
                  << 1
                  << QStringList{"3", "3"} << 3;
          ++testNumber;

          QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                  << static_cast<int>(kspm_mode) << connectSelectionModelFirst << false
                  << QStringList{"4", "11"} << 5
                  << 1
                  << QStringList{"11", "11"} << 2;
          ++testNumber;

          QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                  << static_cast<int>(kspm_mode) << connectSelectionModelFirst << false
                  << QStringList{"4", "11"} << 5
                  << 1
                  << QStringList{"3", "9"} << 0;
          ++testNumber;

          QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                  << static_cast<int>(kspm_mode) << connectSelectionModelFirst << false
                  << QStringList{"4", "11", "15"} << 7
                  << 1
                  << QStringList{"3", "9"} << 2;
          ++testNumber;

          QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                  << static_cast<int>(kspm_mode) << connectSelectionModelFirst << false
                  << QStringList{"4", "8", "10", "11", "16"} << 5
                  << 1
                  << QStringList{"3", "9"} << 0;
          ++testNumber;

          QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                  << static_cast<int>(kspm_mode) << connectSelectionModelFirst << false
                  << QStringList{"4", "8", "10", "11", "16"} << 5
                  << 1
                  << QStringList{"3", "3"} << 3;
          ++testNumber;

          QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                  << static_cast<int>(kspm_mode) << connectSelectionModelFirst << false
                  << QStringList{"4", "8", "10", "11", "16"} << 5
                  << 1
                  << QStringList{"9", "9"} << 2;
          ++testNumber;

          QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                  << static_cast<int>(kspm_mode) << connectSelectionModelFirst << false
                  << QStringList{"4", "11"} << 5
                  << 1
                  << QStringList{"9", "9"} << 2;
          ++testNumber;

          QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                  << static_cast<int>(kspm_mode) << connectSelectionModelFirst << false
                  << QStringList{"4", "6", "11"}
                  << (kspm_mode == KSelectionProxyModel::ChildrenOfExactSelection ? 6 : 5)
                  << 1
                  << QStringList{"9", "9"}
                  << (kspm_mode == KSelectionProxyModel::ChildrenOfExactSelection ? 3 : 2);
          ++testNumber;

          QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                  << static_cast<int>(kspm_mode) << connectSelectionModelFirst << false
                  << QStringList{"4", "8", "11"} << 5
                  << 1
                  << QStringList{"9", "9"} << 2;
          ++testNumber;

          QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
                  << static_cast<int>(kspm_mode) << connectSelectionModelFirst << false
                  << QStringList{"6", "8", "11"} << 4
                  << 0
                  << QStringList{"8", "8"} << 4;
          ++testNumber;
      }
  }

  for (auto connectSelectionModelFirst: {true, false})
  {
      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::ExactSelection) << connectSelectionModelFirst << false
              << QStringList{"2"} << 1
              << 1
              << QStringList{"2", "2"} << 0;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::ExactSelection) << connectSelectionModelFirst << false
              << QStringList{"2", "3", "4"} << 3
              << 1
              << QStringList{"2", "2"} << 0;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::ExactSelection) << connectSelectionModelFirst << false
              << QStringList{"6", "9"} << 2
              << 1
              << QStringList{"2", "2"} << 0;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::ExactSelection) << connectSelectionModelFirst << false
              << QStringList{"6", "9"} << 2
              << 1
              << QStringList{"4", "4"} << 1;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::ExactSelection) << connectSelectionModelFirst << false
              << QStringList{"4", "10", "11"} << 3
              << 1
              << QStringList{"3", "9"} << 0;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::ExactSelection) << connectSelectionModelFirst << false
              << QStringList{"4", "6", "7", "10", "11"} << 5
              << 1
              << QStringList{"10", "11"} << 3;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::ExactSelection) << connectSelectionModelFirst << false
              << QStringList{"4", "5", "6", "7", "8"} << 5
              << 1
              << QStringList{"4", "8"} << 0;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::ExactSelection) << connectSelectionModelFirst << false
              << QStringList{"4", "5", "6", "7", "8"} << 5
              << 1
              << QStringList{"4", "4"} << 1;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::ExactSelection) << connectSelectionModelFirst << false
              << QStringList{"4", "5", "6", "7", "8"} << 5
              << 1
              << QStringList{"6", "6"} << 3;
      ++testNumber;
  }

  for (auto connectSelectionModelFirst: {true, false})
  {
      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"2"} << 1
              << 1
              << QStringList{"2", "2"} << 0;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"2"} << 1
              << 1
              << QStringList{"4", "4"} << 1;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"2", "4"} << 1
              << 1
              << QStringList{"4", "4"} << 1;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"2", "4"} << 1
              << 1
              << QStringList{"2", "2"} << 0;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"4", "9"} << 2
              << 1
              << QStringList{"2", "2"} << 0;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"4", "9"} << 2
              << 1
              << QStringList{"4", "4"} << 1;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"4", "9"} << 2
              << 1
              << QStringList{"9", "9"} << 1;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"4", "9"} << 2
              << 1
              << QStringList{"5", "6"} << 2;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"4", "9"} << 2
              << 1
              << QStringList{"4", "8"} << 1;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"6", "11", "15"} << 3
              << 1
              << QStringList{"9", "9"} << 2;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"6", "11", "15"} << 3
              << 1
              << QStringList{"11", "11"} << 2;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"6", "8", "10", "11"} << 4
              << 1
              << QStringList{"3", "3"} << 2;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"6", "8", "10", "11"} << 4
              << 1
              << QStringList{"2", "2"} << 0;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"6", "8", "10", "11"} << 4
              << 1
              << QStringList{"9", "9"} << 2;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"10", "11"} << 2
              << 1
              << QStringList{"9", "9"} << 0;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"4", "8"} << 2
              << 1
              << QStringList{"3", "3"} << 0;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"12", "13", "14"} << 3
              << 1
              << QStringList{"11", "11"} << 0;
      ++testNumber;

      QTest::newRow(QByteArray("test" + QByteArray::number(testNumber)).data())
              << static_cast<int>(KSelectionProxyModel::SubTrees) << connectSelectionModelFirst << false
              << QStringList{"12", "13", "14"} << 3
              << 1
              << QStringList{"10", "11"} << 0;
      ++testNumber;
  }
}

void KSelectionProxyModelTest::removeRows()
{
    QFETCH(int, kspm_mode);
    QFETCH(bool, connectSelectionModelFirst);
    QFETCH(bool, emulateSingleSelectionMode);
    QFETCH(QStringList, selection);
    QFETCH(int, expectedRowCountBefore);
    QFETCH(int, spyCount);
    QFETCH(QStringList, toRemove);
    QFETCH(int, expectedRowCountAfter);

    DynamicTreeModel tree;
    new ModelTest(&tree, &tree);
    ModelResetCommand resetCommand(&tree);
    resetCommand.setInitialTree(
                " - 1"
                " - - 2"
                " - - - 3"
                " - - - - 4"
                " - - - - - 5"
                " - - - - - 6"
                " - - - - - - 7"
                " - - - - 8"
                " - - - 9"
                " - - - - 10"
                " - - - - 11"
                " - - - - - 12"
                " - - - - - 13"
                " - - - - - 14"
                " - - 15"
                " - - - 16"
                " - - - 17"
                );
    resetCommand.doCommand();

    QItemSelectionModel selectionModel;

    if (emulateSingleSelectionMode)
    {
        QObject::connect(&tree, &QAbstractItemModel::rowsAboutToBeRemoved, &tree,
                         [&tree, &selectionModel](QModelIndex const& p, int s, int e){
            auto rmIdx = tree.index(s, 0, p);
            if (s == e && selectionModel.selectedIndexes().contains(rmIdx))
            {
                auto nextIdx = tree.index(e + 1, 0, rmIdx.parent());
                selectionModel.select(nextIdx, QItemSelectionModel::ClearAndSelect);
            }
        });
    }

    KSelectionProxyModel proxy;
    new ModelTest(&proxy, &proxy);
    proxy.setFilterBehavior(static_cast<KSelectionProxyModel::FilterBehavior>(kspm_mode));

    if (connectSelectionModelFirst) {
        selectionModel.setModel(&tree);
        proxy.setSourceModel(&tree);
        proxy.setSelectionModel(&selectionModel);
    } else {
        proxy.setSourceModel(&tree);
        proxy.setSelectionModel(&selectionModel);
        selectionModel.setModel(&tree);
    }

    QSignalSpy beforeSpy(&proxy, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)));
    QSignalSpy afterSpy(&proxy, SIGNAL(rowsRemoved(QModelIndex,int,int)));

    QItemSelection sel;
    for (auto item : selection) {
        QModelIndexList idxs = tree.match(tree.index(0, 0), Qt::DisplayRole, item, 1, Qt::MatchRecursive);
        QCOMPARE(idxs.size(), 1);
        sel << QItemSelectionRange(idxs.at(0), idxs.at(0));
    }
    selectionModel.select(sel, QItemSelectionModel::Select);

    QCOMPARE(proxy.rowCount(), expectedRowCountBefore);

    for (auto removePairIndex = 0; removePairIndex < toRemove.size(); removePairIndex += 2) {
        QModelIndexList idxs = tree.match(tree.index(0, 0), Qt::DisplayRole, toRemove[removePairIndex], 1, Qt::MatchRecursive);
        QCOMPARE(idxs.size(), 1);

        auto startIdx = idxs.at(0);

        idxs = tree.match(tree.index(0, 0), Qt::DisplayRole, toRemove[removePairIndex + 1], 1, Qt::MatchRecursive);
        QCOMPARE(idxs.size(), 1);

        auto endIdx = idxs.at(0);

        ModelRemoveCommand remove(&tree);
        remove.setAncestorRowNumbers(tree.indexToPath(startIdx.parent()));
        remove.setStartRow(startIdx.row());
        remove.setEndRow(endIdx.row());
        remove.doCommand();
    }

    QCOMPARE(beforeSpy.count(), spyCount);
    QCOMPARE(afterSpy.count(), spyCount);

    QCOMPARE(proxy.rowCount(), expectedRowCountAfter);
}

void KSelectionProxyModelTest::selectionMapping()
{
    QStringListModel strings(days);
    QItemSelectionModel selectionModel(&strings);
    KSelectionProxyModel proxy(&selectionModel);
    proxy.setFilterBehavior(KSelectionProxyModel::SubTrees);
    proxy.setSourceModel(&strings);
    auto idx1 = strings.index(0, 0);
    auto idx2 = strings.index(2, 0);
    QItemSelection sourceSel;
    sourceSel << QItemSelectionRange(idx1, idx2);
    selectionModel.select(sourceSel, QItemSelectionModel::Select);

    QItemSelection proxySel;
    proxySel << QItemSelectionRange(proxy.index(0, 0), proxy.index(2, 0));

    QCOMPARE(proxy.mapSelectionToSource(proxySel), sourceSel);
}

static KSelectionProxyModelTest TEST_KSelectionProxyModelTest;

#include "kselectionproxymodeltest.moc"
