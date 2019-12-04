QT += core gui widgets qml testlib
CONFIG += qt warn_on depend_includepath #testcase

TEMPLATE = app
TARGET = autotests

SOURCES +=  \
    kconcatenaterowsproxymodeltest.cpp \
    kdescendantsproxymodeltest.cpp \
    kextracolumnsproxymodeltest.cpp \
    klinkitemselectionmodeltest.cpp \
    kmodelindexproxymappertest.cpp \
    knumbermodeltest.cpp \
    krearrangecolumnsproxymodeltest.cpp \
    krecursivefilterproxymodeltest.cpp \
    kselectionproxymodeltest.cpp \
    main.cpp \
    TestSuite.cpp

HEADERS += \
    TestSuite.h \
    klinkitemselectionmodeltest.h \
#    test_model_helpers.h

include(../proxymodeltestsuite.pri)

DESTDIR = $$PWD/../../bin

INCLUDEPATH *= $$PWD/../../models_core/include
LIBS *= -L$$DESTDIR -lkitemmodels

HEADERS += \
    TestSuite.h
