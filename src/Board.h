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

#include "CompDefiner.h"
#include "TemplateManager.h"
#include "NodeInfoManager.h"
#include "GroupManager.h"
#include "ColorManager.h"
#include "TextManager.h"
#include "myscrollarea.h"	//TODO Not nice !! The board should not have knowledge of the rendered view

// Board is the main algorithm class.
// Just about everything apart from the GUI rendering code is in here.
// Holds a description of the circuit and handles all manipulation & routing.

#define WIRE_MH 1

class Board : public ElementGrid, public GuiControl
{
public:
	Board(int rows = 35, int cols = 35)
	: ElementGrid(rows, cols)
	, GuiControl()
	, m_infoStr("Use this box to enter a circuit description or other info")
	, m_tmpVecSize(0)
	, m_tmpMaxMH(0)
	, m_tmpNodeId(BAD_NODEID)
	, m_tmpIsRouting(false)
	{
		GlueNbrs();		// Set pointers between neighbouring grid elements
	}

	Board(const Board& o, bool bFullCopy = true)
	: ElementGrid()
	, GuiControl()
	{
		PartialCopy(o);
		if ( bFullCopy ) RebuildAdjacencies();	// Slow
	}

	Board& operator=(const Board& o)
	{
		Clear();
		PartialCopy(o);
		RebuildAdjacencies();	// Slow
		return *this;
	}

	Board& PartialCopy(const Board& o)	// Copies everything but omits RebuildAdjacencies()
	{
		m_infoStr = o.m_infoStr;

		ElementGrid::operator=(o);	// Call operator= in base class
		GuiControl::operator=(o);	// Call operator= in base class

		GlueNbrs();		// Set pointers between neighbouring grid elements

		// Need all component locations before calling GlueWires()
		m_compMgr		= o.m_compMgr;

		for (const auto& mapObj : m_compMgr.GetMapIdToComp())
		{
			const Component& comp = mapObj.second;
			m_nodeInfoMgr.AddComp(comp);
		}

		GlueWires();	// Set pointers between wired grid elements

		//m_nodeInfoMgr	= o.m_nodeInfoMgr;	// Don't copy!	This manager is populated via the above calls to AddComp()
		//m_adjInfoMgr	= o.m_adjInfoMgr;	// Don't copy!	This manager is populated via the above calls to AddComp()
		m_groupMgr		= o.m_groupMgr;
		m_rectMgr		= o.m_rectMgr;
		m_textMgr		= o.m_textMgr;
		m_colorMgr		= o.m_colorMgr;
		m_compDefiner	= o.m_compDefiner;

		// Routing algorithm variables are cleared, not copied
		m_tmpVec.clear();
		m_targetPins.clear();
		m_tmpVecSize	= 0;
		m_tmpMaxMH		= 0;
		m_tmpNodeId		= BAD_NODEID;
		m_tmpIsRouting	= false;

		return *this;
	}

	~Board()
	{
		m_infoStr.clear();
		m_tmpVec.clear();
		m_targetPins.clear();
	}

	bool operator==(const Board& o) const	// Compare persisted info
	{
		if ( m_infoStr != o.m_infoStr ) return false;
		if ( GuiControl::operator!=(o) ) return false;
		if ( ElementGrid::operator!=(o) ) return false;
		return m_compMgr		== o.m_compMgr
			&& m_groupMgr		== o.m_groupMgr
			&& m_rectMgr		== o.m_rectMgr
			&& m_textMgr		== o.m_textMgr
			&& m_compDefiner	== o.m_compDefiner;
	}
	bool operator!=(const Board& o) const
	{
		return !(*this == o);
	}
	void Clear()
	{
		Allocate(GetRows(), GetCols());
		GlueNbrs();	// Set pointers between neighbouring grid elements
		SetInfoStr("Use this box to enter a circuit description or other info");
		m_compMgr.Clear();
		m_tmpVec.clear();
		m_targetPins.clear();
		m_nodeInfoMgr.DeAllocate();
		m_adjInfoMgr.DeAllocate();
		m_groupMgr.Clear();
		m_rectMgr.Clear();
		m_textMgr.Clear();
		m_compDefiner.Clear();
		m_colorMgr.ReAssignColors();
		GuiControl::Clear();	// Clear() base class
	}

	void GlueNbrs()	// Set pointers between neighbouring grid elements
	{
		for (int iRow = 0; iRow < GetRows(); iRow++)
		{
			const int iT(iRow-1), iB(iRow+1);
			for (int iCol = 0; iCol < GetCols(); iCol++)
			{
				const int iL(iCol-1), iR(iCol+1);

				Element* p = Get(iRow, iCol);
				p->SetNbr(NBR_L,	Get(iRow, iL));
				p->SetNbr(NBR_LT,	Get(iT,   iL));
				p->SetNbr(NBR_T,	Get(iT,   iCol));
				p->SetNbr(NBR_RT,	Get(iT,   iR));
				p->SetNbr(NBR_R,	Get(iRow, iR));
				p->SetNbr(NBR_RB,	Get(iB,   iR));
				p->SetNbr(NBR_B,	Get(iB,   iCol));
				p->SetNbr(NBR_LB,	Get(iB,   iL));
				p->SetW(nullptr);	// Must be set by GlueWires()

				// Prevent toroidal routing at board edges
				int okDirs = 0xFF;	// All 8 directions OK by default
				if ( iRow == 0 )			{ ClearCodeBit(NBR_LT, okDirs); ClearCodeBit(NBR_T, okDirs); ClearCodeBit(NBR_RT, okDirs); }
				if ( iRow == GetRows()-1 )	{ ClearCodeBit(NBR_LB, okDirs); ClearCodeBit(NBR_B, okDirs); ClearCodeBit(NBR_RB, okDirs); }
				if ( iCol == 0 )			{ ClearCodeBit(NBR_LT, okDirs); ClearCodeBit(NBR_L, okDirs); ClearCodeBit(NBR_LB, okDirs); }
				if ( iCol == GetCols()-1 )	{ ClearCodeBit(NBR_RT, okDirs); ClearCodeBit(NBR_R, okDirs); ClearCodeBit(NBR_RB, okDirs); }
				p->SetRoutable(okDirs);
			}
		}
	}

	void GlueWires()	// Set pointers between wired grid elements
	{
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++)	// Loop all elements
		{
			Element* pA = GetAt(i);

			const int& compId = pA->GetCompId();
			if ( compId == BAD_COMPID ) continue;			// Skip if element doesn't have a component ID (and therefore no pin)
			const Component& comp = m_compMgr.GetComponentById(compId);
			if ( !comp.GetIsPlaced() ) continue;			// Skip floating components
			if ( comp.GetType() != COMP::WIRE ) continue;	// Skip non-wires
			if ( pA->GetPinIndex() != 0 ) continue;			// Always want "A" to be pin 0 of the wire

			// Definitely have a wire now ...
			int row(0), col(0);
			GetRowCol(pA, row, col);
			const int length = comp.GetSize() - 1;	assert(length > 0);

			switch( comp.GetDirection() )
			{
				case 'W':	col += length;	 break;
				case 'E':	col -= length;	 break;
				case 'N':	row += length;	 break;
				case 'S':	row -= length;	 break;
			}

			MakeToroid(row, col);	// Make co-ordinates wrap around at grid edges

			Element* pB = Get(row, col);	// The element for the other wire-end
			assert(pB->GetNodeId() == pA->GetNodeId());	// Wire ends must have same NodeId
			pA->SetW(pB);	// Give pA a pointer to pB
			pB->SetW(pA);	// Give pB a pointer to pA
		}
	}

	bool Pan(int iDown, int iRight)	// Pan whole circuit w.r.t. the grid. This can grow the grid but never shrink it
	{
		if ( iDown == 0 && iRight == 0 ) return false;

		int incRows(0), incCols(0);	// What we need to grow the grid by if we move too far

		int minRow, minCol, maxRow, maxCol;
		const bool bOK = GetBounds(minRow, minCol, maxRow, maxCol);	// Get the circuit bounds
		if ( !bOK )	return false;	// Can't move an empty circuit

		if ( minRow + iDown  < 0 )						// Too far up ...
		{
			incRows	= -(minRow + iDown);				// ... so grow grid from bottom
			iDown	= -minRow;							// ... and pan to very top
		}
		else if ( maxRow + iDown  >= GetRows() )		// Too far down ...
			incRows = 1 + maxRow + iDown - GetRows();	// ...so grow grid from bottom

		if ( minCol + iRight < 0 )						// Too far left ...
		{
			incCols	= -(minCol + iRight);				// ... so grow grid from right
			iRight	= -minCol;							// ... and pan to very left
		}
		else if ( maxCol + iRight >= GetCols() )		// Too far right ...
			incCols = 1 + maxCol + iRight - GetCols();	// ... so grow grid from right

		GrowThenPan(incRows, incCols, iDown, iRight);
		return true;
	}

	void SmartPan(int iDown, int iRight)	// Pan whole circuit w.r.t. the grid, and grow/shrink grid as needed
	{
		if ( iDown == 0 && iRight == 0 ) return;

		int incRows(0), incCols(0);	// What we need to grow the grid by if we pan too far

		int minRow, minCol, maxRow, maxCol;
		const bool bOK = GetBounds(minRow, minCol, maxRow, maxCol);	// Get the circuit bounds

		if ( !bOK )	// Have empty board, so there is nothing to pan. Just grow or shrink instead.
		{
			if ( iDown  < 0 && GetRows() > 1 )	incRows--;
			if ( iDown  > 0 )					incRows++;
			if ( iRight < 0 && GetCols() > 1 )	incCols--;
			if ( iRight > 0 )					incCols++;

			return GrowThenPan(incRows, incCols, 0, 0);
		}

		if ( minRow + iDown  < 0 )							// Too far up ...
		{
			iDown = -minRow;								// ... so pan to very top
			incRows = std::min(0, maxRow + 1 - GetRows());	// ... and shrink grid from bottom
		}
		else if ( maxRow + iDown  >= GetRows() )			// Too far down ...
			incRows	= 1 + maxRow + iDown - GetRows();		// ...so grow grid from bottom

		if ( minCol + iRight < 0 )							// Too far left ...
		{
			iRight	= -minCol;								// ... so pan to very left
			incCols	= std::min(0, maxCol + 1 - GetCols());	// ... and shrink grid from right
		}
		else if ( maxCol + iRight >= GetCols() )			// Too far right ...
			incCols	= 1 + maxCol + iRight - GetCols();		// ... so grow grid from right

		GrowThenPan(incRows, incCols, iDown, iRight);
	}

	bool Crop(int iRowMargin = -1, int iColMargin = -1)	// Moves whole circuit to the top-left, then crops the grid from the bottom-right, then adds margin
	{
		int minRow, minCol, maxRow, maxCol;
		const bool bOK = GetBounds(minRow, minCol, maxRow, maxCol);	// Get the circuit bounds
		if ( !bOK ) return false;
		Pan(-minRow, -minCol);	// First pan to top-left corner of grid
		GrowThenPan(maxRow - minRow + 1 - GetRows(), maxCol - minCol + 1 - GetCols(), 0, 0); // Then shrink grid from bottom-right

		if ( iRowMargin == -1 ) iRowMargin = GetCropMargin();	// -1 ==> use default
		if ( iColMargin == -1 ) iColMargin = GetCropMargin();	// -1 ==> use defaul
		if ( iRowMargin > 0 || iColMargin > 0 )
		{
			const int incRows = 2 * iRowMargin;
			const int incCols = 2 * iColMargin;
			GrowThenPan(incRows, incCols, iRowMargin, iColMargin);
		}
		return true;
	}

	void GrowThenPan(const int& incRows, const int& incCols, const int& iDown, const int& iRight)
	{
		ElementGrid::Grow(incRows, incCols);	// Grow the base class
		ElementGrid::Pan(iDown, iRight);		// Pan the base class

		GlueNbrs();		// Set pointers between neighbouring grid elements

		// Need all component locations before calling GlueWires()
		for (auto& mapObj : m_compMgr.m_mapIdToComp)
		{
			Component& comp = mapObj.second;
			int newRow = comp.GetRow() + iDown;
			int newCol = comp.GetCol() + iRight;

			MakeToroid(newRow, newCol);	// Make co-ordinates wrap around at grid edges

			comp.SetRow(newRow);
			comp.SetCol(newCol);
		}
		if ( m_compMgr.GetTrax().GetSize() > 0 )	// If have a trax pattern
		{
			Component& comp = m_compMgr.GetTrax();
			int newRow = comp.GetRow() + iDown;
			int newCol = comp.GetCol() + iRight;

			MakeToroid(newRow, newCol);	// Make co-ordinates wrap around at grid edges

			comp.SetRow(newRow);
			comp.SetCol(newCol);
		}

		GlueWires();	// Set pointers between wired grid elements

		// Move all user-defined rectangles
		GetRectMgr().MoveAll(iDown, iRight);

		// Move all ueser-defined text
		GetTextMgr().MoveAll(iDown, iRight);
	}

	bool GetBounds(int& minRow, int& minCol, int& maxRow, int& maxCol) const
	{
		bool bOK(false);
		// First consider all painted nodeIds on the board
		const int numRows( GetRows() ), numCols( GetCols() );
		minRow = numRows - 1;	maxRow = 0;	// Start with min and max at the wrong ends
		minCol = numCols - 1;	maxCol = 0;	// Start with min and max at the wrong ends
		for (int j = 0; j < numRows; j++)	for (int i = 0; i < numCols; i++)
		{
			if ( Get(j,i)->GetNodeId() == BAD_NODEID ) continue;
			minRow = std::min(minRow, j);	maxRow = std::max(maxRow, j);
			minCol = std::min(minCol, i);	maxCol = std::max(maxCol, i);
			bOK = true;
		}
		if ( !m_compMgr.GetIsEmpty() )
		{
			const Rect rect = m_compMgr.GetBounding();
			minRow = std::min(minRow, rect.m_rowMin);	maxRow = std::max(maxRow, rect.m_rowMax);
			minCol = std::min(minCol, rect.m_colMin);	maxCol = std::max(maxCol, rect.m_colMax);
			bOK = true;
		}
		const Rect rect = m_textMgr.GetBounding();
		if ( rect.GetIsValid() )
		{
			minRow = std::min(minRow, rect.m_rowMin);	maxRow = std::max(maxRow, rect.m_rowMax);
			minCol = std::min(minCol, rect.m_colMin);	maxCol = std::max(maxCol, rect.m_colMax);
			bOK = true;
		}
		return bOK;
	}

	void SetNodeId(Element* p, const int& nodeId)	// Helper to make sure we do UpdateCounts() before painting an element
	{
		m_adjInfoMgr.UpdateCounts(p, nodeId);	// Do this BEFORE we call SetNodeId() on the element
		p->SetNodeId(nodeId);					// Write node value
	}

	bool CanPutDown(Component& comp)	// Checks if its possible to place the (floating) component on the board
	{
		if ( comp.GetIsPlaced() ) return false;	// Already on board

		const int	rowTL		= comp.GetRow();
		const int	colTL		= comp.GetCol();
		const bool	bWire 		= comp.GetType() == COMP::WIRE;	// Wire's only get NodeIDs while placed
		const bool	bVia		= comp.GetType() == COMP::VIA;	// Via can go anywhere without a pin
		const bool	bTrax		= comp.GetType() == COMP::TRACKS;
		const int&	compCols	= comp.GetCompCols();
		const int&	compRows	= comp.GetCompRows();
		const int&	boardCols	= GetCols();
		const int&	boardRows	= GetRows();
		const bool	bDiagsOK	= GetDiagsMode() != DIAGSMODE::OFF;

		// Check limits
		const bool bLimitsOK = ( compRows <= boardRows && compCols <= boardCols );
		if ( !bLimitsOK ) return false;

		bool bOK(true);
		int jRow(rowTL);
		for (int j = 0; j < compRows && bOK; j++, jRow++)
		{
			int iCol(colTL);
			for (int i = 0; i < compCols && bOK; i++, iCol++)
			{
				const CompElement*	pComp = comp.GetCompElement(j, i);
				const Element*		pGrid = Get(jRow, iCol);
				if ( bTrax )
				{
					const int traxNodeId = pComp->GetNodeId();
					assert( traxNodeId == BAD_NODEID || pComp->ReadFlagBits(RECTSET) );	// Sanity check

					bOK =  ( traxNodeId == BAD_NODEID )
						|| ( traxNodeId == pGrid->GetNodeId() )
						|| ( !pGrid->GetIsPin() && pGrid->GetNodeId() == BAD_NODEID && !pGrid->GetIsHole() );

					if ( !bOK ) // Special check for unpainted wires on the board
					{
						// If have blank wire ...
						if ( pGrid->GetW() && pGrid->GetNodeId() == BAD_NODEID && !pGrid->GetIsHole() )
						{
							const CompElement* pTraxEnd(nullptr);	// The point in trax corresponding to the other wire end

							int jj(0), ii(0);	// row,col of wire end (w.r.t. board)
							GetRowCol(pGrid->GetW(), jj, ii);

							const int jEndRow(j + jj - jRow), jEndCol(i + ii - iCol);	// row, col of wire end (w.r.t. comp)

							if ( jEndRow >= 0 && jEndRow < compRows && jEndCol >= 0 && jEndCol < compCols )
								pTraxEnd = comp.GetCompElement(jEndRow, jEndCol);
							bOK = pTraxEnd == nullptr || pTraxEnd->GetNodeId() == traxNodeId || pTraxEnd->GetNodeId() == BAD_NODEID;
						}
					}
					if ( bOK && bDiagsOK )
					{
						// Check for crossing diagonals
						if ( j > 0 && i > 0 &&
							 pComp->GetUsed(NBR_LT) && pGrid->GetNbr(NBR_L)->GetUsed(NBR_RT) ) bOK = false;
						if ( j < compRows-1 && i < compCols-1 &&
							 pComp->GetUsed(NBR_RB) && pGrid->GetNbr(NBR_R)->GetUsed(NBR_LB) ) bOK = false;
						if ( j > 0 && i < compCols-1 &&
							 pComp->GetUsed(NBR_RT) && pGrid->GetNbr(NBR_R)->GetUsed(NBR_LT) ) bOK = false;
						if ( j < compRows-1 && i > 0 &&
							 pComp->GetUsed(NBR_LB) && pGrid->GetNbr(NBR_L)->GetUsed(NBR_RB) ) bOK = false;
					}
				}
				else
				{
					const uchar& boardSurface	= pGrid->GetSurface();
					const uchar& compSurface	= pComp->GetSurface();

					// Check surface
					bOK =	( boardSurface == SURFACE_FREE ) ||
							( compSurface  == SURFACE_FREE ) ||
							( boardSurface == SURFACE_GAP && compSurface  == SURFACE_PLUG ) ||
							( compSurface  == SURFACE_GAP && boardSurface == SURFACE_PLUG );
					if ( !bOK ) continue;

					// Check pins
					if ( bVia )	// Via can go anywhere except for pins, holes, (or other vias)
					{
						bOK = !pGrid->GetIsPin() && !pGrid->GetIsHole() && !pGrid->GetIsVia();
					}
					else if ( pComp->GetIsHole() )	// Check holes
					{
						// We've already checked the boardSurface is SURFACE_FREE
						// Now test it is not a via and not painted
						bOK = !pGrid->GetIsVia() && pGrid->GetNodeId() == BAD_NODEID;
					}
					else
					{
						const size_t pinIndex = pComp->GetPinIndex();
						if ( pinIndex == BAD_PININDEX ) continue;

						bOK = !pGrid->GetIsVia(); // Pin can't go on via
						if ( !bOK ) continue;

						// Check bottom grid
						const int& nodeId		= pGrid->GetNodeId();		// Read nodeID on board
						const int& iCompNodeId	= comp.GetNodeId(pinIndex);	// Read component nodeiD

						if ( bWire )	// Wires have no NodeId. Need matching IDs on both ends
							assert( iCompNodeId == BAD_NODEID );	// Shouldn't have an ID yet
						else			// Regular component ...
							bOK = ( nodeId == BAD_NODEID || nodeId == iCompNodeId );	// Need no node ID or matching ID
					}
				}
			}
		}
		if ( bOK && bWire )	// Check for short-circuit
		{
			Element* pW0 = Get(rowTL, colTL);						assert(pW0);
			Element* pW1 = Get(rowTL+compRows-1, colTL+compCols-1);	assert(pW1);
			bOK = pW0->GetNodeId() == BAD_NODEID ||
				  pW1->GetNodeId() == BAD_NODEID ||
				  pW0->GetNodeId() == pW1->GetNodeId();
		}
		return bOK;
	}

	bool PutDown(Component& comp)	// Tries to place the (floating) component on the board
	{
		const bool bOK = CanPutDown(comp);
		if ( !bOK ) return false;

		const int&	rowTL		= comp.GetRow();
		const int&	colTL		= comp.GetCol();
		const bool	bWire		= comp.GetType() == COMP::WIRE;	// Wire's only get NodeIDs while placed
		const bool	bTrax		= comp.GetType() == COMP::TRACKS;
		const int&	compCols	= comp.GetCompCols();
		const int&	compRows	= comp.GetCompRows();
		const bool	bDiagsOK	= GetDiagsMode() != DIAGSMODE::OFF;

		std::set<int> blankWireIds;	// CompIds of unpainted wires in the area covered by trax
		if ( bTrax )
		{
			int jRow(rowTL);
			for (int j = 0; j < compRows; j++, jRow++)
			{
				int iCol(colTL);
				for (int i = 0; i < compCols; i++, iCol++)
				{
					const CompElement*	pComp = comp.GetCompElement(j, i);
					Element*			pGrid = Get(jRow, iCol);
					if ( !pComp->ReadFlagBits(RECTSET) ) continue;				// Skip non-rect points
					if ( pGrid->GetW() && pGrid->GetNodeId() == BAD_NODEID )	// If have unpainted wire ...
						blankWireIds.insert( pGrid->GetCompId() );				// ... store its compId
				}
			}
		}

		int jRow(rowTL);
		for (int j = 0; j < compRows; j++, jRow++)
		{
			int iCol(colTL);
			for (int i = 0; i < compCols; i++, iCol++)
			{
				const CompElement*	pComp = comp.GetCompElement(j, i);
				Element*			pGrid = Get(jRow, iCol);
				if ( bTrax )
				{
					const int traxNodeId = pComp->GetNodeId();
					assert( traxNodeId == BAD_NODEID || pComp->ReadFlagBits(RECTSET) );	// Sanity check
					if ( traxNodeId == BAD_NODEID ) continue;	// Skip blank trax points

					const bool bBlankWire		= pGrid->GetW() && blankWireIds.find( pGrid->GetCompId() ) != blankWireIds.end();
					const bool bExistingNodeId	= !bBlankWire && ( pGrid->GetNodeId() == traxNodeId );

					SetNodeIdByUser(jRow, iCol, traxNodeId, false);	// false ==> don't paint pins

					if ( bExistingNodeId )				// If the board already had the NodeId ...
						pGrid->SetFlagBits(RECTSET);	// ... set RECTSET on the board, so TakeOff() doesn't wipe the point
					else
						pGrid->ClearFlagBits(RECTSET);

						// Fix up crossing diagonals
					if ( bDiagsOK && j > 0 && i > 0 && pComp->GetUsed(NBR_LT) != pGrid->GetUsed(NBR_LT) )
						pGrid->SwapDiagLinks();
				}
				else
				{
					assert( !(pGrid->GetIsHole() && pComp->GetIsHole()) );	// Can't overlay holes

					// Update surface
					pGrid->SetSurface( pGrid->GetSurface() + pComp->GetSurface() );

					const size_t	pinIndex		= pComp->GetPinIndex();
					const bool		bExistingPin	= pGrid->GetIsPin();
					if ( !bExistingPin ) pGrid->SetPinIndex(pinIndex);	// Don't wipe existing pins (e.g. with an IC gap)

					// Update IDs at pin location
					if ( pinIndex == BAD_PININDEX ) continue;

					pGrid->SetCompId( comp.GetId() );

					// Store any user-painted nodeId's under the pins BEFORE placing
					const int& origId = ( pGrid->ReadFlagBits(USERSET) ) ? pGrid->GetNodeId() : BAD_NODEID;
					comp.SetOrigId(pinIndex, origId);

					const int& iCompNodeId = comp.GetNodeId(pinIndex);

					assert(!bWire || iCompNodeId == BAD_NODEID); // Wire shouldn't have a NodeId yet
					if ( !bWire ) { SetNodeId(pGrid, iCompNodeId); pGrid->ClearFlagBits(AUTOSET|VEROSET); pGrid->SetFlagBits(USERSET); }	// Write nodeId & flag
				}
			}
		}
		if ( bWire )	// Handle wires setting the wire ends on the board to same value
		{
			Element* pW0 = Get(rowTL, colTL);						assert(pW0);
			Element* pW1 = Get(rowTL+compRows-1, colTL+compCols-1);	assert(pW1);

			if ( pW0->GetNodeId() == BAD_NODEID ) { SetNodeId(pW0, pW1->GetNodeId() ); pW0->SetFlagBits( pW1->GetFlag() & (USERSET|AUTOSET|VEROSET) ); }	// Write nodeId & flag
			if ( pW1->GetNodeId() == BAD_NODEID ) { SetNodeId(pW1, pW0->GetNodeId() ); pW1->SetFlagBits( pW0->GetFlag() & (USERSET|AUTOSET|VEROSET) ); }	// Write nodeId & flag

			comp.SetNodeId(0, pW0->GetNodeId());
			comp.SetNodeId(1, pW1->GetNodeId());

			assert(pW0->GetNodeId() == pW1->GetNodeId());	// Wire ends must have same NodeId

			pW0->SetW(pW1);	// Link wire ends
			pW1->SetW(pW0);	// Link wire ends
		}
		comp.SetRow(rowTL);
		comp.SetCol(colTL);
		comp.SetIsPlaced(true);
		if ( comp.GetType() == COMP::VIA ) Get(rowTL, colTL)->SetIsVia(true);	// Set via flag

		m_colorMgr.ReAssignColors();	// Forces colors to be worked out again
		return true;
	}

	bool TakeOff(Component& comp)
	{
		if ( !comp.GetIsPlaced() ) return false;	// Can't take off a component that is already floating

		const bool	bWire 		= comp.GetType() == COMP::WIRE;	// Wire's only get NodeIDs while placed
		const bool	bTrax		= comp.GetType() == COMP::TRACKS;
		const int&	compCols	= comp.GetCompCols();
		const int&	compRows	= comp.GetCompRows();
		const int&	rowTL		= comp.GetRow();
		const int&	colTL		= comp.GetCol();

		int jRow(rowTL);
		for (int j = 0; j < compRows; j++, jRow++)
		{
			int iCol(colTL);
			for (int i = 0; i < compCols; i++, iCol++)
			{
				const CompElement*	pComp = comp.GetCompElement(j, i);
				Element*			pGrid = Get(jRow, iCol);
				if ( bTrax )
				{
					if ( !pComp->ReadFlagBits(RECTSET) ) continue;		// Skip non-rect points
					if ( pComp->GetNodeId() == BAD_NODEID ) continue;	// Skip blank areas of the trax comp
					if ( !pGrid->ReadFlagBits(RECTSET) && ( !pGrid->GetIsPin() || pGrid->GetW() ) )
						SetNodeIdByUser(jRow, iCol, BAD_NODEID, false);	// false ==> don't paint pins
					pGrid->ClearFlagBits(RECTSET);
				}
				else
				{
					assert( !pComp->GetIsHole() || pGrid->GetIsHole() );	// Component hole can only be taken off a grid hole

					// Update surface
					pGrid->SetSurface( pGrid->GetSurface() - pComp->GetSurface() );
					switch ( pGrid->GetSurface() )	// Could move this switch statement to SetSurface()
					{
						case SURFACE_GAP:	pGrid->SetPinIndex(BAD_PININDEX);	break;
						case SURFACE_FREE:	pGrid->SetPinIndex(BAD_PININDEX);	break;
					}

					// Update IDs at pin location
					const size_t pinIndex = pComp->GetPinIndex();
					if ( pinIndex == BAD_PININDEX ) continue;

					pGrid->SetCompId(BAD_COMPID);	// Clear compId at pin locations

					const int origNodeId = comp.GetOrigId(pinIndex);

					assert( origNodeId == BAD_NODEID || origNodeId == comp.GetNodeId(pinIndex) );

					SetNodeId(pGrid, origNodeId);			// Restore grid element to original nodeId
					pGrid->ClearFlagBits(AUTOSET|VEROSET);
					pGrid->SetFlagBits(USERSET);
					comp.SetOrigId(pinIndex, BAD_NODEID);	// ...
				}
			}
		}
		if ( bWire )	// Handle wires ends
		{
			comp.SetNodeId(0, BAD_NODEID);
			comp.SetNodeId(1, BAD_NODEID);
			Element* pW0 = Get(rowTL, colTL);	assert(pW0);
			Element* pW1 = pW0->GetW();			assert(pW1);
			pW0->SetW(nullptr);	// Break pointers between board points
			pW1->SetW(nullptr);	// Break pointers between board points
		}
		comp.SetIsPlaced(false);
		if ( comp.GetType() == COMP::VIA ) Get(rowTL, colTL)->SetIsVia(false);	// Clear via flag
		return true;
	}

	void RebuildAdjacencies()
	{
		m_adjInfoMgr.DeAllocate();
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++) m_adjInfoMgr.InitCounts( GetAt(i) );
	}

	Component& GetUserComponent()	// The currently selected component
	{
		assert(m_groupMgr.GetNumUserComps() == 1);	// Should only have one component selected
		return m_compMgr.GetComponentById( m_groupMgr.GetUserCompId() );
	}

	void AddTextBox(MyScrollArea* pScrollArea)
	{
		// Adds a new text box to the board

		// Put the text in the top left of the current visible view.
		// Grow the board if necessary.

		const int& W = GetGRIDPIXELS();	// Square width in pixels
		int iRow = 1 + pScrollArea->verticalScrollBar()->value()   / W;	// The first fully visible row
		int iCol = 1 + pScrollArea->horizontalScrollBar()->value() / W;	// The first fully visible col
		iRow = std::max(0, std::min(GetRows()-1, iRow));
		iCol = std::max(0, std::min(GetCols()-1, iCol));

		const int iOldTextId = GetCurrentTextId();
		const int iTextId = m_textMgr.AddNewRect(iRow, iCol);
		SetCurrentTextId(iTextId);
		TextRect& rect = m_textMgr.GetTextRectById(iTextId);

		if ( iOldTextId != BAD_TEXTID )
		{
			rect = m_textMgr.GetTextRectById(iOldTextId);	// Copy old rect
			rect.Move(iRow - rect.m_rowMin, iCol - rect.m_colMin); // Move it to the top left of the view
		}

		if ( iRow + rect.GetRows() > GetRows() ||
			 iCol + rect.GetCols() > GetCols() )
			GrowThenPan(iRow + rect.GetRows() - GetRows(), iCol + rect.GetCols() - GetCols(), 0, 0);
	}

	int AddComponent(MyScrollArea* pScrollArea, const Component& tmp, bool bDoPlace = true)
	{
		// Adds a new component to the board, and returns its compId

		const int compId = m_compMgr.CreateComp(tmp);	// CompMgr makes a copy of tmp and returns its compId
		if ( compId == INT_MAX ) return BAD_COMPID;		// Reached component limit !!!

		Component& comp = m_compMgr.GetComponentById(compId);

		m_nodeInfoMgr.AddComp(comp);

		assert(!comp.GetIsPlaced()); // Sanity check.  Should not be placed yet

		if ( !bDoPlace ) return compId;

		// Try place the component in free space on the board
		bool bOK(false);

		if ( pScrollArea )	// If we know about the view area ...
		{
			// Put the component in the top left of the current visible view.
			// Grow the board and float the component if necessary.

			const int& W = GetGRIDPIXELS();	// Square width in pixels
			int iRow = 1 + pScrollArea->verticalScrollBar()->value()   / W;	// The first fully visible row
			int iCol = 1 + pScrollArea->horizontalScrollBar()->value() / W;	// The first fully visible col
			iRow = std::max(0, std::min(GetRows()-1, iRow));
			iCol = std::max(0, std::min(GetCols()-1, iCol));
			if ( iRow + comp.GetRows() > GetRows() ||
				 iCol + comp.GetCols() > GetCols() )
				GrowThenPan(iRow + comp.GetRows() - GetRows(), iCol + comp.GetCols() - GetCols(), 0, 0);

			comp.SetRow(iRow);
			comp.SetCol(iCol);
			comp.SetDirection('W');
			bOK = PutDown(comp);	// false ==> the component has to float
		}
		else
		{
			// Try place the component in free space on the board.  Just used for Import() method
			while( !bOK )
			{
				for (int iRow = 0; iRow <= GetRows() - comp.GetCompRows() && !bOK; iRow++)
				for (int iCol = 0; iCol <= GetCols() - comp.GetCompCols() && !bOK; iCol++)
				{
					comp.SetRow(iRow);
					comp.SetCol(iCol);
					comp.SetDirection('W');
					bOK = PutDown(comp);
				}
				if ( !bOK ) Pan(1, 0);	// No free board space, so pan the board down
			}
		}
		return compId;
	}

	void DestroyComponent(Component& comp)	// Destroys a component on the board
	{
		TakeOff(comp);							// Float the component
		m_nodeInfoMgr.RemoveComp(comp);			// Remove the component NodeInfo from m_nodeInfoMgr
		m_groupMgr.RemoveComp(comp.GetId());	// Remove the component from m_groupMgr
		m_compMgr.DestroyComp(comp);			// Destroy the component in the m_compMgr
	}

	int CreateComponent(MyScrollArea* pScrollArea, const COMP& eType, const Component* pComp = nullptr)
	{
		assert( pComp == nullptr || pComp->GetType() == eType );	// Sanity check

		// Try and produce a simple unique Name for the new part if possible
		char buffer[256] = {'\0'};
		std::string nameStr;	// We'll use this string for both Name and Value
		const std::string prefixStr = ( pComp ) ? pComp->GetPrefixStr() : GetDefaultPrefixStr(eType);	// e.g. "C" for capacitors
		bool bNameExists(true);
		for (int iSuffix = 1; iSuffix < INT_MAX && bNameExists; iSuffix++)
		{
			sprintf(buffer,"%s%d", prefixStr.c_str(), iSuffix);
			nameStr = buffer;	// e.g. "C1"
			bNameExists = ( m_compMgr.GetComponentIdFromName(nameStr) != BAD_COMPID );
		}

		const size_t numPins = ( pComp ) ? pComp->GetNumPins() : GetDefaultNumPins(eType);
		std::vector<int> nodeList;
		nodeList.resize(numPins, BAD_NODEID);
		Component tmp(nameStr, nameStr, eType, nodeList);
		if ( pComp )
		{
			tmp = *pComp;
			tmp.SetId(0);
			tmp.SetNameStr(nameStr);
			tmp.SetValueStr(nameStr);
			tmp.SetIsPlaced(false);
			tmp.ClearNodeIds();
			// If pComp has a sensible Value field, then use it
			if ( pComp->GetValueStr() != pComp->GetNameStr() )
				tmp.SetValueStr( pComp->GetValueStr() );
		}
		const bool bDoPlace = ( pComp == nullptr || pComp->GetIsTemplate() );	// Leave copied components floating
		return AddComponent(pScrollArea, tmp, bDoPlace);
	}

	bool BreakComponentIntoPads(Component& comp)
	{
		const COMP& eType = comp.GetType();
		if ( eType == COMP::VIA || eType == COMP::PAD || eType == COMP::WIRE ) return false;	// Not real components
		if ( !comp.GetIsPlaced() ) return false;	// Can't break a floating component

		std::vector<int> nodeList;	// Re-used for each new pad
		nodeList.resize(1, BAD_NODEID);

		const size_t numPins = comp.GetNumPins();
		for (size_t iPinIndex = 0; iPinIndex < numPins; iPinIndex++)	// Loop component pins
		{
			// Create a new PAD component for the pin, with suitable name, value, nodeId
			static char buffer[32];
			sprintf(buffer, "_%d", (int)(iPinIndex+1));
			nodeList[0] = comp.GetNodeId(iPinIndex);
			Component tmp(comp.GetNameStr() + std::string(buffer), comp.GetValueStr(), COMP::PAD, nodeList);

			// Find board location of existing pin, and put the new PAD there
			int row, col;
			if ( GetPinRowCol(comp.GetId(), iPinIndex, row, col) )
			{
				tmp.SetRow(row);
				tmp.SetCol(col);
				AddComponent(nullptr, tmp, false);	// Add PAD floating over the existing pin
			}
		}
		DestroyComponent(comp);	// All pins have been copied, so destroy the old component
		PlaceFloaters();		// Unfloat the new PADs
		return true;
	}

	void PasteTracks(bool bTidy)
	{
		assert( GetRoutingEnabled() );
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++)
		{
			Element* p	= GetAt(i);
			Element* pW	= p->GetW();

			if ( bTidy )	// Clear all non-pins and wires that are USER_SET ...
			{
				if ( ( !p->GetIsPin() || pW ) && p->ReadFlagBits(USERSET) && !p->ReadFlagBits(AUTOSET|VEROSET) )
				{
					SetNodeId(p, BAD_NODEID);
					if ( pW ) SetNodeId(pW, BAD_NODEID);
				}
			}

			p->ClearFlagBits(AUTOSET|VEROSET); p->SetFlagBits(USERSET);	// Don't do this on pW, or the tidy option will wipe wires !!!

			// For wires, the "Paste" operation either paints the board at the wire-ends or wipes it.
			// Fix-up the nodeId info on the wire component...
			if ( pW )
			{
				const int& nodeId	= p->GetNodeId();
				Component& comp		= m_compMgr.GetComponentById( p->GetCompId() );
				comp.SetNodeId(0, nodeId);	comp.SetNodeId(1, nodeId);
				comp.SetOrigId(0, nodeId);	comp.SetOrigId(1, nodeId);
			}
		}
	}

	void WipeTracks()
	{
		FloatAllComps();	// Float all components

		// If we have a placed trax component, only wipe the board within it then destroy it
		Component& comp = m_compMgr.GetTrax();
		if ( comp.GetSize() > 0 && comp.GetIsPlaced() )
		{
			Component& comp = m_compMgr.GetTrax();
			const int	rowTL		= comp.GetRow();
			const int	colTL		= comp.GetCol();
			const int&	compCols	= comp.GetCompCols();
			const int&	compRows	= comp.GetCompRows();

			int jRow(rowTL);
			for (int j = 0; j < compRows; j++, jRow++)
			{
				int iCol(colTL);
				for (int i = 0; i < compCols; i++, iCol++)
				{
					if ( !comp.GetCompElement(j,i)->ReadFlagBits(RECTSET) ) continue;
					Element* p = Get(jRow, iCol);
					assert( !p->GetIsPin() && !p->GetIsHole() && !p->GetW() && p->GetCompId() == BAD_COMPID );	// Sanity check
					SetNodeId(p, BAD_NODEID);
					p->SetSurface(SURFACE_FREE);
					p->ClearFlagBits(AUTOSET|VEROSET|RECTSET);
					p->SetFlagBits(USERSET);
				}
			}
			m_compMgr.ClearTrax();
			m_rectMgr.Clear();
		}
		else	// ... otherwise wipe all the points on the board. The floating trax component won't get wiped
		{
			for (int j = 0; j < GetRows(); j++)	for (int i = 0; i <= GetCols(); i++)
			{
				Element* p = Get(j, i);
				assert( !p->GetIsPin() && !p->GetIsHole() && !p->GetW() && p->GetCompId() == BAD_COMPID );	// Sanity check
				SetNodeId(p, BAD_NODEID);
				p->SetSurface(SURFACE_FREE);
				p->ClearFlagBits(AUTOSET|VEROSET);
				p->SetFlagBits(USERSET);
			}
		}
		PlaceFloaters();		// Unfloat components
	}

	void FloatAllComps()	// Float all components (i.e. take them off the board)
	{
		for (auto& mapObj : m_compMgr.m_mapIdToComp)
		{
			Component& comp = mapObj.second;
			TakeOff(comp);
		}
	}

	void PlaceFloaters()	// Try to place down all the floating components
	{
		// Do trax component first.
		// If the trax comp won't go down, then leave the rest floating
		Component& comp	= m_compMgr.GetTrax();
		PutDown(comp);
		if ( comp.GetSize() > 0 && !comp.GetIsPlaced() ) return;
		while(true)
		{
			bool bPlacedOK(false);
			for (auto& mapObj : m_compMgr.m_mapIdToComp)
				if ( PutDown( mapObj.second ) ) bPlacedOK = true;
			if ( !bPlacedOK ) break;	// Couldn't place any more components
		}
	}

	int GetComponentId(int row, int col)	// Pick the most relevant component at the location
	{
		MakeToroid(row, col);	// Make co-ordinates wrap around at grid edges

		// Most to least prefered order is ...
		// ... unplaced plugs, unplaced non-plugs, placed plugs, placed non-plugs
		for (int iLoop = 0; iLoop < 4; iLoop++)
		{
			const bool bReqPlaced = ( iLoop / 2 == 1 );	// true ==> Only consider placed components
			const bool bReqPlug   = ( iLoop % 2 == 0 );	// true ==> Only consider components that can go under ICs
			for (const auto& mapObj : m_compMgr.GetMapIdToComp())
			{
				const Component&	comp	= mapObj.second;
				const bool			bPlug	= IsPlug( comp.GetType() );
				const bool			bPlaced = comp.GetIsPlaced();
				if ( bPlaced != bReqPlaced ) continue;
				if ( bPlug != bReqPlug ) continue;
				int rowTL = comp.GetRow();
				int colTL = comp.GetCol();

				MakeToroid(rowTL, colTL);	// Make co-ordinates wrap around at grid edges

				if ( row >= rowTL && row < rowTL + comp.GetCompRows() &&
					 col >= colTL && col < colTL + comp.GetCompCols() )
					return comp.GetId();
			}
			Component& comp = m_compMgr.GetTrax();
			if ( comp.GetSize() > 0 )
			{
				const bool bPlug	= IsPlug( comp.GetType() );
				const bool bPlaced	= comp.GetIsPlaced();
				if ( bPlaced != bReqPlaced ) continue;
				if ( bPlug != bReqPlug ) continue;
				int rowTL = comp.GetRow();
				int colTL = comp.GetCol();

				MakeToroid(rowTL, colTL);	// Make co-ordinates wrap around at grid edges

				if ( row >= rowTL && row < rowTL + comp.GetCompRows() &&
					 col >= colTL && col < colTL + comp.GetCompCols() )
				{
					if ( comp.GetCompElement(row-rowTL, col-colTL)->ReadFlagBits(RECTSET) )
						return comp.GetId();
				}
			}
		}
		return BAD_COMPID;
	}

	int GetTextId(int row, int col)	// Pick the most relevant text box at the location
	{
		MakeToroid(row, col);	// Make co-ordinates wrap around at grid edges

		TextRect bestRect;
		int bestId(BAD_TEXTID);
		for (const auto& mapObj : m_textMgr.m_mapIdtoText)
		{
			const TextRect& rect = mapObj.second;
			if ( !rect.ContainsPoint(row, col) ) continue;
			if ( !bestRect.GetIsValid() || rect.GetArea() < bestRect.GetArea() )
			{
				bestRect = rect;
				bestId	 = mapObj.first;
			}
		}
		return bestId;
	}

	void CheckAllComplete()
	{
		assert(!m_tmpIsRouting);	// Need to calc full MH values rather than quit early for routing
		Element* pDummy(nullptr);
		for (size_t n = 0; n < m_nodeInfoMgr.GetSize(); n++)
		{
			NodeInfo* pNodeInfo = m_nodeInfoMgr.GetAt(n);
			pNodeInfo->SetIsComplete(false);

			const int& nodeId = pNodeInfo->GetNodeId();
			if ( nodeId == BAD_NODEID ) continue;

			BuildTargetPins(nodeId);

			bool bComplete(true);
			auto iterEnd = m_targetPins.end();
			for (auto iterI = m_targetPins.begin(); iterI != iterEnd && bComplete; ++iterI)
			{
				Element* pI = *iterI;

				Manhatten(pI, pDummy, 0);

				for (auto iterJ = iterI; iterJ != iterEnd && bComplete; ++iterJ)
				{
					Element* pJ = *iterJ;
					bComplete = ( pJ->GetMH() != BAD_MH );
				}
			}
			pNodeInfo->SetIsComplete(bComplete);
		}
	}

	bool GetPinRowCol(const int& compId, const size_t& iPinIndex, int& row, int& col) const
	{
		if ( compId == BAD_COMPID || iPinIndex == BAD_PININDEX ) return false;

		for (row = 0; row < GetRows(); row++)
		for (col = 0; col < GetCols(); col++)
		{
			Element* p = Get(row, col);
			if ( p->GetCompId() == compId && p->GetPinIndex() == iPinIndex )
				return true;	// (row, col) ==> output
		}
		return false;
	}

	void FloodNodeId(const int& nodeId)
	{
		for (int row = 0; row < GetRows(); row++)
		for (int col = 0; col < GetCols(); col++)
		{
			Element* p = Get(row, col);
			if ( p->GetMH() == BAD_MH) continue;
			SetNodeIdByUser(row, col, nodeId, true);	// true ==> paint pins
		}
	}

	bool SetNodeIdByUser(const int& row, const int& col, const int& nodeId, const bool& bPaintPins)
	{
		// returns false if nothing changed

		Element*		p			= Get(row, col);
		const int&		compId		= p->GetCompId();
		const size_t	pinIndex	= p->GetPinIndex();
		Element*		pW			= p->GetW();	// Other end of wire
		const bool		bWire		= ( pW != nullptr );
		const bool		bPin		= p->GetIsPin();
		const bool		bHole		= p->GetIsHole();

		if ( bHole ) return false;	// No change

		assert( !bPin || compId != BAD_COMPID );	// Sanity check

		// Handle special case first.
		if ( bPin && !bWire && !bPaintPins )
		{
			// If trying to paint the board under a non-wire pin ...
			// ... we can modify the "origId" for the pin, but are only allowed
			// ... to wipe it or make it match the nodeId of the pin. Then quit.

			Component& comp = m_compMgr.GetComponentById(compId);
			assert( comp.GetType() != COMP::WIRE );	// Sanity check

			if ( nodeId != BAD_NODEID && nodeId != comp.GetNodeId(pinIndex) ) return false;	// Can't set a bad origId

			if ( comp.GetOrigId(pinIndex) == nodeId ) return false;	// origId is already as required

			// Need to do (RemoveComp/ SetNodeId/ AddComp) to ensure m_nodeInfoMgr is updated OK
			m_nodeInfoMgr.RemoveComp(comp);
			comp.SetOrigId(pinIndex, nodeId);
			m_nodeInfoMgr.AddComp(comp);
			return true;
		}

		// Now do regular cases:  Paint the board as needed...

		if ( nodeId == p->GetNodeId() ) return false;	// No change

		// Set the NodeId on the element
		SetNodeId(p, nodeId);
		p->ClearFlagBits(AUTOSET|VEROSET);
		p->SetFlagBits(USERSET);
		// .. and on the element at the other wire end
		if ( bWire )
		{
			SetNodeId(pW, nodeId);
			pW->ClearFlagBits(AUTOSET|VEROSET);
			pW->SetFlagBits(USERSET);
		}

		// Set the nodeId at the component pin
		if ( bPin )
		{
			Component& comp = m_compMgr.GetComponentById(compId);
			assert( bWire == (comp.GetType() == COMP::WIRE) );	// Sanity check

			if ( bWire )	// Wire
			{
				const size_t otherPinIndex = ( pinIndex == 0 ) ? 1 : 0;
				comp.SetNodeId(pinIndex, nodeId);
				comp.SetOrigId(pinIndex, nodeId);
				comp.SetNodeId(otherPinIndex, nodeId);
				comp.SetOrigId(otherPinIndex, ( comp.GetOrigId(otherPinIndex) > 0 ) ? nodeId : 0);
			}
			else			// Regular component
			{
				assert( bPaintPins );	// Sanity check

				// Need to do (RemoveComp/ SetNodeId/ AddComp) to ensure m_nodeInfoMgr is updated OK
				m_nodeInfoMgr.RemoveComp(comp);
				comp.SetNodeId(pinIndex, nodeId);
				if ( comp.GetOrigId(pinIndex) != nodeId )	// Modifying a pin on a previously painted track ...
					comp.SetOrigId(pinIndex, BAD_NODEID);	// ... should wipe the track under the pin
				m_nodeInfoMgr.AddComp(comp);
			}
		}
		return true;
	}

	void CalcSolder()	// Work out locations of solder blobs to join vertical veroboard tracks together
	{
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++)
		{
			Element* p = GetAt(i);
			p->SetSolderR(false);	// Clear solder
		}
		const bool& bVertical = GetVerticalStrips();	// Strip direction
		for (size_t n = 0; n < m_adjInfoMgr.GetSize(); n++)
		{
			const int& nodeId = m_adjInfoMgr.GetAt(n)->GetNodeId();
			if ( nodeId == BAD_NODEID ) continue;	// Want valid nodeIds
			const int& numCols = ( bVertical ) ? GetCols() : GetRows();
			for (int col = 0; col < numCols; col++)	// Loop cols/rows
				SetSolder(nodeId, col, bVertical);
		}
	}

	void SetSolder(const int& nodeId, const int& col, const bool& bVertical)
	{
		// Sets elements of bSolderR to indicate solder blob between col and col+1
		if (  bVertical && col == GetCols()-1 ) return;
		if ( !bVertical && col == GetRows()-1 ) return;
		
		assert( nodeId != BAD_NODEID );
		int bestRow(-1), bestRowPins(-INT_MAX), bestRowPads(INT_MAX);
		const int rowMax = ( bVertical ) ? GetRows() : GetCols();
		for (int row = 0; row < rowMax; row++)
		{
			const Element*	pC			= bVertical ? Get(row, col)		: Get(col,   row);
			const Element*	pR			= bVertical ? Get(row, col+1)	: Get(col+1, row);
			const bool		bMatch		= pC->GetNodeId() == nodeId && pR->GetNodeId() == nodeId;
			const bool		bLastRow	= row == rowMax-1;
			if ( bMatch )
			{
				int rowPins(0), rowPads(0);	// Try avoid putting a blob where we have a pad. Otherwise prefer pin locations.
			
				if ( pC->GetIsPin() )
				{
					if ( m_compMgr.GetComponentById(pC->GetCompId()).GetType() == COMP::PAD )
						rowPads++;
					else 
						rowPins++;
				}
				if ( pR->GetIsPin() )
				{
					if ( m_compMgr.GetComponentById(pR->GetCompId()).GetType() == COMP::PAD )
						rowPads++;
					else 
						rowPins++;
				}
				const bool bIsBetter =	( bestRow == -1 ) ||
										( rowPads < bestRowPads ) ||
										( rowPads == bestRowPads &&	rowPins > bestRowPins );
				if ( bIsBetter )
				{
					bestRow		= row;
					bestRowPins	= rowPins;
					bestRowPads	= rowPads;
				}
			}
			if ( bLastRow || !bMatch )
			{
				if ( bestRow != -1 )
				{
					if ( bVertical )
						Get(bestRow, col)->SetSolderR(true);
					else
						Get(col, bestRow)->SetSolderR(true);
				}
				bestRow		= -1;		// Reset
				bestRowPins	= -INT_MAX;	// Reset
				bestRowPads	=  INT_MAX;	// Reset
			}
		}
	}

	void AutoFillVero()
	{
		int minRow, minCol, maxRow, maxCol;
		GetBounds(minRow, minCol, maxRow, maxCol);

		const bool& bVertical = GetVerticalStrips();

		// Note: The terms "top" and "bot" in the following code should be
		//		 taken to mean "left" and "right" if making horizontal strips.
		const int jMin	= ( bVertical ) ? minCol : minRow;
		const int jMax	= ( bVertical ) ? maxCol : maxRow;	
		const int iMin	= ( bVertical ) ? minRow : minCol;
		const int iMax	= ( bVertical ) ? maxRow : maxCol;
		for (int j = jMin; j <= jMax; j++)
		{
			int nodeIdTop(BAD_NODEID), lenTop(INT_MAX);
			for (int i = iMin; i <= iMax; i++)
			{
				Element*	pC		= ( bVertical ) ? Get(i,j) : Get(j,i);
				const int&	nodeId	= pC->GetNodeId();
				if ( nodeId != BAD_NODEID )
				{
					if ( nodeIdTop == nodeId )
						lenTop++;		// Increment top count
					else
					{
						nodeIdTop = nodeId; lenTop = 1;	// Start top count
					}
					continue;
				}
				if ( pC->GetIsPin() ) continue;		// Can't assign pins, so skip
				if ( pC->GetIsHole() ) continue;	// Can't assign holes, so skip

				// Have a blank non-pin element at this point
				// Search for first used nodeId below

				int nodeIdBot(BAD_NODEID), lenBot(INT_MAX);
				for (int ii = i+1; ii <= iMax; ii++)
				{
					const Element*	pB		= ( bVertical ) ? Get(ii,j) : Get(j,ii);
					const int&		nodeId	= pB->GetNodeId();
	
					if ( nodeId == BAD_NODEID )
					{
						if ( nodeIdBot == BAD_NODEID )
							continue;					// Skip
						else
							break;						// End bottom count
					}
					else if ( nodeIdBot == BAD_NODEID )
					{
						nodeIdBot = nodeId; lenBot = 1;	// Start bottom count
					}
					else if ( nodeIdBot == nodeId )
						lenBot++;						// Increment bottom count
					else
						break;							// End bottom count
				}

				if ( lenTop == INT_MAX && lenBot == INT_MAX )
					SetNodeId(pC, GetNewNodeId());	// Unused strip, so make a new nodeId
				else if ( lenTop < lenBot )
					SetNodeId(pC, nodeIdTop);
				else
					SetNodeId(pC, nodeIdBot);
				pC->ClearFlagBits(USERSET|AUTOSET);
				pC->SetFlagBits(VEROSET);

				nodeIdTop = pC->GetNodeId(); lenTop = 1;	// Start top count
			}
		}
		m_colorMgr.ReAssignColors();	// Forces colors to be worked out again
	}

	// Routing functions

	void WipeAutoSetPoints()
	{
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++)
		{
			Element* p	= GetAt(i);
			Element* pW	= p->GetW();
			bool bWipe = p->ReadFlagBits(AUTOSET) & !p->ReadFlagBits(USERSET);	// Clear if AUTOSET and not USERSET
			if ( pW ) bWipe &= ( !pW->ReadFlagBits(USERSET) );	// If it's a wire, the other end must not be USETSET either

			if ( bWipe ) SetNodeId(p, BAD_NODEID);
			p->ClearFlagBits(AUTOSET);
			p->SetFlagBits(USERSET);
			if ( pW )
			{
				if ( bWipe ) SetNodeId(pW, BAD_NODEID);
				pW->ClearFlagBits(AUTOSET);
				pW->SetFlagBits(USERSET);
			}
		}
	}

	void WipeRouteIds()
	{
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++)
		{
			Element* p = GetAt(i);
			p->SetRouteId(BAD_ROUTEID);
		}
	}

	void BuildTargetPins(const int& nodeId)
	{
		assert( nodeId != BAD_NODEID );
		m_targetPins.clear();
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++)
		{
			Element* p = GetAt(i);
			if ( !p->GetIsPin() ) continue;
			if ( p->GetNodeId() != nodeId ) continue;
			if ( p->GetW() ) continue;	// Wires are not really target pins
			m_targetPins.push_back(p);
		}
	}

	void Route()
	{
		m_tmpIsRouting = true;	// Remember to set this false at end

		WipeAutoSetPoints();

		m_nodeInfoMgr.SortByLowestDifficulty(m_compMgr);

		const size_t numNodes = m_nodeInfoMgr.GetSize();
		for (size_t ii = 0; ii < numNodes; ii++)	// Loop all nodes used by components
		{
			const int& iRouteNodeId = m_nodeInfoMgr.GetAt(ii)->GetNodeId();
			if ( iRouteNodeId == BAD_NODEID ) continue;

			WipeRouteIds();

			// Populate m_targetPins with all (non-wire) component pins with the specified NodeId;
			// These are the things on the board that the routing algorithm will try and connect together.
			BuildTargetPins(iRouteNodeId);

			unsigned int RID(BAD_ROUTEID);
			while(true)
			{
				RID++;
				assert(RID < UINT_MAX);	// Should be safely < UINT_MAX in practice

				// Get next target pin with invalid route id and set its route ID to RID. (i.e. start a new route)
				Element* pNew(nullptr);
				auto iterEnd = m_targetPins.end();
				for(auto iterI = m_targetPins.begin(); iterI != iterEnd && pNew == nullptr; ++iterI)
				{
					Element* p = *iterI;
					if ( p->GetRouteId() != BAD_ROUTEID ) continue;
					p->SetRouteId(RID);
					pNew = p;
				}
				if ( !pNew ) break;	// All target pins done

				bool bRouteHasOnePoint = true;

				while(true)  // Try to grow the route
				{
					if ( bRouteHasOnePoint )
					{
						Element* pBestTarget(nullptr);

						// Populate the board with increasing Manhatten style distances starting at "pNew".

						Manhatten(pNew, pBestTarget, BAD_ROUTEID);	// Quits early if hit a target pin with invalid routeID
						if ( !pBestTarget ) break;			// No target points could reach pNew
						Backtrace(pBestTarget, iRouteNodeId, RID);	// Extend route
						bRouteHasOnePoint = false;
					}
					else
					{
						Element* pBestTarget(nullptr);
						Element* pBestRoutePoint(nullptr);
						unsigned int iBestMH(BAD_MH);
						// Loop p through all target pins with invalid routeID
						for(auto iterI = m_targetPins.begin(); iterI != iterEnd; ++iterI)
						{
							Element* p = *iterI;
							if ( p->GetRouteId() != BAD_ROUTEID ) continue;

							Element* pOut(nullptr);
							const unsigned int MH = Manhatten(p, pOut, RID, iBestMH);	// Quits early if hit any pixel with routeID == RID or MH is >= iBestMH
							if ( MH < iBestMH ) { iBestMH = MH; pBestTarget = p; pBestRoutePoint = pOut; }	// Update info on best so far
						}
						if ( !pBestTarget ) break;	// No target points could reach routeID == RID

						Manhatten(pBestTarget, pBestRoutePoint, RID);
						Backtrace(pBestRoutePoint, iRouteNodeId, RID);	// Extend route
					}
				}	// Continue growing the route
			}
		}
		m_tmpIsRouting = false;
	}

	void UpdateMHvector(Element* p, const unsigned int& iMH, Element*& pOut, const unsigned int& iTargetRouteId)
	{
		pOut = nullptr;

		if ( p->GetMH() != BAD_MH ) return;	// MH already set to something.  We won't improve on it !!!

		const int& nodeId = p->GetNodeId();
		if ( nodeId == m_tmpNodeId || ( m_tmpIsRouting && nodeId == BAD_NODEID ) )
		{
			p->SetMH(iMH);
			m_tmpVec[m_tmpVecSize++] = p;
			m_tmpMaxMH = std::max(m_tmpMaxMH, iMH);	// Update m_tmpMaxMH

			Element* pW = p->GetW();	// The other end of the wire (if any)

			// If we're routing then set pOut if p is a component pin we've not yet encountered
			if ( m_tmpIsRouting && p->GetRouteId() == iTargetRouteId )
			{
				if ( iTargetRouteId != BAD_ROUTEID || p->GetIsPin() )
				{
					const bool bFreeWire = pW && nodeId == BAD_NODEID;
					if ( !bFreeWire ) pOut = p;	// Don't treat unpainted wires as component pins (or we get dog-leg tracks)
				}
			}
			if ( pW )
			{
				assert( nodeId == pW->GetNodeId() );	// Sanity check
				const unsigned int iOtherWireEndMH = iMH + WIRE_MH;	// Wires always increase MH by WIRE_MH
				pW->SetMH(iOtherWireEndMH);
				m_tmpVec[m_tmpVecSize++] = pW;
				m_tmpMaxMH = std::max(m_tmpMaxMH, iOtherWireEndMH);	// Update m_tmpMaxMH
			}
		}
	}

	unsigned int Manhatten(Element* pStart, Element*& pOut, const unsigned int& iTargetRouteId, const unsigned int threshMH = BAD_MH)
	{
		// Populate the grid with connected Manhatten-style "distances" to pStart.
		// Horizontally/Vertically adjacent grid points give a "distance" of 2.
		// Diagonally adjacent grid points give a "distance" of 3.
		// Wires give a "distance" of WIRE_MH regardless of their length.

		pOut = nullptr;

		const bool bDiagsOK = ( GetDiagsMode() != DIAGSMODE::OFF );

		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++)	GetAt(i)->SetMH(BAD_MH); 	// Set "infinite" MH distance at each grid element

		m_tmpNodeId = pStart->GetNodeId();	// The NodeId to trace
		if ( m_tmpNodeId == BAD_NODEID ) return BAD_MH;	// Don't trace invalid NodeID

		m_tmpVec.resize(iSize, nullptr);	// Clear the set of visited points ...
		m_tmpVecSize	= 0;				// 
		m_tmpMaxMH		= 0;				// ... and the max MH value in the set

		unsigned int iMH(0);	// Strictly increasing MH value, starting with 0 on the start point

		// Set MH distance on start point
		Element* pA1 = pStart;
		if ( pA1 ) UpdateMHvector(pA1, iMH, pOut, iTargetRouteId);	if ( pOut ) return 0;

		while ( true )
		{
			iMH++;	// Increase MH (think of this as distance from start point).

			// Now see what visited points have an MH value that is "one step away" from this target value.
			// For visited wires, both wires-ends are in the set of visited points.
			// Therefore we only need to consider the 8 neighbours for each visited point.
			if ( iMH >= threshMH ) break;		// Quit if iMH exceeds threshMH
			if ( iMH > m_tmpMaxMH + 3 ) break;	// Can't reach out further from the set of visited points (3 is the diagonal MH increment)

			for (size_t i = 0; i < m_tmpVecSize; i++)	// Loop through visited points
			{
				const Element* p = m_tmpVec[i];
				const bool bOK = p->GetNodeId() == m_tmpNodeId;	// true ==> p already has correct NodeId

				if ( p->GetMH() + 2 == iMH )	// Visit point p's H/V neighbours
				{
					for (int iNbr = 0; iNbr < 8; iNbr += 2)	// Even iNbr ==> non-diagonal
					{
						Element* pNbr = p->GetNbr(iNbr);
						const bool bDirOK = ( bOK && p->GetUsed(iNbr) ) ||
											( m_tmpIsRouting && p->HaveNonBlankPins(iNbr) && !p->IsBlocked(iNbr, m_tmpNodeId) && !p->IsUselessWire(iNbr, m_tmpNodeId) );
						if ( bDirOK ) { UpdateMHvector(pNbr, iMH, pOut, iTargetRouteId); if ( pOut ) return iMH; }
					}
				}
				if ( !bDiagsOK ) continue;

				if ( p->GetMH() + 3 == iMH )	// Visit point p's diagonal neighbours
				{
					for (int iNbr = 1; iNbr < 8; iNbr += 2)	// Odd iNbr ==> diagonal
					{
						Element* pNbr = p->GetNbr(iNbr);
						const bool bDirOK = ( bOK && p->GetUsed(iNbr) ) ||
											( m_tmpIsRouting && p->HaveNonBlankPins(iNbr) && !p->IsBlocked(iNbr, m_tmpNodeId) && !p->IsUselessWire(iNbr, m_tmpNodeId) );
						if ( bDirOK ) { UpdateMHvector(pNbr, iMH, pOut, iTargetRouteId); if ( pOut ) return iMH; }
					}
				}
			}
		}
		return BAD_MH;
	}

	// Backtrace from pEnd to point with MH = 0
	void Backtrace(Element* pEnd, const int& nodeId, const unsigned int& iRouteId)
	{
		assert(m_tmpIsRouting);

		Element* p = pEnd;
		if ( p->GetMH() == BAD_MH ) return;

		const bool bDiagsOK = ( GetDiagsMode() != DIAGSMODE::OFF );

		unsigned int MH = p->GetMH();
		while ( true )	// Backtrace
		{
			Element* pW = p->GetW();
			assert( !p->GetIsHole() );
			p->SetRouteId(iRouteId);	// Always set route Id
			if ( pW ) pW->SetRouteId(iRouteId);

			if ( !p->GetIsPin() || pW != nullptr ) // For non-pins and wires
			{
				if ( p->GetNodeId() == BAD_NODEID )	// Set NodeId if not set yet.
				{
					SetNodeId(p, nodeId);	p->ClearFlagBits(USERSET); p->SetFlagBits(AUTOSET);
					if ( pW ) { SetNodeId(pW, nodeId); pW->ClearFlagBits(USERSET); pW->SetFlagBits(AUTOSET); }
				}
				else if ( p->ReadFlagBits(USERSET) )
				{
					assert(p->GetNodeId() == nodeId);
					p->SetFlagBits(AUTOSET);
					if ( pW ) pW->SetFlagBits(AUTOSET);
				}
			}

			if ( MH == 0 ) break;

			// Now decide where to back trace to.

			// Check wire first...
			bool bOK = ( pW && pW->GetMH() == MH - WIRE_MH );	// Wires always change MH by WIRE_MH
			if ( bOK )
			{
				p = pW;
				MH -= WIRE_MH;
				continue;
			}
			for (int iLoop = 0; iLoop < 2 && !bOK; iLoop++)	// First pass to give preference to nbrs that are not wire ends
			{
				// ... then non-diagonals
				for (int iNbr = 0; iNbr < 8 && !bOK; iNbr += 2)				// Even iNbr ==> non-diagonal
				{
					Element* pNbr = p->GetNbr(iNbr);
					if ( iLoop == 0 &&  pNbr->GetW() ) continue;	// Skip if nbr is a wire
					if ( iLoop == 1 && !pNbr->GetW() ) continue;	// Skip if nbr is a non-wire
					if ( !p->IsBlocked(iNbr, nodeId) && pNbr->GetMH() == MH - 2 ) { p = pNbr; MH -= 2; bOK = true; }
				}
				// ... then diagonals (if allowed) ...
				for (int iNbr = 1; iNbr < 8 && !bOK && bDiagsOK; iNbr += 2)	// Odd iNbr ==> diagonal
				{
					Element* pNbr = p->GetNbr(iNbr);
					if ( iLoop == 0 &&  pNbr->GetW() ) continue;	// Skip if nbr is a wire
					if ( iLoop == 1 && !pNbr->GetW() ) continue;	// Skip if nbr is a non-wire
					if ( !p->IsBlocked(iNbr, nodeId) && pNbr->GetMH() == MH - 3 ) { p = pNbr; MH -= 3; bOK = true; }
				}
			}
			if ( bOK ) continue;

			assert(0);	// Oh dear. Something went badly wrong !!!
			break;
		}
	}

	// GUI helpers for manipulating user-selected components
	void SelectAllComps(bool bRestrictToRects)
	{
		const RectManager& rectMgr = GetRectMgr();

		if ( bRestrictToRects )
		{
			// Build the trax object
			Rect bounding = rectMgr.GetBounding() | rectMgr.GetCurrent();
			m_compMgr.BuildTrax(rectMgr, *this, bounding.m_rowMin, bounding.m_rowMax, bounding.m_colMin, bounding.m_colMax);
			assert( m_compMgr.GetTrax().GetIsPlaced() );
		}
		for (const auto& mapObj : m_compMgr.GetMapIdToComp())
		{
			const Component& comp = mapObj.second;
			if ( bRestrictToRects )	// If we have rectangles defined ...
			{
				// ... only select components within them.
				const int&	L = comp.GetCol();
				const int&	T = comp.GetRow();
				const int	R = L + comp.GetCompCols() - 1;
				const int	B = T + comp.GetCompRows() - 1;

				const bool bOK = rectMgr.ContainsPoint(T, L) || rectMgr.ContainsPoint(T, R) ||
								 rectMgr.ContainsPoint(B, L) || rectMgr.ContainsPoint(B, R);
				if ( !bOK ) continue;
			}
			SetCurrentCompId( mapObj.first );
			if ( !m_groupMgr.GetIsUserComp(GetCurrentCompId()) )	// If current comp not in user-group ...
				m_groupMgr.UpdateUserGroup(GetCurrentCompId());		// ... add current comp (and its siblings) to user group
		}
		SetCurrentTextId(BAD_TEXTID);
	}

	bool ConfirmDestroyUserComps()	// returns false if user-group is empty or has only wires & vias
	{
		std::list<int> userCompIds;
		m_groupMgr.GetGroupCompIds(USER_GROUPID, userCompIds);
		for (const auto& compId : userCompIds)
		{
			switch( m_compMgr.GetComponentById(compId).GetType() )
			{
				case COMP::WIRE:
				case COMP::VIA:		continue;
				default:			return true;
			}
		}
		return false;
	}

	void DestroyUserComps()	// Destroy components in the user-group
	{
		WipeAutoSetPoints();
		std::list<int> userCompIds;
		m_groupMgr.GetGroupCompIds(USER_GROUPID, userCompIds);
		for (const auto& compId : userCompIds) DestroyComponent( m_compMgr.GetComponentById(compId) );
		assert( m_groupMgr.GetNumUserComps() == 0 );	// User group should be empty now
		SetCurrentCompId(BAD_COMPID);
		PlaceFloaters();		// See if we can now place floating components down
		GetRectMgr().Clear();
		m_compMgr.ClearTrax();	// Clear the trax object
	}

	void MoveUserCompText(const int& deltaRow, const int& deltaCol)	// Move text label
	{
		Component& comp	= GetUserComponent();
		if ( deltaRow == 0 && deltaCol == 0 )	// (0,0) ==> reset rather than shift
		{
			comp.SetLabelOffsetRow(0);
			comp.SetLabelOffsetCol(0);
		}
		else
		{
			comp.SetLabelOffsetRow( comp.GetLabelOffsetRow() + deltaRow);
			comp.SetLabelOffsetCol( comp.GetLabelOffsetCol() + deltaCol);
		}
	}

	void StretchUserComp(const bool& bGrow)	// Stretch the selected component length
	{
		Component& comp	= GetUserComponent();
		if ( bGrow )
		{
			// Get bottom-right corner of footprint
			int maxRow(comp.GetRow() + comp.GetCompRows() - 1);
			int maxCol(comp.GetCol() + comp.GetCompCols() - 1);

			// Work out what the stretch will do to it
			if ( comp.GetDirection() == 'W' || comp.GetDirection() == 'E' )
				maxCol += GetStretchIncrement(comp.GetType());
			else
				maxRow += GetStretchIncrement(comp.GetType());

			if ( maxRow + 1 - GetRows() > 0 )		// If we'll go too far down
				Pan(-(maxRow + 1 - GetRows()), 0);	// ... pan the whole circuit up to make room
			if ( maxCol + 1 - GetCols() > 0 )		// If we'll go too far right
				Pan(0, -(maxCol + 1 - GetCols()));	// ... pan the whole circuit left to make room
		}
		WipeAutoSetPoints();
		TakeOff(comp);
		comp.Stretch(bGrow);
		PutDown(comp);
		PlaceFloaters();	// See if we can now place floating components down
	}

	void StretchWidthUserComp(const bool& bGrow)	// Stretch the selected component width (just for DIPs at the moment)
	{
		Component& comp	= GetUserComponent();
		if ( bGrow )
		{
			// Get bottom-right corner of footprint
			int maxRow(comp.GetRow() + comp.GetCompRows() - 1);
			int maxCol(comp.GetCol() + comp.GetCompCols() - 1);

			// Work out what the stretch will do to it
			if ( comp.GetDirection() == 'W' || comp.GetDirection() == 'E' )
				maxRow++;
			else
				maxCol++;

			if ( maxRow + 1 - GetRows() > 0 )		// If we'll go too far down
				Pan(-(maxRow + 1 - GetRows()), 0);	// ... pan the whole circuit up to make room
			if ( maxCol + 1 - GetCols() > 0 )		// If we'll go too far right
				Pan(0, -(maxCol + 1 - GetCols()));	// ... pan the whole circuit left to make room
		}
		WipeAutoSetPoints();
		TakeOff(comp);
		comp.StretchWidth(bGrow);
		PutDown(comp);
		PlaceFloaters();	// See if we can now place floating components down
	}

	void ChangeTypeUserComp(const COMP& eType)
	{
		Component& comp	= GetUserComponent();
		assert( comp.GetType() != eType );	// Current type must differ from new type

		const bool bStretch = ( eType == COMP::CAP_CERAMIC || eType == COMP::CAP_FILM || eType == COMP::DIODE );
		if ( !bStretch )	// If the new type has a fixed footprint, grow grid if necessary
		{
			// Get bottom-right corner of new footprint
			int numRows(0), numCols(0);
			GetMakeInstructions(eType, numRows, numCols);
			if ( comp.GetDirection() == 'N' || comp.GetDirection() == 'S' ) std::swap(numRows, numCols);
			int maxRow(comp.GetRow() + numRows - 1);
			int maxCol(comp.GetCol() + numCols - 1);

			if ( maxRow + 1 - GetRows() > 0 )		// If corner will be too far down
				Pan(-(maxRow + 1 - GetRows()), 0);	// ... pan the whole circuit up to make room
			if ( maxCol + 1 - GetCols() > 0 )		// If corner will be too far right
				Pan(0, -(maxCol + 1 - GetCols()));	// ... pan the whole circuit left to make room
		}

		// Now change the component type
		const int oldPinSeparation	= GetPinSeparation( comp.GetType() );	// For LEDs, and electro caps
		const int oldLength			= ( oldPinSeparation > 0 ) ? oldPinSeparation : comp.GetCols();
		WipeAutoSetPoints();
		TakeOff(comp);
		comp.FootPrint::Build(eType);
		comp.SetDefaultPinFlags();
		comp.SetDefaultStrings();
		comp.AddDefaultShapes();
		if ( bStretch )
		{
			while ( comp.GetCols() < oldLength ) comp.Stretch(true);	// true  ==> grow
			while ( comp.GetCols() > oldLength ) comp.Stretch(false);	// false ==> shrink
		}
		PutDown(comp);
		PlaceFloaters();	// See if we can now place floating components down
	}

	void CopyUserComps()	// Make a blank copy of the user-group components and float them
	{
		std::list<int> userCompIds;
		m_groupMgr.GetGroupCompIds(USER_GROUPID, userCompIds);

		CopyComps(userCompIds);
	}

	bool MoveUserComps(const int& deltaRow, const int& deltaCol)	// Move user-group components, and return true if the grid was panned
	{
		if ( deltaRow == 0 && deltaCol == 0 ) return false;

		std::list<int> userCompIds;
		m_groupMgr.GetGroupCompIds(USER_GROUPID, userCompIds);

		return MoveComps(userCompIds, deltaRow, deltaCol);
	}

	void RotateUserComps(const bool& bCW)	// Rotate the selected components
	{
		std::list<int> userCompIds;
		m_groupMgr.GetGroupCompIds(USER_GROUPID, userCompIds);

		RotateComps(userCompIds, bCW);
	}

	void CopyComps(const std::list<int>& compIds)	// Make a blank copy of the components and float them
	{
		if ( compIds.empty() ) return;

		const bool bMakeNewGroup	= ( compIds.size() > 1 );	// If we have multiple components, the copies will be put into a single new group
		const int  newGroupId		= ( bMakeNewGroup ) ? m_groupMgr.GetNewGroupId() : BAD_GROUPID;	// Make new groupId bigger than all others
		if ( bMakeNewGroup && newGroupId == INT_MAX ) return;	// Fail if we've reached the max allowed groupId
		for (auto& compId : compIds)
		{
			const Component&	comp		= m_compMgr.GetComponentById( compId );
			const int			newCompId	= CreateComponent(nullptr, comp.GetType(), &comp);	// Create blank copy of the component and get its compId
			if ( newCompId == BAD_COMPID ) break;	// Reached component limit
			if ( bMakeNewGroup ) m_groupMgr.Add(newGroupId, newCompId);
		}
	}

	bool MoveTextBox(const int& deltaRow, const int& deltaCol)	// Move text box, and return true if the grid was panned
	{
		bool bPanned(false);	// Set true if we pan the grid

		if ( deltaRow == 0 && deltaCol == 0 ) return false;

		// Move the rect and update the manager
		GetTextMgr().MoveRect(GetCurrentTextId(), deltaRow, deltaCol);

		TextRect& rect = GetTextMgr().GetTextRectById( GetCurrentTextId() );
		if ( !rect.GetIsValid() ) return false;	//TODO ???

		// Pan the circuit as needed if the rect has gone out of bounds
		if ( rect.m_rowMin < 0 )								// If we've gone too far up ...
			bPanned = Pan(-rect.m_rowMin, 0);					// ... pan the whole circuit down
		else if ( rect.m_rowMax + 1 - GetRows() > 0 )			// If we've gone too far down ...
			bPanned = Pan(-(rect.m_rowMax + 1 - GetRows()), 0);	// ... pan the whole circuit up ...
		if ( rect.m_colMin < 0 )								// If we've gone too far left ...
			bPanned = Pan(0, -rect.m_colMin);					// ... pan the whole circuit right
		else if ( rect.m_colMax + 1 - GetCols() > 0 )			// If we've gone too far right ...
			bPanned = Pan(0, -(rect.m_colMax + 1 - GetCols()));	// ... pan the whole circuit left

		return bPanned;
	}

	bool MoveComps(const std::list<int>& compIds, const int& deltaRow, const int& deltaCol)	// Move components and return true if the grid was panned
	{
		bool bPanned(false);	// Set true if we pan the grid

		if ( deltaRow == 0 && deltaCol == 0 ) return bPanned;

		// Treat the components as a single large footprint with LT at (minRow, minCol)
		Rect rect = GetFootprintBounds(compIds);
		if ( !rect.GetIsValid() ) return false;

		// Work out what the new bounds would be are after the move
		rect.Move(deltaRow, deltaCol);

		// If the group will move out of bounds, pan the circuit first to grow the grid.
		if ( rect.m_rowMin < 0 )								// If we'll go too far up ...
			bPanned = Pan(-rect.m_rowMin, 0);					// ... pan the whole circuit down to make room
		else if ( rect.m_rowMax + 1 - GetRows() > 0 )			// If we'll go too far down ...
			bPanned = Pan(-(rect.m_rowMax + 1 - GetRows()), 0);	// ... pan the whole circuit up to make room
		if ( rect.m_colMin < 0 )								// If we'll go too far left ...
			bPanned = Pan(0, -rect.m_colMin);					// ... pan the whole circuit right to make room
		else if ( rect.m_colMax + 1 - GetCols() > 0 )			// If we'll go too far right ...
			bPanned = Pan(0, -(rect.m_colMax + 1 - GetCols()));	// ... pan the whole circuit left to make room

		WipeAutoSetPoints();

		// Take off all comps, and finally the trax comp. Move them but keep them floating.
		for (auto& compId : compIds)
		{
			Component& comp = m_compMgr.GetComponentById( compId );
			int newRow = comp.GetRow() + deltaRow;
			int newCol = comp.GetCol() + deltaCol;
			MakeToroid(newRow, newCol);	// Make co-ordinates wrap around at grid edges
			TakeOff(comp);
			comp.SetRow(newRow);
			comp.SetCol(newCol);
		}
		// The trax comp
		Component& comp = m_compMgr.GetTrax();
		if ( comp.GetSize() > 0 )
		{
			int newRow = comp.GetRow() + deltaRow;
			int newCol = comp.GetCol() + deltaCol;
			MakeToroid(newRow, newCol);	// Make co-ordinates wrap around at grid edges
			TakeOff(comp);
			comp.SetRow(newRow);
			comp.SetCol(newCol);
		}

		// First put down the trax comp, and then all other comps.
		// If the trax comp won't go down, then leave the rest floating
		PutDown(comp);
		if ( comp.GetSize() == 0 || comp.GetIsPlaced() )
		{
			for (auto& compId : compIds)
				PutDown( m_compMgr.GetComponentById( compId ) );
			PlaceFloaters();	// See if we can now place floating components down
		}
		return bPanned;
	}

	void RotateComps(const std::list<int>& compIds, const bool& bCW)	// Rotate components
	{
		// Treat the components as a single large footprint with LT at (minRow, minCol)
		Rect rect = GetFootprintBounds(compIds);
		if ( !rect.GetIsValid() ) return;

		const int DY = 1 + rect.m_rowMax - rect.m_rowMin;	// >= 1		// The group height before rotation
		const int DX = 1 + rect.m_colMax - rect.m_colMin;	// >= 1		// The group width  before rotation

		// We'll try and rotate about the centre of the group.
		// Need to avoid the group drifting as we rotate it
		int dy = DY - DX;
		int dx = DX - DY;
		if ( dy > 0 ) { dy /= 2; } else { dy = -dy; dy /= 2; dy = -dy; }
		if ( dx > 0 ) { dx /= 2; } else { dx = -dx; dx /= 2; dx = -dx; }

		// Work out how the bounds would be modified by the rotation, and grow the grid if needed
		{
			const int deltaT = std::max(0, -rect.m_rowMin - dy);
			const int deltaB = std::max(0,  rect.m_rowMin + dy + DX - GetRows());
			const int deltaL = std::max(0, -rect.m_colMin - dx);
			const int deltaR = std::max(0,  rect.m_colMin + dx + DY - GetCols());

			GrowThenPan(deltaT + deltaB, deltaL + deltaR, deltaT, deltaL);

			// GrowThenPan modifies rows and cols so we must recalculate the bounds
			rect = GetFootprintBounds(compIds);
			if ( !rect.GetIsValid() ) return;
		}

		// Work out the new centre location
		if ( bCW ) dx += (DY - 1); else dy += (DX - 1);

		const int newCentreRow = rect.m_rowMin + dy;
		const int newCentreCol = rect.m_colMin + dx;

		WipeAutoSetPoints();

		// Take off all comps, and finally the trax comp. Move and rotate them but keep them floating.
		for (auto& compId : compIds)
		{
			Component& comp	= m_compMgr.GetComponentById( compId );
			TakeOff(comp);

			int newRow(newCentreRow), newCol(newCentreCol);	// Start with the new group centre

			// Then correct for location of the comp's LT corner w.r.t. the group's LT corner
			if ( bCW )	{ newRow += (comp.GetCol() - rect.m_colMin); newCol -= (comp.GetRow() - rect.m_rowMin); }
			else		{ newRow -= (comp.GetCol() - rect.m_colMin); newCol += (comp.GetRow() - rect.m_rowMin); }

			// Then correct for the fact that comp.Rotate() rotates about the comp's centre rather than its LT corner
			if ( bCW )	newCol -= ( comp.GetCompRows() - 1 );
			else		newRow -= ( comp.GetCompCols() - 1 );

			MakeToroid(newRow, newCol);	// Make co-ordinates wrap around at grid edges
			comp.SetRow(newRow);
			comp.SetCol(newCol);
			comp.Rotate(bCW);	// Rotate the component ...
		}
		// The trax comp
		{
			Component& comp	= m_compMgr.GetTrax();
			if ( comp.GetSize() > 0 )
			{
				TakeOff(comp);

				int newRow(newCentreRow), newCol(newCentreCol);	// Start with the new group centre

				// Then correct for location of the comp's LT corner w.r.t. the group's LT corner
				if ( bCW )	{ newRow += (comp.GetCol() - rect.m_colMin); newCol -= (comp.GetRow() - rect.m_rowMin); }
				else		{ newRow -= (comp.GetCol() - rect.m_colMin); newCol += (comp.GetRow() - rect.m_rowMin); }

				// Then correct for the fact that comp.Rotate() rotates about the comp's centre rather than its LT corner
				if ( bCW )	newCol -= ( comp.GetCompRows() - 1 );
				else		newRow -= ( comp.GetCompCols() - 1 );

				MakeToroid(newRow, newCol);	// Make co-ordinates wrap around at grid edges
				comp.SetRow(newRow);
				comp.SetCol(newCol);
				comp.Rotate(bCW);	// Rotate the component ...
			}
		}

		// First put down the trax comp, and then all other comps.
		// If the trax comp won't go down, then leave the rest floating
		{
			Component& comp	= m_compMgr.GetTrax();
			PutDown(comp);
			if ( comp.GetSize() == 0 || comp.GetIsPlaced() )
			{
				for (auto& compId : compIds)
					PutDown( m_compMgr.GetComponentById( compId ) );
				PlaceFloaters();	// See if we can now place floating components down
			}
		}
	}

	Rect GetFootprintBounds(const std::list<int>& compIds)
	{
		Rect bounding;
		for (auto& compId : compIds)
		{
			const Component& comp	= m_compMgr.GetComponentById( compId );
			bounding |= comp.GetFootprintRect();
		}
		const Component& comp	= m_compMgr.GetTrax();
		if ( comp.GetSize() > 0 )
			bounding |= comp.GetFootprintRect();
		return bounding;
	}

	void				SetInfoStr(const std::string& str)	{ m_infoStr = str; }
	const std::string&	GetInfoStr() const	{ return m_infoStr; }
	void				CalculateColors()	{ m_colorMgr.CalculateColors(m_adjInfoMgr, this); }
	int					GetNewNodeId()		{ return m_nodeInfoMgr.GetNewNodeId(m_adjInfoMgr); }
	CompManager&		GetCompMgr()		{ return m_compMgr; }
	NodeInfoManager&	GetNodeInfoMgr()	{ return m_nodeInfoMgr; }
	GroupManager&		GetGroupMgr()		{ return m_groupMgr; }
	RectManager&		GetRectMgr()		{ return m_rectMgr; }
	TextManager&		GetTextMgr()		{ return m_textMgr; }
	ColorManager&		GetColorMgr()		{ return m_colorMgr; }

	CompDefiner&		GetCompDefiner()	{ return m_compDefiner; }
	int	 GetCurrentPinId() const			{ return m_compDefiner.GetCurrentPinId(); }
	int	 GetCurrentShapeId() const			{ return m_compDefiner.GetCurrentShapeId(); }
	bool SetCurrentPinId(const int& i)		{ return m_compDefiner.SetCurrentPinId(i); }
	bool SetCurrentShapeId(const int& i)	{ return m_compDefiner.SetCurrentShapeId(i); }

	// Command enablers for GUI
	bool GetDisableCompText()
	{
		if ( GetMirrored() ) return true;
		if ( ( GetGroupMgr().GetNumUserComps() != 1 ) || ( GetCompMode() == COMPSMODE::OFF || GetCompMode() == COMPSMODE::OUTLINE ) ) return true;
		const Component& comp	= GetUserComponent();
		const COMP&		 eType	= comp.GetType();
		return ( eType == COMP::WIRE || eType == COMP::VIA );	// No labels for wires and vias
	}
	bool GetDisableRotate()
	{
		if ( GetMirrored() ) return true;
		if ( ( GetGroupMgr().GetNumUserComps() < 1 ) || ( GetCompMode() == COMPSMODE::OFF ) ) return true;
		if ( GetGroupMgr().GetNumUserComps() > 1 ) return false;
		const Component& comp = GetUserComponent();
		return ( comp.GetType() == COMP::VIA );	// Can't rotate a via
	}
	bool GetDisableStretch(bool bGrow)
	{
		if ( GetMirrored() ) return true;
		if ( ( GetGroupMgr().GetNumUserComps() != 1 ) || ( GetCompMode() == COMPSMODE::OFF ) ) return true;
		const Component& comp = GetUserComponent();
		return !comp.CanStretch(bGrow);
	}
	bool GetDisableStretchWidth(bool bGrow)
	{
		if ( GetMirrored() ) return true;
		if ( ( GetGroupMgr().GetNumUserComps() != 1 ) || ( GetCompMode() == COMPSMODE::OFF ) ) return true;
		const Component& comp = GetUserComponent();
		return !comp.CanStretchWidth(bGrow);
	}
	bool GetDisableChangeType()
	{
		if ( GetMirrored() ) return true;
		if ( ( GetGroupMgr().GetNumUserComps() != 1 ) || ( GetCompMode() == COMPSMODE::OFF ) ) return true;
		const Component& comp	= GetUserComponent();
		const COMP		 eType	= comp.GetType();
		return !AllowTypeChange(eType, eType);
	}
	bool GetDisableWipe() const
	{
		return GetTrackMode() == TRACKMODE::OFF;
	}
	// Merge interface functions
	virtual void UpdateMergeOffsets(MergeOffsets& o) override
	{
		ElementGrid::UpdateMergeOffsets(o);	// Call UpdateMergeOffsets in base class
		GuiControl::UpdateMergeOffsets(o);	// Call UpdateMergeOffsets in base class
		m_compMgr.UpdateMergeOffsets(o);
		m_groupMgr.UpdateMergeOffsets(o);
		m_textMgr.UpdateMergeOffsets(o);
	}
	virtual void ApplyMergeOffsets(const MergeOffsets& o) override	// Called on the source board
	{
		ElementGrid::ApplyMergeOffsets(o);	// Call ApplyMergeOffsets in base class
		GuiControl::ApplyMergeOffsets(o);	// Call ApplyMergeOffsets in base class
		m_compMgr.ApplyMergeOffsets(o);
		m_groupMgr.ApplyMergeOffsets(o);
		m_textMgr.ApplyMergeOffsets(o);
	}
	void Merge(Board& src)
	{
		GetRectMgr().Clear();	// Wipe rect manager

		Crop(GetCropMargin(), 0);	// Crop this board (the destination board) with zero margin for cols
		src.Crop(0, 0);				// Crop the source board (the one to merge in) with zero margins

		// Get merge offsets from this board
		MergeOffsets o;
		UpdateMergeOffsets(o);

		// Grow this board to make room for the merge
		Grow(src.GetRows()+1, std::max(0, src.GetCols() - GetCols()));
		GlueNbrs();	// Set pointers between neighbouring grid elements

		// Apply offsets to the source board, so we can merge it with no conflicts
		src.ApplyMergeOffsets(o);

		// Now merge ...
		ElementGrid::Merge(src, o);	// Call Merge in base class
		GuiControl::Merge(src);		// Call Merge in base class

		// Merge the components.  Need all component locations before calling GlueWires()
		m_compMgr.Merge(src.m_compMgr);
		for (const auto& mapObj : m_compMgr.GetMapIdToComp())
		{
			const Component& comp = mapObj.second;
			if ( comp.GetId() >= o.deltaCompId )	// Only want to add the new components
				m_nodeInfoMgr.AddComp(comp);
		}

		GlueWires();	// Set pointers between wired grid elements

		// Merge the group info
		m_groupMgr.Merge(src.m_groupMgr);
		RebuildAdjacencies();	// Slow

		// Merge the text label
		m_textMgr.Merge(src.m_textMgr);

		Crop();	// Final crop (with default margin)
	}

	// Persist functions
	virtual void Load(DataStream& inStream) override
	{
		Clear();

		int iVrtVersion(0);
		inStream.Load(iVrtVersion);
		inStream.SetVersion(iVrtVersion);
		inStream.SetOK(iVrtVersion <= VRT_VERSION_CURRENT);
		if ( !inStream.GetOK() ) return;	// Unsupported VRT version

		inStream.Load(m_infoStr);

		GuiControl::Load(inStream);			// Call Load() on base class
		ElementGrid::Load(inStream);		// Call Load() on base class

		GlueNbrs();		// Set pointers between neighbouring grid elements

		// Need all component locations before calling GlueWires()
		m_compMgr.Load(inStream);			// Call Load() on component manager

		for (const auto& mapObj : m_compMgr.GetMapIdToComp())
		{
			const Component& comp = mapObj.second;
			m_nodeInfoMgr.AddComp(comp);
		}

		GlueWires();	// Set pointers between wired grid elements

		m_groupMgr.Load(inStream);			// Call Load() on group manager

		if ( inStream.GetVersion() >= VRT_VERSION_10 )
			m_rectMgr.Load(inStream);		// Call Load() on rect manager

		if ( inStream.GetVersion() >= VRT_VERSION_14 )
			m_textMgr.Load(inStream);		// Call Load() on text manager

		if ( inStream.GetVersion() >= VRT_VERSION_23 )
			m_compDefiner.Load(inStream);	// Call Load() on component definer

		RebuildAdjacencies();
	}

	virtual void Save(DataStream& outStream) override
	{
		outStream.Save(VRT_VERSION_CURRENT);
		outStream.Save(m_infoStr);
		GuiControl::Save(outStream);	// Call Save() on base class
		ElementGrid::Save(outStream);	// Call Save() on base class
		m_compMgr.Save(outStream);		// Call Save() on the component manager
		m_groupMgr.Save(outStream);		// Call Save() on the group manager
		m_rectMgr.Save(outStream);		// Call Save() on rect manager			// Added in VRT_VERSION_10
		m_textMgr.Save(outStream);		// Call Save() on text manager			// Added in VRT_VERSION_14
		m_compDefiner.Save(outStream);	// Call Save() on component definer		// Added in VRT_VERSION_23
	}

	bool Import(const TemplateManager& templateMgr, const std::string& filename, std::string& errorStr)	// Import Protel V1 / Tango netlist (exported from TinyCAD / gEDA)
	{
		Clear();

		std::string			nameStr;	// "R23","U1"			==> TinyCAD "Ref"     / gEDA "refdes"
		std::string			valueStr;	// "4k7", "TL072"		==> TinyCAD "Name"    / gEDA "device"
		std::string			typeStr;	// "RESISTOR","DIP8"	==> TinyCAD "Package" / gEDA "footprint"
		std::string			typeStrCut;	// Cut down version of typeStr. e.g.  DIP40 ==> DIP
		std::string			pinStr;		// Number of pins, or pin number
		std::string			netStr;		// Net name
		std::vector<int>	nodeList;

		// List of package identifiers for footprints with variable numbers of pins/lengths.
		// "PADS" ==> Create separate on-board PAD objects for an off-board part.
		// "SWITCH_ST_DIP" must be tested before "SWITCH_ST_DIP".

		//TODO Need to add STRIP_100, BLOCK_100, BLOCK_200 to the variable size set as they only support 2 pins by default

		const std::string strVar[10] = {"SIP", "DIP", "PADS", "SWITCH_ST_DIP", "SWITCH_ST", "SWITCH_DT", "RESISTOR", "DIODE", "CAP_CERAMIC", "CAP_FILM"};

		std::ifstream inStream;
		inStream.open(filename.c_str(), std::ios::in | std::ios::binary);
		bool bOK = inStream.is_open();
		bool bPart(false), bNet(false);	// Flags indicating "part" and "netlist" sections
		int iRow(0);					// Row counter within "part" and "netlist" sections
		int iNodeId(BAD_NODEID);		// Increase this with each imported node

		std::list<std::string> offBoard;	// List of off-board part names
		while( bOK )	// Loop through file
		{
			if ( inStream.eof() ) break;

			std::string str;							// For reading from file.  Ensure clear before reading
			StringHelper::getline_safe(inStream, str);	// Read the whole line and handle line-ending nicely

			if ( str == "[" ) { bOK = !bPart && !bNet;	bPart = true;	iRow = 0;	continue; }
			if ( str == "]" ) { bOK =  bPart && !bNet;	bPart = false;				continue; }
			if ( str == "(" ) { bOK = !bPart && !bNet;	bNet  = true;	iRow = 0; 	continue; }
			if ( str == ")" ) { bOK = !bPart &&  bNet;	bNet  = false;				continue; }

			if ( bPart )	// We're in a "Part" description section
			{
				if ( iRow == 0 )
				{
					nameStr = str;	// TinyCAD "Ref" / gEDA "refdes"		==> VeroRoute "Name"
					if ( bOK )
					{
						bOK  = ( str.find("-") == std::string::npos );	// Name should not have a "-"
						if ( !bOK ) errorStr = "Part section: " + nameStr + "\nPart names must not contain a minus sign";
					}
					if ( bOK )
					{
						bOK = ( m_compMgr.GetComponentIdFromName(nameStr) == BAD_COMPID );	// Name must be unique
						if ( !bOK ) errorStr = "Part section: " + nameStr + "\nPart names must be unique";
					}
				}
				if ( iRow == 1 )
				{
					typeStrCut = typeStr = str;	// TinyCAD "Package" / gEDA "footprint"	==> VeroRoute "Type"
				}
				if ( iRow == 2 )
				{
					valueStr = str;	// TinyCAD "Name" / gEDA "device"		==> VeroRoute "Value"

					// Build the part and place it ...

					// If footprint is variable length, then get the number of pins/length from typeStr
					int numPins(0), nLength(0);	// Invalid by default
					for (int i = 0; i < 10; i++)
					{
						const std::string&	strTmp	= strVar[i];	// e.g. "SIP", "DIP, etc
						const auto			L		= strTmp.length();
						if ( typeStr.length() >= L && typeStr.substr(0, L) == strTmp )
						{
							pinStr		= typeStr.substr(L);	// e.g. "DIP40" ==> "40"
							typeStrCut	= typeStr.substr(0, L);	// e.g. "DIP40" ==> "DIP"
							if ( typeStrCut == "PADS" )				// If we have an off-board part ...
							{
								typeStrCut = "SIP";					// ... treat it as a SIP for the moment
								offBoard.push_back(nameStr);		// ... and add it to the list of off-board parts
							}
							if ( typeStrCut == "RESISTOR" || typeStrCut == "DIODE" || typeStrCut == "CAP_CERAMIC" || typeStrCut == "CAP_FILM" )
							{
								nLength = atoi( pinStr.c_str() );	// Missing or zero ==> Use default length
								if ( nLength > 0 )					// The length is in 100ths of a mil ...
									nLength += 1;					// ... so must add 1 to get part length in grid squares
							}
							else	// DIP/SIP/SWITCH
							{
								numPins = atoi( pinStr.c_str() );
								if ( numPins == 0 )					// Missing or zero ...
									numPins = -1;					// ... use -1 instead.  Don't use 0 as that implies "use default".
							}
							break;
						}
					}

					bool bCustom(false);	// true ==> We've found a custom template with matching import string
					Component custom;		// The matching custom template

					const COMP eType = GetTypeFromImportStr(typeStrCut);
					if ( bOK )
					{
						bOK = ( eType != COMP::CUSTOM && eType != COMP::TRACKS && eType != COMP::INVALID );
						if ( !bOK )	// Search template manager
							bOK = bCustom = templateMgr.GetFromImportStr(typeStrCut, custom);
						if ( !bOK ) errorStr = "Part section: " + nameStr + "\nVeroRoute does not support the part type: " + typeStr;
					}

					// Check pins per component is within limits
					if ( numPins == 0 ) numPins = ( bCustom ) ? (int) custom.GetNumPins() : GetDefaultNumPins(eType);
					if ( bOK )
					{
						bOK = ( numPins > 0 );
						if ( !bOK ) errorStr = "Part section: " + nameStr + "\nInternal error: Part type has no pins " + typeStr;
					}
					// Check length is within limits for compoennts with fixed numbers of pins
					if ( nLength > 0 )
					{
						if ( bOK )
						{
							bOK = bCustom || ( nLength >= GetMinLength(eType) );
							if ( !bOK ) errorStr = "Part section: " + nameStr + "\nInternal error: Part length is too small " + typeStr;
						}
						if ( bOK )
						{
							bOK = bCustom || ( nLength <= GetMaxLength(eType) );
							if ( !bOK ) errorStr = "Part section: " + nameStr + "\nInternal error: Part length is too large " + typeStr;
						}
					}
					if ( bOK )
					{
						bOK = bCustom || ( numPins >= GetMinNumPins(eType) );
						if ( !bOK ) errorStr = "Part section: " + nameStr + "\nPart type has fewer pins than VeroRoute supports: " + typeStr;
					}
					if ( bOK )
					{
						bOK = bCustom || ( numPins <= GetMaxNumPins(eType) );
						if ( !bOK ) errorStr = "Part section: " + nameStr + "\nPart type has more pins than VeroRoute supports: " + typeStr;
					}
					if ( bOK )
					{
						if ( bCustom )
						{
							assert(custom.GetType() == COMP::CUSTOM);
							custom.SetNameStr(nameStr);
							custom.SetValueStr(valueStr);
							bOK = ( AddComponent(nullptr, custom) != BAD_COMPID );	// Create part and place it

						}
						else
						{
							nodeList.resize(numPins, BAD_NODEID);
							Component tmp(nameStr, valueStr, eType, nodeList);
							if ( nLength > 0 )
							{
								while ( tmp.GetCols() < nLength ) tmp.Stretch(true);	// grow
								while ( tmp.GetCols() > nLength ) tmp.Stretch(false);	// shrink
							}
							bOK = ( AddComponent(nullptr, tmp) != BAD_COMPID );	// Create part and place it
						}
						if ( !bOK ) errorStr = "Part section: " + nameStr + "\nInternal error creating and placing the part";
					}
				}
			}
			if ( bNet )	// We're in a "Netlist" description section
			{
				if ( iRow == 0 )
				{
					netStr = str;
					iNodeId++;	// str has the net name, so make a new NodeID for it
				}
				else
				{
					// The line should have a part identifier and pin number separated by a '-'.
					const auto pos = str.find("-");
					if ( bOK )
					{
						bOK  = ( pos != std::string::npos );	// Should have a "-" on the line
						if ( !bOK ) errorStr = "Net section: " + netStr + "\nLine has no minus sign: " + str;
					}
					if ( bOK)
					{
						pinStr = str.substr(pos+1);
						bOK = ( pinStr.find("-") == std::string::npos );	// Should only have one "-" on the line
						if ( !bOK )	errorStr = "Net section: " + netStr + "\nLine has more than one minus sign: " + str;
					}
					const auto nameStr = ( bOK ) ? str.substr(0, pos) : std::string("");
					if ( bOK )
					{
						bOK = !StringHelper::IsEmptyStr(nameStr);	// Should have part name
						if ( !bOK ) errorStr = "Net section: " + netStr + "\nLine has no part identifier: " + str;
					}
					if ( bOK )
					{
						// Paint the component pin using SetNodeIdByUser
						const size_t	iPinIndex	= atoi(pinStr.c_str()) - 1;
						const int		compId		= m_compMgr.GetComponentIdFromName(nameStr);

						bOK = compId != BAD_COMPID;
						if ( !bOK ) errorStr = "Net section: " + netStr + "\nLine has unknown part identifier: " + str;

						if ( bOK )
						{
							bOK = iPinIndex < m_compMgr.GetComponentById(compId).GetNumPins();
							if ( !bOK ) errorStr = "Net section: " + netStr + "\nLine has invalid pin number: " + str;
						}
						if ( bOK )
						{
							int row, col;
							bOK = GetPinRowCol(compId, iPinIndex, row, col);
							if ( !bOK )	errorStr = "Net section: " + netStr + "\nLine: " + str + "\nInternal error mapping the pin to a board location";
							if ( bOK )
								SetNodeIdByUser(row, col, iNodeId, true);	// true ==> paint pins
						}
					}
				}
			}
			iRow++;
		}
		if ( inStream.is_open() ) inStream.close();

		// Break the SIPS representing off-board parts into PADs
		if ( bOK )
			for (const auto& nameStr : offBoard)
				BreakComponentIntoPads( m_compMgr.GetComponentById( m_compMgr.GetComponentIdFromName(nameStr) ) );

		return bOK;
	}
private:
	std::string				m_infoStr;		// General info

	// Component definer
	CompDefiner				m_compDefiner;	// For defining a custom component

	// Managers
	CompManager				m_compMgr;		// Manages components on the board
	NodeInfoManager			m_nodeInfoMgr;	// Info on the components that use each nodeId
	AdjInfoManager			m_adjInfoMgr;	// Manages info on adjacencies between nodeIds (for coloring algorithm)
	GroupManager			m_groupMgr;		// Handles grouping of components for the GUI
	RectManager				m_rectMgr;		// Handles the set of user-defined rectangles
	TextManager				m_textMgr;		// Handles the set of user-defined text labels
	ColorManager			m_colorMgr;		// Handles color assignment to nodeIds

	// Routing algorithm // Don't persist or copy
	std::vector<Element*>	m_tmpVec;		// For MH calc.  For the set of nodes considered so far
	std::vector<Element*>	m_targetPins;	// Set of pins with correct nodeId
	size_t					m_tmpVecSize;	// Set of visited elements during routing and ...
	unsigned int			m_tmpMaxMH;		// ... the largest MH value in the set
	int						m_tmpNodeId;	// The node ID that were using in the MH calc
	bool					m_tmpIsRouting;	// true ==> auto-routing is in progress
};
