#-------------------------------------------------
#
# Project created by QtCreator 2013-07-01T09:05:55
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

CONFIG += optimize_full

TARGET = veroroute
TEMPLATE = app


SOURCES += main.cpp\
    mainwindow.cpp \
    mainwindow_drawing.cpp \
    mainwindow_events.cpp \
    renderingdialog.cpp \
    hotkeysdialog.cpp \
    infodialog.cpp \
    controldialog.cpp \
    bomdialog.cpp \
    templatesdialog.cpp \
    pindialog.cpp \
    textdialog.cpp \
    wiredialog.cpp \
    CompDefiner.cpp \
    compdialog.cpp \
    Component.cpp \
    Board_routing.cpp \
    Board_components.cpp \
    Board_import.cpp \
    Board.cpp \
    FootPrint.cpp \    
    GWriter.cpp


HEADERS  += mainwindow.h \
    controldialog.h \
    compdialog.h \
    renderingdialog.h \
    hotkeysdialog.h \
    infodialog.h \
    bomdialog.h \
    templatesdialog.h \
    pindialog.h \
    textdialog.h \
    wiredialog.h \
    myscrollarea.h \
    Board.h \
    GuiControl.h \
    Common.h \
    CompTypes.h \
    CompDefiner.h \
    Component.h \
    Pin.h \
    TrackElement.h \
    CompElement.h \
    Element.h \
    Grid.h \
    FootPrint.h \
    AdjInfo.h \
    AdjInfoManager.h \
    ColorManager.h \
    CompManager.h \
    GroupManager.h \
    GWriter.h \
    HistoryManager.h \
    MyRGB.h \
    NodeInfo.h \
    NodeInfoManager.h \
    Template.h \
    TemplateManager.h \
    Persist.h \
    Rect.h \
    RectManager.h \
    Shape.h \
    StringHelper.h \
    TextRect.h \
    TextManager.h \
    Version.h \
    VrtVersion.h


FORMS    += mainwindow.ui \
    controldialog.ui \
    compdialog.ui \
    renderingdialog.ui \
    hotkeysdialog.ui \
    infodialog.ui \
    bomdialog.ui \
    templatesdialog.ui \
    pindialog.ui \
    textdialog.ui \
    wiredialog.ui
    

RESOURCES     = veroroute.qrc

DESTDIR = ..

DISTFILES +=
