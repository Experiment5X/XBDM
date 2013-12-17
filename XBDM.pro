TEMPLATE = lib
CONFIG += console
CONFIG -= qt
CONFIG += debug
CONFIG += c++11

TARGET = XBDM

unix {
    CONFIG += staticlib app_bundle
}

SOURCES += \
    Xbdm.cpp

HEADERS += \
    XbdmDefinitions.h \
    Xbdm.h

win32: LIBS += -lws2_32
