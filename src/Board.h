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
#include "PolygonHelper.h"
#include "ConnectionMatrix.h"

#define SMART_PAN_CAN_AUTOCROP true

class MyScrollArea;

// Board is the main algorithm class.
// Just about everything apart from the GUI rendering code is in here.
// Holds a description of the circuit and handles all manipulation & routing.

class Board : public ElementGrid, public GuiControl
{
public:
	Board(int lyrs = 1, int rows = 35, int cols = 35)
	: ElementGrid()
	, GuiControl()
	, m_infoStr("Use this box to enter a circuit description or other info")
	, m_tmpVecSize(0)
	, m_bRouteMinimal(true)
	, m_bHasVias(false)
	{
		Allocate(lyrs, rows, cols);
		GlueNbrs();		// Set pointers between neighbouring grid elements
	}

	~Board() {}

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
		m_targetPins.clear();
		m_tmpVec.clear();
		m_tmpVecSize	= 0;
		m_bRouteMinimal	= true;
		m_bHasVias = false;
		return *this;
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
			&& m_compDefiner	== o.m_compDefiner
			&& m_colorMgr		== o.m_colorMgr;
	}
	bool operator!=(const Board& o) const
	{
		return !(*this == o);
	}
	void Clear()
	{
		Allocate(GetLyrs(), GetRows(), GetCols());
		GlueNbrs();	// Set pointers between neighbouring grid elements
		SetInfoStr("Use this box to enter a circuit description or other info");
		m_compMgr.Clear();
		m_targetPins.clear();
		m_tmpVec.clear();
		m_nodeInfoMgr.DeAllocate();
		m_adjInfoMgr.DeAllocate();
		m_groupMgr.Clear();
		m_rectMgr.Clear();
		m_textMgr.Clear();
		m_compDefiner.Clear();
		m_colorMgr.Clear();
		GuiControl::Clear();	// Clear() base class
	}

	void GlueNbrs()	// Set pointers between neighbouring grid elements
	{
		for (int iLyr = 0, iLyrs = GetLyrs(); iLyr < iLyrs; iLyr++)
		for (int iRow = 0, iRows = GetRows(); iRow < iRows; iRow++)
		{
			const int iT(iRow-1), iB(iRow+1);
			for (int iCol = 0, iCols = GetCols(); iCol < iCols; iCol++)
			{
				const int iL(iCol-1), iR(iCol+1);

				Element* p = Get(iLyr, iRow, iCol);
				p->SetNbr(NBR_L,	Get(iLyr, iRow, iL));
				p->SetNbr(NBR_LT,	Get(iLyr, iT,   iL));
				p->SetNbr(NBR_T,	Get(iLyr, iT,   iCol));
				p->SetNbr(NBR_RT,	Get(iLyr, iT,   iR));
				p->SetNbr(NBR_R,	Get(iLyr, iRow, iR));
				p->SetNbr(NBR_RB,	Get(iLyr, iB,   iR));
				p->SetNbr(NBR_B,	Get(iLyr, iB,   iCol));
				p->SetNbr(NBR_LB,	Get(iLyr, iB,   iL));
				p->SetNbr(NBR_X, iLyrs == 1 ? nullptr : Get((iLyr + 1 ) % 2, iRow, iCol) );

				p->ClearWires();	// Wires must be set by GlueWires()

				// Prevent toroidal routing at board edges
				int okDirs = CODEBITS_ALL;	// All neighbour directions OK by default
				if ( iRow == 0 )			{ ClearCodeBit(NBR_LT, okDirs); ClearCodeBit(NBR_T, okDirs); ClearCodeBit(NBR_RT, okDirs); }
				if ( iRow == GetRows()-1 )	{ ClearCodeBit(NBR_LB, okDirs); ClearCodeBit(NBR_B, okDirs); ClearCodeBit(NBR_RB, okDirs); }
				if ( iCol == 0 )			{ ClearCodeBit(NBR_LT, okDirs); ClearCodeBit(NBR_L, okDirs); ClearCodeBit(NBR_LB, okDirs); }
				if ( iCol == GetCols()-1 )	{ ClearCodeBit(NBR_RT, okDirs); ClearCodeBit(NBR_R, okDirs); ClearCodeBit(NBR_RB, okDirs); }
				p->SetRoutable(okDirs);
			}
		}
	}

	bool ToggleLyrPref(int iLyr, int iRow, int iCol, bool bReset)
	{
		Element* p = Get(iLyr, iRow, iCol);
		if ( !p->GetHasPin() || p->GetHasWire() ) return false;	// Want pins only.  Hole sharing wires could contradict each other, so ignore them

		const int&		compId		= p->GetCompId();	assert(compId != BAD_COMPID);
		const size_t	pinIndex	= p->GetPinIndex();	assert(pinIndex != BAD_PININDEX);
		Component&		comp		= m_compMgr.GetComponentById(compId);
		const uchar		oldPref		= comp.GetLayerPref(pinIndex);
		uchar			newPref(LAYER_X);
		if ( !bReset )
		{
			switch( oldPref )
			{
				case LAYER_X:	newPref = ( iLyr == 0 ) ? LAYER_B : LAYER_T;	break;
				case LAYER_B:	newPref = LAYER_T;	break;
				case LAYER_T:	newPref = LAYER_B;	break;
			}
		}
		const bool bChanged = ( newPref != oldPref );
		if ( bChanged ) comp.SetLayerPref(pinIndex, newPref);
		return bChanged;
	}

	int GetPerimeterCode(const Element* p)	// Helper for the GUI "blobs"
	{
		const bool	bDiagsOK	= GetDiagsMode() != DIAGSMODE::OFF;
		const bool	bMinDiags	= GetDiagsMode() == DIAGSMODE::MIN;

		int iCode = p->GetPerimeterCode(bDiagsOK, bMinDiags);	// 0 to 255

		// For a 2-layer board, modify perimeter code to handle pin layer preferences
		if ( GetLyrs() == 1 || !p->GetHasPin() || p->GetHasWire() ) return iCode;

		const bool		bBottomLayer	= p->IsLayer0();	// true ==> p is on bottom layer
		const int&		compId			= p->GetCompId();		assert(compId != BAD_COMPID);
		const size_t	pinIndex		= p->GetPinIndex();		assert(pinIndex != BAD_PININDEX);
		const int		iLayerPrefP		= m_compMgr.GetComponentById(compId).GetLayerPref(pinIndex);

		for (int iNbr = 0; iNbr < 8; iNbr ++)
		{
			if ( !ReadCodeBit(iNbr, iCode) ) continue;

			const Element* q = p->GetNbr(iNbr);
			if ( !q->GetHasPin() || q->GetHasWire() ) continue;

			const int&		compId		= q->GetCompId();		assert(compId != BAD_COMPID);
			const size_t	pinIndex	= q->GetPinIndex();		assert(pinIndex != BAD_PININDEX);
			const int		iLayerPrefQ	= m_compMgr.GetComponentById(compId).GetLayerPref(pinIndex);

			const bool bOK = ( iLayerPrefP == LAYER_X && iLayerPrefQ == LAYER_X ) ||
							 ( bBottomLayer ? ( iLayerPrefP == LAYER_B || iLayerPrefQ == LAYER_B )
											: ( iLayerPrefP == LAYER_T || iLayerPrefQ == LAYER_T ) );
			if ( !bOK ) ClearCodeBit(iNbr, iCode);
		}
		return iCode;
	}

	void GlueWires()	// Set pointers between wired grid elements
	{
		for (const auto& mapObj : m_compMgr.GetMapIdToComp())	// Iterate components
		{
			const int&			compId	= mapObj.first;
			const Component&	comp	= mapObj.second;
			if ( comp.GetType() == COMP::WIRE && comp.GetIsPlaced() )
			{
				const int& lyr  = comp.GetLyr();	assert( lyr == 0 );
				const int& rowA = comp.GetRow();
				const int& colA = comp.GetCol();
				const int  rowB = comp.GetLastRow();
				const int  colB = comp.GetLastCol();

				Element* pA = Get(lyr, rowA, colA);	assert( pA->GetNumWires() < 2 );
				Element* pB = Get(lyr, rowB, colB);	assert( pB->GetNumWires() < 2 );

				assert( pA->GetNumCompIds() > 0 && pA->GetNumCompIds() < 3 );
				assert( pB->GetNumCompIds() > 0 && pB->GetNumCompIds() < 3 );
				assert( pB->GetNodeId() == pA->GetNodeId() );	// Wire ends must have same NodeId

				const int iSlotA = pA->GetSlotFromCompId(compId);
				const int iSlotB = pB->GetSlotFromCompId(compId);
				pA->SetW( iSlotA, pB );	// Give pA a pointer to pB
				pB->SetW( iSlotB, pA );	// Give pB a pointer to pA				
			}
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

		GrowThenPan(0, incRows, incCols, iDown, iRight);
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

			return GrowThenPan(0, incRows, incCols, 0, 0);
		}

		if ( minRow + iDown < 0 )								// Too far up ...
		{
			if ( SMART_PAN_CAN_AUTOCROP )
				incRows = std::min(0, maxRow + 1 - GetRows());	// ... so shrink grid from bottom
			//else
			//	incRows	= -(minRow + iDown);					// ... so grow grid from bottom
			iDown	= -minRow;									// ... and pan to very top
		}
		else if ( maxRow + iDown >= GetRows() )					// Too far down ...
			incRows	= 1 + maxRow + iDown - GetRows();			// ...so grow grid from bottom

		if ( minCol + iRight < 0 )								// Too far left ...
		{
			if ( SMART_PAN_CAN_AUTOCROP )
				incCols	= std::min(0, maxCol + 1 - GetCols());	// ... so shrink grid from right
			//else
			//	incCols	= -(minCol + iRight);					// ... so grow grid from right
			iRight	= -minCol;									// ... and pan to very left
		}
		else if ( maxCol + iRight >= GetCols() )				// Too far right ...
			incCols	= 1 + maxCol + iRight - GetCols();			// ... so grow grid from right

		GrowThenPan(0, incRows, incCols, iDown, iRight);
	}

	bool Crop(int iRowMargin = -1, int iColMargin = -1)	// Moves whole circuit to the top-left, then crops the grid from the bottom-right, then adds margin
	{
		int minRow, minCol, maxRow, maxCol;
		const bool bOK = GetBounds(minRow, minCol, maxRow, maxCol);	// Get the circuit bounds
		if ( !bOK ) return false;

		Pan(-minRow, -minCol);	// First pan to top-left corner of grid
		GrowThenPan(0, maxRow - minRow + 1 - GetRows(), maxCol - minCol + 1 - GetCols(), 0, 0); // Then shrink grid from bottom-right

		if ( iRowMargin == -1 ) iRowMargin = GetCropMargin();	// -1 ==> use default
		if ( iColMargin == -1 ) iColMargin = GetCropMargin();	// -1 ==> use defaul
		if ( iRowMargin > 0 || iColMargin > 0 )
		{
			const int incRows = 2 * iRowMargin;
			const int incCols = 2 * iColMargin;
			GrowThenPan(0, incRows, incCols, iRowMargin, iColMargin);
		}
		return true;
	}

	void GrowThenPan(const int& incLyrs, const int& incRows, const int& incCols, const int& iDown, const int& iRight)
	{
		ElementGrid::Grow(incLyrs, incRows, incCols);	// Grow the base class
		ElementGrid::Pan(iDown, iRight);				// Pan the base class

		GlueNbrs();		// Set pointers between neighbouring grid elements

		// Need all component locations before calling GlueWires()
		for (auto& mapObj : m_compMgr.m_mapIdToComp)
		{
			Component& comp	= mapObj.second;
			int newRow = comp.GetRow() + iDown;
			int newCol = comp.GetCol() + iRight;

			MakeToroid(newRow, newCol);	// Make co-ordinates wrap around at grid edges

			comp.SetRow(newRow);
			comp.SetCol(newCol);
		}
		Component& trax = m_compMgr.GetTrax();
		if ( trax.GetSize() > 0 )	// If have a trax pattern
		{
			int newRow = trax.GetRow() + iDown;
			int newCol = trax.GetCol() + iRight;

			MakeToroid(newRow, newCol);	// Make co-ordinates wrap around at grid edges

			trax.SetRow(newRow);
			trax.SetCol(newCol);
		}

		GlueWires();	// Set pointers between wired grid elements

		// Move all user-defined rectangles
		GetRectMgr().MoveAll(iDown, iRight);

		// Move all user-defined text
		GetTextMgr().MoveAll(iDown, iRight);

		if ( incLyrs > 0 )	// If we added a layer ...
		{
			// ... do TakeOff() and PutDown() for all placed parts
			// ... so new layer is set correctly
			for (auto& mapObj : m_compMgr.m_mapIdToComp)
			{
				Component& comp	= mapObj.second;
				if ( !comp.GetIsPlaced() ) continue;
				TakeOff(comp);
				PutDown(comp);
			}
		}

		// Move any warning points
		for (auto& o : m_warnPoints[0])	o += QPointF(iRight, iDown);
		for (auto& o : m_warnPoints[1])	o += QPointF(iRight, iDown);
	}

	bool GetBounds(int& minRow, int& minCol, int& maxRow, int& maxCol) const
	{
		bool bOK(false);
		// First consider all painted nodeIds on the board
		const int numLyrs( GetLyrs() ), numRows( GetRows() ), numCols( GetCols() );
		minRow = numRows - 1;	maxRow = 0;	// Start with min and max at the wrong ends
		minCol = numCols - 1;	maxCol = 0;	// Start with min and max at the wrong ends
		for (int k = 0; k < numLyrs; k++)
		for (int j = 0; j < numRows; j++)
		for (int i = 0; i < numCols; i++)
		{
			if ( Get(k,j,i)->GetNodeId() == BAD_NODEID && !Get(k,j,i)->GetHasPin() ) continue;
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

	void RebuildAdjacencies()
	{
		m_adjInfoMgr.DeAllocate();
		for (int i = 0, iSize = GetSize(); i < iSize; i++) m_adjInfoMgr.InitCounts( GetAt(i) );
	}

	Component& GetUserComponent()	// The currently selected component
	{
		assert( m_groupMgr.GetNumUserComps() == 1 );	// Should only have one component selected
		Component& comp = m_compMgr.GetComponentById( m_groupMgr.GetUserCompId() );
		assert( comp.GetType() != COMP::INVALID );
		return comp;
	}

	// Methods to get objects at a grid location
	int  GetComponentId(int row, int col);	// Pick the most relevant component at the location
	int  GetTextId(int row, int col);		// Pick the most relevant text box at the location

	// Methods to handle variable pad/hole and PCB tolerances
	void	GetPadWidths_MIL(std::list<int>& o, int& iDefaultWidth) const;
	void	GetHoleWidths_MIL(std::list<int>& o, int& iDefaultWidth) const;
	void	GetSeparations(double& minTrackSeparation_mil, double& minGroundFill_mil);
	void	CalcMIN_SEPARATION();	// Sets m_dMinSeparation and m_warnPoints[]
	void	CalcGroundFillBounds();
	void	GetGroundFillBounds(int& L, int& R, int& T, int& B) const;

	// Methods to paint/unpaint nodeIds
	void SetNodeId(Element* p, const int& nodeId, const bool bAllLyrs);	// Helper to make sure we do UpdateCounts() before painting an element
	void WipeFlagBits(Element* p, const char& i, const bool bAllLyrs);
	void MarkFlagBits(Element* p, const char& i, const bool bAllLyrs);
	bool SetNodeIdByUser(const int& lyr, const int& row, const int& col, const int& nodeId, const bool& bPaintPins);
	void FloodNodeId(const int& nodeId);
	void AutoFillVero();
	void CalcSolder();	// Work out locations of solder blobs to join veroboard tracks together
	void SetSolder(const int& nodeId, const int& col, const bool& bVertical);

	// Routing methods
	void WipeAutoSetPoints(const int nodeId = BAD_NODEID);
	void BuildTargetPins(const int& nodeId);
	void Route(bool bMinimal);
	void UpdateVias();
	const bool& GetHasVias() const { return m_bHasVias; }
	unsigned int Flood(const int& nodeId);
	unsigned int Flood();
	void Flood_Helper(const bool bBuildTracks);
	void Flood_Grow(const int& iFloodNodeId, Element* pJ, const int& iNbr, const bool& bBuildTracks, unsigned int& iMH, unsigned int& iMaxMH, bool& bDone);
	void Backtrace(Element* pEnd, const int& nodeId);
	bool BacktraceHelper(Element*& p, unsigned int& MH, const int& nodeId, const int& iDeltaMH, const int& iNbr, const int& iLoop);
	void Manhatten(Element* p);
	void ManhattenHelper(const Element* p, const int& iNbr, const unsigned int& iRouteID, unsigned int& iMH, unsigned int& iMaxMH);
	void CheckAllComplete();
	void PasteTracks(bool bTidy);
	void WipeTracks();

	// Methods for component creation/destruction
	void DestroyComponent(Component& comp);	// Destroys a component on the board
	int  CreateComponent(int iRow, int iCol, const COMP& eType, const Component* pComp = nullptr);
	int  AddComponent(int iRow, int iCol, const Component& tmp, bool bDoPlace = true);
	void AddTextBox(int iRow, int iCol);

	// Methods for component placement/removal
	bool CanPutDown(Component& comp);	// Checks if its possible to place the (floating) component on the board
	bool PutDown(Component& comp);		// Tries to place the (floating) component on the board
	bool TakeOff(Component& comp);
	void FloatAllComps();				// Float all components (i.e. take them off the board)
	void PlaceFloaters();				// Try to place down all the floating components
	void FixCorruption();

	// GUI helpers for manipulating user-selected components
	void SelectAllComps(bool bRestrictToRects);
	bool ConfirmDestroyUserComps();	// returns false if user-group is empty or has only wires and markers
	void DestroyUserComps();		// Destroy components in the user-group
	void MoveUserCompText(const int& deltaRow, const int& deltaCol);	// Move text label
	void StretchUserComp(const bool& bGrow);		// Stretch the selected component length
	void StretchWidthUserComp(const bool& bGrow);	// Stretch the selected component width (just for DIPs)
	void ChangeTypeUserComp(const COMP& eType);
	void CopyUserComps();											// Make a blank copy of the user-group components and float them
	bool MoveUserComps(const int& deltaRow, const int& deltaCol);	// Move user-group components, and return true if the grid was panned
	void RotateUserComps(const bool& bCW);							// Rotate the selected components
	void CopyComps(const std::list<int>& compIds);					// Make a blank copy of the components and float them
	bool MoveTextBox(const int& deltaRow, const int& deltaCol);		// Move text box, and return true if the grid was panned
	bool MoveComps(const std::list<int>& compIds, const int& deltaRow, const int& deltaCol);	// Move components and return true if the grid was panned
	void RotateComps(const std::list<int>& compIds, const bool& bCW);	// Rotate components
	Rect GetFootprintBounds(const std::list<int>& compIds);
	void CustomPCBshapes();	// Allow some parts (e.g. DIPs) to be drawn differently in PCB mode

	// Command enablers for GUI
	bool GetDisableCompText();
	bool GetDisableMove();
	bool GetDisableRotate();
	bool GetDisableStretch(bool bGrow);
	bool GetDisableStretchWidth(bool bGrow);
	bool GetDisableChangeType();
	bool GetDisableChangeCustom();
	bool GetDisableWipe() const;

	void				SetInfoStr(const std::string& str)	{ m_infoStr = str; }
	const std::string&	GetInfoStr() const	{ return m_infoStr; }
	void				CalculateColors()	{ m_colorMgr.CalculateColors(m_adjInfoMgr, this); }
	int					GetNewNodeId()
	{
		const int nodeId = m_nodeInfoMgr.GetNewNodeId(m_adjInfoMgr);
		m_colorMgr.Unfix(nodeId);	// Ensure auto-color is used for new nodeId.
		return nodeId;
	}
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

	// Helper for flying wires
	bool GetAllowFlyWire(Element* p) const
	{
		return p->GetNodeId() != BAD_NODEID && !p->GetHasWire() && p->IsLayer0() && m_compMgr.GetAllowFlyWire(p->GetCompId());
	}

	// Helpers for locations of close tracks
	std::list<QPointF>& GetWarnPoints(int iLayer)	{ return m_warnPoints[iLayer]; }
	void ClearWarnPoints()							{ m_warnPoints[0].clear(); m_warnPoints[1].clear(); }
	bool GetHaveWarnPoints() const					{ return !m_warnPoints[0].empty() || !m_warnPoints[1].empty(); }

	// Import Protel V1 / Tango netlist (exported from TinyCAD / gEDA)
	bool ImportTango(const TemplateManager& templateMgr, const std::string& filename, std::string& errorStr);
	// Import OrcadPCB2 netlist (exported from KiCAD)
	bool ImportOrcad(const TemplateManager& templateMgr, const std::string& filename, std::string& errorStr);
	bool BreakComponentIntoPads(Component& comp);
	bool GetPinRowCol(const int& compId, const size_t& iPinIndex, int& row, int& col) const;

	// Merge interface functions
	virtual void UpdateMergeOffsets(MergeOffsets& o) override
	{
		ElementGrid::UpdateMergeOffsets(o);	// Call UpdateMergeOffsets in base class
		GuiControl::UpdateMergeOffsets(o);	// Call UpdateMergeOffsets in base class
		m_compMgr.UpdateMergeOffsets(o);
		m_groupMgr.UpdateMergeOffsets(o);
		m_textMgr.UpdateMergeOffsets(o);
		m_colorMgr.UpdateMergeOffsets(o);
	}
	virtual void ApplyMergeOffsets(const MergeOffsets& o) override	// Called on the source board
	{
		ElementGrid::ApplyMergeOffsets(o);	// Call ApplyMergeOffsets in base class
		GuiControl::ApplyMergeOffsets(o);	// Call ApplyMergeOffsets in base class
		m_compMgr.ApplyMergeOffsets(o);
		m_groupMgr.ApplyMergeOffsets(o);
		m_textMgr.ApplyMergeOffsets(o);
		m_colorMgr.ApplyMergeOffsets(o);
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
		Grow(std::max(0, src.GetLyrs() - GetLyrs()),
			 src.GetRows()+1,
			 std::max(0, src.GetCols() - GetCols()));
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

		// Merge the colors
		m_colorMgr.Merge(src.m_colorMgr);

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
		CustomPCBshapes();					// Some parts (e.g. DIPs) are drawn differently in PCB mode

		for (const auto& mapObj : m_compMgr.GetMapIdToComp())
			m_nodeInfoMgr.AddComp(mapObj.second);
		
		GlueWires();	// Set pointers between wired grid elements

		if ( inStream.GetVersion() < VRT_VERSION_26 )
			FixLegacyWires();

		m_groupMgr.Load(inStream);			// Call Load() on group manager

		if ( inStream.GetVersion() >= VRT_VERSION_10 )
			m_rectMgr.Load(inStream);		// Call Load() on rect manager

		if ( inStream.GetVersion() >= VRT_VERSION_14 )
			m_textMgr.Load(inStream);		// Call Load() on text manager

		if ( inStream.GetVersion() >= VRT_VERSION_23 )
			m_compDefiner.Load(inStream);	// Call Load() on component definer

		if ( inStream.GetVersion() >= VRT_VERSION_41 )
			m_colorMgr.Load(inStream);		// Call Load() on color manager

		RebuildAdjacencies();

		FixCorruption();
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
		m_colorMgr.Save(outStream);		// Call Save() on color manager			// Added in VRT_VERSION_41
	}
private:
	inline void UpdateMH(Element* p, const unsigned int& iRouteID, const unsigned int& iMH, unsigned int& iMaxMH)
	{
		m_tmpVec[m_tmpVecSize++] = p;	// Add p to set of visited points
		p->UpdateMH(iRouteID, iMH, iMaxMH);
	}
	void FixLegacyWires()
	{
		// Find all placed wires and update the surface and hole codes on the board
		for (const auto& mapObj : m_compMgr.GetMapIdToComp())
		{
			const Component& comp = mapObj.second;
			if ( comp.GetType() == COMP::WIRE && comp.GetIsPlaced() )
			{
				const int& lyr = comp.GetLyr();	assert( lyr == 0 );
				int jRow( comp.GetRow() );
				for (int j = 0, jRows = comp.GetCompRows(); j < jRows; j++, jRow++)
				{
					int iCol( comp.GetCol() );
					for (int i = 0, iCols = comp.GetCompCols(); i < iCols; i++, iCol++)
					{
						Element* p = Get(lyr, jRow, iCol);
						// Want GetIsPin() methods to be private so commented out following assert
						// assert( comp.GetCompElement(j, i)->GetIsPin() == p->GetIsPin() );
						assert( p->GetSurface() == SURFACE_PLUG || p->GetSurface() == SURFACE_FULL );
						const bool bGap = ( p->GetSurface() & SURFACE_GAP ) > 0;
						p->SetOccupancy(true);	// true ==> WIRE
						if ( bGap ) p->SetSurface( p->GetSurface() + SURFACE_GAP );
					}
				}
			}
		}
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

	// Pixmap info for ground fill extent	// Don't persist or copy
	int						m_gndL = 0;		// Set by CalcGroundFillBounds()
	int						m_gndR = 0;		// Set by CalcGroundFillBounds()
	int						m_gndT = 0;		// Set by CalcGroundFillBounds()
	int						m_gndB = 0;		// Set by CalcGroundFillBounds()

	// Track warnings		// Don't persist or copy
	std::list<QPointF>		m_warnPoints[2];// For showing locations with min track separation on the 2 layers
	double					m_dMinSeparation = DBL_MAX;	// Units of grid squares

	// Routing algorithm	// Don't persist or copy
	ConnectionMatrix		m_connectionMatrix;	// Tracks connectivity between target pins
	std::vector<Element*>	m_targetPins;		// Set of pins to route.
	std::vector<Element*>	m_tmpVec;			// The set of visited points.
	size_t					m_tmpVecSize;		// The number of visited points.
	bool					m_bRouteMinimal;	// true ==> don't build tracks between pins that are already connected
	bool					m_bHasVias;			// true ==> there are routed vias in the design (as opposed to "wires-as-tracks" vias)
};
