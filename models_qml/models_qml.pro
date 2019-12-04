CONFIG += qt plugin
QT += qml

TARGET = itemmodelsplugin
TEMPLATE = lib

SOURCES += \
    kconcatenaterowsproxymodel_qml.cpp \
    plugin.cpp

HEADERS += \
    kconcatenaterowsproxymodel_qml.h \
    plugin.h

DISTFILES += \
    qmldir

DESTDIR = $$PWD/../bin

INCLUDEPATH *= $$PWD/../models_core/include
LIBS *= -L$$DESTDIR -lkitemmodels

qmldir.files  = qmldir
qmldir.path   = $$DESTDIR
INSTALLS      += qmldir
