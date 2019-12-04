QT += testlib
QT += core gui widgets qml
CONFIG += qt warn_on depend_includepath testcase

TEMPLATE = app

#Test_CRP {
    TARGET = test_CRP
    SOURCES += kconcatenaterowsproxymodeltest.cpp
#}

#Test_DP_smoke {
#    TARGET = test_DP_smoke
#    SOURCES += kdescendantsproxymodel_smoketest.cpp
#}

SOURCES +=  \
#    kconcatenaterows_qml.cpp \
#    kconcatenaterowsproxymodeltest.cpp \
#    kdescendantsproxymodel_smoketest.cpp \
#    kdescendantsproxymodeltest.cpp \
#    kextracolumnsproxymodeltest.cpp \
#    klinkitemselectionmodeltest.cpp \
#    kmodelindexproxymappertest.cpp \
#    knumbermodeltest.cpp \
#    krearrangecolumnsproxymodeltest.cpp \
#    krecursivefilterproxymodeltest.cpp \
#    kselectionproxymodel_smoketest.cpp \
#    kselectionproxymodeltest.cpp \
#    kselectionproxymodeltestsuite.cpp

#HEADERS += \
#    klinkitemselectionmodeltest.h \
#    kselectionproxymodeltestsuite.h \
#    test_model_helpers.h

include(../proxymodeltestsuite.pri)

DISTFILES += \
    concatenaterowstest.qml \
    pythontest.py

DESTDIR = $$PWD/../../bin

INCLUDEPATH *= $$PWD/../../models_core/include
LIBS *= -L$$DESTDIR -lkitemmodels
