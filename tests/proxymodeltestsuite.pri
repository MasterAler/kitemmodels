RESOURCES += \
    $$PWD/proxymodeltestsuite/eventlogger.qrc

HEADERS += \
    $$PWD/proxymodeltestsuite/dynamictreemodel.h \
    $$PWD/proxymodeltestsuite/dynamictreewidget.h \
    $$PWD/proxymodeltestsuite/eventloggerregister.h \
    $$PWD/proxymodeltestsuite/indexfinder.h \
    $$PWD/proxymodeltestsuite/modelcommander.h \
    $$PWD/proxymodeltestsuite/modeldumper.h \
    $$PWD/proxymodeltestsuite/modeleventlogger.h \
    $$PWD/proxymodeltestsuite/modelselector.h \
    $$PWD/proxymodeltestsuite/modelspy.h \
    $$PWD/proxymodeltestsuite/modeltest.h \
    $$PWD/proxymodeltestsuite/persistentchangelist.h \
    $$PWD/proxymodeltestsuite/proxymodeltest.h

SOURCES += \
    $$PWD/proxymodeltestsuite/dynamictreemodel.cpp \
    $$PWD/proxymodeltestsuite/dynamictreewidget.cpp \
    $$PWD/proxymodeltestsuite/eventloggerregister.cpp \
    $$PWD/proxymodeltestsuite/modelcommander.cpp \
    $$PWD/proxymodeltestsuite/modeldumper.cpp \
    $$PWD/proxymodeltestsuite/modeleventlogger.cpp \
    $$PWD/proxymodeltestsuite/modelselector.cpp \
    $$PWD/proxymodeltestsuite/modelspy.cpp \
    $$PWD/proxymodeltestsuite/modeltest.cpp \
    $$PWD/proxymodeltestsuite/proxymodeltest.cpp \
#    $$PWD/proxymodeltestsuite/templates/datachanged.cpp \
#    $$PWD/proxymodeltestsuite/templates/init.cpp \
#    $$PWD/proxymodeltestsuite/templates/layoutchanged.cpp \
#    $$PWD/proxymodeltestsuite/templates/main.cpp \
#    $$PWD/proxymodeltestsuite/templates/modelreset.cpp \
#    $$PWD/proxymodeltestsuite/templates/rowsinserted.cpp \
#    $$PWD/proxymodeltestsuite/templates/rowsremoved.cpp

INCLUDEPATH *= $$PWD/proxymodeltestsuite
