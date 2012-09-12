#! [0]
CONFIG      += plugin
QT          += widgets designer
#! [0]
TARGET      = $$qtLibraryTarget($$TARGET)
#! [1]
TEMPLATE    = lib
#! [1]
DESTDIR = $$QT.designer.plugins/designer

#! [2]
HEADERS     = worldtimeclock.h \
              worldtimeclockplugin.h
SOURCES     = worldtimeclock.cpp \
              worldtimeclockplugin.cpp
#! [2]

# install
target.path = $$[QT_INSTALL_PLUGINS]/designer
sources.files = $$SOURCES $$HEADERS *.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/qttools/designer/worldtimeclockplugin
INSTALLS += target sources
