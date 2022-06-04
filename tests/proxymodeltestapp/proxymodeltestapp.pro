TEMPLATE = app

QT += core gui widgets quickwidgets testlib
CONFIG += c++11

HEADERS += \
    breadcrumbdirectionwidget.h \
    breadcrumbnavigationwidget.h \
    breadcrumbswidget.h \
    checkablewidget.h \
    descendantpmwidget.h \
    kidentityproxymodelwidget.h \
    kreparentingproxymodel.h \
    lessthanwidget.h \
    mainwindow.h \
    matchcheckingwidget.h \
    modelcommanderwidget.h \
    proxyitemselectionwidget.h \
    proxymodeltestwidget.h \
    recursivefilterpmwidget.h \
    selectioninqmlwidget.h \
    selectionpmwidget.h

SOURCES += \
    breadcrumbdirectionwidget.cpp \
    breadcrumbnavigationwidget.cpp \
    breadcrumbswidget.cpp \
    checkablewidget.cpp \
    descendantpmwidget.cpp \
    kidentityproxymodelwidget.cpp \
    kreparentingproxymodel.cpp \
    lessthanwidget.cpp \
    main.cpp \
    mainwindow.cpp \
    matchcheckingwidget.cpp \
    modelcommanderwidget.cpp \
    proxyitemselectionwidget.cpp \
    proxymodeltestwidget.cpp \
    recursivefilterpmwidget.cpp \
    selectioninqmlwidget.cpp \
    selectionpmwidget.cpp

include(../proxymodeltestsuite.pri)

DESTDIR = $$PWD/../../bin

INCLUDEPATH *= $$PWD/../../models_core/include
LIBS *= -L$$DESTDIR -lkitemmodels

DEFINES += SRC_DIR='\\"$$PWD\\"'
