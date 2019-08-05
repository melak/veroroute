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

#include "Board.h"
#include "myscrollarea.h"	//TODO Not nice !! The board should not have knowledge of the rendered view

// Methods for component creation/destruction

void Board::DestroyComponent(Component& comp)	// Destroys a component on the board
{
	TakeOff(comp);							// Float the component
	m_nodeInfoMgr.RemoveComp(comp);			// Remove the component NodeInfo from m_nodeInfoMgr
	m_groupMgr.RemoveComp(comp.GetId());	// Remove the component from m_groupMgr
	m_compMgr.DestroyComp(comp);			// Destroy the component in the m_compMgr
}

int Board::CreateComponent(MyScrollArea* pScrollArea, const COMP& eType, const Component* pComp)
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

int Board::AddComponent(MyScrollArea* pScrollArea, const Component& tmp, bool bDoPlace)
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

void Board::AddTextBox(MyScrollArea* pScrollArea)
{
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


// Methods for component placement/removal

bool Board::CanPutDown(Component& comp)	// Checks if its possible to place the (floating) component on the board
{
	if ( comp.GetIsPlaced() ) return false;	// Already on board

	const int&	compId			= comp.GetId();
	const int	rowTL			= comp.GetRow();
	const int	colTL			= comp.GetCol();
	const bool	bWire			= comp.GetType() == COMP::WIRE;	// Wire's only get NodeIDs while placed
	const bool	bVia			= comp.GetType() == COMP::VIA;	// Via can go anywhere without a pin
	const bool	bTrax			= comp.GetType() == COMP::TRACKS;
	const int&	compCols		= comp.GetCompCols();
	const int&	compRows		= comp.GetCompRows();
	const int&	boardCols		= GetCols();
	const int&	boardRows		= GetRows();
	const bool	bDiagsOK		= GetDiagsMode() != DIAGSMODE::OFF;
	const bool	bAllowWireCross	= GetWireCross();
	const bool	bAllowHoleShare	= GetWireShare();

	if ( bAllowHoleShare && bWire )
	{
		// pA and pB are the opposite ends of the wire
		Element* pA = Get(rowTL, colTL);						assert(pA);
		Element* pB = Get(rowTL+compRows-1, colTL+compCols-1);	assert(pB);
		assert( !pA->GetCompExists(compId) && !pB->GetCompExists(compId) );
		if ( pA->GetWireExists(pB) || pB->GetWireExists(pA) ) return false;	// No duplicates !!!
	}
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
					|| ( !pGrid->GetHasPin() && pGrid->GetNodeId() == BAD_NODEID && !pGrid->GetIsHole() );

				if ( !bOK ) // Special check for unpainted wires on the board
				{
					// If have blank wire ...
					if ( pGrid->GetHasWire() && pGrid->GetNodeId() == BAD_NODEID && !pGrid->GetIsHole() )
					{
						const CompElement* pTraxEnd(nullptr);	// The point in trax corresponding to the other wire end

						for (int iSlot = 0; iSlot < 2 && bOK; iSlot++)
						{
							Element* pW = pGrid->GetW(iSlot);
							if ( pW == nullptr ) continue;
							int jj(0), ii(0);	// row,col of wire end (w.r.t. board)
							GetRowCol(pW, jj, ii);

							const int jEndRow(j + jj - jRow), jEndCol(i + ii - iCol);	// row, col of wire end (w.r.t. comp)

							if ( jEndRow >= 0 && jEndRow < compRows && jEndCol >= 0 && jEndCol < compCols )
								pTraxEnd = comp.GetCompElement(jEndRow, jEndCol);
							bOK = pTraxEnd == nullptr || pTraxEnd->GetNodeId() == traxNodeId || pTraxEnd->GetNodeId() == BAD_NODEID;
						}
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
				const uchar& boardHoleUse	= pGrid->GetHoleUse();
				const uchar& compHoleUse	= pComp->GetHoleUse();

				// Check surface and hole use.
				// Need separate checks for SURFACE_FREE since that can be
				// added to anything including SURFACE_HOLE.
				bOK =	( boardSurface == SURFACE_FREE ) ||
						( compSurface  == SURFACE_FREE ) ||
						( boardSurface + compSurface <= SURFACE_FULL );
				bOK &=	( boardHoleUse + compHoleUse <= HOLE_FULL );
				bOK &=	( !bWire || bAllowHoleShare || ( boardHoleUse + compHoleUse <= HOLE_WIRE ) );
				bOK &=	( !bWire || bAllowWireCross || ( boardSurface <= ( bAllowHoleShare ? SURFACE_WIRE_END : SURFACE_GAP ) ) );
				if ( !bOK ) continue;

				// Check pins
				if ( bVia )	// Via can go anywhere except for pins, holes, (or other vias)
				{
					bOK = !pGrid->GetHasPin() && !pGrid->GetIsHole() && !pGrid->GetIsVia();
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

bool Board::PutDown(Component& comp)	// Tries to place the (floating) component on the board
{
	const bool bOK = CanPutDown(comp);
	if ( !bOK ) return false;

	const int&	compId		= comp.GetId();
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
				if ( !pComp->ReadFlagBits(RECTSET) ) continue;		// Skip non-rect points
				if ( pGrid->GetNodeId() != BAD_NODEID ) continue;	// Skip painted points
				// If have wire(s) ...
				if ( pGrid->GetW(0) ) blankWireIds.insert( pGrid->GetCompId()  );	// ... store its compId
				if ( pGrid->GetW(1) ) blankWireIds.insert( pGrid->GetCompId2() );	// ... store its compId
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

				const bool bBlankWire		= pGrid->GetHasWire() &&
											( blankWireIds.find( pGrid->GetCompId()  ) != blankWireIds.end() ||
											  blankWireIds.find( pGrid->GetCompId2() ) != blankWireIds.end() );
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

				// Update surface and hole use
				pGrid->SetSurface( pGrid->GetSurface() + pComp->GetSurface() );
				pGrid->SetHoleUse( pGrid->GetHoleUse() + pComp->GetHoleUse() );

				// Update IDs at pin location (No pin ==> Leave existing pinIndexes and compIds)
				const size_t pinIndex = pComp->GetPinIndex();
				if ( pinIndex == BAD_PININDEX ) continue;

				// Work out which slot to use if we have a wire
				const int iSlot = pGrid->GetFreeSlot();
				// Want GetIsPin() methods to be private so commented out following assert
				// assert( ( iSlot == 0 && !pGrid->GetIsPin() ) || ( iSlot == 1 && !pGrid->GetIsPin2() ) );

				// Store any user-painted nodeId's under the pin (i.e. "oridId") BEFORE placing
				// If we are about to place a wire in the same hole as an existing wire, then
				// inherit origId from the existing wire
				if ( bWire && pGrid->GetNumWires() == 1 )
				{
					const int iSlotOther = ( iSlot == 0 ) ? 1 : 0;
					size_t	iOtherPinIndex;
					int		iOtherCompId;
					pGrid->GetSlotInfo(iSlotOther, iOtherPinIndex, iOtherCompId);
					assert( iOtherPinIndex != BAD_PININDEX && iOtherCompId != BAD_COMPID );

					const Component& otherComp = m_compMgr.GetComponentById( iOtherCompId );
					assert( otherComp.GetType() == COMP::WIRE );
					const int& origId = otherComp.GetOrigId(iOtherPinIndex);
					comp.SetOrigId(pinIndex, origId);
				}
				else
				{
					const int origId = ( pGrid->ReadFlagBits(USERSET) ) ? pGrid->GetNodeId() : BAD_NODEID;
					comp.SetOrigId(pinIndex, origId);
				}

				pGrid->SetSlotInfo(iSlot, pinIndex, compId);

				const int& iCompNodeId = comp.GetNodeId(pinIndex);

				assert(!bWire || iCompNodeId == BAD_NODEID); // Wire shouldn't have a NodeId yet
				if ( !bWire ) { SetNodeId(pGrid, iCompNodeId); pGrid->ClearFlagBits(AUTOSET|VEROSET); pGrid->SetFlagBits(USERSET); }	// Write nodeId & flag
			}
		}
	}
	if ( bWire )	// Handle wires setting the wire ends on the board to same value
	{
		// pA and pB are the opposite ends of the wire
		Element*	pA		= Get(rowTL, colTL);						assert(pA);
		Element*	pB		= Get(rowTL+compRows-1, colTL+compCols-1);	assert(pB);
		const int	iSlotA	= pA->GetSlotFromCompId(compId);
		const int	iSlotB	= pB->GetSlotFromCompId(compId);
		const int	nodeIdA	= pA->GetNodeId();
		const int	nodeIdB	= pB->GetNodeId();

		pA->SetW(iSlotA, pB);	// Link wire ends
		pB->SetW(iSlotB, pA);	// Link wire ends

		if ( nodeIdA == BAD_NODEID ) { SetNodeId(pA, nodeIdB ); pA->SetFlagBits( pB->GetFlag() & (USERSET|AUTOSET|VEROSET) ); }	// Write nodeId & flag
		if ( nodeIdB == BAD_NODEID ) { SetNodeId(pB, nodeIdA ); pB->SetFlagBits( pA->GetFlag() & (USERSET|AUTOSET|VEROSET) ); }	// Write nodeId & flag
		assert(pA->GetNodeId() == pB->GetNodeId());	// Wire ends must have same NodeId

		const int&	nodeId	= pA->GetNodeId();
		const char& iFlag	= pA->GetFlag();

		comp.SetNodeId(0, nodeId);
		comp.SetNodeId(1, nodeId);

		WIRELIST wireList;	// Helper for chains of wires

		// Handle all connected wires.  (We just need the wirelist on one end, so use pA)
		pA->GetWireList(wireList);	// Get list containing pA and its wired points
		if ( wireList.size() > 2 )
		{
			for (auto& o : wireList)
			{
				Element* p = const_cast<Element*> (o.first);
				if ( p == pA || p == pB ) continue;	// Can skip pA and pB (we've done these already)
				if ( p->GetNodeId() == BAD_NODEID ) { SetNodeId(p, nodeId); p->SetFlagBits( iFlag & (USERSET|AUTOSET|VEROSET) ); }	// Write nodeId & flag
			}
		}
	}
	comp.SetRow(rowTL);
	comp.SetCol(colTL);
	comp.SetIsPlaced(true);
	if ( comp.GetType() == COMP::VIA ) Get(rowTL, colTL)->SetIsVia(true);	// Set via flag

	m_colorMgr.ReAssignColors();	// Forces colors to be worked out again
	return true;
}

bool Board::TakeOff(Component& comp)
{
	if ( !comp.GetIsPlaced() ) return false;	// Can't take off a component that is already floating

	const bool	bWire 		= comp.GetType() == COMP::WIRE;	// Wire's only get NodeIDs while placed
	const bool	bTrax		= comp.GetType() == COMP::TRACKS;
	const int&	compId		= comp.GetId();
	const int&	compCols	= comp.GetCompCols();
	const int&	compRows	= comp.GetCompRows();
	const int&	rowTL		= comp.GetRow();
	const int&	colTL		= comp.GetCol();

	// If we have a wire, then pA and pB are the opposite ends of the wire.
	// Find out which wire slots are used before we take off the wire.
	Element*	pA			= ( bWire ) ? Get(rowTL, colTL) : nullptr;
	Element*	pB			= ( bWire ) ? Get(rowTL+compRows-1, colTL+compCols-1) : nullptr;
	int		iSlotA(-1), iSlotB(-1), iOrigIdA(BAD_NODEID), iOrigIdB(BAD_NODEID), tmpCompId;
	size_t	iPinIndex;
	if ( pA )
	{
		iSlotA = pA->GetSlotFromCompId(compId);
		pA->GetSlotInfo(iSlotA, iPinIndex, tmpCompId);	assert(tmpCompId == compId);
		iOrigIdA = comp.GetOrigId( iPinIndex );
	}
	if ( pB )
	{
		iSlotB = pB->GetSlotFromCompId(compId) ;
		pB->GetSlotInfo(iSlotB, iPinIndex, tmpCompId);	assert(tmpCompId == compId);
		iOrigIdB = comp.GetOrigId( iPinIndex );
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
				if ( !pComp->ReadFlagBits(RECTSET) ) continue;		// Skip non-rect points
				if ( pComp->GetNodeId() == BAD_NODEID ) continue;	// Skip blank areas of the trax comp
				if ( !pGrid->ReadFlagBits(RECTSET) && ( !pGrid->GetHasPin() || pGrid->GetHasWire() ) )
					SetNodeIdByUser(jRow, iCol, BAD_NODEID, false);	// false ==> don't paint pins
				pGrid->ClearFlagBits(RECTSET);
			}
			else
			{
				assert( !pComp->GetIsHole() || pGrid->GetIsHole() );	// Component hole can only be taken off a grid hole

				// Update surface and hole use
				pGrid->SetSurface( pGrid->GetSurface() - pComp->GetSurface() );
				pGrid->SetHoleUse( pGrid->GetHoleUse() - pComp->GetHoleUse() );
				if ( pGrid->GetHoleUse() == HOLE_FREE )
				{
					pGrid->SetSlotInfo(0, BAD_PININDEX, BAD_COMPID);	// Slot 0
					pGrid->SetSlotInfo(1, BAD_PININDEX, BAD_COMPID);	// Slot 1
				}
				else if ( bWire && ( pGrid == pA || pGrid == pB ) )	// Taking off a wire-end
				{
					pGrid->SetSlotInfo( ( pGrid == pA ) ? iSlotA : iSlotB, BAD_PININDEX, BAD_COMPID);
				}

				// Update IDs at pin locations of component
				const size_t pinIndex = pComp->GetPinIndex();
				if ( pinIndex == BAD_PININDEX ) continue;

				const int origNodeId = comp.GetOrigId(pinIndex);	// Note the original NodeId ...
				comp.SetOrigId(pinIndex, BAD_NODEID);				// ... before wiping it
				assert( origNodeId == BAD_NODEID || origNodeId == comp.GetNodeId(pinIndex) );

				// Wire-ends need special treatment, so just handle non-wires here
				if ( !bWire )
				{
					SetNodeId(pGrid, origNodeId);			// Restore grid element to original nodeId
					pGrid->ClearFlagBits(AUTOSET|VEROSET);
					pGrid->SetFlagBits(USERSET);
				}
			}
		}
	}
	if ( bWire )	// Handle wires ends
	{
		comp.SetNodeId(0, BAD_NODEID);
		comp.SetNodeId(1, BAD_NODEID);
		pA->SetW(iSlotA, nullptr);	// Break pointers between board points
		pB->SetW(iSlotB, nullptr);	// Break pointers between board points

		WIRELIST wireList;	// Helper for chains of wires
		for (int iEnd = 0; iEnd < 2; iEnd++)
		{
			Element* pEnd = ( iEnd == 0 ) ? pA : pB;
			pEnd->GetWireList(wireList);	// Get list containing pEnd and its wired points

			int origId(BAD_NODEID);
			for (auto& o : wireList)	// See if any of the wires in the set have a valid origId
			{
				const Element* pW = o.first;
				for (int iSlot = 0; iSlot < 2 && origId == BAD_NODEID; iSlot++)
				{
					pW->GetSlotInfo(iSlot, iPinIndex, tmpCompId);
					if ( iPinIndex == BAD_PININDEX ) continue;
					assert(tmpCompId != BAD_COMPID);
					Component& comp = m_compMgr.GetComponentById( tmpCompId );
					assert( comp.GetType() == COMP::WIRE );
					origId = comp.GetOrigId(iPinIndex);
					assert(origId == BAD_NODEID || origId == comp.GetNodeId(iPinIndex));
				}
				if ( origId != BAD_NODEID ) break;
			}
			if ( origId == BAD_NODEID )	// If no wired points connected to pEnd have a valid origId ...
			{
				// ... wipe all their nodeIds
				for (auto& o : wireList)
				{
					Element* pW = const_cast<Element*> (o.first);
					// Wipe the nodeId's on the wire components ...
					for (int iSlot = 0; iSlot < 2; iSlot++)
					{
						pW->GetSlotInfo(iSlot, iPinIndex, tmpCompId);
						if ( iPinIndex == BAD_PININDEX ) continue;
						assert(tmpCompId != BAD_COMPID);
						Component& comp = m_compMgr.GetComponentById( tmpCompId );
						comp.SetNodeId(iPinIndex, BAD_NODEID);
					}
					// ... and on the corresponding board points
					SetNodeId(pW, BAD_NODEID);
					pW->ClearFlagBits(AUTOSET|VEROSET);
					pW->SetFlagBits(USERSET);	//PROBABLY A BUG >>>>> pW->SetFlagBits(AUTOSET|VEROSET);
				}
			}
		}
		// Finally fix up case where we had a single wire
		if ( !pA->GetHasWire() )	// If we've taken off the last wire at the location
		{
			SetNodeId(pA, iOrigIdA);
			pA->ClearFlagBits(AUTOSET|VEROSET);
			pA->SetFlagBits(USERSET);
		}
		if ( !pB->GetHasWire() )	// If we've taken off the last wire at the location
		{
			SetNodeId(pB, iOrigIdB);
			pB->ClearFlagBits(AUTOSET|VEROSET);
			pB->SetFlagBits(USERSET);
		}
	}
	comp.SetIsPlaced(false);
	if ( comp.GetType() == COMP::VIA ) Get(rowTL, colTL)->SetIsVia(false);	// Clear via flag
	return true;
}

void Board::FloatAllComps()	// Float all components (i.e. take them off the board)
{
	for (auto& mapObj : m_compMgr.m_mapIdToComp) TakeOff( mapObj.second );
}

void Board::PlaceFloaters()	// Try to place down all the floating components
{
	// Do trax component first.
	// If the trax comp won't go down, then leave the rest floating
	Component& trax	= m_compMgr.GetTrax();
	PutDown(trax);
	if ( trax.GetSize() > 0 && !trax.GetIsPlaced() ) return;
	while(true)
	{
		bool bPlacedOK(false);
		for (auto& mapObj : m_compMgr.m_mapIdToComp)
			if ( PutDown( mapObj.second ) ) bPlacedOK = true;
		if ( !bPlacedOK ) break;	// Couldn't place any more components
	}
}


// GUI helpers for manipulating user-selected components

void Board::SelectAllComps(bool bRestrictToRects)
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
			const int	R = comp.GetLastCol();
			const int	B = comp.GetLastRow();

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

bool Board::ConfirmDestroyUserComps()	// returns false if user-group is empty or has only wires & vias
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

void Board::DestroyUserComps()	// Destroy components in the user-group
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

void Board::MoveUserCompText(const int& deltaRow, const int& deltaCol)	// Move text label
{
	Component& comp	= GetUserComponent();
	if ( deltaRow == 0 && deltaCol == 0 )	// (0,0) ==> reset rather than shift
	{
		comp.SetLabelOffsetRow(0);
		comp.SetLabelOffsetCol(0);
	}
	else
	{
		comp.SetLabelOffsetRow( comp.GetLabelOffsetRow() + deltaRow );
		comp.SetLabelOffsetCol( comp.GetLabelOffsetCol() + deltaCol );
	}
}

void Board::StretchUserComp(const bool& bGrow)	// Stretch the selected component length
{
	Component& comp	= GetUserComponent();
	if ( bGrow )
	{
		// Get bottom-right corner of footprint
		int maxRow( comp.GetLastRow() );
		int maxCol( comp.GetLastCol() );

		// Work out what the stretch will do to it
		if ( comp.GetDirection() == 'W' || comp.GetDirection() == 'E' )
			maxCol += GetStretchIncrement(comp.GetType());
		else
			maxRow += GetStretchIncrement(comp.GetType());

		if ( maxRow + 1 - GetRows() > 0 )		// If we'll go too far down ...
			Pan(-(maxRow + 1 - GetRows()), 0);	// ... pan the whole circuit up to make room
		if ( maxCol + 1 - GetCols() > 0 )		// If we'll go too far right ...
			Pan(0, -(maxCol + 1 - GetCols()));	// ... pan the whole circuit left to make room
	}
	WipeAutoSetPoints();
	TakeOff(comp);
	comp.Stretch(bGrow);
	PutDown(comp);
	PlaceFloaters();	// See if we can now place floating components down
}

void Board::StretchWidthUserComp(const bool& bGrow)	// Stretch the selected component width (just for DIPs)
{
	Component& comp	= GetUserComponent();
	if ( bGrow )
	{
		// Get bottom-right corner of footprint
		int maxRow( comp.GetLastRow() );
		int maxCol( comp.GetLastCol() );

		// Work out what the stretch will do to it
		if ( comp.GetDirection() == 'W' || comp.GetDirection() == 'E' )
			maxRow++;
		else
			maxCol++;

		if ( maxRow + 1 - GetRows() > 0 )		// If we'll go too far down ...
			Pan(-(maxRow + 1 - GetRows()), 0);	// ... pan the whole circuit up to make room
		if ( maxCol + 1 - GetCols() > 0 )		// If we'll go too far right ...
			Pan(0, -(maxCol + 1 - GetCols()));	// ... pan the whole circuit left to make room
	}
	WipeAutoSetPoints();
	TakeOff(comp);
	comp.StretchWidth(bGrow);
	PutDown(comp);
	PlaceFloaters();	// See if we can now place floating components down
}

void Board::ChangeTypeUserComp(const COMP& eType)
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

		if ( maxRow + 1 - GetRows() > 0 )		// If corner will be too far down ...
			Pan(-(maxRow + 1 - GetRows()), 0);	// ... pan the whole circuit up to make room
		if ( maxCol + 1 - GetCols() > 0 )		// If corner will be too far right ...
			Pan(0, -(maxCol + 1 - GetCols()));	// ... pan the whole circuit left to make room
	}

	// Now change the component type
	const int oldPinSeparation	= GetPinSeparation( comp.GetType() );	// For LEDs, and electro caps
	const int oldLength			= ( oldPinSeparation > 0 ) ? oldPinSeparation : comp.GetCols();
	WipeAutoSetPoints();
	TakeOff(comp);
	comp.BuildDefault(eType);
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

void Board::CopyUserComps()	// Make a blank copy of the user-group components and float them
{
	std::list<int> userCompIds;
	m_groupMgr.GetGroupCompIds(USER_GROUPID, userCompIds);

	CopyComps(userCompIds);
}

bool Board::MoveUserComps(const int& deltaRow, const int& deltaCol)	// Move user-group components, and return true if the grid was panned
{
	if ( deltaRow == 0 && deltaCol == 0 ) return false;

	std::list<int> userCompIds;
	m_groupMgr.GetGroupCompIds(USER_GROUPID, userCompIds);

	return MoveComps(userCompIds, deltaRow, deltaCol);
}

void Board::RotateUserComps(const bool& bCW)	// Rotate the selected components
{
	std::list<int> userCompIds;
	m_groupMgr.GetGroupCompIds(USER_GROUPID, userCompIds);

	RotateComps(userCompIds, bCW);
}

void Board::CopyComps(const std::list<int>& compIds)	// Make a blank copy of the components and float them
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

bool Board::MoveTextBox(const int& deltaRow, const int& deltaCol)	// Move text box, and return true if the grid was panned
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

bool Board::MoveComps(const std::list<int>& compIds, const int& deltaRow, const int& deltaCol)	// Move components and return true if the grid was panned
{
	bool bPanned(false);	// Set true if we pan the grid

	if ( deltaRow == 0 && deltaCol == 0 ) return bPanned;

	Component& trax			= m_compMgr.GetTrax();
	const bool bHidingComps	= ( GetGroupMgr().GetNumUserComps() > 0 ) && ( GetCompMode()  == COMPSMODE::OFF );
	const bool bHidingTrax	= ( trax.GetSize() > 0 ) && ( GetTrackMode() == TRACKMODE::OFF );
	if ( bHidingComps || bHidingTrax ) return bPanned;
	const bool bNoComps		= ( GetGroupMgr().GetNumUserComps() == 0 );
	const bool bNoTrax		= ( trax.GetSize() == 0 );
	if ( bNoComps && bNoTrax ) return bPanned;

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
	if ( trax.GetSize() > 0 )
	{
		int newRow = trax.GetRow() + deltaRow;
		int newCol = trax.GetCol() + deltaCol;
		MakeToroid(newRow, newCol);	// Make co-ordinates wrap around at grid edges
		TakeOff(trax);
		trax.SetRow(newRow);
		trax.SetCol(newCol);
	}

	// First put down the trax comp, and then all other comps.
	// If the trax comp won't go down, then leave the rest floating
	PutDown(trax);
	if ( trax.GetSize() == 0 || trax.GetIsPlaced() )
	{
		for (auto& compId : compIds)
			PutDown( m_compMgr.GetComponentById( compId ) );
		PlaceFloaters();	// See if we can now place floating components down
	}
	return bPanned;
}

void Board::RotateComps(const std::list<int>& compIds, const bool& bCW)	// Rotate components
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
	// Take off the trax comp
	Component& trax = m_compMgr.GetTrax();
	if ( trax.GetSize() > 0 )
	{
		TakeOff(trax);

		int newRow(newCentreRow), newCol(newCentreCol);	// Start with the new group centre

		// Then correct for location of the trax's LT corner w.r.t. the group's LT corner
		if ( bCW )	{ newRow += (trax.GetCol() - rect.m_colMin); newCol -= (trax.GetRow() - rect.m_rowMin); }
		else		{ newRow -= (trax.GetCol() - rect.m_colMin); newCol += (trax.GetRow() - rect.m_rowMin); }

		// Then correct for the fact that trax.Rotate() rotates about the trax's centre rather than its LT corner
		if ( bCW )	newCol -= ( trax.GetCompRows() - 1 );
		else		newRow -= ( trax.GetCompCols() - 1 );

		MakeToroid(newRow, newCol);	// Make co-ordinates wrap around at grid edges
		trax.SetRow(newRow);
		trax.SetCol(newCol);
		trax.Rotate(bCW);	// Rotate the component ...
	}


	// First put down the trax comp, and then all other comps.
	// If the trax comp won't go down, then leave the rest floating
	PutDown(trax);
	if ( trax.GetSize() == 0 || trax.GetIsPlaced() )
	{
		for (auto& compId : compIds)
			PutDown( m_compMgr.GetComponentById( compId ) );
		PlaceFloaters();	// See if we can now place floating components down
	}
}

Rect Board::GetFootprintBounds(const std::list<int>& compIds)
{
	Rect bounding;
	for (auto& compId : compIds)
	{
		const Component& comp	= m_compMgr.GetComponentById( compId );
		bounding |= comp.GetFootprintRect();
	}
	const Component& trax = m_compMgr.GetTrax();
	if ( trax.GetSize() > 0 )
		bounding |= trax.GetFootprintRect();
	return bounding;
}
