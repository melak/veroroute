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


SOURCES += bomdialog.cpp \
    compdialog.cpp \
    controldialog.cpp \
    finddialog.cpp \
    hotkeysdialog.cpp \
    infodialog.cpp \
    main.cpp \
    mainwindow.cpp \
    mainwindow_drawing.cpp \
    mainwindow_events.cpp \
    pindialog.cpp \
    renderingdialog.cpp \
    templatesdialog.cpp \
    textdialog.cpp \
    wiredialog.cpp \
    Board.cpp \
    Board_routing.cpp \
    Board_components.cpp \
    Board_import.cpp \
    CompDefiner.cpp \
    Component.cpp \
    CurveList.cpp \
    FootPrint.cpp \  
    GuiControl.cpp \
    GWriter.cpp


HEADERS  += bomdialog.h \
    controldialog.h \
    compdialog.h \
    finddialog.h \
    hotkeysdialog.h \
    infodialog.h \
    mainwindow.h \
    pindialog.h \
    renderingdialog.h \
    templatesdialog.h \
    textdialog.h \
    wiredialog.h \
    myscrollarea.h \
    AdjInfo.h \
    AdjInfoManager.h \   
    Board.h \
    ColorManager.h \
    Common.h \
    Component.h \
    CompDefiner.h \
    CompElement.h \
    CompManager.h \    
    CompTypes.h \
    ConnectionMatrix.h \
    CurveList.h \
    Element.h \
    FootPrint.h \
    Grid.h \
    GroupManager.h \
    GuiControl.h \
    GPainter.h \
    GWriter.h \
    HistoryManager.h \
    MyRGB.h \
    NodeInfo.h \
    NodeInfoManager.h \
    Pin.h \
    PolygonHelper.h \
    Persist.h \
    Rect.h \
    RectManager.h \
    Shape.h \
    SimplexFont.h \
    SpanningTreeHelper.h \
    StringHelper.h \
    Template.h \
    TemplateManager.h \
    TextRect.h \
    TextManager.h \
    TrackElement.h \
    Transform.h \
    Version.h \
    VrtVersion.h
   

FORMS    += bomdialog.ui \
    compdialog.ui \
    controldialog.ui \
    finddialog.ui \
    hotkeysdialog.ui \
    infodialog.ui \
    mainwindow.ui \
    pindialog.ui \
    renderingdialog.ui \
    templatesdialog.ui \
    textdialog.ui \
    wiredialog.ui
    

RESOURCES     = veroroute.qrc

DESTDIR = ..

DISTFILES +=

QMAKE_INSTALL_FILE    = install -m 644
QMAKE_INSTALL_PROGRAM = install -m 755

unix {
    target.path = $${PREFIX}/bin/
    target.files = ../veroroute

    manpage.path = $${PREFIX}/share/man/man1
    manpage.files = ../veroroute.1

    desktopentry.path = $${PREFIX}/share/applications
    desktopentry.files = ../veroroute.desktop

    pixmapA.path = $${PREFIX}/share/pixmaps
    pixmapA.files = ../veroroute.png

    pixmapB.path = $${PREFIX}/share/veroroute
    pixmapB.files = ../veroroute.png

    tutorials.path = $${PREFIX}/share/veroroute/tutorials
    tutorials.files = ../tutorials/*

    gedasymbols.path = $${PREFIX}/share/gEDA/sym
    gedasymbols.files = ../libraries/gEDA/veroroute_*

    gedalib.path = $${PREFIX}/share/gEDA/gafrc.d
    gedalib.files = ../libraries/gEDA/veroroute-clib.scm

    INSTALLS += target manpage desktopentry pixmapA pixmapB tutorials gedasymbols gedalib
}

