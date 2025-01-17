/*
 * This file is part of the proxy model test suite.
 *
 * Copyright 2009  Stephen Kelly <steveire@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "mainwindow.h"

#include <QTabWidget>

#include "dynamictreemodel.h"

#include "breadcrumbswidget.h"
#include "breadcrumbnavigationwidget.h"
#include "breadcrumbdirectionwidget.h"
#include "checkablewidget.h"
#include "descendantpmwidget.h"
#include "selectionpmwidget.h"
// #include "statesaverwidget.h"
#include "proxymodeltestwidget.h"
#include "proxyitemselectionwidget.h"
#ifdef QT_SCRIPT_LIB
#include "reparentingpmwidget.h"
#endif
#include "recursivefilterpmwidget.h"
#include "lessthanwidget.h"
#include "matchcheckingwidget.h"
#include "kidentityproxymodelwidget.h"
#ifdef QT_QUICKWIDGETS_LIB
#include "selectioninqmlwidget.h"
#endif

MainWindow::MainWindow() : QMainWindow()
{

    QTabWidget *tabWidget = new QTabWidget(this);

    tabWidget->addTab(new MatchCheckingWidget(), QStringLiteral("Match Checking PM"));
    tabWidget->addTab(new DescendantProxyModelWidget(), QStringLiteral("descendant PM"));
    tabWidget->addTab(new SelectionProxyWidget(), QStringLiteral("selection PM"));
#ifdef QT_QUICKWIDGETS_LIB
    tabWidget->addTab(new SelectionInQmlWidget(), QStringLiteral("selection PM in QML"));
#endif
    tabWidget->addTab(new KIdentityProxyModelWidget(), QStringLiteral("Identity PM"));
    tabWidget->addTab(new CheckableWidget(), QStringLiteral("Checkable"));
    tabWidget->addTab(new BreadcrumbsWidget(), QStringLiteral("Breadcrumbs"));
    tabWidget->addTab(new BreadcrumbNavigationWidget(), QStringLiteral("Breadcrumb Navigation"));
    tabWidget->addTab(new BreadcrumbDirectionWidget(), QStringLiteral("Breadcrumb Direction"));
    tabWidget->addTab(new ProxyItemSelectionWidget(), QStringLiteral("Proxy Item selection"));
#ifdef QT_SCRIPT_LIB
    tabWidget->addTab(new ReparentingProxyModelWidget(), QStringLiteral("reparenting PM"));
#endif
    tabWidget->addTab(new RecursiveFilterProxyWidget(), QStringLiteral("Recursive Filter"));
    tabWidget->addTab(new LessThanWidget(), QStringLiteral("Less Than"));
    tabWidget->addTab(new ProxyModelTestWidget(), QStringLiteral("Proxy Model Test"));
//   tabWidget->addTab(new StateSaverWidget(), "State Saver Test");

    setCentralWidget(tabWidget);
}

MainWindow::~MainWindow()
{
}

