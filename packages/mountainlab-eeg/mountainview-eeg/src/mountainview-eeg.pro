QT += core gui network

QT += qml

CONFIG += c++11
QMAKE_CXXFLAGS += -Wno-reorder #qaccordion

CONFIG -= app_bundle #Please apple, don't make a bundle today

include(../../../../mlcommon/mlcommon.pri)
include(../../../../mlcommon/mda.pri)
include(../../../../mlcommon/taskprogress.pri)

QT += widgets
QT+=concurrent

DESTDIR = ../../bin
OBJECTS_DIR = ../build
MOC_DIR=../build
TARGET = mountainview-eeg
TEMPLATE = app

INCLUDEPATH += plugins views
VPATH += plugins views
HEADERS += clustermetricsplugin.h clustermetricsview.h
SOURCES += clustermetricsplugin.cpp clustermetricsview.cpp

HEADERS += timeseriesplugin.h timeseriesview.h
SOURCES += timeseriesplugin.cpp timeseriesview.cpp
INCLUDEPATH += views/timeseriesview
VPATH += views/timeseriesview
HEADERS += mvtimeseriesrendermanager.h mvtimeseriesviewbase.h
SOURCES += mvtimeseriesrendermanager.cpp mvtimeseriesviewbase.cpp

HEADERS += spectrogramplugin.h spectrogramview.h
SOURCES += spectrogramplugin.cpp spectrogramview.cpp

INCLUDEPATH += msv/contextmenuhandlers
VPATH += msv/contextmenuhandlers

SOURCES += mveegmain.cpp

HEADERS += mveegcontext.h
SOURCES += mveegcontext.cpp

INCLUDEPATH += controlwidgets
VPATH += controlwidgets
HEADERS += openviewscontrol.h eegprefscontrol.h
SOURCES += openviewscontrol.cpp eegprefscontrol.cpp

INCLUDEPATH += ../../../../mountainview/src/core
VPATH += ../../../../mountainview/src/core
HEADERS += \
    closemehandler.h flowlayout.h imagesavedialog.h \
    mountainprocessrunner.h mvabstractcontextmenuhandler.h \
    mvabstractcontrol.h mvabstractview.h mvabstractviewfactory.h \
    mvcontrolpanel2.h mvmainwindow.h mvstatusbar.h \
    mvabstractcontext.h tabber.h tabberframe.h taskprogressview.h actionfactory.h mvabstractplugin.h

SOURCES += \
    closemehandler.cpp flowlayout.cpp imagesavedialog.cpp \
    mountainprocessrunner.cpp mvabstractcontextmenuhandler.cpp \
    mvabstractcontrol.cpp mvabstractview.cpp mvabstractviewfactory.cpp \
    mvcontrolpanel2.cpp mvmainwindow.cpp mvstatusbar.cpp \
    mvabstractcontext.cpp tabber.cpp tabberframe.cpp taskprogressview.cpp actionfactory.cpp mvabstractplugin.cpp

INCLUDEPATH += ../../../../mountainview/src/misc
VPATH += ../../../../mountainview/src/misc
HEADERS += mvmisc.h mvutils.h paintlayer.h paintlayerstack.h renderablewidget.h multiscaletimeseries.h
SOURCES += mvmisc.cpp mvutils.cpp paintlayer.cpp paintlayerstack.cpp renderablewidget.cpp multiscaletimeseries.cpp

INCLUDEPATH += ../../../../mountainsort/src/utils
VPATH += ../../../../mountainsort/src/utils
HEADERS += get_sort_indices.h msmisc.h
SOURCES += get_sort_indices.cpp msmisc.cpp

INCLUDEPATH += ../../../../prv-gui/src
VPATH += ../../../../prv-gui/src
HEADERS += prvgui.h
SOURCES += prvgui.cpp

INCLUDEPATH += ../../../../mountainview/src/controlwidgets
VPATH += ../../../../mountainview/src/controlwidgets
HEADERS += resolveprvsdialog.h
SOURCES += resolveprvsdialog.cpp
FORMS += resolveprvsdialog.ui

INCLUDEPATH += ../../../../mountainview/src/3rdparty/qaccordion/include
VPATH += ../../../../mountainview/src/3rdparty/qaccordion/include
VPATH += ../../../../mountainview/src/3rdparty/qaccordion/src
HEADERS += qAccordion/qaccordion.h qAccordion/contentpane.h qAccordion/clickableframe.h
SOURCES += qaccordion.cpp contentpane.cpp clickableframe.cpp

RESOURCES += ../../../../mountainview/src/mountainview.qrc \
    ../../../../mountainview/src/3rdparty/qaccordion/icons/qaccordionicons.qrc

#TODO: Do we need openmp for mountainview?
#OPENMP
!macx {
  QMAKE_LFLAGS += -fopenmp
  QMAKE_CXXFLAGS += -fopenmp
}
#-std=c++11   # AHB removed since not in GNU gcc 4.6.3

