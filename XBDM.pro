TEMPLATE = lib
CONFIG += console
CONFIG -= qt
CONFIG += debug
CONFIG += C++11

TARGET = XBDM

SOURCES += \
    main.cpp \
    Xbdm.cpp

HEADERS += \
    XbdmDefinitions.h \
    Xbdm.h

win32: LIBS += -lws2_32
