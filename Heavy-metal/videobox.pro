TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        player.c \
        pump.c \
        showboard.c \
        videobox.c

HEADERS += \
    common.h \
    defines.h \
    player.h \
    pump.h \
    showboard.h

TARGET = videobox
