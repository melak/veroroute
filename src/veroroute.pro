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
    myscrollarea.cpp \
    padoffsetdialog.cpp \
    pindialog.cpp \
    renderingdialog.cpp \
    templatesdialog.cpp \
    textdialog.cpp \
    wiredialog.cpp \
    Board.cpp \
    Board_routing.cpp \
    Board_components.cpp \
    Board_import.cpp \
    ColorManager.cpp \
    CompTypes.cpp \
    CompDefiner.cpp \
    Component.cpp \
    CurveList.cpp \
    FootPrint.cpp \
    GuiControl.cpp \
    GWriter.cpp \
    SimplexFont.cpp


HEADERS  += bomdialog.h \
    controldialog.h \
    compdialog.h \
    finddialog.h \
    hotkeysdialog.h \
    infodialog.h \
    mainwindow.h \
    padoffsetdialog.h \
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
    CompTypes.h \
    CompDefiner.h \
    CompElement.h \
    CompManager.h \
    Component.h \
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
    VeroRouteAndroid.h \
    Version.h \
    VrtVersion.h
   

FORMS    += bomdialog.ui \
    compdialog.ui \
    controldialog.ui \
    finddialog.ui \
    hotkeysdialog.ui \
    infodialog.ui \
    mainwindow.ui \
    padoffsetdialog.ui \
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

# For Android builds, we don't want the whole unix section.  Just the following section ...
android {
ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android-sources

DISTFILES += android-sources/AndroidManifest.xml \

assets.files = ../tutorials/tutorial_0.vrt \
    ../tutorials/tutorial_1.vrt \
    ../tutorials/tutorial_2.vrt \
    ../tutorials/tutorial_3.vrt \
    ../tutorials/tutorial_4.vrt \
    ../tutorials/tutorial_5.vrt \
    ../tutorials/tutorial_6.vrt \
    ../tutorials/tutorial_7.vrt \
    ../tutorials/tutorial_8.vrt \
    ../tutorials/tutorial_9.vrt \
    ../tutorials/tutorial_10.vrt \
    ../tutorials/tutorial_11.vrt \
    ../tutorials/tutorial_12.vrt \
    ../tutorials/tutorial_13.vrt \
    ../tutorials/tutorial_14.vrt \
    ../tutorials/tutorial_15.vrt \
    ../tutorials/tutorial_16.vrt \
    ../tutorials/tutorial_17.vrt \
    ../tutorials/tutorial_18.vrt \
    ../tutorials/tutorial_19.vrt \
    ../tutorials/tutorial_20.vrt \
    ../tutorials/tutorial_21.vrt \
    ../tutorials/tutorial_22.vrt \
    ../tutorials/tutorial_23.vrt \
    ../tutorials/tutorial_24.vrt \
    ../tutorials/tutorial_25.vrt

assets.path = /assets/tutorials/

INSTALLS += target assets
}

unix : !android {
    target.path = $${PREFIX}/bin/
    target.files = ../veroroute

    manpage.path = $${PREFIX}/share/man/man1
    manpage.files = ../veroroute.1

    # If we've specified a PREFIX then replace the existing "../veroroute.desktop"
    # with one produced from "../veroroute.desktop.default"
    !isEmpty(PREFIX) {
        system(sed 's_/usr_$${PREFIX}_' ../veroroute.desktop.default > ../veroroute.desktop)
    }

    desktopentry.path = $${PREFIX}/share/applications
    desktopentry.files = ../veroroute.desktop

    pixmapA.path = $${PREFIX}/share/pixmaps
    pixmapA.files = ../veroroute.png

    pixmapB.path = $${PREFIX}/share/veroroute
    pixmapB.files = ../veroroute.png

    pixmapC.path = $${PREFIX}/share/icons/hicolor/72x72/apps
    pixmapC.files = ../veroroute.png

    tutorials.path = $${PREFIX}/share/veroroute/tutorials
    tutorials.files = ../tutorials/*

    gedasymbols.path = $${PREFIX}/share/gEDA/sym
    gedasymbols.files = ../libraries/gEDA/veroroute_*

    gedalib.path = $${PREFIX}/share/gEDA/gafrc.d
    gedalib.files = ../libraries/gEDA/veroroute-clib.scm

    INSTALLS += target manpage desktopentry pixmapA pixmapB pixmapC tutorials gedasymbols gedalib
}

