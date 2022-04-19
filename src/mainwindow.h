/*
	VeroRoute - Qt based Veroboard/Perfboard/PCB layout & routing application.

	Copyright (C) 2017  Alex Lawrow    ( dralx@users.sourceforge.net )

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "VeroRouteAndroid.h"
#include <QtGui>
#include <QApplication>
#include <QColorDialog>
#include <QDockWidget>
#include <QFileDialog>
#include <QLabel>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QWidget>
#include "HistoryManager.h"
#include "GWriter.h"
#include "myscrollarea.h"

#ifndef VEROROUTE_ANDROID
#define USE_PIXMAP_CACHE
#endif

namespace Ui { class MainWindow; }

class ControlDialog;
class TemplatesDialog;
class RenderingDialog;
class WireDialog;
class HotkeysDialog;
class InfoDialog;
class CompDialog;
class TextDialog;
class BomDialog;
class PinDialog;
class PadOffsetDialog;
class FindDialog;

const size_t MAX_RECENT_FILES = 10;

#ifdef VEROROUTE_ANDROID
#define ANDROID_VSCROLL_WIDTH  QString("QScrollBar:vertical { width: 20px; }")
#define ANDROID_HSCROLL_HEIGHT QString("QScrollBar:horizontal { height: 20px; }")
#endif

class MainWindow : public QMainWindow
{
	friend class ControlDialog;
	friend class TemplatesDialog;
	friend class RenderingDialog;
	friend class WireDialog;
	friend class HotkeysDialog;
	friend class InfoDialog;
	friend class CompDialog;
	friend class TextDialog;
	friend class BomDialog;
	friend class PinDialog;
	friend class PadOffsetDialog;
	friend class FindDialog;
	friend class MyScrollArea;

	Q_OBJECT

public:
	enum class MOUSE_MODE
	{
		SELECT			= 0,	// Select objects (parts, text boxes, etc)
		PAINT_PINS		= 1,	// Paint component pins (and the board)
		ERASE_PINS		= 2,	// Erase component pins (and the board)
		PAINT_GRID		= 3,	// Paint grid points only (not component pins)
		ERASE_GRID		= 4,	// Erase grid points only (not component pins)
		PAINT_FLOOD		= 5,	// Flood-fill all connected tracks & pins
		DEFINE_RECT		= 6,	// Define grey rectangle areas
		RESIZE_TEXT		= 7,	// Resize text rectangle
		SMART_PAN		= 8		// Move whole layout and resize/crop grid
	};

	explicit MainWindow(const QString& localDataPathStr, const QString& tutorialsPathStr, QWidget* parent = nullptr);
	~MainWindow();

	void ResetView(MOUSE_MODE eMouseMode = MOUSE_MODE::SELECT, bool bTutorial = false);
	void CheckFolders();
	void CheckHistory();
	// Helper for mouse
	void GetPixMapXY(const QPoint& currentPoint, int& pixmapX, int& pixmapY) const;
	bool GetRowCol(const QPoint& currentPoint, int& row, int& col, double& deltaRow, double& deltaCol) const;
	bool GetRowCol(const QPoint& currentPoint, const int rows, const int cols, int& row, int& col, double& deltaRow, double& deltaCol) const;
	bool HaveZeroDeltaRowCol(int& deltaRow, int& deltaCol);

	// Helpers for rendering
	void GetFirstRowCol(int& iRow, int& iCol) const;
	void GetXY(const GuiControl& guiCtrl, double row, double col, int& X, int& Y) const;
	void GetLRTB(const GuiControl& guiCtrl, double percent, double row, double col, int& L, int& R, int& T, int& B) const;
	void GetLRTB(const GuiControl& guiCtrl, const Component& comp, int& L, int& R, int& T, int& B) const;
	void GetLRTB(const GuiControl& guiCtrl, const Rect& rect, int& L, int& R, int& T, int& B) const;
	void GetXY(const GuiControl& guiCtrl, const Component& comp, int& X, int& Y) const;
	void GetRulerExact(Board& board, const QPoint& p, QPointF& pOut) const;

	const bool&	GetCtrlKeyDown() const	{ return m_bCtrlKeyDown;	}
	const bool&	GetShiftKeyDown() const	{ return m_bShiftKeyDown;	}

	bool		GetPaintAction() const	{ return m_eMouseMode >= MOUSE_MODE::PAINT_PINS && m_eMouseMode <= MOUSE_MODE::PAINT_FLOOD; }
	bool		GetPaintPins() const	{ return m_eMouseMode == MOUSE_MODE::PAINT_PINS; }
	bool		GetErasePins() const	{ return m_eMouseMode == MOUSE_MODE::ERASE_PINS; }
	bool		GetPaintBoard() const	{ return m_eMouseMode == MOUSE_MODE::PAINT_GRID; }
	bool		GetEraseBoard() const	{ return m_eMouseMode == MOUSE_MODE::ERASE_GRID; }
	bool		GetPaintFlood() const	{ return m_eMouseMode == MOUSE_MODE::PAINT_FLOOD; }
	bool		GetDefiningRect() const	{ return m_eMouseMode == MOUSE_MODE::DEFINE_RECT; }
	bool		GetResizingText() const	{ return m_eMouseMode == MOUSE_MODE::RESIZE_TEXT; }
	bool		GetSmartPan() const		{ return m_eMouseMode == MOUSE_MODE::SMART_PAN || GetCtrlKeyDown(); }

	void		SetCtrlKeyDown(bool b)	{ m_bCtrlKeyDown	= b; }
	void		SetShiftKeyDown(bool b)	{ m_bShiftKeyDown	= b; }
	void		SetPaintPins(bool b);
	void		SetErasePins(bool b);
	void		SetPaintBoard(bool b);
	void		SetEraseBoard(bool b);
	void		SetPaintFlood(bool b);
	void		SetDefiningRect(bool b);
	void		SetResizingText(bool b);
	void		SetSmartPan(bool b);

	const QString&	GetFileName() const { return m_fileName; }

	bool GetHaveFloatingPin(int& iFloatingNodeId);
	void MousePressEvent(const QPoint& pos, const bool& bLeftClick = true, const bool& bRightClick = true);
	void MouseDoubleClickEvent(const QPoint& pos);
	void MouseMoveEvent(const QPoint& pos);
	void MouseReleaseEvent(const QPoint& pos);
protected:
	void paintEvent(QPaintEvent* event);
	void wheelEvent(QWheelEvent* event);
	void keyPressEvent(QKeyEvent* event);
	void keyReleaseEvent(QKeyEvent* event);
#ifndef VEROROUTE_ANDROID
	void commonKeyPressEvent(QKeyEvent* event);		// So child dialogs can relay Ctrl and Shift to the main window
	void commonKeyReleaseEvent(QKeyEvent* event);	// So child dialogs can relay Ctrl and Shift to the main window
	void specialKeyPressEvent(QKeyEvent* event);	// So child dialogs can do Ctrl+Q etc
#endif
	void dragEnterEvent(QDragEnterEvent *e);
	void dropEvent(QDropEvent *e);
	bool eventFilter(QObject* object, QEvent* event);	// Used with installEventFilter to intercept the Android "back" button
public slots:
	void Startup();
	// File menu items
	void New();
	void Open();
	void OpenRecent();
	void Merge();
	void Save();
	void SaveAs();
	void ImportTango();
	void ImportOrcad();
	void WritePDF();
	void WritePNG();
	void WriteGerber(const bool& bTwoLayerGerber, const bool& bMetric);
	void WriteGerber1in()	{ WriteGerber(false, false); }
	void WriteGerber1mm()	{ WriteGerber(false, true); }
	void WriteGerber2in()	{ WriteGerber(true, false); }
	void WriteGerber2mm()	{ WriteGerber(true, true); }
	void ClearRecentFiles();
	void Quit();
	// View menu items
	void ZoomIn();
	void ZoomOut();
	void Crop();
	void ToggleGrid();
	void ToggleText();
	void ToggleFlipH();
	void ToggleFlipV();
	void TogglePinLabels();
	void ToggleFlyWires();
	void ToggleRuler();
	// Toolbar items
	void VeroV();
	void VeroH();
	void Fat();
	void Thin();
	void Curved();
	void ToggleDiagsMin();
	void ToggleDiagsMax();
	void ToggleFill();
	void ToggleSelectArea();
	void TogglePaintGrid();
	void ToggleEraseGrid();
	void TogglePaintPins();
	void ToggleErasePins();
	void TogglePaintFlood();
	void ResetMouseMode();
	// Edit menu items
	void Undo();
	void Redo();
	void SmartPanOn();
	void Copy();
	void Group();
	void Ungroup();
	void SelectAll();
	void SelectAllInRects();
	void Delete();
	// Add menu items
	void AddMarker()			{ AddPart(COMP::MARK); }
	void AddPad()				{ AddPart(COMP::PAD); }
	void AddPadFlyWire()		{ AddPart(COMP::PAD_FLYINGWIRE); }
	void AddWire()				{ AddPart(COMP::WIRE); }
	void AddResistor()			{ AddPart(COMP::RESISTOR); }
	void AddInductor()			{ AddPart(COMP::INDUCTOR); }
	void AddCrystal()			{ AddPart(COMP::CRYSTAL); }
	void AddDiode()				{ AddPart(COMP::DIODE); }
	void AddLED()				{ AddPart(COMP::LED); }
	void AddCapCeramic()		{ AddPart(COMP::CAP_CERAMIC); }
	void AddCapFilm()			{ AddPart(COMP::CAP_FILM); }
	void AddCapFilmWide()		{ AddPart(COMP::CAP_FILM_WIDE); }
	void AddCapElectro200NP()	{ AddPart(COMP::CAP_ELECTRO_200_NP); }
	void AddCapElectro250NP()	{ AddPart(COMP::CAP_ELECTRO_250_NP); }
	void AddCapElectro300NP()	{ AddPart(COMP::CAP_ELECTRO_300_NP); }
	void AddCapElectro400NP()	{ AddPart(COMP::CAP_ELECTRO_400_NP); }
	void AddCapElectro500NP()	{ AddPart(COMP::CAP_ELECTRO_500_NP); }
	void AddCapElectro600NP()	{ AddPart(COMP::CAP_ELECTRO_600_NP); }
	void AddCapElectro200()		{ AddPart(COMP::CAP_ELECTRO_200); }
	void AddCapElectro250()		{ AddPart(COMP::CAP_ELECTRO_250); }
	void AddCapElectro300()		{ AddPart(COMP::CAP_ELECTRO_300); }
	void AddCapElectro400()		{ AddPart(COMP::CAP_ELECTRO_400); }
	void AddCapElectro500()		{ AddPart(COMP::CAP_ELECTRO_500); }
	void AddCapElectro600()		{ AddPart(COMP::CAP_ELECTRO_600); }
	void AddTO92()				{ AddPart(COMP::TO92); }
	void AddTO18()				{ AddPart(COMP::TO18); }
	void AddTO39()				{ AddPart(COMP::TO39); }
	void AddTO220()				{ AddPart(COMP::TO220); }
	void AddTrimVert()			{ AddPart(COMP::TRIM_VERT); }
	void AddTrimVertOffset()	{ AddPart(COMP::TRIM_VERT_OFFSET); }
	void AddTrimVertOffsetWide(){ AddPart(COMP::TRIM_VERT_OFFSET_WIDE); }
	void AddTrimFlat()			{ AddPart(COMP::TRIM_FLAT); }
	void AddTrimFlatWide()		{ AddPart(COMP::TRIM_FLAT_WIDE); }
	void AddSIP()				{ AddPart(COMP::SIP); }
	void AddDIP()				{ AddPart(COMP::DIP); }
	void AddStrip100()			{ AddPart(COMP::STRIP_100); }
	void AddBlock100()			{ AddPart(COMP::BLOCK_100); }
	void AddBlock200()			{ AddPart(COMP::BLOCK_200); }
	void AddSwitchST()			{ AddPart(COMP::SWITCH_ST); }
	void AddSwitchDT()			{ AddPart(COMP::SWITCH_DT); }
	void AddSwitchST_DIP()		{ AddPart(COMP::SWITCH_ST_DIP); }
	void AddVeroNumbers()		{ AddPart(COMP::VERO_NUMBER); }
	void AddVeroLetters()		{ AddPart(COMP::VERO_LETTER); }
	void AddTextBox()
	{
		SetCurrentTextId(BAD_TEXTID);
		int iRow, iCol;
		GetFirstRowCol(iRow, iCol);
		m_board.AddTextBox(iRow, iCol);
		UpdateHistory("add text");
		ShowTextDialog();
		UpdateControls();
		RepaintSkipRouting();
	}
	// Windows menu items + Other dialogs
	void ToggleControlDialog();
	void ToggleCompDialog();
	void ToggleTemplatesDialog();
	void ToggleRenderingDialog();
	void ToggleInfoDialog();
	void TogglePinDialog();
	void ShowControlDialog();
	void ShowCompDialog();
	void HideCompDialog();
	void ShowTemplatesDialog();
	void HideTemplatesDialog();
	void ShowRenderingDialog();
	void HideRenderingDialog();
	void ShowInfoDialog();
	void HideInfoDialog();
	void ShowPinDialog();
	void HidePinDialog();
	void ShowBomDialog();
	void HideBomDialog();
	void ShowWireDialog();
	void HideWireDialog();
	void ShowFindDialog();
	void HideFindDialog();
	void ShowTextDialog();
	void ShowAbout();
	void ShowSupport();
	void ShowHotkeysDialog();
	void ShowPadOffsetDialog();	// Triggered by clicking on a pin for 1000ms
	void HidePadOffsetDialog(bool bForce = false);
	// Layers menu items
	void AddLayer();
	void RemoveLayer();
	void SwitchLayer();
	void ToggleVias();
	void ResetLayerPrefs();
	// Help menu items
	void LoadFirstTutorial();
	void LoadPrevTutorial();
	void LoadNextTutorial();
	void LoadTutorial();		// Helper
	// Check version against Sourceforge
	void UpdateCheck();
	void HandleNetworkReply(QNetworkReply* reply);

	// Helpers for child dialogs
	void ShowDlg(QWidget* p);
	void HideDlg(QWidget* p);
	void HideAllDockedDlgs();
	void HideAllNonDockedDlgs();

	// View controls
	void TrackSliderChanged(int i);	// Actually a helper for the following 3 checkboxes
	void CheckBoxMonoChanged(bool b);
	void CheckBoxColorChanged(bool b);
	void CheckBoxPcbChanged(bool b);
	void SaturationSliderChanged(int i);
	void CompSliderChanged(int i);	// Actually a helper for the following 3 checkboxes
	void CheckBoxLineChanged(bool b);
	void CheckBoxNameChanged(bool b);
	void CheckBoxValueChanged(bool b);
	void FillSliderChanged(int i);
	void MarginChanged(int i);
	void SetShowGrid(bool b);
	void SetShowText(bool b);
	void SetFlipH(bool b);
	void SetFlipV(bool b);
	void SetShowPinLabels(bool b);
	void SetShowFlyWires(bool b);
	void SetFill(bool b);
	// Part controls
	void SetCompName(const QString& str);
	void SetCompValue(const QString& str);
	void SetCompType(const QString& str);
	void SetCompCustomFlag(const bool& b);
	void SetCompPadWidth(const int& i);
	void SetCompHoleWidth(const int& i);
	void CompRotateCCW()	{ CompRotate(false); }
	void CompRotateCW()		{ CompRotate(true); }
	void CompGrow()			{ CompStretch(true); }
	void CompShrink()		{ CompStretch(false); }
	void CompGrow2()		{ CompStretchWidth(true); }
	void CompShrink2()		{ CompStretchWidth(false); }
	void CompTextCentre()	{ CompTextMove(0,  0); }
	void CompTextL()		{ CompTextMove(0, -1); }
	void CompTextR()		{ CompTextMove(0,  1); }
	void CompTextT()		{ CompTextMove(-1, 0); }
	void CompTextB()		{ CompTextMove( 1, 0); }
	// Pin Shift
	void PadCentre()		{ PadMove(0,  0); }
	void PadMoveL()			{ PadMove(0, -1); }
	void PadMoveR()			{ PadMove(0,  1); }
	void PadMoveT()			{ PadMove(-1, 0); }
	void PadMoveB()			{ PadMove( 1, 0); }
	// Bad Nodes lists
	void SetNodeId(QListWidgetItem* p);
	void ListNodes(bool bRebuild = true);
	// Routing controls
	void EnableRouting(bool b);
	void EnableFastRouting(bool b);
	void Paste();
	void Tidy();
	void WipeTracks();
	// Node color
	void AutoColor(bool b);
	void SelectNodeColor();
	// Track controls
	void SetTracksVeroV(bool b);
	void SetTracksVeroH(bool b);
	void SetTracksFat(bool b);
	void SetTracksThin(bool b);
	void SetTracksCurved(bool b);
	void SetDiagonalsOff(bool b);
	void SetDiagonalsMin(bool b);
	void SetDiagonalsMax(bool b);
	// Rendering options
	void SetBrightness(int i);
	void SetTrackWidth(int i);
	void SetTagWidth(int i);
	void SetHoleWidth(int i);
	void SetPadWidth(int i);
	void SetGapWidth(int i);
	void SetMaskWidth(int i);
	void SetSilkWidth(int i);
	void SetEdgeWidth(int i);
	void SetViaPadWidth(int i);
	void SetViaHoleWidth(int i);
	void SetTextSizeComp(int i);
	void SetTextSizePins(int i);
	void SetTargetRows(int i);
	void SetTargetCols(int i);
	void SetShowTarget(bool b);
	void SetXthermals(bool b);
	void SetShowCloseTracks(bool b);
	void SetAntialiasOff(bool b);
	void SetAntialiasOn(bool b);
	// Find parts by name/value
	void ClearFind();
	void Find(bool bUseName, bool bExact, const QString& str);
	size_t GetNumFound();
	// Wire options
	void SetWireShare(bool b);
	void SetWireCross(bool b);
	// For text box dialog
	void SizeChanged(int i);
	void ToggleBold();
	void ToggleItalic();
	void ToggleUnderline();
	void AlignL();
	void AlignC();
	void AlignR();
	void AlignJ();
	void AlignTop();
	void AlignMid();
	void AlignBot();
	void SetText(const QString&);
	void ChooseTextColor();
	// For component editor dialog
	CompDefiner& GetCompDefiner() { return m_board.GetCompDefiner(); }
	// For template dialog
	TemplateManager& GetTemplateManager() { return m_templateMgr; }
	// Info dialog
	void SetInfoStr(const QString& str)	{ m_board.SetInfoStr(str.toStdString()); }
	void OpenVrt(const QString& fileName, bool bMerge, bool bCrashRecovery = false);	// Helper for opening a vrt using Open(), Merge(), dropEvent(), or the command line
	// Component editor
	void DefinerSetValueStr(const QString& str);
	void DefinerSetPrefixStr(const QString& str);
	void DefinerSetTypeStr(const QString& str);
	void DefinerSetImportStr(const QString& str);
	void DefinerWidthChanged(int i);
	void DefinerHeightChanged(int i);
	void DefinerPadWidthChanged(int i);
	void DefinerHoleWidthChanged(int i);
	void DefinerSetPinShapeType(const QString& str);
	void DefinerToggledPinLabels(bool b);
	void DefinerToggledCustomFlag(bool b);
	void DefinerToggleShapeLine(bool b);
	void DefinerToggleShapeFill(bool b);
	void DefinerSetPinNumber(int i);
	void DefinerIncPinNumber(bool b);
	void DefinerSetSurface(const QString& str);
	void DefinerSetShapeType(const QString& str);
	void DefinerSetCX(double d);
	void DefinerSetCY(double d);
	void DefinerSetDX(double d);
	void DefinerSetDY(double d);
	void DefinerSetA1(double d);
	void DefinerSetA2(double d);
	void DefinerSetA3(double d);
	void DefinerBuild();
	void DefinerToggleEditor();
	void DefinerAddLine();
	void DefinerAddRect();
	void DefinerAddRoundedRect();
	void DefinerAddEllipse();
	void DefinerAddArc();
	void DefinerAddChord();
	void DefinerChooseColor();
	void DefinerRaise();
	void DefinerLower();

	// GUI update
	void UpdateControls();
private:
	// GUI update
	void UpdateRecentFiles(const QString* pFileName, bool bAdd);
	void UpdateWindowTitle();
	void UpdateRulerInfo();
	void UpdateCompDialog();
	void EnableCompDialogControls();
	void UpdateBOM();
	void UpdateTemplatesDialog();
	void UpdateTextDialog(bool bFull = false);

	void DestroyPixmapCache();
#ifdef USE_PIXMAP_CACHE
	void CreatePixmapCache(const GuiControl& guiCtrl, ColorManager& colorManager);
	void PaintDiag(const GuiControl& guiCtrl, QPainter& painter, const QColor& color, const QPointF& pCorner, bool bLT);
#endif
	void PaintViaGrey(const GuiControl& guiCtrl, QPainter& painter, const QPointF& pC);
	void PaintPadGrey(const GuiControl& guiCtrl, QPainter& painter, QPen& pen, const QPointF& pC, const int& iPadWidthMIL = 0);
	void PaintVia(const GuiControl& guiCtrl, QPainter& painter,  const QColor& color, const QPointF& pC, const bool& bGap = false);	// Helper
	void PaintPad(const GuiControl& guiCtrl, QPainter& painter,  const QColor& color, const QPointF& pC, const int& iPadWidthMIL = 0, const int& iHoleWidth_MIL = 0, const bool& bGap = false);	// Helper
	void PaintBlob(const GuiControl& guiCtrl, QPainter& painter, const QColor& color, const QPointF& pC, const QPointF& pCoffset,
				   const int& iPadWidthMIL, const int& iPerimeterCode, const int& iTagCode,
				   const bool bHavePad, const bool bIsGnd, const bool bGap = false);	// Helper
	void PaintBoard();
	void PaintCompDefiner();
	void HandleRouting(const bool bSingleRoute = false);
	void RepaintWithListNodes(bool bNow = false);
	void RepaintWithRouting(bool bNow = false);
	void RepaintSkipRouting(bool bNow = false);
	void ShowCurrentRectSize();	// Show current rect size in status bar
	int	 GetCurrentLayer() const			{ return m_board.GetCurrentLayer(); }
	int	 GetCurrentNodeId() const			{ return m_board.GetCurrentNodeId(); }
	int  GetCurrentCompId() const			{ return m_board.GetCurrentCompId(); }
	int  GetCurrentTextId() const			{ return m_board.GetCurrentTextId(); }
	int  GetCurrentPinId() const			{ return m_board.GetCurrentPinId(); }
	int  GetCurrentShapeId() const			{ return m_board.GetCurrentShapeId(); }
	void SetCurrentNodeId(const int& i)		{ m_board.SetCurrentNodeId(i);	ListNodes(false); }
	void SetCurrentCompId(const int& i)		{ m_board.SetCurrentCompId(i);	UpdateControls(); }
	void SetCurrentTextId(const int& i)		{ m_board.SetCurrentTextId(i);	UpdateControls(); }
	void SetCurrentPinId(const int& i)		{ m_board.SetCurrentPinId(i);	UpdateCompDialog(); }
	void SetCurrentShapeId(const int& i)	{ m_board.SetCurrentShapeId(i);	UpdateCompDialog(); UpdateControls(); }
	TextRect& GetCurrentTextRect()			{ return m_board.GetTextMgr().GetTextRectById( GetCurrentTextId() ); }
	// Helpers for slots
	void ZoomHelper(int delta);
	void AddPart(COMP eType)
	{
		if ( m_board.GetCompEdit() ) return;	// Do nothing in component editor mode

		ResetMouseMode();

		int iRow, iCol;
		GetFirstRowCol(iRow, iCol);
		const int compId = m_board.CreateComponent(iRow, iCol, eType);
		if ( compId == BAD_COMPID ) return;	// Reached component limit

		GroupManager& groupMgr = m_board.GetGroupMgr();
		groupMgr.ResetUserGroup( compId );	// Reset the user group with the current comp (and its siblings)
		SetCurrentCompId(compId);

		UpdateHistory("add part"); UpdateControls(); UpdateBOM(); RepaintSkipRouting();
	}
	void AddFromTemplate(const Component& compTemp)
	{
		if ( compTemp.GetType() != COMP::CUSTOM ) return AddPart( compTemp.GetType() );

		if ( m_board.GetCompEdit() ) return;	// Do nothing in component editor mode

		ResetMouseMode();

		int iRow, iCol;
		GetFirstRowCol(iRow, iCol);
		const int compId = m_board.CreateComponent(iRow, iCol, compTemp.GetType(), &compTemp);
		if ( compId == BAD_COMPID ) return;	// Reached component limit

		GroupManager& groupMgr = m_board.GetGroupMgr();
		groupMgr.ResetUserGroup( compId );	// Reset the user group with the current comp (and its siblings)
		SetCurrentCompId(compId);

		UpdateHistory("add part"); UpdateControls(); UpdateBOM(); RepaintSkipRouting();
	}
	void CompRotate(const bool& bCW);
	void CompStretch(const bool& bGrow);
	void CompStretchWidth(const bool& bGrow);
	void CompTextMove(const int& deltaRow, const int& deltaCol);
	void PadMove(const int& deltaRowMil, const int& deltaColMil);
	void UpdatePadInfo();

	// Helpers
	void SetQuality(QPainter& p);
	void ResetRuler();
	bool CanModifyRuler() const;
public:
	bool CanZoomIn() const;
	bool CanZoomOut() const;
private:
	bool GetIsModified() const;
	bool GetMatchesVrtFile(const std::string& fileName) const;
	void ResetHistory(const std::string& str);
	void UpdateHistory(const std::string& str, const int objId = -1);
	void UpdateUndoRedoControls();
	void SetMouseActionString(const std::string& str, const int objId = -1);

	// Helper to auto-append suffix when writing a file
	QString GetSaveFileName(const QString& caption, const QString& nameFilter, const QString& defaultSuffix);

	QColor	GetBackgroundColor() const;	// For screen only.  PDF is always white.

	// Pens
	QPen	m_rulerPen;
	QPen	m_backgroundPen;
	QPen	m_greyPen;
	QPen	m_blackPen;
	QPen	m_whitePen;
	QPen	m_redPen;
	QPen	m_orangePen;
	QPen	m_yellowPen;
	QPen	m_lightBluePen;
	QPen	m_varPen;
	QPen	m_dotPen;
	QPen	m_dashPen;
	QBrush	m_backgroundBrush;
	QBrush	m_darkBrush;
	QBrush	m_varBrush;

	QNetworkAccessManager	m_networkMgr;	// For checking version against Sourceforge
	QAction*				m_recentFileAction[MAX_RECENT_FILES];
	QAction*				m_separator			= nullptr;	// At the end of the recent files list
	Ui::MainWindow*			ui					= nullptr;
	MyScrollArea*			m_scrollArea		= nullptr;	// The mainwindow contains a scrollable area ...
	QLabel*					m_label				= nullptr;	// ... for a QLabel widget that ...
	QPixmap					m_mainPixmap;					// ... contains a pixmap image of the whole board
	QLabel*					m_labelInfo			= nullptr;	// For permanent status bar text
	QLabel*					m_labelStatus		= nullptr;	// For permanent status bar text
#ifdef VEROROUTE_DEBUG
	QLabel*					m_labelDebug		= nullptr;	// For permanent status bar text
#endif

	ControlDialog*			m_controlDlg		= nullptr;	QDockWidget* m_dockControlDlg	= nullptr;
	CompDialog*				m_compDlg			= nullptr;	QDockWidget* m_dockCompDlg		= nullptr;
	TemplatesDialog*		m_templatesDlg		= nullptr;	QDockWidget* m_dockTemplatesDlg	= nullptr;
	RenderingDialog*		m_renderingDlg		= nullptr;	QDockWidget* m_dockRenderingDlg	= nullptr;
	InfoDialog*				m_infoDlg			= nullptr;	QDockWidget* m_dockInfoDlg		= nullptr;
	PinDialog*				m_pinDlg			= nullptr;	QDockWidget* m_dockPinDlg		= nullptr;
	WireDialog*				m_wireDlg			= nullptr;
	HotkeysDialog*			m_hotkeysDlg		= nullptr;
	TextDialog*				m_textDlg			= nullptr;
	BomDialog*				m_bomDlg			= nullptr;
	PadOffsetDialog*		m_padOffsetDlg		= nullptr;
	FindDialog*				m_findDlg			= nullptr;

	Board					m_board;			// *** The main object ***
	HistoryManager			m_historyMgr;		// Class to manage undo/redo
	TemplateManager			m_templateMgr;		// For component templates
	GWriter					m_gWriter;			// Writes Gerber files
	QString					m_fileName;			// The loaded/saved .vrt file
	QString					m_pdfFileName;		// The saved PDF file
	QString					m_gerberFileName;	// The saved Gerber file
	int						m_mouseObjId = -1;	// For the undo/redo history
	std::string				m_mouseActionString;// For the undo/redo history
	std::string				m_localDataPathStr;	// The path to the "history" and "templates" folders
	std::string				m_tutorialsPathStr;	// The path to the "tutorials" folder and "veroroute.png"

	// Cached pixmaps containing pre-colored pads and blobs.
#ifdef USE_PIXMAP_CACHE
	QPixmap**	m_ppPixmapPad		= nullptr;	// A pad in the host element
	QPixmap**	m_ppPixmapVia		= nullptr;	// A via in the host element
	QPixmap**	m_ppPixmapDiag		= nullptr;	// For filling small diagonal gaps not covered by blob pixmaps
	QPixmap**	m_ppPixmapBlob		= nullptr;	// A composite shape with all the host element connections
	QPixmap*	m_pPixmapDiagLT		= nullptr;	// Used instead of m_ppPixmapDiag for custom colors
	QPixmap*	m_pPixmapDiagRT		= nullptr;	// Used instead of m_ppPixmapDiag for custom colors
	int			m_radPixmapPad		= 0;		// Half pixmap width ...
	int			m_radPixmapVia		= 0;		// ...
	int			m_radPixmapDiag		= 0;		// ...
	int			m_radPixmapBlob		= 0;		// ...
#endif
	QPoint		m_mousePos;
	QPoint		m_clickedPos;
	bool		m_bRepaint			= false;	// Flag to make paintEvent() do something useful
	bool		m_bMouseClick		= false;	// Flag of click beginning
	bool		m_bLeftClick		= false;
	bool		m_bRightClick		= false;
	bool		m_bCtrlKeyDown		= false;
	bool		m_bShiftKeyDown		= false;

	MOUSE_MODE	m_eMouseMode		= MOUSE_MODE::SELECT;

	bool		m_bWritePDF			= false;	// true ==> draw to PDF file instead of screen
	bool		m_bWriteGerber		= false;	// true ==> draw to Gerber file instead of screen
	bool		m_bTwoLayerGerber	= false;	// true ==> 2-layer Gerber output instead of 1-layer
	bool		m_bHistoryDir		= false;	// true ==> have "history" folder
	bool		m_bTemplatesDir		= false;	// true ==> have "templates" folder
	bool		m_bRuler			= false;
	bool		m_bModifyRulerA		= false;
	bool		m_bUpdatingControls	= false;
	bool		m_bReRoute			= false;
	bool		m_bReListNodes		= false;
	int			m_XGRIDOFFSET		= 0;		// So we can centre when writing to PDF
	int			m_YGRIDOFFSET		= 0;		// So we can centre when writing to PDF
	int			m_XCORRECTION		= 0;		// So we can fully render pads larger than 100 mil diameter
	int			m_YCORRECTION		= 0;		// So we can fully render pads larger than 100 mil diameter
	int			m_gridRow			= 0;		// Board row corresponding to mouse position
	int			m_gridCol			= 0;		// Board col correspondong to mouse position
	QPoint		m_rulerA;
	QPoint		m_rulerB;
	int			m_iTutorialNumber	= -1;		// Tutorial file number 0,1,2,... (or -1 if not in tutorial mode)
};
