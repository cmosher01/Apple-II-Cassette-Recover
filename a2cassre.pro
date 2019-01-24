QT -= gui

SOURCES += a2cassre.cpp

LIBS += -L/usr/local/lib -lSDL2
INCLUDEPATH += /usr/local/include/SDL2

target.path = /opt/$${TARGET}/bin
INSTALLS += target
