QT       += core gui printsupport serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets network

CONFIG += c++17
CONFIG += link_pkgconfig
PKGCONFIG += openssl
# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ap323.c \
    ap441.c \
    ap445.c \
    apcommon.c \
    drivers.cpp \
    adduserdialog.cpp \
    createlogin.cpp \
    crypto.cpp \
    instrconnection.cpp \
    license.cpp \
    loghistory.cpp \
    main.cpp \
    mainwindow.cpp \
    serialcomm.cpp \
    userhistory.cpp \
    userlistdialog.cpp

HEADERS += \
    ap323.h \
    ap441.h \
    ap445.h \
    apcommon.h \
    drivers.h \
    adduserdialog.h \
    createlogin.h \
    crypto.h \
    instrconnection.h \
    license.h \
    loghistory.h \
    mainwindow.h \
    serialcomm.h \
    userhistory.h \
    userlistdialog.h

FORMS += \
    adduserdialog.ui \
    createlogin.ui \
    mainwindow.ui \
    userlistdialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    images.qrc

unix:!macx: LIBS += -L$$PWD/../../../../../../../opt/keysight/iolibs/ -lktvisa32

INCLUDEPATH += $$PWD/../../../../../../../opt/keysight/iolibs
DEPENDPATH += $$PWD/../../../../../../../opt/keysight/iolibs


TRANSLATIONS = \
    ../languages/rus.ts
