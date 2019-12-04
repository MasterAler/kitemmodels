QT       -= gui

TARGET = kitemmodels
TEMPLATE = lib

DEFINES += KITEMMODELS_LIBRARY

SOURCES += \
        src/kbreadcrumbselectionmodel.cpp \
        src/kcheckableproxymodel.cpp \
        src/kconcatenaterowsproxymodel.cpp \
        src/kdescendantsproxymodel.cpp \
        src/kextracolumnsproxymodel.cpp \
        src/klinkitemselectionmodel.cpp \
        src/kmodelindexproxymapper.cpp \
        src/knumbermodel.cpp \
        src/krearrangecolumnsproxymodel.cpp \
        src/krecursivefilterproxymodel.cpp \
        src/kselectionproxymodel.cpp

HEADERS += \
        include/kitemmodels_debug.h \
        include/kitemmodels_export.h \
        include/kbihash_p.h \
        include/kbreadcrumbselectionmodel.h \
        include/kcheckableproxymodel.h \
        include/kconcatenaterowsproxymodel.h \
        include/kdescendantsproxymodel.h \
        include/kextracolumnsproxymodel.h \
        include/klinkitemselectionmodel.h \
        include/kmodelindexproxymapper.h \
        include/knumbermodel.h \
        include/krearrangecolumnsproxymodel.h \
        include/krecursivefilterproxymodel.h \
        include/kselectionproxymodel.h \
        include/kvoidpointerfactory_p.h

INCLUDEPATH *= $$PWD/include
DESTDIR = $$PWD/../bin
