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

Q_DECL_CONSTEXPR static const bool ALLOW_DELAY_BASED_SMART_PAN = true;
Q_DECL_CONSTEXPR static const bool ALLOW_DELAY_BASED_PAD_SHIFT = true;

// Following 2 are to slow down the auto-panning while moving components with the mouse
static std::chrono::steady_clock::time_point g_lastAutoPanTime;
static bool g_bHaveAutoPanned = false;

#ifdef VEROROUTE_ANDROID
#define AVOID_RECT_REDRAWS
#endif

#ifdef AVOID_RECT_REDRAWS
// Following is to avoid too many redraws while defining rectangles
static std::chrono::steady_clock::time_point g_lastDrawRect;
#endif

static std::chrono::steady_clock::time_point g_lastMouseClickTime;	// For implementation of ALLOW_DELAY_BASED_SMART_PAN / ALLOW_DELAY_BASED_PAD_SHIFT
static bool g_bPinClicked = false;									// For implementation of ALLOW_DELAY_BASED_PAD_SHIFT

void MainWindow::GetPixMapXY(const QPoint& currentPoint, int& pixmapX, int& pixmapY) const
{
	pixmapX = currentPoint.x() + m_scrollArea->horizontalScrollBar()->value();
	pixmapY = currentPoint.y() + m_scrollArea->verticalScrollBar()->value();

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

bool MainWindow::HaveZeroDeltaRowCol(int& deltaRow, int& deltaCol)
{
	// Modify deltaRow/deltaCol to account for out-of-grid locations
	int pixmapX(0), pixmapY(0);
	GetPixMapXY(m_mousePos, pixmapX, pixmapY);

	const int&	W	 = m_board.GetGRIDPIXELS();
	const int	rows = ( m_board.GetCompEdit() ) ? m_board.GetCompDefiner().GetScreenRows() : m_board.GetRows();
	const int	cols = ( m_board.GetCompEdit() ) ? m_board.GetCompDefiner().GetScreenCols() : m_board.GetCols();

	if ( deltaRow == 0 ) { if ( pixmapY > W * rows ) deltaRow++; }
	if ( deltaCol == 0 ) { if ( pixmapX > W * cols ) deltaCol++; }
	if ( deltaRow == 0 ) { if ( pixmapY < 0 ) deltaRow--; }
	if ( deltaCol == 0 ) { if ( pixmapX < 0 ) deltaCol--; }
	return deltaRow == 0 && deltaCol == 0;
}

void MainWindow::wheelEvent(QWheelEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
	m_mousePos = event->position().toPoint();
#else
	m_mousePos = event->posF().toPoint();
#endif

	if ( GetShiftKeyDown() ) return;	// Ignore wheel events while trying to group components
	const bool bBack = ( event->angleDelta().y() < 0 );
	if ( GetCtrlKeyDown() )
	{
		if ( bBack ) ZoomOut(); else ZoomIn();
	}
	else if ( m_board.GetCompEdit() )
		DefinerIncPinNumber(!bBack);
	else if ( !m_board.GetMirrored() )
		CompStretch(!bBack);
	event->accept();	// If we don't do this, we can get the same event passed multiple times if we're on MS Windows.
}

bool MainWindow::CanModifyRuler() const
{
	return m_bRuler && !( m_board.GetCompEdit() || GetSmartPan() || GetShiftKeyDown() || GetResizingText() ||
						  GetDefiningRect() || GetPaintPins() || GetErasePins() || GetPaintBoard() || GetEraseBoard() || GetPaintFlood() );
}

bool MainWindow::GetHaveFloatingPinTH(int& iFloatingNodeId)	// Checks for a floating through-hole pin
{
	iFloatingNodeId = BAD_NODEID;

	const Element* pC = m_board.Get(m_board.GetCurrentLayer(), m_gridRow, m_gridCol);
	if ( pC->GetHasPin() ) return false;	// Skip if we already have a placed pin

	for (auto& mapObj : m_board.GetCompMgr().GetMapIdToComp())
	{
		const Component& comp = mapObj.second;
		if ( comp.GetIsPlaced() ) continue;	// Only want floating components
		const auto& eType = comp.GetType();
		if ( eType == COMP::WIRE || eType == COMP::MARK || eType == COMP::VERO_NUMBER || eType == COMP::VERO_LETTER ) continue;
		if ( comp.GetIsSOIC() ) continue;	// SOIC parts are not through-hole

		const int j(m_gridRow - comp.GetRow());		if ( j < 0 || j >= comp.GetCompRows() ) continue;
		const int i(m_gridCol - comp.GetCol());		if ( i < 0 || i >= comp.GetCompCols() ) continue;

		const CompElement* p = comp.GetCompElement(j, i);
		if ( !p->GetIsPin() ) continue;

		iFloatingNodeId = comp.GetNodeId( p->GetPinIndex() );
		return true;
	}
	return false;
}

void MainWindow::MousePressEvent(const QPoint& pos, bool bLeftClick, bool bRightClick)
{
	SetMouseActionString("");

	m_bReRoute = m_bReListNodes = false;	// Reset both flags

	g_bPinClicked = false;

	g_bHaveAutoPanned = false;	// Reset flags for avoiding repeated re-draws

	if ( !m_findDlg->isVisible() ) ClearFind();	// Clear the set of found components

	m_mousePos = m_clickedPos = pos;
	if ( !m_board.GetCompEdit() && m_board.GetMirrored() ) return;

	const TRACKMODE&	trackMode	= m_board.GetTrackMode();
	const COMPSMODE&	compMode	= m_board.GetCompMode();
	CompDefiner&		compDefiner	= m_board.GetCompDefiner();
	const int&			layer		= m_board.GetCurrentLayer();

	// Get row col
	double dRow(0), dCol(0);	// Fractional correction to row, col for SwapDiagLinks() call

	bool bInGrid(false);
	if ( m_board.GetCompEdit() )
		bInGrid = GetRowCol(m_mousePos, compDefiner.GetScreenRows(), compDefiner.GetScreenCols(), m_gridRow, m_gridCol, dRow, dCol);
	else
		bInGrid = GetRowCol(m_mousePos, m_gridRow, m_gridCol, dRow, dCol);
	if ( !bInGrid)
		return HidePadOffsetDialog();

	m_bMouseClick	= true;		// Set the flag meaning "click begin"
	m_bLeftClick	= bLeftClick;
	m_bRightClick	= bRightClick;
#ifdef VEROROUTE_ANDROID
	m_bLeftClick	= m_bRightClick = true;	// Making the point that there is no difference between left/right clicks in the Android version of the app
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
			SetMouseActionString(( pinId == BAD_ID ) ? "unselect point in footprint" : "select point in footprint");
		}
		if ( GetCurrentShapeId() != shapeId )
		{
			SetCurrentShapeId(shapeId);
			SetMouseActionString(( shapeId == BAD_ID ) ? "unselect shape" : "select shape");
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
			SetMouseActionString("select parts / tracks by area");
#ifdef AVOID_RECT_REDRAWS
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
					SetMouseActionString("select text box");

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
					SetMouseActionString("(un)select part(s)");
			}
			else if ( !groupMgr.GetIsUserComp( GetCurrentCompId() ) )
			{
				CompManager& compMgr = m_board.GetCompMgr();
				groupMgr.ResetUserGroup( GetCurrentCompId() );	// Reset the user group with the current comp (and its siblings)
				compMgr.ClearTrax();
				UpdateControls();
				if ( GetCurrentTextId() == BAD_TEXTID )
					SetMouseActionString("(un)select part(s)");
			}
		}
	}

	// Painting/Unpainting the component pins or board
	if ( GetSmartPan() || GetShiftKeyDown() || trackMode == TRACKMODE::OFF ) return;

	if ( GetCurrentTextId() != BAD_TEXTID )
	{
		if ( m_dockPinDlg->isVisible() ) m_dockPinDlg->hide();
		HidePadOffsetDialog();
		return RepaintSkipRouting();	// Don't modify nodeId or paint if editing text
	}

	const Element* pC = m_board.Get(layer, m_gridRow, m_gridCol);

	const bool bColor = ( trackMode == TRACKMODE::COLOR );
	if ( GetPaintFlood() && bColor )
	{
		if ( m_dockPinDlg->isVisible() ) m_dockPinDlg->hide();
		HidePadOffsetDialog();
#ifdef VEROROUTE_ANDROID
		if ( m_bMouseClick )
#else
		if ( m_bLeftClick )
#endif
		{
			if ( GetCurrentNodeId() == BAD_NODEID )			// If trying to left-click paint a BAD_NODEID ...
				SetCurrentNodeId( m_board.GetNewNodeId() );	// ... use a new NodeId instead

			m_board.GetColorMgr().ReAssignColors();	// Forces colors to be worked out again

			assert( GetCurrentNodeId() != BAD_NODEID );
			assert( !m_board.GetRoutingEnabled() );	// Sanity check

			const int tmp = GetCurrentNodeId();	// Need to temporarily change current nodeId for HandleRouting()
			SetCurrentNodeId( pC->GetNodeId() );
			HandleRouting(true);	// Work out all MH distances for the flood
			SetCurrentNodeId(tmp);	// Restore current nodeId

			m_board.FloodNodeId( GetCurrentNodeId() );
			SetMouseActionString("paint (flood)", GetCurrentNodeId());
			m_bReRoute = m_bReListNodes = true;
		}
	}
	else if ( ( (GetPaintPins() || GetErasePins()) && bColor ) || GetPaintBoard() || GetEraseBoard() )
	{
		if ( m_dockPinDlg->isVisible() ) m_dockPinDlg->hide();
		HidePadOffsetDialog();

		// Handle competing diagonals first
		bool bDoSwap(false);
		if ( GetPaintBoard() && m_board.GetTrackMode() != TRACKMODE::OFF )
		{
			const bool bCloseToCrossPoint = ( hypot(std::min(dRow, 1.0 - dRow), std::min(dCol, 1.0 - dCol)) <= 0.5 );	// true ==> clicked close to crossing diagonals point
			if ( bCloseToCrossPoint )	// Only consider clicks close to crossing diagonals point
			{
				const int	dR = ( dRow > 0.5 ) ? 1 : 0;	// Correct row, col to account for crossing ...
				const int	dC = ( dCol > 0.5 ) ? 1 : 0;	// ... point being near corner of element
				const int&	layer	 = m_board.GetCurrentLayer();

				Element* pRB = m_board.Get(layer, m_gridRow + dR, m_gridCol + dC);
				if ( pRB->CanSwapDiagLinks() )
				{
					Element* pLB = pRB->GetNbr(NBR_L);
					bDoSwap	= ( pRB->GetNodeId() == GetCurrentNodeId() && !ReadCodeBit(NBR_LT, pRB->GetCode()) )	// pRB has correct nodeID but no diagonal to LT
						   || ( pLB->GetNodeId() == GetCurrentNodeId() && !ReadCodeBit(NBR_RT, pLB->GetCode()) );	// pLB has correct nodeID but no diagonal to RT
					if ( bDoSwap )
					{
						pRB->SwapDiagLinks();
						SetMouseActionString("swap competing diagonals");
						m_bReRoute = m_bReListNodes = true;
					}
				}
			}
		}

		const bool bTruePin = pC->GetHasPin() && !pC->GetHasWire();
		if ( ( GetPaintPins() || GetErasePins() ) && !bTruePin && !bDoSwap )	// Restrict painting/erasing pins to true pins (not wires)
			return;

#ifdef VEROROUTE_ANDROID
		//TODO Could allow this in Desktop version too
		const bool bClickedValidNodeID = pC->GetNodeId() != BAD_NODEID;
		if ( GetPaintBoard() && bClickedValidNodeID && pC->GetNodeId() == GetCurrentNodeId() && pC->ReadFlagBits(USERSET) && !bDoSwap )	// If we're painting board and clicked on a point with matching valid nodeID
		{
			const bool bChanged = m_board.SetNodeIdByUser(layer, m_gridRow, m_gridCol, BAD_NODEID, false);	// ... then erase the point instead of painting it
			if ( !bChanged ) return;
			SetMouseActionString("erase", GetCurrentNodeId());
			m_bReRoute = m_bReListNodes = true;
		}
		else
#endif

#ifdef VEROROUTE_ANDROID
		if ( ( GetPaintPins() || GetPaintBoard() ) && !bDoSwap )	// Paint
#else
		if ( m_bLeftClick && !bDoSwap )	// Paint
#endif
		{
			if ( GetCurrentNodeId() == BAD_NODEID )			// If trying to left-click paint a BAD_NODEID ...
				SetCurrentNodeId( m_board.GetNewNodeId() );	// ... use a new NodeId instead

			m_board.GetColorMgr().ReAssignColors();	// Forces colors to be worked out again

			const bool bChanged = m_board.SetNodeIdByUser(layer, m_gridRow, m_gridCol, GetCurrentNodeId(), GetPaintPins() || GetErasePins());
			if ( !bChanged ) return;
			SetMouseActionString("paint", GetCurrentNodeId());
			m_bReRoute = m_bReListNodes = true;
		}

#ifdef VEROROUTE_ANDROID
		else if ( ( GetErasePins() || GetEraseBoard() ) && !bDoSwap )	// Erase
#else
		else if ( m_bRightClick && !bDoSwap )	// Erase
#endif
		{
			// Erase both layers if it makes it easier to let a floating part fall into place.
			int iFloatingNodeId(BAD_NODEID);
			const bool bEraseBothLayers = m_board.GetLyrs() > 1 &&					// If 2-layer board ...
										  !GetPaintPins() && !GetErasePins() &&		// ... and not erasing a pin
										  GetHaveFloatingPinTH(iFloatingNodeId);	// ... then see if we have a floating through-hole pin and gets its nodeId

			bool bChanged = m_board.SetNodeIdByUser(layer, m_gridRow, m_gridCol, BAD_NODEID, GetPaintPins() || GetErasePins());
			if ( bEraseBothLayers )
			{
				const int	layerOther	= ( layer == 0 ) ? 1 : 0;
				const int&	otherNodeId	= m_board.Get(layerOther, m_gridRow, m_gridCol)->GetNodeId();
				// Only erase if the other layer has a valid nodeId that does not match the floating nodeId
				if ( otherNodeId != BAD_NODEID && otherNodeId != iFloatingNodeId )
					bChanged = m_board.SetNodeIdByUser(layerOther, m_gridRow, m_gridCol, BAD_NODEID, GetPaintPins() || GetErasePins()) || bChanged;
			}
			if ( !bChanged ) return;
			SetMouseActionString("erase", GetCurrentNodeId());
			m_bReRoute = m_bReListNodes = true;
		}
	}
	else
	{
		if ( ALLOW_DELAY_BASED_PAD_SHIFT && pC->GetHasPin() && !pC->GetHasWire() )
			g_bPinClicked = true;

		if ( !pC->GetHasPin() || pC->GetHasWire() )	// Hide the pad offset dialog if we click on a place that cannot have a pad offset
			HidePadOffsetDialog();
	}

	g_lastMouseClickTime = std::chrono::steady_clock::now();

	if ( m_bReRoute )
	{
		m_board.WipeAutoSetPoints();
		m_board.PlaceFloaters();	// See if we can now place floating components down
		RepaintWithRouting();
	}
	else
		RepaintSkipRouting();
}

void MainWindow::MouseDoubleClickEvent(const QPoint& pos)
{
	SetMouseActionString("");

	g_bPinClicked = false;

	m_mousePos = pos;
	if ( !m_board.GetCompEdit() && m_board.GetMirrored() ) return;
	if ( m_board.GetCompEdit() ) return;

	if ( GetSmartPan() || GetShiftKeyDown() ) return;

	if ( GetDefiningRect() )
	{
		SetDefiningRect(false);	// Quit define rectangles mode
		UpdateControls();
		return;
	}

	if ( GetCurrentTextId() != BAD_TEXTID )
		return ShowTextDialog();

	const bool bCompsOn	= m_board.GetCompMode()  != COMPSMODE::OFF;
	const bool bTrackOn	= m_board.GetTrackMode() != TRACKMODE::OFF;
	const bool bPCB		= m_board.GetTrackMode() == TRACKMODE::PCB;

	if ( !bTrackOn && !bCompsOn ) return;

	const bool bSameLocation = ( PolygonHelper::Length(m_mousePos - m_clickedPos) ) < m_board.GetGRIDPIXELS();	// Tolerance of one grid square
	if ( !bSameLocation ) return;

	// Get row col
	double dRow(0), dCol(0);	// Fractional correction to row, col for SwapDiagLinks() call
	const bool bInGrid			= GetRowCol(m_mousePos, m_gridRow, m_gridCol, dRow, dCol);
	if ( !bInGrid ) return;

	const int&	layer		= m_board.GetCurrentLayer();
	Element*	pC			= m_board.Get(layer, m_gridRow, m_gridCol);
	const bool	bWire		= pC->GetHasWire();
	const bool	bTruePin	= pC->GetHasPin() && !bWire;

	// Cursor modification
	centralWidget()->setCursor(Qt::CrossCursor);

	const bool bCloseToGridPoint = ( hypot(dRow - 0.5, dCol - 0.5) <= 0.5 );	// true ==> clicked close to grid point

	// Handle changing layer preference for PCBs via double-clicking on a component pin
	if ( bPCB && bTruePin && !GetPaintAction() && m_board.GetLyrs() == 2 && bCloseToGridPoint )	// Only consider clicks that are close to the grid point
	{
		const bool bToggled = m_board.ToggleLyrPref(layer, m_gridRow, m_gridCol);	assert(bToggled);
		if ( bToggled )
		{
			SetMouseActionString("change pin layer preference", pC->GetCompId());

			// Also handle change of nodeID ...
			if ( GetCurrentNodeId() != pC->GetNodeId() )
			{
				SetCurrentNodeId( pC->GetNodeId() );
				m_bReRoute = true;	// Dont' need to set m_bReListNodes when choosing different nodeID
			}
			return;
		}
	}

	// Handle competing diagonals
	if ( bTrackOn && !GetPaintAction() )
	{
		const bool bCloseToCrossPoint = ( hypot(std::min(dRow, 1.0 - dRow), std::min(dCol, 1.0 - dCol)) <= 0.5 );	// true ==> clicked close to crossing diagonals point
		if ( bCloseToCrossPoint )	// Only consider clicks close to crossing diagonals point
		{
			const int	dR	= ( dRow > 0.5 ) ? 1 : 0;	// Correct row, col to account for crossing ...
			const int	dC	= ( dCol > 0.5 ) ? 1 : 0;	// ... point being near corner of element
			Element*	pRB	= m_board.Get(layer, m_gridRow + dR, m_gridCol + dC);
			if ( pRB->SwapDiagLinks() )
			{
				SetMouseActionString("swap competing diagonals");
				m_bReRoute = m_bReListNodes = true;
				m_board.WipeAutoSetPoints();
				m_board.PlaceFloaters();	// See if we can now place floating components down
				return;
			}
		}
	}

	// Handle selection of nodeId when double-clicking on a pin (when tracks are displayed)
	if ( bTrackOn && bWire && !GetPaintBoard() && !GetEraseBoard() && !GetPaintFlood() && bCloseToGridPoint )	// Only consider clicks that are close to the grid point
	{
		if ( GetCurrentNodeId() != pC->GetNodeId() )
		{
			SetCurrentNodeId( pC->GetNodeId() );
			SetMouseActionString( ( GetCurrentNodeId() == BAD_NODEID ) ? "unselect net" : "select net", 0);
			m_bReRoute = true;	// Dont' need to set m_bReListNodes when choosing different nodeID
		}
		return;
	}
	if ( bTrackOn && bTruePin && !GetPaintPins() && !GetErasePins() && !GetPaintFlood()	&& bCloseToGridPoint )	// Only consider clicks that are close to the grid point
	{
		if ( GetCurrentNodeId() != pC->GetNodeId() )
		{
			SetCurrentNodeId( pC->GetNodeId() );
			SetMouseActionString( ( GetCurrentNodeId() == BAD_NODEID ) ? "unselect net" : "select net", 0);
			m_bReRoute = true;	// Dont' need to set m_bReListNodes when choosing different nodeID
		}
		return;
	}

	// Handle component rotation
	/*
	if ( bCompsOn && ( m_eMouseMode == MOUSE_MODE::SELECT || GetPaintPins() || GetErasePins() ) && GetCurrentCompId() != BAD_COMPID )
	{
		if ( !bTruePin )	//	... Only allow rotate if we did not click on a pin
		{
			m_bReRoute = m_bReListNodes = false;	// Set these false since CompRotateCW() calls RepaintWithListNodes() directly
			SetMouseActionString("");	// CompRotateCW() will write history
			return CompRotateCW();
		}
	}
	*/

	// Handle selection of nodeId (fallback case)
	if ( bTrackOn && !GetPaintAction() )
	{
		if ( GetCurrentNodeId() != pC->GetNodeId() )
		{
			SetCurrentNodeId( pC->GetNodeId() );
			SetMouseActionString( ( GetCurrentNodeId() == BAD_NODEID ) ? "unselect net" : "select net", 0);
			m_bReRoute = true;	// Dont' need to set m_bReListNodes when choosing different nodeID
		}
	}
}

void MainWindow::MouseMoveEvent(const QPoint& pos)
{
	m_mousePos = pos;
	if ( !m_board.GetCompEdit() && m_board.GetMirrored() ) return;
	if ( !m_bMouseClick ) return;
	const TRACKMODE&	trackMode	= m_board.GetTrackMode();
	const COMPSMODE&	compMode	= m_board.GetCompMode();
	CompDefiner&		compDefiner	= m_board.GetCompDefiner();
	const int&			layer		= m_board.GetCurrentLayer();

	if ( GetPaintFlood() ) return;		// Ignore mouse move while flooding
	if ( GetShiftKeyDown() ) return;	// Ignore mouse move while trying to group components

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
		bInGrid = GetRowCol(m_mousePos, compDefiner.GetScreenRows(), compDefiner.GetScreenCols(), row, col, dRow, dCol);
	else
		bInGrid = GetRowCol(m_mousePos, row, col, dRow, dCol);

	int deltaRow = row - m_gridRow;
	int deltaCol = col - m_gridCol;

	int oldRow(m_gridRow), oldCol(m_gridCol);
	m_gridRow = row;
	m_gridCol = col;

	// g_bPinClicked needs to be set false in a limited set of cases.
	// If we don't do anything with the move operation, then we can preserve g_bPinClicked
	if ( m_board.GetCompEdit() || GetPaintAction() || GetDefiningRect() || GetResizingText() || CanModifyRuler() )
		g_bPinClicked = false;

	if ( m_board.GetCompEdit() )
	{
		if ( GetCurrentShapeId() != BAD_ID )
		{
			if ( HaveZeroDeltaRowCol(deltaRow, deltaCol) ) return;	// No change

			compDefiner.SetCurrentPinId(BAD_ID);				// Clear pin selection
			compDefiner.MoveCurrentShape(deltaRow, deltaCol);	// Move the shape
			SetMouseActionString("move shape", GetCurrentShapeId());
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

		// Handle competing diagonals first
		bool bDoSwap(false);
		const bool bCloseToCrossPoint = ( hypot(std::min(dRow, 1.0 - dRow), std::min(dCol, 1.0 - dCol)) <= 0.5 );	// true ==> clicked close to crossing diagonals point
		if ( bCloseToCrossPoint )	// Only consider clicks close to crossing diagonals point
		{
			const int	dR = ( dRow > 0.5 ) ? 1 : 0;	// Correct row, col to account for crossing ...
			const int	dC = ( dCol > 0.5 ) ? 1 : 0;	// ... point being near corner of element
			const int&	layer	 = m_board.GetCurrentLayer();

			Element* pRB = m_board.Get(layer, m_gridRow + dR, m_gridCol + dC);
			if ( pRB->CanSwapDiagLinks() )
			{
				Element* pLB = pRB->GetNbr(NBR_L);
				bDoSwap	= ( pRB->GetNodeId() == GetCurrentNodeId() && !ReadCodeBit(NBR_LT, pRB->GetCode()) )	// pRB has correct nodeID but no diagonal to LT
					   || ( pLB->GetNodeId() == GetCurrentNodeId() && !ReadCodeBit(NBR_RT, pLB->GetCode()) );	// pLB has correct nodeID but no diagonal to RT
				if ( bDoSwap )
				{
					pRB->SwapDiagLinks();
					SetMouseActionString("swap competing diagonals");
					m_bReRoute = m_bReListNodes = true;
				}
			}
		}

		assert( !GetPaintPins() && !GetErasePins() && !GetPaintFlood() );	// Sanity check
#ifdef VEROROUTE_ANDROID
		if ( GetPaintBoard() && !bDoSwap )	// Paint
#else
		if ( m_bLeftClick && !bDoSwap )		// Paint
#endif
		{
			const bool bChanged = m_board.SetNodeIdByUser(layer, m_gridRow, m_gridCol, GetCurrentNodeId(), false);	// false ==> Only allow paint board (not pins)
			if ( !bChanged ) return;	// No change
			SetMouseActionString("paint", GetCurrentNodeId());
			m_bReRoute = m_bReListNodes = true;
		}

#ifdef VEROROUTE_ANDROID
		if ( GetEraseBoard() && !bDoSwap )	// Erase
#else
		if ( m_bRightClick && !bDoSwap )	// Erase
#endif
		{
			// Erase both layers if it makes it easier to let a floating part fall into place.
			int iFloatingNodeId(BAD_NODEID);
			const bool bEraseBothLayers = m_board.GetLyrs() > 1 &&					// If 2-layer board ...
										  !GetPaintPins() && !GetErasePins() &&		// ... and not erasing a pin
										  GetHaveFloatingPinTH(iFloatingNodeId);	// ... then see if we have a floating through-hole pin and gets its nodeId

			bool bChanged = m_board.SetNodeIdByUser(layer, m_gridRow, m_gridCol, BAD_NODEID, false);	// false ==> Only allow erase board (not pins)
			if ( bEraseBothLayers )
			{
				const int	layerOther	= ( layer == 0 ) ? 1 : 0;
				const int&	otherNodeId	= m_board.Get(layerOther, m_gridRow, m_gridCol)->GetNodeId();
				// Only erase if the other layer has a valid nodeId that does not match the floating nodeId
				if ( otherNodeId != BAD_NODEID && otherNodeId != iFloatingNodeId )
					bChanged = m_board.SetNodeIdByUser(layerOther, m_gridRow, m_gridCol, BAD_NODEID, false) || bChanged;	// false ==> Only allow erase board (not pins)
			}
			if ( !bChanged ) return;	// No change
			SetMouseActionString("erase", GetCurrentNodeId());
			m_bReRoute = m_bReListNodes = true;
		}
		m_board.WipeAutoSetPoints();
		m_board.PlaceFloaters();	// See if we can now place floating components down
	}
	else if ( !GetSmartPan() && GetCurrentTextId() != BAD_TEXTID )
	{
		if ( HaveZeroDeltaRowCol(deltaRow, deltaCol) ) return;	// No change

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
				m_bReRoute = m_bReListNodes = true;
			}
		}
		SetMouseActionString(GetResizingText() ? "resize text box" : "move text box", GetCurrentTextId());
	}
	else if ( !GetSmartPan() && GetCurrentCompId() != BAD_COMPID && compMode != COMPSMODE::OFF )	// Move user-group components
	{
		if ( HaveZeroDeltaRowCol(deltaRow, deltaCol) ) return;	// No change

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
		SetMouseActionString(bPlural ? "move parts" : "move part", bPlural ? -1 : GetCurrentCompId());
		m_bReRoute = m_bReListNodes = true;
	}
	else if ( GetSmartPan() )	// If we're not moving anything else, we can smart pan
	{
		if ( HaveZeroDeltaRowCol(deltaRow, deltaCol) ) return;	// No change

		m_board.SmartPan(deltaRow, deltaCol);	// Pan whole circuit w.r.t. grid area, growing/shrinking as needed
		SetMouseActionString("move whole layout", 0);
		m_bReRoute = m_bReListNodes = true;
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
#ifdef AVOID_RECT_REDRAWS
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
			if ( m_bReRoute )
				RepaintWithRouting();
			else
				RepaintSkipRouting();

			m_bReRoute = false;
		}
	}

	if ( abs(deltaRow) > 0 || abs(deltaCol) > 0 )
		g_bPinClicked = false;
}

void MainWindow::MouseReleaseEvent(const QPoint& pos)
{
	bool bShowPadOffsetDialog(false);
	if ( g_bPinClicked && !m_board.GetVeroTracks() && !m_bRuler )
	{
		const auto elapsed		= std::chrono::steady_clock::now() - g_lastMouseClickTime;
		const auto duration_ms	= std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
		bShowPadOffsetDialog = ( duration_ms >= 1000 );
	}
	g_bPinClicked = false;

	m_mousePos = pos;
	releaseMouse();

	if ( GetSmartPan() )
		SetSmartPan(false);

	if ( !m_board.GetCompEdit() && m_board.GetMirrored() ) return;

	m_bMouseClick = false;

	if ( m_board.GetCompEdit() )
	{
	}
	else if ( GetResizingText() )
	{
		centralWidget()->setCursor(Qt::OpenHandCursor);
		SetResizingText(false);
	}
	else if ( GetDefiningRect() )
	{
		centralWidget()->setCursor(Qt::SizeFDiagCursor);
		m_board.GetRectMgr().EndNewRect();
		SelectAllInRects();
		ShowCurrentRectSize();
	}
	else if ( GetPaintPins() || GetErasePins() || GetPaintBoard() || GetEraseBoard() || GetPaintFlood() )
		centralWidget()->setCursor(Qt::CrossCursor);
	else
		centralWidget()->setCursor(Qt::OpenHandCursor);

	UpdateHistory(m_mouseActionString, m_mouseObjId);
	UpdateControls();

	if ( m_bReListNodes )
		RepaintWithListNodes();
	else if ( m_bReRoute )
		RepaintWithRouting();
	else
		RepaintSkipRouting();

	if ( bShowPadOffsetDialog )
		ShowPadOffsetDialog();

	m_bReRoute = m_bReListNodes = false;	// Clear both flags
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	commonKeyPressEvent(event);

	if ( GetCtrlKeyDown() ) return;		// Try to keep Ctrl key input handled by menu items
	if ( GetShiftKeyDown() ) return;	// Ignore other key presses while trying to group components

	if ( !m_board.GetCompEdit() && m_board.GetMirrored() ) return;

	const TRACKMODE&	trackMode		= m_board.GetTrackMode();
	const COMPSMODE&	compMode		= m_board.GetCompMode();
	CompDefiner&		compDefiner		= m_board.GetCompDefiner();
	GroupManager&		groupMgr		= m_board.GetGroupMgr();
	const int			nComps			= groupMgr.GetNumUserComps();
	const bool			bIsAutoRepeat	= event->isAutoRepeat();

#ifdef _TEST_SOIC
	switch(event->key())
	{
		case Qt::Key_F4: m_iDebugMode = ( m_iDebugMode + 1 ) % DEBUGMODE_END;	break;
	}
#endif
	
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

	const bool	bSingle	= ( m_board.GetGroupMgr().GetNumUserComps() == 1 );
	const int	objID	= ( bSingle ) ? m_board.GetUserComponent().GetId() : -1;
	// Component manipulation
	switch( event->key() )
	{
		case Qt::Key_Underscore:
		case Qt::Key_Minus:		CompShrink();	break;
		case Qt::Key_Plus:
		case Qt::Key_Equal:		CompGrow();		break;
		case Qt::Key_Left:		if ( !m_board.GetDisableMove() ) { m_board.MoveUserComps(0,-1);	UpdateHistory(bSingle ? "move part" : "move parts", objID); RepaintWithRouting(); } break;
		case Qt::Key_Right:		if ( !m_board.GetDisableMove() ) { m_board.MoveUserComps(0, 1);	UpdateHistory(bSingle ? "move part" : "move parts", objID); RepaintWithRouting(); } break;
		case Qt::Key_Up:		if ( !m_board.GetDisableMove() ) { m_board.MoveUserComps(-1,0);	UpdateHistory(bSingle ? "move part" : "move parts", objID); RepaintWithRouting(); } break;
		case Qt::Key_Down:		if ( !m_board.GetDisableMove() ) { m_board.MoveUserComps( 1,0);	UpdateHistory(bSingle ? "move part" : "move parts", objID); RepaintWithRouting(); } break;
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
			case Qt::Key_P:		if ( trackMode != TRACKMODE::COLOR || compMode == COMPSMODE::OFF || GetPaintBoard() || GetEraseBoard() || GetPaintFlood() ) return;
								SetPaintPins(true);		break;
			case Qt::Key_Space:	if ( trackMode == TRACKMODE::OFF || GetPaintPins() || GetErasePins() || GetPaintFlood() ) return;
								SetPaintBoard(true);	break;
			case Qt::Key_F:		if ( trackMode != TRACKMODE::COLOR || compMode == COMPSMODE::OFF || GetPaintBoard() || GetEraseBoard() || GetPaintPins() || GetErasePins() || m_board.GetRoutingEnabled() ) return;
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
#ifdef VEROROUTE_ANDROID
	if ( event->key() == Qt::Key_Back )
	{
		if ( m_bHistoryDir && m_historyMgr.GetCanUndo() )
			QTimer::singleShot(0, this, SLOT(Undo()));
		return event->accept();
	}
#else
	commonKeyReleaseEvent(event);

	if ( GetSmartPan() )
		SetSmartPan(false);

	if ( !m_board.GetCompEdit() && m_board.GetMirrored() ) return;
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
	event->accept();	// If we don't do this, we can get the same event passed multiple times if we're on MS Windows.
#endif
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
											 tr("Your layout is not saved. You will lose changes if you open a new one.  Continue?"),
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

void MainWindow::SetMouseActionString(const std::string& str, const int objId)
{
	m_mouseActionString	= str;
	m_mouseObjId		= objId;
}
