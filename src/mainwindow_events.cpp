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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "PolygonHelper.h"
#include "wiredialog.h"
#include "bomdialog.h"
#include "finddialog.h"
#include "pindialog.h"
#include <QtGlobal>

static const bool ALLOW_DELAY_BASED_SMART_PAN = true;
static const bool ALLOW_DELAY_BASED_PAD_SHIFT = true;

// Following 2 are to slow down the auto-panning while moving components with the mouse
static std::chrono::steady_clock::time_point g_lastAutoPanTime;
static bool g_bHaveAutoPanned = false;

#ifdef VEROROUTE_ANDROID
// Following is avoid too many redraws while defining rectangles
static std::chrono::steady_clock::time_point g_lastDrawRect;
#endif

static std::chrono::steady_clock::time_point g_lastMouseClickTime;	// For implementation of ALLOW_DELAY_BASED_SMART_PAN / ALLOW_DELAY_BASED_PAD_SHIFT
static bool g_bPinClicked = false;									// For implementation of ALLOW_DELAY_BASED_PAD_SHIFT

void MainWindow::GetPixMapXY(const QPoint& currentPoint, int& pixmapX, int& pixmapY) const
{
	const int iLeftDlgWidth = m_dockInfoDlg->isVisible() ? m_dockInfoDlg->width() :
							  m_dockPinDlg->isVisible()  ? m_dockPinDlg->width() : 0;

	pixmapX = currentPoint.x() + m_scrollArea->horizontalScrollBar()->value() - iLeftDlgWidth;
	pixmapY = currentPoint.y() + m_scrollArea->verticalScrollBar()->value() - ui->menuBar->height();

	int gndL, gndR, gndT, gndB;
	m_board.GetGroundFillBounds(gndL, gndR, gndT, gndB);

	pixmapX += gndL;
	pixmapY += gndT;
}

bool MainWindow::GetRowCol(const QPoint& currentPoint, int& row, int& col, double& deltaRow, double& deltaCol) const
{
	return GetRowCol(currentPoint, m_board.GetRows(), m_board.GetCols(), row, col, deltaRow, deltaCol);
}

bool MainWindow::GetRowCol(const QPoint& currentPoint, const int rows, const int cols, int& row, int& col, double& deltaRow, double& deltaCol) const
{
	const int& W = m_board.GetGRIDPIXELS();	// Square width in pixels

	int pixmapX(0), pixmapY(0);
	GetPixMapXY(currentPoint, pixmapX, pixmapY);

	deltaRow = pixmapY * 1.0 / W;
	deltaCol = pixmapX * 1.0 / W;
	row		 = pixmapY / W;
	col		 = pixmapX / W;
	deltaRow -= row;	// We just want an error in terms of grid squares
	deltaCol -= col;
	const bool bInGrid = ( row >= 0 && row < rows ) && ( col >= 0 && col < cols);
	row = std::max(0, std::min(rows-1, row));
	col = std::max(0, std::min(cols-1, col));
	return bInGrid;
}

void MainWindow::wheelEvent(QWheelEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
	m_mousePos = QPoint(static_cast<int>(event->position().x()), static_cast<int>(event->position().y()));
#else
	m_mousePos = QPoint(static_cast<int>(event->posF().x()), static_cast<int>(event->posF().y()));
#endif

	if ( GetShiftKeyDown() ) return;	// Ignore wheel events while trying to group components
	const bool bBack = ( event->angleDelta().y() < 0 );
	if ( GetCtrlKeyDown() )
	{
		if ( bBack ) ZoomOut(); else ZoomIn();
	}
	else if ( !m_board.GetMirrored() )
	{
		if ( m_board.GetCompEdit() )
			DefinerIncPinNumber(!bBack);
		else
			CompStretch(!bBack);
	}
	event->accept();	// If we don't do this, we can get the same event passed multiple times if we're on MS Windows.
}

bool MainWindow::CanModifyRuler() const
{
	return m_bRuler && !( m_board.GetCompEdit() || GetSmartPan() || GetShiftKeyDown() || GetResizingText() ||
						  GetDefiningRect() || GetPaintPins() || GetErasePins() || GetPaintBoard() || GetEraseBoard() || GetPaintFlood() );
}

void MainWindow::mousePressEvent(QMouseEvent* event)
{
	g_bPinClicked = false;

	g_bHaveAutoPanned = false;	// Reset flags for avoiding repeated re-draws

	if ( m_wireDlg->isVisible() )	HideWireDialog();
	if ( m_bomDlg->isVisible() )	HideBomDialog();
	if ( m_findDlg->isVisible() )
		HideFindDialog();
	else
		ClearFind();	// Clear the set of found components

	m_mousePos = event->pos();
	if ( m_board.GetMirrored() ) return;

	const TRACKMODE&	trackMode	= m_board.GetTrackMode();
	const COMPSMODE&	compMode	= m_board.GetCompMode();
	CompDefiner&		compDefiner	= m_board.GetCompDefiner();
	const int&			layer		= m_board.GetCurrentLayer();

	// Get row col
	double dRow(0), dCol(0);	// Fractional correction to row, col for SwapDiagLinks() call

	bool bInGrid(false);
	if ( m_board.GetCompEdit() )
		bInGrid = GetRowCol(event->pos(), compDefiner.GetScreenRows(), compDefiner.GetScreenCols(), m_gridRow, m_gridCol, dRow, dCol);
	else
		bInGrid = GetRowCol(event->pos(), m_gridRow, m_gridCol, dRow, dCol);

	if ( !bInGrid)
	{
		if ( m_dockPinDlg->isVisible() ) m_dockPinDlg->hide();
		return HidePadOffsetDialog();
	}

	m_bMouseClick	= true;		// Set the flag meaning "click begin"
#ifdef VEROROUTE_ANDROID
	m_bLeftClick	= m_bRightClick = true;	// Making the point that there is no difference between left/right clicks in the Android version of the app
#else
	m_bLeftClick	= ( event->button() & Qt::LeftButton );
	m_bRightClick	= ( event->button() & Qt::RightButton );
#endif

	if ( m_board.GetCompEdit() )
	{
		HidePadOffsetDialog();
		// Pin/Shape selection
		const int pinId		= compDefiner.GetPinId(m_gridRow, m_gridCol);
		const int shapeId	= compDefiner.GetShapeId(m_gridRow + dRow - 0.5, m_gridCol + dCol - 0.5);
		if ( GetCurrentPinId() != pinId )
		{
			SetCurrentPinId(pinId);
			//if ( pinId == BAD_ID )
			//	mouseActionString = ( pinId == BAD_ID ) ? "Unselect footprint grid point" : "Select footprint grid point";
		}
		if ( GetCurrentShapeId() != shapeId )
		{
			SetCurrentShapeId(shapeId);
			//if ( shapeId == BAD_ID )
			//	mouseActionString = ( shapeId == BAD_ID ) ? "Unselect shape" : "Select shape";
		}
		UpdateCompDialog();
		return RepaintSkipRouting();
	}

	// Cursor modification
	if ( GetSmartPan() )
		centralWidget()->setCursor(Qt::ClosedHandCursor);
	else if ( GetPaintPins() || GetErasePins() || GetPaintBoard() || GetEraseBoard() || GetPaintFlood() )
		centralWidget()->setCursor(Qt::CrossCursor);
	else if ( GetResizingText() )
		centralWidget()->setCursor(Qt::SizeFDiagCursor);
	else if ( GetCurrentTextId() != BAD_TEXTID || GetCurrentCompId() != BAD_COMPID )
		centralWidget()->setCursor(Qt::ClosedHandCursor);
	else
		centralWidget()->setCursor(Qt::OpenHandCursor);

	if ( CanModifyRuler() )
	{
		HidePadOffsetDialog();
		const QPoint current(m_gridCol, m_gridRow);
		if      ( current == m_rulerA ) m_bModifyRulerA = false;	// Do nothing, but prefer end B next time
		else if ( current == m_rulerB ) m_bModifyRulerA = true;		// Do nothing, but prefer end A next time
		else
		{
			if ( m_bModifyRulerA )	m_rulerA = current;
			else					m_rulerB = current;
			m_bModifyRulerA = !m_bModifyRulerA;
		}
	}

	if ( GetDefiningRect() )
	{
		if ( m_dockPinDlg->isVisible() ) m_dockPinDlg->hide();
		HidePadOffsetDialog();
#ifdef VEROROUTE_ANDROID
		if ( m_bMouseClick )
#else
		if ( m_bLeftClick )	// Only define rectangles using left-click
#endif
		{
			centralWidget()->setCursor(Qt::SizeFDiagCursor);
			m_board.GetRectMgr().StartNewRect(m_gridRow, m_gridCol);
			SelectAllInRects();
			UpdateHistory("Select parts in area(s)");
#ifdef VEROROUTE_ANDROID
			g_lastDrawRect = std::chrono::steady_clock::now();
#endif
			ShowCurrentRectSize();
		}
#ifndef VEROROUTE_ANDROID
		else				// Right/middle click quits rectangle mode
		{
			SetDefiningRect( false );
			UpdateControls();
		}
#endif
		return RepaintSkipRouting();
	}

	if ( !GetSmartPan() && !GetShiftKeyDown() )
		m_board.GetRectMgr().Clear();

	SetResizingText(false);

	if ( !GetSmartPan() )	// If not grabbing the board ...
	{
		// Text box selection
		if ( m_board.GetShowText() && m_board.GetTrackMode() != TRACKMODE::PCB )
		{
			const int textId = m_board.GetTextId(m_gridRow, m_gridCol);
			if ( GetCurrentTextId() != textId )	// If we're changing textId
			{
				// If we've clicked away from an empty text box, then delete it
				if ( GetCurrentTextId() != BAD_TEXTID && StringHelper::IsEmptyStr( GetCurrentTextRect().GetStr() ) )
					m_board.GetTextMgr().DestroyRect( GetCurrentTextId() );

				if ( textId != BAD_TEXTID )
					m_mouseActionString = "Select text box";

				SetCurrentTextId(textId);
			}
			if ( GetCurrentTextId() != BAD_TEXTID )
			{
				// If we're on the bottom-right corner of the text box, then resize it rather than move it
				const TextRect& rect = GetCurrentTextRect();
				if ( rect.m_rowMax == m_gridRow && rect.m_colMax == m_gridCol && dRow >= 0.5 && dCol >= 0.5 )
					SetResizingText(true);
			}
		}
		if ( compMode != COMPSMODE::OFF )
		{
			// Component selection (if text is not selected)
			const int compId = ( GetCurrentTextId() == BAD_TEXTID ) ? m_board.GetComponentId(m_gridRow, m_gridCol) : BAD_COMPID;
			if ( GetCurrentCompId() != compId ) SetCurrentCompId(compId);

			// Group manipulation
			GroupManager& groupMgr = m_board.GetGroupMgr();
			if ( GetShiftKeyDown() )
			{
				groupMgr.UpdateUserGroup( GetCurrentCompId() );	// Add/remove current comp (and its siblings) to user group
				UpdateControls();
				if ( GetCurrentTextId() == BAD_TEXTID )
					m_mouseActionString = "(Un)select part(s)";
			}
			else if ( !groupMgr.GetIsUserComp( GetCurrentCompId() ) )
			{
				CompManager& compMgr = m_board.GetCompMgr();
				groupMgr.ResetUserGroup( GetCurrentCompId() );	// Reset the user group with the current comp (and its siblings)
				compMgr.ClearTrax();
				UpdateControls();
				if ( GetCurrentTextId() == BAD_TEXTID )
					m_mouseActionString = "(Un)select part(s)";
			}
		}
	}

	// Painting/Unpainting the component pins or board
	if ( GetSmartPan() || GetShiftKeyDown() || trackMode == TRACKMODE::OFF ) return;

	if ( GetCurrentTextId() != BAD_TEXTID )
	{
		if ( m_dockPinDlg->isVisible() ) m_dockPinDlg->hide();
		HidePadOffsetDialog();
		return RepaintWithRouting();	// Don't modify nodeId or paint if editing text
	}

	const Element* pC = m_board.Get(layer, m_gridRow, m_gridCol);

	if ( GetPaintFlood() )
	{
		if ( m_dockPinDlg->isVisible() ) m_dockPinDlg->hide();
		HidePadOffsetDialog();
#ifdef VEROROUTE_ANDROID
		if ( m_bMouseClick )
#else
		if ( m_bLeftClick )	// Only define rectangles using left-click
#endif
		{
			if ( GetCurrentNodeId() == BAD_NODEID )			// If trying to left-click paint a BAD_NODEID ...
				SetCurrentNodeId( m_board.GetNewNodeId() );	// ... use a new NodeId instead

			m_board.GetColorMgr().ReAssignColors();	// Forces colors to be worked out again

			assert( GetCurrentNodeId() != BAD_NODEID );
			assert( !m_board.GetRoutingEnabled() );	// Sanity check

			const int tmp = GetCurrentNodeId();	// Need to temporarily change current nodeId for HandleRouting()
			SetCurrentNodeId( pC->GetNodeId() );
			HandleRouting();		// Work out MH distances for the flood
			SetCurrentNodeId(tmp);	// Restore current nodeId

			m_board.FloodNodeId( GetCurrentNodeId() );
			m_mouseActionString = "Paint (flood)";
		}
	}
	else if ( GetPaintPins() || GetErasePins() || GetPaintBoard() || GetEraseBoard() )
	{
		if ( m_dockPinDlg->isVisible() ) m_dockPinDlg->hide();
		HidePadOffsetDialog();
#ifdef VEROROUTE_ANDROID
		const bool bClickedValidNodeID = pC->GetNodeId() != BAD_NODEID;
		const bool bTruePin = pC->GetHasPin() && !pC->GetHasWire();
		if ( bClickedValidNodeID && GetPaintBoard() && bTruePin )	// If we're painting board and clicked on a true pin with a valid nodeID
		{
			SetCurrentNodeId( pC->GetNodeId() );	// ... then change current nodeID to that of the pin
			m_mouseActionString = "Select Net";
		}
		else if ( bClickedValidNodeID && GetPaintBoard() && pC->GetNodeId() == GetCurrentNodeId() )	// If we're painting board and clicked on a point with matching valid nodeID
		{
			const bool bChanged = m_board.SetNodeIdByUser(layer, m_gridRow, m_gridCol, BAD_NODEID, false);	// ... then erase the point instead of painting it
			if ( !bChanged ) return;
			m_mouseActionString = "Erase";
		}
		/* A bit too easy to mess up with this block
		else if ( bClickedValidNodeID && GetPaintPins() && bTruePin && pC->GetNodeId() == GetCurrentNodeId() )	// If we're painting pins and clicked on a true pin with matching valid nodeID
		{
			const bool bChanged = m_board.SetNodeIdByUser(layer, m_gridRow, m_gridCol, BAD_NODEID, true);	// .. then erase the pin instead of painting it
			if ( !bChanged ) return;
			m_mouseActionString = "Erase";
		}*/
		else if ( ( GetPaintPins() || GetErasePins() ) && !bTruePin )	// Restrict painting/erasing pins to true pins (not wires)
		{
			return;
		}
		else
#endif

#ifdef VEROROUTE_ANDROID
		if ( GetPaintPins() || GetPaintBoard() )	// Paint
#else
		if ( m_bLeftClick )	// Paint
#endif
		{
			if ( GetCurrentNodeId() == BAD_NODEID )			// If trying to left-click paint a BAD_NODEID ...
				SetCurrentNodeId( m_board.GetNewNodeId() );	// ... use a new NodeId instead

			m_board.GetColorMgr().ReAssignColors();	// Forces colors to be worked out again

			const bool bChanged = m_board.SetNodeIdByUser(layer, m_gridRow, m_gridCol, GetCurrentNodeId(), GetPaintPins() || GetErasePins());
			if ( !bChanged ) return;
			m_mouseActionString = "Paint";
		}

#ifdef VEROROUTE_ANDROID
		else if ( GetErasePins() || GetEraseBoard() )	// Erase
#else
		else if ( m_bRightClick )	// Erase
#endif
		{
			const bool bChanged = m_board.SetNodeIdByUser(layer, m_gridRow, m_gridCol, BAD_NODEID, GetPaintPins() || GetErasePins());
			if ( !bChanged ) return;
			m_mouseActionString = "Erase";
		}
	}
	else	// Set/Unset current nodeId from board
	{
#ifdef VEROROUTE_ANDROID
		SetCurrentNodeId( pC->GetNodeId() );
#else
		if ( m_bLeftClick )
			SetCurrentNodeId( pC->GetNodeId() );
		if ( m_bRightClick )
			SetCurrentNodeId( BAD_NODEID );
#endif
		m_mouseActionString = ( GetCurrentNodeId() == BAD_NODEID) ? "Unselect Net" : "Select Net";

		if ( ALLOW_DELAY_BASED_PAD_SHIFT && pC->GetHasPin() && !pC->GetHasWire() )
			g_bPinClicked = true;

		// Pin labels editos is only useful if we have selected a single component with pin labels
		bool bPinLabels(false);
		if ( m_board.GetGroupMgr().GetNumUserComps() == 1 )
		{
			const Component& comp = m_board.GetCompMgr().GetComponentById( GetCurrentCompId() );
			bPinLabels = ( comp.GetPinFlags() & PIN_LABELS );
		}
		if ( !bPinLabels && m_dockPinDlg->isVisible() )
			HideDlg(m_dockPinDlg);

		if ( !pC->GetHasPin() || pC->GetHasWire() )	// Hide the pad offset dialog if we click on a place that cannot have a pad offset
			HidePadOffsetDialog();
	}

	g_lastMouseClickTime = std::chrono::steady_clock::now();

	m_board.WipeAutoSetPoints();
	m_board.PlaceFloaters();	// See if we can now place floating components down
	RepaintWithRouting();
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
	g_bPinClicked = false;

	m_mousePos = event->pos();
	if ( m_board.GetMirrored() ) return;
	if ( m_board.GetCompEdit() ) return;

	if ( GetSmartPan() || GetShiftKeyDown() ) return;

#ifdef VEROROUTE_ANDROID
	//TODO Could allow this in Desktop version too
	if ( GetDefiningRect() )
	{
		SetDefiningRect(false);	// Quit define rectangles mode
		UpdateControls();
		return;
	}
#endif

	if ( GetCurrentTextId() != BAD_TEXTID )
		return ShowTextDialog();

	if ( m_board.GetTrackMode() == TRACKMODE::OFF ) return;

	// Get row col
	double dRow(0), dCol(0);	// Fractional correction to row, col for SwapDiagLinks() call
	const bool bInGrid = GetRowCol(event->pos(), m_gridRow, m_gridCol, dRow, dCol);
	if ( !bInGrid ) return;

	// Cursor modification
	centralWidget()->setCursor(Qt::CrossCursor);

	// Handle changing layer preference for PCBs via double-clicking on a component pin
	if ( m_board.GetTrackMode() == TRACKMODE::PCB && m_board.GetLyrs() == 2 && !GetPaintBoard() )
	{
		if ( hypot(dRow - 0.5, dCol - 0.5) < 0.25 )	// Only consider clicks that are close to the grid point
		{
			if ( m_board.ToggleLyrPref(m_board.GetCurrentLayer(), m_gridRow, m_gridCol) )
			{
				UpdateHistory("Change pin layer preference");
				return;
			}
		}
	}

	// Handle competing diagonals
	if ( !GetPaintAction() && m_board.GetTrackMode() != TRACKMODE::OFF )	//TODO Making !GetPaintAction() explicit
	{
		const int	dR = ( dRow > 0.5 ) ? 1 : 0;	// Correct row, col to account for crossing ...
		const int	dC = ( dCol > 0.5 ) ? 1 : 0;	// ... point being near corner of element
		const int&	layer	 = m_board.GetCurrentLayer();

		const bool	bSwapped = m_board.Get(layer, m_gridRow + dR, m_gridCol + dC)->SwapDiagLinks();
		if ( bSwapped )
		{
			m_board.WipeAutoSetPoints();
			m_board.PlaceFloaters();	// See if we can now place floating components down
			UpdateHistory("Toggle competing diagonals");

			// mouseReleaseEvent() will do RepaintWithRouting() and ListNodes().  No need to do it here.
			return;
		}
	}

#ifdef VEROROUTE_ANDROID
	// Component rotation
	//TODO Could allow this in Desktop version too if we change how toggle diagonals works (e.g. require space bar to be pressed ?)
	if ( !GetPaintAction() && m_board.GetCompMode() != COMPSMODE::OFF && m_eMouseMode == MOUSE_MODE::SELECT && GetCurrentCompId() != BAD_COMPID )
		return CompRotateCW();

	// Handle leaving PaintBoard mode via double-clicking on a component pin
	//TODO See is this get confused with swapping diagonals
	if ( GetPaintBoard() && GetCurrentNodeId() != BAD_NODEID )
	{
		if ( hypot(dRow - 0.5, dCol - 0.5) < 0.25 )	// Only consider clicks that are close to the grid point
		{
			const Element* pC = m_board.Get(m_board.GetCurrentLayer(), m_gridRow, m_gridCol);
			if ( pC->GetHasPin() && !pC->GetHasWire() && pC->GetNodeId() == GetCurrentNodeId() )
			{
				SetCurrentNodeId(BAD_NODEID);
				SetPaintBoard(false);
				return;
			}
		}
	}
#endif
}

void MainWindow::mouseMoveEvent(QMouseEvent* event)
{
	g_bPinClicked = false;

	m_mousePos = event->pos();
	if ( m_board.GetMirrored() ) return;
	if ( !m_bMouseClick ) return;
	const TRACKMODE&	trackMode	= m_board.GetTrackMode();
	const COMPSMODE&	compMode	= m_board.GetCompMode();
	CompDefiner&		compDefiner	= m_board.GetCompDefiner();
	const int&			W			= m_board.GetGRIDPIXELS();
	const int&			layer		= m_board.GetCurrentLayer();

	if ( GetPaintPins() || GetErasePins() || GetPaintFlood() ) return;	// Ignore mouse move while painting pins or flooding
	if ( GetShiftKeyDown() ) return;									// Ignore mouse move while trying to group components

	if ( ALLOW_DELAY_BASED_SMART_PAN
		 && !m_board.GetCompEdit() && !GetDefiningRect() && !GetPaintBoard() && !GetEraseBoard() && !CanModifyRuler()
		 && GetCurrentTextId() == BAD_TEXTID && GetCurrentCompId() == BAD_COMPID )
	{
		const auto elapsed		= std::chrono::steady_clock::now() - g_lastMouseClickTime;
		const auto duration_ms	= std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
		if ( duration_ms > 500 )	// ... Force smart pan after 500 ms of last mouse click
			SetSmartPan(true);
	}

	if ( m_board.GetCompEdit() && GetCurrentShapeId() != BAD_ID && m_bMouseClick )
		centralWidget()->setCursor(Qt::ClosedHandCursor);
	else if ( GetSmartPan() )
		centralWidget()->setCursor(Qt::ClosedHandCursor);
	else if ( GetDefiningRect() || GetResizingText() )
		centralWidget()->setCursor(Qt::SizeFDiagCursor);
	else if ( GetPaintBoard() || GetEraseBoard() )
		centralWidget()->setCursor(Qt::CrossCursor);
	else if ( GetCurrentTextId() != BAD_TEXTID && m_bMouseClick )
		centralWidget()->setCursor(Qt::ClosedHandCursor);
	else if ( GetCurrentCompId() != BAD_COMPID && m_bMouseClick )
		centralWidget()->setCursor(Qt::ClosedHandCursor);
	else
		centralWidget()->setCursor(Qt::OpenHandCursor);

	// Get row col
	int row(0), col(0);
	double dRow(0), dCol(0);	// Fractional correction to row, col for SwapDiagLinks() call
	bool bInGrid(false);
	if ( m_board.GetCompEdit() )
		bInGrid = GetRowCol(event->pos(), compDefiner.GetScreenRows(), compDefiner.GetScreenCols(), row, col, dRow, dCol);
	else
		bInGrid = GetRowCol(event->pos(), row, col, dRow, dCol);

	int deltaRow = row - m_gridRow;
	int deltaCol = col - m_gridCol;

	int oldRow(m_gridRow), oldCol(m_gridCol);
	m_gridRow = row;
	m_gridCol = col;

	if ( m_board.GetCompEdit() )
	{
		if ( GetCurrentShapeId() != BAD_ID )
		{
			int pixmapX(0), pixmapY(0);
			GetPixMapXY(event->pos(), pixmapX, pixmapY);

			if ( deltaRow == 0 ) { if ( pixmapY > W * compDefiner.GetScreenRows() ) deltaRow++; }
			if ( deltaCol == 0 ) { if ( pixmapX > W * compDefiner.GetScreenCols() ) deltaCol++; }
			if ( deltaRow == 0 ) { if ( pixmapY < 0 ) deltaRow--; }
			if ( deltaCol == 0 ) { if ( pixmapX < 0 ) deltaCol--; }
			if ( deltaRow == 0 && deltaCol == 0 ) return;	// No change

			compDefiner.SetCurrentPinId(BAD_ID);				// Clear pin selection
			compDefiner.MoveCurrentShape(deltaRow, deltaCol);	// Move the shape
			UpdateCompDialog();
		}
	}
	else if ( !GetSmartPan() && GetDefiningRect() )
	{
		if ( deltaRow != 0 || deltaCol != 0 ) 	// No change of row or column ==> No change in current rect size
		{
			m_board.GetRectMgr().UpdateNewRect(m_gridRow, m_gridCol);
			SelectAllInRects();
		}
	}
	else if ( !GetSmartPan() && ( GetPaintBoard() || GetEraseBoard() ) && trackMode != TRACKMODE::OFF )	// (Un)Paint nodeId on board but NOT pins
	{
		if ( !bInGrid ) return;

		assert( !GetPaintPins() && !GetErasePins() && !GetPaintFlood() );	// Sanity check
#ifdef VEROROUTE_ANDROID
		if ( GetPaintBoard() )	// Paint
#else
		if ( m_bLeftClick )		// Paint
#endif
		{
			const bool bChanged = m_board.SetNodeIdByUser(layer, m_gridRow, m_gridCol, GetCurrentNodeId(), false);	// false ==> Only allow paint board (not pins)
			if ( !bChanged ) return;	// No change
			m_mouseActionString = "Paint";
		}

#ifdef VEROROUTE_ANDROID
		if ( GetEraseBoard() )	// Erase
#else
		if ( m_bRightClick )	// Erase
#endif
		{
			const bool bChanged = m_board.SetNodeIdByUser(layer, m_gridRow, m_gridCol, BAD_NODEID, false);	// false ==> Only allow erase board (not pins)
			if ( !bChanged ) return;	// No change
			m_mouseActionString = "Erase";
		}
		m_board.WipeAutoSetPoints();
		m_board.PlaceFloaters();	// See if we can now place floating components down
	}
	else if ( !GetSmartPan() && GetCurrentTextId() != BAD_TEXTID )
	{
		int pixmapX(0), pixmapY(0);
		GetPixMapXY(event->pos(), pixmapX, pixmapY);

		if ( deltaRow == 0 ) { if ( pixmapY > W * m_board.GetRows() ) deltaRow++; }
		if ( deltaCol == 0 ) { if ( pixmapX > W * m_board.GetCols() ) deltaCol++; }
		if ( deltaRow == 0 ) { if ( pixmapY < 0 ) deltaRow--; }
		if ( deltaCol == 0 ) { if ( pixmapX < 0 ) deltaCol--; }
		if ( deltaRow == 0 && deltaCol == 0 ) return;	// No change

		if ( g_bHaveAutoPanned )	// If we've auto-panned the grid before ...
		{
			const auto elapsed		= std::chrono::steady_clock::now() - g_lastAutoPanTime;
			const auto duration_ms	= std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
			if ( duration_ms >= 0 && duration_ms < 40 ) return;	// ... do nothing if within 40 ms of the last auto-pan
		}

		if ( GetResizingText() )
		{
			m_board.GetTextMgr().UpdateRect( GetCurrentTextId(), m_gridRow, m_gridCol );
		}
		else
		{
			const bool bAutoPanned = m_board.MoveTextBox(deltaRow, deltaCol);	// Move the text box and note if the grid was auto-panned
			if ( bAutoPanned )
			{
				g_bHaveAutoPanned = true;
				g_lastAutoPanTime = std::chrono::steady_clock::now();
			}
		}
		m_mouseActionString = ( GetResizingText() ) ? "Resize text box" : "Move text box";
	}
	else if ( !GetSmartPan() && GetCurrentCompId() != BAD_COMPID && compMode != COMPSMODE::OFF )	// Move user-group components
	{
		int pixmapX(0), pixmapY(0);
		GetPixMapXY(event->pos(), pixmapX, pixmapY);

		if ( deltaRow == 0 ) { if ( pixmapY > W * m_board.GetRows() ) deltaRow++; }
		if ( deltaCol == 0 ) { if ( pixmapX > W * m_board.GetCols() ) deltaCol++; }
		if ( deltaRow == 0 ) { if ( pixmapY < 0 ) deltaRow--; }
		if ( deltaCol == 0 ) { if ( pixmapX < 0 ) deltaCol--; }
		if ( deltaRow == 0 && deltaCol == 0 ) return;	// No change

		if ( g_bHaveAutoPanned )	// If we've auto-panned the grid before ...
		{
			const auto elapsed		= std::chrono::steady_clock::now() - g_lastAutoPanTime;
			const auto duration_ms	= std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
			if ( duration_ms >= 0 && duration_ms < 40 ) return;	// ... do nothing if within 40 ms of the last auto-pan
		}
		const bool bAutoPanned = m_board.MoveUserComps(deltaRow, deltaCol);	// Move the components and note if the grid was auto-panned
		if ( bAutoPanned )
		{
			g_bHaveAutoPanned = true;
			g_lastAutoPanTime = std::chrono::steady_clock::now();
		}
		const bool bPlural = ( m_board.GetGroupMgr().GetNumUserComps() > 1 );
		m_mouseActionString = ( bPlural ? "Move parts" : "Move part" );
	}
	else if ( GetSmartPan() )	// If we're not moving anything else, we can smart pan
	{
		int pixmapX(0), pixmapY(0);
		GetPixMapXY(event->pos(), pixmapX, pixmapY);

		if ( deltaRow == 0 ) { if ( pixmapY > W * m_board.GetRows() ) deltaRow = 1; }
		if ( deltaCol == 0 ) { if ( pixmapX > W * m_board.GetCols() ) deltaCol = 1; }
		if ( deltaRow == 0 ) { if ( pixmapY < W ) deltaRow = -1; }
		if ( deltaCol == 0 ) { if ( pixmapX < W ) deltaCol = -1; }
		if ( deltaRow == 0 && deltaCol == 0 ) return;	// No change
		m_board.SmartPan(deltaRow, deltaCol);	// Pan whole circuit w.r.t. grid area, growing/shrinking as needed
		m_mouseActionString = "Move whole layout";
	}

	if ( CanModifyRuler() )
	{
		const QPoint old(oldCol, oldRow);
		const QPoint current(m_gridCol, m_gridRow);
		bool bModifyA = ( old == m_rulerA );
		bool bModifyB = !bModifyA && ( old == m_rulerB );
		if ( !(bModifyA || bModifyB) )
			bModifyA = PolygonHelper::Length(current - m_rulerA) < PolygonHelper::Length(current - m_rulerB);	// Choose nearest
		if ( bModifyA )	m_rulerA = current;
		else			m_rulerB = current;
		m_bModifyRulerA = !bModifyA;
	}

	if ( abs(deltaRow) <= 1 && abs(deltaCol) <= 1 )	// If not moved mouse too fast ...
	{
		if ( GetDefiningRect() )
		{
			if ( deltaRow != 0 || deltaCol != 0 )	// No change of row or column ==> No change in current rect size
			{
				ShowCurrentRectSize();
#ifdef VEROROUTE_ANDROID
				const auto elapsed		= std::chrono::steady_clock::now() - g_lastDrawRect;
				const auto duration_ms	= std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
				if ( duration_ms >= 0 && duration_ms < 200 ) return;	// ... do nothing if within 40 ms of the last draw
				g_lastDrawRect = std::chrono::steady_clock::now();
#endif
				RepaintSkipRouting();
			}
		}
		else
		{
			if ( GetResizingText() || m_board.GetCompEdit() )
				RepaintSkipRouting();
			else
				RepaintWithRouting();
		}
	}
}

void MainWindow::mouseReleaseEvent(QMouseEvent* event)
{
	bool bShowPadOffsetDialog(false);
	if ( g_bPinClicked && !m_board.GetVeroTracks() && !m_bRuler )
	{
		const auto elapsed		= std::chrono::steady_clock::now() - g_lastMouseClickTime;
		const auto duration_ms	= std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
		bShowPadOffsetDialog = ( duration_ms >= 1000 );
	}
	g_bPinClicked = false;

	m_mousePos = event->pos();
	releaseMouse();

	if ( GetSmartPan() )
		SetSmartPan(false);

	if ( m_board.GetMirrored() ) return;

	m_bMouseClick = false;

	if ( m_board.GetCompEdit() )
	{
		UpdateHistory(m_mouseActionString);
		return RepaintSkipRouting();
	}
	if ( GetResizingText() )
	{
		centralWidget()->setCursor(Qt::OpenHandCursor);
		SetResizingText(false);
		UpdateHistory("Resize text box");
		return RepaintSkipRouting();
	}
	if ( GetDefiningRect() )
	{
		centralWidget()->setCursor(Qt::SizeFDiagCursor);
		m_board.GetRectMgr().EndNewRect();
		SelectAllInRects();
		ShowCurrentRectSize();
		return RepaintSkipRouting();
	}
	if ( GetPaintPins() || GetErasePins() || GetPaintBoard() || GetEraseBoard() || GetPaintFlood() )
		centralWidget()->setCursor(Qt::CrossCursor);
	else
		centralWidget()->setCursor(Qt::OpenHandCursor);

	UpdateHistory(m_mouseActionString);
	UpdateControls();
	RepaintWithListNodes();

	if ( bShowPadOffsetDialog )
		ShowPadOffsetDialog();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	g_bPinClicked = false;

	commonKeyPressEvent(event);

	// Pad offsets (Ctrl + Cursor keys)	// Deprecated this as it does not write to history, and we really only want to do that at the end when Ctrl key is released
	/*
	if ( GetCtrlKeyDown() && !m_board.GetVeroTracks() && !m_board.GetCompEdit() )
	{
		const Element* pC =  m_board.Get(0, m_gridRow, m_gridCol);
		if ( pC->GetHasPin() && !pC->GetHasWire() )	// Wires can share holes so cannot have offset pads
		{
			Component&		comp		= m_board.GetCompMgr().GetComponentById( pC->GetCompId() );
			const size_t	pinIndex	= pC->GetPinIndex();
			int dx(0), dy(0);
			switch( event->key() )
			{
				case Qt::Key_Left:	 dx--; break;
				case Qt::Key_Right:	 dx++; break;
				case Qt::Key_Up:	 dy--; break;
				case Qt::Key_Down:	 dy++; break;
			}
			comp.IncCompPinOffsets(pinIndex, dx, dy);

			int X,Y;
			comp.GetCompPinOffsets(pinIndex, X, Y);	// Get offsets in mil

			char buffer[256] = {'\0'};
			sprintf(buffer,"(X, Y) pad offset = (%d, %d) mil,    (%.4f, %.4f) mm", X, Y, X * 0.0254, Y * 0.0254);
			ui->statusBar->showMessage(QString(buffer), 1000);

			return RepaintSkipRouting();
		}
	}
	*/
	if ( GetCtrlKeyDown() ) return;		// Try to keep Ctrl key input handled by menu items
	if ( GetShiftKeyDown() ) return;	// Ignore other key presses while trying to group components

	if ( m_board.GetMirrored() ) return;

	const TRACKMODE&	trackMode		= m_board.GetTrackMode();
	const COMPSMODE&	compMode		= m_board.GetCompMode();
	CompDefiner&		compDefiner		= m_board.GetCompDefiner();
	GroupManager&		groupMgr		= m_board.GetGroupMgr();
	const int			nComps			= groupMgr.GetNumUserComps();
	const bool			bIsAutoRepeat	= event->isAutoRepeat();

	if ( m_board.GetCompEdit() )
	{
		if ( GetCurrentShapeId() != BAD_ID )
		{
			switch( event->key() )
			{
				case Qt::Key_Left:	compDefiner.MoveCurrentShape( 0, -1); UpdateCompDialog(); RepaintSkipRouting(); break;
				case Qt::Key_Right:	compDefiner.MoveCurrentShape( 0,  1); UpdateCompDialog(); RepaintSkipRouting(); break;
				case Qt::Key_Up:	compDefiner.MoveCurrentShape(-1,  0); UpdateCompDialog(); RepaintSkipRouting(); break;
				case Qt::Key_Down:	compDefiner.MoveCurrentShape( 1,  0); UpdateCompDialog(); RepaintSkipRouting(); break;
			}
			if ( !bIsAutoRepeat )
			{
				switch( event->key() )
				{
					case Qt::Key_Backspace:
					case Qt::Key_Delete:	Delete();	break;
				}
			}
		}
		event->accept();
		return;
	}

	bool bUpdateControls(false);

	// Rectangle mode
	if ( !bIsAutoRepeat )
	{
		switch( event->key() )
		{
			case Qt::Key_R:	SetDefiningRect(true);	bUpdateControls = true; break;
		}
	}

	const bool bPlural = ( m_board.GetGroupMgr().GetNumUserComps() > 1 );
	// Component manipulation
	switch( event->key() )
	{
		case Qt::Key_Underscore:
		case Qt::Key_Minus:		CompShrink();	break;
		case Qt::Key_Plus:
		case Qt::Key_Equal:		CompGrow();		break;
		case Qt::Key_Left:		if ( !m_board.GetDisableMove() ) { m_board.MoveUserComps(0,-1);	UpdateHistory(bPlural ? "Move parts left"	: "Move part left");  RepaintWithRouting(); } break;
		case Qt::Key_Right:		if ( !m_board.GetDisableMove() ) { m_board.MoveUserComps(0, 1);	UpdateHistory(bPlural ? "Move parts right"	: "Move part right"); RepaintWithRouting(); } break;
		case Qt::Key_Up:		if ( !m_board.GetDisableMove() ) { m_board.MoveUserComps(-1,0);	UpdateHistory(bPlural ? "Move parts up"		: "Move part up");    RepaintWithRouting(); } break;
		case Qt::Key_Down:		if ( !m_board.GetDisableMove() ) { m_board.MoveUserComps( 1,0);	UpdateHistory(bPlural ? "Move parts down"	: "Move part down");  RepaintWithRouting(); } break;
	}
	if ( !bIsAutoRepeat )
	{
		switch( event->key() )
		{
			case Qt::Key_Z:			CompRotateCCW();	break;
			case Qt::Key_X:			CompRotateCW();		break;
			case Qt::Key_Delete:	if ( GetCurrentTextId() != BAD_TEXTID || ( nComps && compMode != COMPSMODE::OFF ) )
										Delete();	//	So delete works like the backspace keyboard shortcut
									break;
		}
	}
	// Painting/Unpainting
	if ( !bIsAutoRepeat )
	{
		// Only one flag for paint-board/paint-pins/paint-flood must be true
		switch( event->key() )
		{
			case Qt::Key_P:		if ( trackMode == TRACKMODE::OFF || compMode == COMPSMODE::OFF || GetPaintBoard() || GetEraseBoard() || GetPaintFlood() ) return;
								SetPaintPins(true);		break;
			case Qt::Key_Space:	if ( trackMode == TRACKMODE::OFF || GetPaintPins() || GetErasePins() || GetPaintFlood() ) return;
								SetPaintBoard(true);	break;
			case Qt::Key_F:		if ( trackMode == TRACKMODE::OFF || compMode == COMPSMODE::OFF || GetPaintBoard() || GetEraseBoard() || GetPaintPins() || GetErasePins() ) return;
								if ( m_board.GetRoutingEnabled() ) return;
								SetPaintFlood(true);	break;
			case Qt::Key_W:		WipeTracks();	break;
		}
	}

	if ( !GetDefiningRect() )
		m_board.GetRectMgr().Clear();

	if ( bUpdateControls )
		UpdateControls();
#endif
	event->accept();	// If we don't do this, we can get the same event passed multiple times if we're on MS Windows.
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	g_bPinClicked = false;

	commonKeyReleaseEvent(event);

	if ( GetSmartPan() )
		SetSmartPan(false);

	if ( m_board.GetMirrored() ) return;
	if ( event->isAutoRepeat() ) return;

	if ( m_board.GetCompEdit() )
		UpdateCompDialog();
	else
	{
		switch( event->key() )
		{
			case Qt::Key_R:		SetDefiningRect(false);	break;
			case Qt::Key_P:		SetPaintPins(false);	break;
			case Qt::Key_F:		SetPaintFlood(false);	break;
			case Qt::Key_Space:	SetPaintBoard(false);	break;
			default:
				if ( GetCurrentTextId() != BAD_TEXTID && m_bMouseClick )
					centralWidget()->setCursor(Qt::ClosedHandCursor);
				else if ( GetCurrentCompId() != BAD_COMPID && m_bMouseClick )
					centralWidget()->setCursor(Qt::ClosedHandCursor);
				else
					centralWidget()->setCursor(Qt::OpenHandCursor);
		}
		UpdateControls();
		RepaintWithListNodes();
	}
#endif
	event->accept();	// If we don't do this, we can get the same event passed multiple times if we're on MS Windows.
}

#ifndef VEROROUTE_ANDROID
void MainWindow::commonKeyPressEvent(QKeyEvent* event)		// So child dialogs can relay Ctrl and Shift to the main window
{
	g_bPinClicked = false;
	switch( event->key() )
	{
		case Qt::Key_Shift:		return SetShiftKeyDown(true);
		case Qt::Key_Control:	if ( m_bMouseClick ) centralWidget()->setCursor(Qt::ClosedHandCursor);
								return SetCtrlKeyDown(true);
	}
}

void MainWindow::commonKeyReleaseEvent(QKeyEvent* event)	// So child dialogs can relay Ctrl and Shift to the main window
{
	g_bPinClicked = false;
	switch( event->key() )
	{
		case Qt::Key_Shift:		return SetShiftKeyDown(false);
		case Qt::Key_Control:	return SetCtrlKeyDown(false);
	}
}

void MainWindow::specialKeyPressEvent(QKeyEvent* event)		// So child dialogs can do Ctrl+Q etc
{
	commonKeyPressEvent(event);
	if ( !GetCtrlKeyDown() ) return;
	switch( event->key() )
	{
		case Qt::Key_N:	return New();
		case Qt::Key_O:	return Open();
		case Qt::Key_M:	return Merge();
		case Qt::Key_S:	return GetShiftKeyDown() ? SaveAs() : Save();
		case Qt::Key_Q:	return Quit();
	}
}
#endif

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
	if ( e->mimeData()->hasUrls() )
		e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e)
{
	if ( e->mimeData()->hasUrls() )
	{
		QString fileName = e->mimeData()->urls().first().toLocalFile();
		if ( fileName.isEmpty() ) return;

		SetCtrlKeyDown(false);	// Clear flag since key release can get missed.
		if ( GetIsModified() )
			if ( QMessageBox::question(this, tr("Confirm Open"),
											 tr("Your circuit is not saved. You will lose changes if you open a new one.  Continue?"),
											 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No ) return;
		OpenVrt(fileName, false);
	}
}

bool MainWindow::eventFilter(QObject* object, QEvent* event)
{
	// Intercept the Android "back" button event
	if ( event->type() == QEvent::KeyRelease && qobject_cast<QMenu*>(object) )
	{
		QKeyEvent* ke = (QKeyEvent*)event;
		if ( ke->key() == Qt::Key_Back )
		{
			ke->accept();
			return true;
		}
	}
	return QWidget::eventFilter(object, event);
}

void MainWindow::SetPaintPins(bool b)
{
	if ( b == GetPaintPins() ) return;
	if ( b ) HideAllNonDockedDlgs();
	m_eMouseMode = ( b ) ? MOUSE_MODE::PAINT_PINS : MOUSE_MODE::SELECT;
	centralWidget()->setCursor(b ? Qt::CrossCursor : Qt::OpenHandCursor);
	UpdateControls();
}
void MainWindow::SetErasePins(bool b)
{
	if ( b == GetErasePins() ) return;
	if ( b ) HideAllNonDockedDlgs();
	m_eMouseMode = ( b ) ? MOUSE_MODE::ERASE_PINS : MOUSE_MODE::SELECT;
	centralWidget()->setCursor(b ? Qt::CrossCursor : Qt::OpenHandCursor);
	UpdateControls();
}
void MainWindow::SetPaintBoard(bool b)
{
	if ( b == GetPaintBoard() ) return;
	if ( b ) HideAllNonDockedDlgs();
	m_eMouseMode = ( b ) ? MOUSE_MODE::PAINT_GRID : MOUSE_MODE::SELECT;;
	centralWidget()->setCursor(b ? Qt::CrossCursor : Qt::OpenHandCursor);
	UpdateControls();
}
void MainWindow::SetEraseBoard(bool b)
{
	if ( b == GetEraseBoard() ) return;
	if ( b ) HideAllNonDockedDlgs();
	m_eMouseMode = ( b ) ? MOUSE_MODE::ERASE_GRID : MOUSE_MODE::SELECT;;
	centralWidget()->setCursor(b ? Qt::CrossCursor : Qt::OpenHandCursor);
	UpdateControls();
}
void MainWindow::SetPaintFlood(bool b)
{
	if ( b == GetPaintFlood() ) return;
	if ( b ) HideAllNonDockedDlgs();
	m_eMouseMode = ( b ) ? MOUSE_MODE::PAINT_FLOOD : MOUSE_MODE::SELECT;
	centralWidget()->setCursor(b ? Qt::CrossCursor : Qt::OpenHandCursor);
	UpdateControls();
}
void MainWindow::SetDefiningRect(bool b)
{
	if ( b == GetDefiningRect() ) return;
	if ( b ) HideAllNonDockedDlgs();
	m_eMouseMode = ( b ) ? MOUSE_MODE::DEFINE_RECT : MOUSE_MODE::SELECT;
	centralWidget()->setCursor(b ? Qt::SizeFDiagCursor : Qt::OpenHandCursor);
	UpdateControls();
}
void MainWindow::SetResizingText(bool b)
{
	if ( b == GetResizingText() ) return;
	if ( b ) { m_wireDlg->hide();	m_bomDlg->hide();	m_findDlg->hide(); }	// Mutually exclusive with 	m_textDlg
	m_eMouseMode = ( b ) ? MOUSE_MODE::RESIZE_TEXT : MOUSE_MODE::SELECT;
	UpdateControls();
}
void MainWindow::SetSmartPan(bool b)
{
	if ( b == GetSmartPan() ) return;
	if ( b ) HideAllNonDockedDlgs();
	m_eMouseMode = ( b ) ? MOUSE_MODE::SMART_PAN : MOUSE_MODE::SELECT;
	centralWidget()->setCursor(Qt::OpenHandCursor);
	UpdateControls();
}
