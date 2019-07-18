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
    infodialog.cpp \
    controldialog.cpp \
    bomdialog.cpp \
    templatesdialog.cpp \
    pindialog.cpp \
    textdialog.cpp \
    FootPrint.cpp \
    CompDefiner.cpp \
    compdialog.cpp \
    Component.cpp


HEADERS  += mainwindow.h \
    controldialog.h \
    compdialog.h \
    renderingdialog.h \
    infodialog.h \
    bomdialog.h \
    templatesdialog.h \
    pindialog.h \
    textdialog.h \
    myscrollarea.h \
    Board.h \
    GuiControl.h \
    Common.h \
    CompTypes.h \
    CompDefiner.h \
    Component.h \
    Pin.h \
    CompElement.h \
    Element.h \
    Grid.h \
    FootPrint.h \
    AdjInfo.h \
    AdjInfoManager.h \
    ColorManager.h \
    CompManager.h \
    GroupManager.h \
    HistoryManager.h \
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
    renderingdialog.ui \
    infodialog.ui \
    bomdialog.ui \
    templatesdialog.ui \
    pindialog.ui \
    textdialog.ui \
    compdialog.ui

RESOURCES     = veroroute.qrc

DESTDIR = ..

DISTFILES +=
