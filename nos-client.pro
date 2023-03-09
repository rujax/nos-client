QT       += core xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

greaterThan(QT_MAJOR_VERSION, 3):!lessThan(QT_MINOR_VERSION, 4) {
    QT += network
} else {
    message(Qt $$QT_VERSION Network not supported.)
}

greaterThan(QT_MAJOR_VERSION, 4):!lessThan(QT_MINOR_VERSION, 2) {
    QT += winextras
} else {
    message(Qt $$QT_VERSION WinExtras not supported.)
}

CONFIG += c++11

CONFIG(release, debug|release): DEFINES += QT_NO_DEBUG_OUTPUT

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

QMAKE_LFLAGS += -static

QMAKE_CXXFLAGS += -Wno-missing-field-initializers

DEFINES += QT_MESSAGELOGCONTEXT

SOURCES += \
    accountwindow.cpp \
    cdn.cpp \
    client.cpp \
    logger.cpp \
    main.cpp \
    mainwindow.cpp \
    otablewidget.cpp \
    refreshwindow.cpp \
    transferwindow.cpp

HEADERS += \
    account.h \
    accountwindow.h \
    cdn.h \
    client.h \
    config.h \
    jobqueue.h \
    logger.h \
    mainwindow.h \
    otablewidget.h \
    qstringhash.h \
    qstringmap.h \
    qstringvector.h \
    refreshwindow.h \
    transferwindow.h \
    workerqueue.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    nos-client.qrc

RC_ICONS = nos-client.ico

DISTFILES += \
    .gitignore \
    deployfile \
    nos-client.iss \
    nos-client.qss

VERSION = 0.1.9
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
