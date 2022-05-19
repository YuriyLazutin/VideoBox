TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        logger.c \
        player.c \
        pump.c \
        showboard.c \
        videobox.c

HEADERS += \
    defines.h \
    logger.h \
    player.h \
    pump.h \
    showboard.h

TARGET = videobox
