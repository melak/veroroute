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
#include "PolygonHelper.h"

// Methods to get objects at a grid location

int Board::GetComponentId(int row, int col)	// Pick the most relevant component at the location
{
	MakeToroid(row, col);	// Make co-ordinates wrap around at grid edges

	// Most to least prefered order is ...
	// ... unplaced plugs, unplaced non-plugs, placed plugs, placed non-plugs
	for (int iLoop = 0; iLoop < 4; iLoop++)
	{
		const bool bReqPlaced = ( iLoop / 2 == 1 );	// true ==> Only consider placed components
		const bool bReqPlug   = ( iLoop % 2 == 0 );	// true ==> Only consider components that can go under ICs

		int iMinArea(INT_MAX), iBestCompID(BAD_COMPID);	// If two comps share a grid point, pick one with smallest area
		for (const auto& mapObj : m_compMgr.GetMapIdToComp())
		{
			const Component&	comp	= mapObj.second;
			const bool			bPlug	= CompTypes::IsPlug( comp.GetType() );
			const bool			bPlaced = comp.GetIsPlaced();
			if ( bPlaced != bReqPlaced ) continue;
			if ( bPlug != bReqPlug ) continue;
			int rowTL = comp.GetRow();
			int colTL = comp.GetCol();

			MakeToroid(rowTL, colTL);	// Make co-ordinates wrap around at grid edges

			if ( row >= rowTL && row < rowTL + comp.GetCompRows() &&
				 col >= colTL && col < colTL + comp.GetCompCols() )
			{
				const int area = comp.GetCompRows() * comp.GetCompCols();
				if ( area <= iMinArea )
				{
					iMinArea	= area;
					iBestCompID	= comp.GetId();
				}
			}
		}
		if ( iBestCompID != BAD_COMPID ) return iBestCompID;

		Component& trax = m_compMgr.GetTrax();
		if ( trax.GetSize() > 0 )
		{
			const bool bPlug	= CompTypes::IsPlug( trax.GetType() );
			const bool bPlaced	= trax.GetIsPlaced();
			if ( bPlaced != bReqPlaced ) continue;
			if ( bPlug != bReqPlug ) continue;
			int rowTL = trax.GetRow();
			int colTL = trax.GetCol();

			MakeToroid(rowTL, colTL);	// Make co-ordinates wrap around at grid edges

			if ( row >= rowTL && row < rowTL + trax.GetCompRows() &&
				 col >= colTL && col < colTL + trax.GetCompCols() )
			{
				if ( trax.GetCompElement(row-rowTL, col-colTL)->ReadFlagBits(RECTSET) )
					return trax.GetId();
			}
		}
	}
	return BAD_COMPID;
}

int Board::GetTextId(int row, int col)	// Pick the most relevant text box at the location
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

// Methods to handle variable pad/hole and PCB tolerances

void Board::GetPadWidths_MIL(std::list<int>& o, int& iDefaultWidth) const
{
	iDefaultWidth = GetPAD_MIL();
	m_compMgr.GetPadWidths(o, iDefaultWidth);
	if ( GetFatTracks() && std::find(o.begin(), o.end(), iDefaultWidth) == o.end() )
		o.push_back( iDefaultWidth );
}
void Board::GetHoleWidths_MIL(std::list<int>& o, int& iDefaultWidth) const
{
	iDefaultWidth = GetHOLE_MIL();
	m_compMgr.GetHoleWidths(o, iDefaultWidth);
}
void Board::GetSeparations(double& minTrackSeparation_mil, double& minGroundFill_mil)
{
	if ( GetCompEdit() || GetVeroTracks() ) return;

	// Calc minimum track separation in mil
	const double dMinSep	= 100.0 * m_dMinSeparation;	// Min separation in MIL, without ground fill
	const double dGap		= GetGroundFill() ? GetGAP_MIL() : 100.0;
	minTrackSeparation_mil	= std::min(dGap, dMinSep);
	// Calc minimum ground-fill width in mil
	// To have a ground fill with no isolated islands, this must be > 0 (and probably at least 8 mil)
	minGroundFill_mil		= GetGroundFill() ? std::max(0.0, dMinSep - dGap * 2.0) : 100.0;

	// If the minimum track separation is determined by the gap,
	// then all locations have min separation, so don't show warning points in the view
	if ( minTrackSeparation_mil - dGap == 0.0 ) ClearWarnPoints();
}
void Board::CalcMIN_SEPARATION()	// Sets m_dMinSeparation and m_warnPoints[]
{
	ClearWarnPoints();	// Wipe list of warning locations on both layers
	m_dMinSeparation = DBL_MAX;

	if ( GetCompEdit() || GetVeroTracks() ) return;

	const int nRings = 2;	// 2 ==> Max pad size supported by VeroRoute could be up to 200 mil in future
	int Xmil, Ymil;	// For pad offsets

	const bool bStandardBlobs = ( MAX_PAD_OFFSET_MIL <= 50 );	// true ==> legs for offset pads will be within a grid square

	// Get bounds to minimise looping
	int minRow, minCol, maxRow, maxCol;
	GetBounds(minRow, minCol, maxRow, maxCol);

	const bool&	bVero			= GetVeroTracks();
	const bool	bMonoPCB		= GetTrackMode() == TRACKMODE::MONO || GetTrackMode() == TRACKMODE::PCB;
	const bool	bGroundFill		= !bVero && bMonoPCB && GetGroundFill();
	const bool	bSolderMask		= false;
	const bool	bForceXthermals	= GetXthermals();

	for (int k = 0, kMax = GetLyrs(); k < kMax; k++)	// Check all layers
	{
		PolygonHelper polygonHelper;

		for (int j = minRow; j <= maxRow; j++)
		for (int i = minCol; i <= maxCol; i++)
		{
			const Element*	pA			= Get(k, j, i);
			const int&		nodeIdA		= pA->GetNodeId();
			const bool		bHasPinA	= pA->GetHasPinTH();
			const bool		bSoicA		= pA->GetHasPinSOIC() && k == LYR_TOP;
			if ( nodeIdA == BAD_NODEID && !bHasPinA ) continue;	// Skip if no track and no pin
			const bool		bIsGndA		= bGroundFill && nodeIdA == GetGroundNodeId(k) && nodeIdA != BAD_NODEID;

			MyPointF pointA(i, j, 0.005 * GetTRACK_MIL());	// The blob centre for pA (note: track radius !!!)
			MyPointF padA(pointA);							// The pad centre for pA (pad radius will be set below)
			bool	 bPadA(true);							// Set false if there is no pad at pA
			bool	 bPadOffsetA(false);					// Set true if we have an offset pad at pA
			int		 iPadWidthMIL_A(0);						// Needed for tag length
			if ( pA->GetHasWire() )
				padA.m_radius = 0.005 * GetPAD_MIL();	// No custom pad size for wires
			else if ( bHasPinA )
			{
				size_t	pinIndex;
				int		compId;
				GetSlotInfoForTH(pA, pinIndex, compId);	//TODO If hole sharing, preferring TH part over SOIC par

				const Component& comp	= m_compMgr.GetComponentById( compId );	// Non-wire part must use slot 0
				iPadWidthMIL_A = comp.GetCustomPads() ? comp.GetPadWidth() : GetPAD_MIL();	// Handle custom pad sizes
				padA.m_radius = 0.005 * iPadWidthMIL_A;
				// Handle pad offsets
				comp.GetCompPinOffsets(pinIndex, Xmil, Ymil);
				padA += QPointF(0.01 * Xmil, 0.01 * Ymil);
				bPadOffsetA	= ( Xmil != 0 || Ymil != 0 );
			}
			else if ( pA->GetIsVia() )
				padA.m_radius = 0.005 * GetVIAPAD_MIL();
			else
				bPadA = false;

			std::list<MyPolygonF> blobA;	// Blob A points (in units of grid squares)
			std::list<MyPolygonF> soicA;

			const int	iPerimeterCodeA	= GetPerimeterCode(pA);
			const int	iTagCodeA		= ( bPadA && bIsGndA && !bSoicA ) ? GetTagCode(pA, iPerimeterCodeA) : 0;
			const bool	bBlobA			= !bPadOffsetA || iPerimeterCodeA != 0 || ( bForceXthermals && bIsGndA );
			if ( bBlobA )
				CalcBlob(1, pointA, padA, iPadWidthMIL_A, iPerimeterCodeA, iTagCodeA, blobA, bHasPinA, bSoicA, bIsGndA);	// 1 ==> scale of 1 grid square
			if ( bSoicA )
				CalcSOIC(1, pointA, pA->GetPinIndex(), GetCompMgr().GetComponentById(pA->GetCompId()).GetDirection(), soicA, bSolderMask, bIsGndA);

			// Only need to loop half the directions in the following loop (the i,j scan takes care of the other half)
			for (int jj = std::max(minRow,j-nRings); jj <= j; jj++)
			for (int ii = std::max(minCol,i-nRings), iiMax = std::min(maxCol,i+nRings); ii <= iiMax; ii++)
			{
				if ( jj == j && ii == i ) continue;	// Skip pA
				const Element*	pB			= Get(k, jj, ii);
				const int&		nodeIdB		= pB->GetNodeId();
				const bool		bHasPinB	= pB->GetHasPinTH();
				const bool		bSoicB		= pB->GetHasPinSOIC() && k == LYR_TOP;
				if ( nodeIdB == BAD_NODEID && !bHasPinB ) continue;	// Skip if no track and no pin
				if ( nodeIdB == nodeIdA ) continue;
				const bool		bIsGndB		= bGroundFill && nodeIdB == GetGroundNodeId(k) && nodeIdB != BAD_NODEID;

				MyPointF pointB(ii, jj, 0.005 * GetTRACK_MIL());	// The blob centre for pB (note: track radius !!!)
				MyPointF padB(pointB);								// The pad centre for pB (pad radius will be set below)
				bool	 bPadB(true);								// Set false if there is no pad at pB
				bool	 bPadOffsetB(false);						// Set true if we have an offset pad at pB
				int		 iPadWidthMIL_B(0);

				if ( pB->GetHasWire() )
					padB.m_radius = 0.005 * GetPAD_MIL();	// No custom pad size for wires
				else if ( bHasPinB )
				{
					size_t	pinIndex;
					int		compId;
					GetSlotInfoForTH(pB, pinIndex, compId);	//TODO If hole sharing, preferring TH part over SOIC par

					const Component& comp	= m_compMgr.GetComponentById( compId );	// Non-wire part must use slot 0
					iPadWidthMIL_B = comp.GetCustomPads() ? comp.GetPadWidth() : GetPAD_MIL();	// Handle custom pad sizes
					padB.m_radius = 0.005 * iPadWidthMIL_B;
					// Handle pad offsets
					comp.GetCompPinOffsets(pinIndex, Xmil, Ymil);
					padB += QPointF(0.01 * Xmil, 0.01 * Ymil);
					bPadOffsetB	= ( Xmil != 0 || Ymil != 0 );
				}
				else if ( pB->GetIsVia() )
					padB.m_radius = 0.005 * GetVIAPAD_MIL();
				else
					bPadB = false;

				std::list<MyPolygonF> blobB;	// Blob B points (in units of grid squares)
				std::list<MyPolygonF> soicB;
				const int	iPerimeterCodeB	= GetPerimeterCode(pB);
				const int	iTagCodeB		= ( bPadB && bIsGndB && !bSoicB ) ? GetTagCode(pB, iPerimeterCodeB) : 0;
				const bool	bBlobB			= !bPadOffsetB || iPerimeterCodeB != 0 || ( bForceXthermals && bIsGndB );
				if ( bBlobB )
					CalcBlob(1, pointB, padB, iPadWidthMIL_B, iPerimeterCodeB, iTagCodeB, blobB, bHasPinB, bSoicB, bIsGndB);	// 1 ==> scale of 1 grid square
				if ( bSoicB )
					CalcSOIC(1, pointB, pB->GetPinIndex(), GetCompMgr().GetComponentById(pB->GetCompId()).GetDirection(), soicB, bSolderMask, bIsGndB);
				
				const bool bCompareBlobs = !bStandardBlobs || ( abs(jj - j) < 2 && abs(ii - i) < 2 );	// Standard blobs ==> just consider neighbouring grid points

				// Pad A to Pad B
				if ( bPadA && bPadB ) polygonHelper.CalcSeparation(padA, padB);

				// Pad A to Blob B
				if ( bPadA && bBlobB ) for(const auto& b : blobB) polygonHelper.CalcSeparation(padA, b);

				// Pad B to Blob A
				if ( bPadB && bBlobA ) for(const auto& a : blobA) polygonHelper.CalcSeparation(padB, a);

				// Blob A to Blob B
				if ( bCompareBlobs && bBlobA && bBlobB ) for(const auto& a : blobA) for(const auto& b : blobB) polygonHelper.CalcSeparation(a, b);

				// SOIC track A to SOIC track B
				if ( bCompareBlobs && bSoicA && bSoicB ) for(const auto& a : soicA) for(const auto& b : soicB) polygonHelper.CalcSeparation(a, b);

				//TODO Need to consider other separations
				// Pad A to SOIC track B
				if ( bPadA && bSoicB ) for(const auto& b : soicB) polygonHelper.CalcSeparation(padA, b);

				// Pad B to SOIC track A
				if ( bPadB && bSoicA ) for(const auto& a : soicA) polygonHelper.CalcSeparation(padB, a);
				
				// SOIC track A to Blob B
				if ( bCompareBlobs && bSoicA && bBlobB ) for(const auto& a : soicA) for(const auto& b : blobB) polygonHelper.CalcSeparation(a, b);
				
				// SOIC track B to Blob A
				if ( bCompareBlobs && bSoicB && bBlobA ) for(const auto& b : soicB) for(const auto& a : blobA) polygonHelper.CalcSeparation(a, b);
			}
		}
		if ( polygonHelper.m_Dmin > m_dMinSeparation ) continue;
		if ( polygonHelper.m_Dmin < m_dMinSeparation )	// If min for layer is lowest across all layers ...
			ClearWarnPoints();							// ... wipe all warning points

		for (auto& p : const_cast<const QPolygonF&>(polygonHelper.m_pWarn)) m_warnPoints[k].push_back(p);
		m_dMinSeparation = polygonHelper.m_Dmin;
	}	// Next layer
}

void Board::CalcGroundFillBounds()
{
	const int&	W = GetGRIDPIXELS();	// Square width in pixels
	const int	C = W >> 1;				// Half square width in pixels

	int deltaL(0), deltaR(0), deltaT(0), deltaB(0);	// Maximum pad protrusions (in pixels) at edge of grid

	// Loop points on edge of grid and look for large pads there
	int minRow(0), minCol(0), maxRow(GetRows()-1), maxCol(GetCols()-1);
	for (int j = minRow; j <= maxRow; j++)
	{
		const int iStep = ( j == minRow || j == maxRow ) ? 1 : std::max(1, maxCol - minCol);
		for (int i = minCol; i <= maxCol; i += iStep)
		{
			const Element* p = Get(0, j, i);	// Sufficient to check layer 0 when looking for TH pins
			if ( !p->GetHasPinTH() ) continue;
			if ( p->GetHasWire() ) continue;	// Wires can't have custom sized pads or offset pads

			size_t	pinIndex;
			int		compId;
			GetSlotInfoForTH(p, pinIndex, compId);	//TODO If hole sharing, preferring TH part over SOIC part

			const Component& comp	= m_compMgr.GetComponentById( compId );
			int Xmil(0), Ymil(0);	// Pad offsets
			comp.GetCompPinOffsets(pinIndex, Xmil, Ymil);

			if ( !comp.GetCustomPads() && Xmil == 0 && Ymil == 0) continue;	// Skip pad if not custom or offset

			const int w			= comp.GetPadWidth();
			const int padWidth	= GetHalfPixelsFromMIL( w ) + 1;	// +1 for back compatibility (i.e. max pad size was 98)
			const int delta		= padWidth - C;		// The protrusion for a pad without offset

			if ( i == minCol ) deltaL = std::max(deltaL, delta - ( Xmil * W ) / 100);
			if ( i == maxCol ) deltaR = std::max(deltaR, delta + ( Xmil * W ) / 100);
			if ( j == minRow ) deltaT = std::max(deltaT, delta - ( Ymil * W ) / 100);
			if ( j == maxRow ) deltaB = std::max(deltaB, delta + ( Ymil * W ) / 100);
		}
	}
	m_gndL = 0 - deltaL;
	m_gndT = 0 - deltaT;
	m_gndR = W * GetCols() + deltaR;
	m_gndB = W * GetRows() + deltaB;

	// Loop all shapes and get a safe estimate of their outer bounds
	const bool bMonoPCB		= GetTrackMode() == TRACKMODE::MONO || GetTrackMode() == TRACKMODE::PCB;
	const bool bFill		= !bMonoPCB && GetFillSaturation() > 0;	// No component fill in Mono/PCB mode
	for (const auto& mapObj : m_compMgr.GetMapIdToComp())
	{
		const Component& comp = mapObj.second;

		double L,R,T,B;
		comp.GetSafeBounds(L, R, T, B, bFill);

		// Footprint centre
		const double X = C * ( comp.GetCol() + comp.GetLastCol() + 1 );
		const double Y = C * ( comp.GetRow() + comp.GetLastRow() + 1 );

		m_gndL = std::min(m_gndL, static_cast<int>(X + W * L));
		m_gndT = std::min(m_gndT, static_cast<int>(Y + W * T));
		m_gndR = std::max(m_gndR, static_cast<int>(X + W * R));
		m_gndB = std::max(m_gndB, static_cast<int>(Y + W * B));
	}
}

void Board::GetGroundFillBounds(int& L, int& R, int& T, int& B) const
{
	L = m_gndL; R = m_gndR; T = m_gndT; B = m_gndB;
}

// Methods to paint/unpaint nodeIds

void Board::SetNodeId(Element* p, int nodeId, bool bAllLyrs)	// Helper to make sure we do UpdateCounts() before painting an element
{
	// For all layers case, always write base layer first
	Element* q	= bAllLyrs ? p->GetNbr(NBR_X) : nullptr;
	Element* p1	= std::min(p, q);
	Element* p2	= std::max(p, q);
	if ( p1 )
	{
		if ( !GetRoutingEnabled() )	// No point updating m_adjInfoMgr since RebuildAdjacencies() is called after routing
			m_adjInfoMgr.UpdateCounts(p1, nodeId);	// Do this BEFORE we call SetNodeId() on the element
		p1->SetNodeId(nodeId);	// Write node value
	}
	if ( p2 )
	{
		if ( !GetRoutingEnabled() )	// No point updating m_adjInfoMgr since RebuildAdjacencies() is called after routing
			m_adjInfoMgr.UpdateCounts(p2, nodeId);	// Do this BEFORE we call SetNodeId() on the element
		p2->SetNodeId(nodeId);	// Write node value
	}
}

void Board::WipeFlagBits(Element* p, char i, bool bAllLyrs)
{
	// For all layers case, always write base layer first
	Element* q	= bAllLyrs ? p->GetNbr(NBR_X) : nullptr;
	Element* p1	= std::min(p, q);
	Element* p2	= std::max(p, q);
	if ( p1 ) p1->WipeFlagBits(i);
	if ( p2 ) p2->WipeFlagBits(i);
}

void Board::MarkFlagBits(Element* p, char i, bool bAllLyrs)
{
	// For all layers case, always write base layer first
	Element* q	= bAllLyrs ? p->GetNbr(NBR_X) : nullptr;
	Element* p1	= std::min(p, q);
	Element* p2	= std::max(p, q);
	if ( p1 ) p1->MarkFlagBits(i);
	if ( p2 ) p2->MarkFlagBits(i);
}

bool Board::SetNodeIdByUser(int lyr, int row, int col, int nodeId, bool bPaintPins)
{
	Element*	p		= Get(lyr, row, col);
	if ( p->GetIsHole() || ( p->GetSoicProtected() && nodeId != BAD_NODEID ) ) return false;	// No change

	const bool	bWire	= p->GetHasWire();
	const bool	bPin	= p->GetLyrHasPin();
	assert( !bPin || p->GetHasComp() );			// Sanity check
	assert( !bWire || p->GetHasPinTH() );		// Wires must have TH pins
	assert( !bWire || !p->GetHasPinSOIC() );	// Wires can't share with SOICs
	
	WIRELIST wireList;	// Helper for chains of wires

	// Handle special case first.
	if ( bPin && !bWire && !bPaintPins )
	{
		// If trying to paint the board under a non-wire pin ...
		// ... we can modify the "origId" for the pin, but are only allowed
		// ... to wipe it or make it match the nodeId of the pin. Then quit.

		bool bChanged(false);
		for (int iSlot = 0; iSlot < 2; iSlot++)
		{
			size_t	pinIndex;
			int		compId;
			p->GetSlotInfo(iSlot, pinIndex, compId);
			if ( compId == BAD_COMPID ) continue;
			Component& comp = m_compMgr.GetComponentById( compId );
			assert( comp.GetType() != COMP::INVALID );
			assert( comp.GetType() != COMP::WIRE );	// Sanity check
			assert( pinIndex != BAD_PININDEX );

			if ( nodeId != BAD_NODEID && nodeId != comp.GetNodeId(pinIndex) ) continue;	// Can't set a bad origId

			if ( comp.GetOrigId(lyr, pinIndex) == nodeId ) continue;	// origId is already as required

			// Need to do (RemoveComp/ SetNodeId/ AddComp) to ensure m_nodeInfoMgr is updated OK
			m_nodeInfoMgr.RemoveComp(comp);
			comp.SetOrigId(lyr, pinIndex, nodeId);
			m_nodeInfoMgr.AddComp(comp);
			bChanged = true;
		}
		return bChanged;
	}

	// Now do regular cases:  Paint the board as needed...
	if ( bWire )
	{
		// Just need to get origId.  Any used slot will do
		size_t	pinIndex;
		int		compId;
		p->GetSlotInfo(p->GetUsedSlot(), pinIndex, compId);

		const Component& comp	= m_compMgr.GetComponentById( compId );
		assert( comp.GetType() == COMP::WIRE );	// Sanity check
		const int		 origId	= comp.GetOrigId(lyr, pinIndex);

		if ( nodeId == p->GetNodeId() && origId == nodeId && p->ReadFlagBits(USERSET) )
			return false;	// No change
	}
	else if ( nodeId == p->GetNodeId() && p->ReadFlagBits(USERSET) )
		return false;		// No change

	// Set the NodeId on the element and all connected wire points
	p->GetWireList(wireList);	// Get list containing p and its wired points
	for (const auto& o : wireList)
	{
		Element* pW = const_cast<Element*> (o.first);
		const bool bAllLyrs = pW->GetHasPinTH();
		SetNodeId(pW, nodeId, bAllLyrs);
		WipeFlagBits(pW, AUTOSET|VEROSET, bAllLyrs);
		MarkFlagBits(pW, USERSET, bAllLyrs);
	}

	// Set the nodeId at the component pin
	if ( bPin )
	{
		if ( bWire )	// Wire
		{
			size_t	pinIndex;
			int		compId;
			for (const auto& o : wireList)
			{
				Element* pL = const_cast<Element*> (o.first);
				for (int iSlot = 0; iSlot < 2; iSlot++)
				{
					Element* pW = pL->GetW(iSlot);

					pL->GetSlotInfo(iSlot, pinIndex, compId);
					assert( (pW == nullptr && compId == BAD_COMPID && pinIndex == BAD_PININDEX) ||
							(pW != nullptr && compId != BAD_COMPID && pinIndex != BAD_PININDEX) );

					if ( pW == nullptr ) continue;

					Component& comp = m_compMgr.GetComponentById( compId );
					assert( comp.GetType() == COMP::WIRE );	// Sanity check
					const size_t otherPinIndex = ( pinIndex == 0 ) ? 1 : 0;
					if ( pL == p )	// If it's the point that was clicked on, then set both the nodeId and origId
					{
						comp.SetNodeId(pinIndex, nodeId);
						for (int iLyr = 0; iLyr < 2; iLyr++)
							comp.SetOrigId(iLyr, pinIndex, nodeId);
					}
					comp.SetNodeId(otherPinIndex, nodeId);
					for (int iLyr = 0; iLyr < 2; iLyr++)
						comp.SetOrigId(iLyr, otherPinIndex, ( comp.GetOrigId(iLyr, otherPinIndex) != BAD_NODEID ) ? nodeId : BAD_NODEID);
				}
			}
		}
		else			// Regular component
		{
			for (int iSlot = 0; iSlot < 2; iSlot++)
			{
				size_t	pinIndex;
				int		compId;
				p->GetSlotInfo(iSlot, pinIndex, compId);
				if ( compId == BAD_COMPID ) continue;
				Component& comp = m_compMgr.GetComponentById( compId );
				assert( comp.GetType() != COMP::INVALID );
				assert( bPaintPins && comp.GetType() != COMP::WIRE );	// Sanity check
				assert( pinIndex != BAD_PININDEX );

				// Need to do (RemoveComp/ SetNodeId/ AddComp) to ensure m_nodeInfoMgr is updated OK
				m_nodeInfoMgr.RemoveComp(comp);
				comp.SetNodeId(pinIndex, nodeId);
				for (int iLyr = 0; iLyr < 2; iLyr++)
					if ( comp.GetOrigId(iLyr, pinIndex) != nodeId )	// Modifying a pin on a previously painted track ...
						comp.SetOrigId(iLyr, pinIndex, BAD_NODEID);	// ... should wipe the track under the pin
				m_nodeInfoMgr.AddComp(comp);
			}
		}
	}
	return true;
}

void Board::FloodNodeId(int nodeId)
{
	for (int lyr = 0, lyrs = GetLyrs(); lyr < lyrs; lyr++)
	for (int row = 0, rows = GetRows(); row < rows; row++)
	for (int col = 0, cols = GetCols(); col < cols; col++)
	{
		Element* p = Get(lyr, row, col);
		if ( p->GetMH() == BAD_MH) continue;
		if ( p->GetHasWire() )
		{
			// Just need to get origId.  Any used slot will do
			size_t pinIndex;
			int compId;
			p->GetSlotInfo(p->GetUsedSlot(), pinIndex, compId);

			const Component& comp	= m_compMgr.GetComponentById( compId );
			assert( comp.GetType() == COMP::WIRE );	// Sanity check
			const int		 origId	= comp.GetOrigId(lyr, pinIndex);

			if ( origId != p->GetNodeId() || !p->ReadFlagBits(USERSET) ) continue; // Don't paint directly if it wasn't painted directly in the first place
		}
		SetNodeIdByUser(lyr, row, col, nodeId, true);	// true ==> paint pins
	}
}

void Board::AutoFillVero()
{
	int minRow, minCol, maxRow, maxCol;
	GetBounds(minRow, minCol, maxRow, maxCol);

	const bool& bVertical = GetVerticalStrips();

	// Note: The terms "top" and "bot" in the following code should be
	//		 taken to mean "left" and "right" if making horizontal strips.
	const int k(0);
	const int jMin	= ( bVertical ) ? minCol : minRow;
	const int jMax	= ( bVertical ) ? maxCol : maxRow;
	const int iMin	= ( bVertical ) ? minRow : minCol;
	const int iMax	= ( bVertical ) ? maxRow : maxCol;
	for (int j = jMin; j <= jMax; j++)
	{
		int nodeIdTop(BAD_NODEID), lenTop(INT_MAX);
		for (int i = iMin; i <= iMax; i++)
		{
			Element*	pC		= ( bVertical ) ? Get(k,i,j) : Get(k,j,i);
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
			if ( pC->GetHasPin() ) continue;		// Can't assign pins, so skip
			if ( pC->GetIsHole() ) continue;		// Can't assign holes, so skip
			if ( pC->GetSoicProtected() ) continue;	// Can't assign in SOIC area, so skip

			// Have a blank non-pin element at this point
			// Search for first used nodeId below

			int nodeIdBot(BAD_NODEID), lenBot(INT_MAX);
			for (int ii = i+1; ii <= iMax; ii++)
			{
				const Element*	pB		= ( bVertical ) ? Get(k,ii,j) : Get(k,j,ii);
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

			const bool bAllLyrs(false);
			if ( lenTop == INT_MAX && lenBot == INT_MAX )
				SetNodeId(pC, GetNewNodeId(), bAllLyrs);	// Unused strip, so make a new nodeId
			else if ( lenTop < lenBot )
				SetNodeId(pC, nodeIdTop, bAllLyrs);
			else
				SetNodeId(pC, nodeIdBot, bAllLyrs);
			WipeFlagBits(pC, USERSET|AUTOSET, bAllLyrs);
			MarkFlagBits(pC, VEROSET, bAllLyrs);

			nodeIdTop = pC->GetNodeId(); lenTop = 1;	// Start top count
		}
	}
	m_colorMgr.ReAssignColors();	// Forces colors to be worked out again
}

void Board::CalcSolder()	// Work out locations of solder blobs to join veroboard tracks together
{
	for (int i = 0, iSize = GetSize(); i < iSize; i++)
		GetAt(i)->SetSolderR(false);	// Clear solder

	const bool& bVertical = GetVerticalStrips();	// Strip direction

	std::vector<int> nodeIds;
	m_adjInfoMgr.GetBoardNodeIds(nodeIds);	// The set of valid nodeIDs on the board

	for (size_t n = 0, N = nodeIds.size(); n < N; n++)
		for (int col = 0, numCols = bVertical ? GetCols() : GetRows(); col < numCols; col++)	// Loop cols/rows
			SetSolder(nodeIds[n], col, bVertical);
}

void Board::SetSolder(int nodeId, int col, bool bVertical)
{
	// Sets elements of bSolderR to indicate solder blob between col and col+1
	const int colMax = ( bVertical ) ? GetCols() : GetRows();
	if ( col == colMax-1 ) return;

	assert( nodeId != BAD_NODEID );
	int bestRow(-1), bestRowPins(-INT_MAX), bestRowPads(INT_MAX);
	const int lyr(0);
	const int rowMax = ( bVertical ) ? GetRows() : GetCols();
	for (int row = 0; row < rowMax; row++)
	{
		const Element*	pC			= bVertical ? Get(lyr, row, col)	: Get(lyr, col,   row);
		const Element*	pR			= bVertical ? Get(lyr, row, col+1)	: Get(lyr, col+1, row);
		const bool		bMatch		= pC->GetNodeId() == nodeId && pR->GetNodeId() == nodeId;
		const bool		bLastRow	= row == rowMax-1;
		if ( bMatch )
		{
			int rowPins(0), rowPads(0);	// Try avoid putting a blob where we have a pad. Otherwise prefer pin locations.

			if ( pC->GetHasPinTH() )
			{
				if ( pC->GetHasWire() )	// Don't treat wire like pad
					rowPins++;
				else
				{
					const Component& comp = m_compMgr.GetComponentById( pC->GetCompId() );
					assert( comp.GetType() != COMP::INVALID );
					if ( comp.GetType() == COMP::PAD || comp.GetAllowFlyWire() )
						rowPads++;
					else
						rowPins++;
				}
			}
			if ( pR->GetHasPinTH() )
			{
				if ( pR->GetHasWire() )	// Don't treat wire like pad
					rowPins++;
				else
				{
					const Component& comp = m_compMgr.GetComponentById( pR->GetCompId() );
					assert( comp.GetType() != COMP::INVALID );
					if ( comp.GetType() == COMP::PAD || comp.GetAllowFlyWire() )
						rowPads++;
					else
						rowPins++;
				}
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
				// Finally check that there isn't a wire joining the strips
				bool bHaveWire(false);	// Set true if a wire joins the strips
				for (int r = bestRow; r >= 0 && !bHaveWire; r--)		// Search one way ...
				{
					const Element*	pC	= bVertical ? Get(lyr, r, col)		: Get(lyr, col,   r);
					if ( pC->GetNodeId() != nodeId ) break;
					const Element*	pR	= bVertical ? Get(lyr, r, col+1)	: Get(lyr, col+1, r);
					if ( pR->GetNodeId() != nodeId ) break;
					bHaveWire = ( pC->GetW(0) == pR || pC->GetW(1) == pR );
				}
				for (int r = bestRow; r < rowMax && !bHaveWire; r++)	// ... and the other
				{
					const Element*	pC	= bVertical ? Get(lyr, r, col)		: Get(lyr, col,   r);
					if ( pC->GetNodeId() != nodeId ) break;
					const Element*	pR	= bVertical ? Get(lyr, r, col+1)	: Get(lyr, col+1, r);
					if ( pR->GetNodeId() != nodeId ) break;
					bHaveWire = ( pC->GetW(0) == pR || pC->GetW(1) == pR );
				}
				if ( !bHaveWire )
				{
					if ( bVertical )
						Get(lyr, bestRow, col)->SetSolderR(true);
					else
						Get(lyr, col, bestRow)->SetSolderR(true);
				}
			}
			bestRow		= -1;		// Reset
			bestRowPins	= -INT_MAX;	// Reset
			bestRowPads	=  INT_MAX;	// Reset
		}
	}
}


// Command enablers for GUI

bool Board::GetDisableCompText() const
{
	if ( GetMirrored() ) return true;
	if ( ( m_groupMgr.GetNumUserComps() != 1 ) || ( GetCompMode() == COMPSMODE::OFF || GetCompMode() == COMPSMODE::OUTLINE ) ) return true;
	const Component& comp	= GetUserComponent();
	const COMP&		 eType	= comp.GetType();
	return ( eType == COMP::WIRE || eType == COMP::MARK || eType == COMP::VERO_NUMBER || eType == COMP::VERO_LETTER );	// No labels for wires/markers/vero-labels
}

bool Board::GetDisableMove() const
{
	if ( GetMirrored() ) return true;
	const Component& trax	= m_compMgr.GetTrax();
	const bool bHidingComps	= ( m_groupMgr.GetNumUserComps() > 0 ) && ( GetCompMode()  == COMPSMODE::OFF );
	const bool bHidingTrax	= ( trax.GetSize() > 0 ) && ( GetTrackMode() == TRACKMODE::OFF );
	if ( bHidingComps || bHidingTrax ) return true;
	const bool bNoComps		= ( m_groupMgr.GetNumUserComps() == 0 );
	const bool bNoTrax		= ( trax.GetSize() == 0 );
	return bNoComps && bNoTrax;
}

bool Board::GetDisableRotate() const
{
	if ( GetMirrored() ) return true;
	const Component& trax	= m_compMgr.GetTrax();
	const bool bHidingComps	= ( m_groupMgr.GetNumUserComps() > 0 ) && ( GetCompMode()  == COMPSMODE::OFF );
	const bool bHidingTrax	= ( trax.GetSize() > 0 ) && ( GetTrackMode() == TRACKMODE::OFF );
	if ( bHidingComps || bHidingTrax ) return true;
	const bool bNoComps		= ( m_groupMgr.GetNumUserComps() == 0 );
	const bool bMark		= ( m_groupMgr.GetNumUserComps() == 1 && GetUserComponent().GetType() == COMP::MARK );	// Can't rotate a marker
	const bool bNoTrax		= ( trax.GetSize() == 0 );
	return ( bNoComps || bMark ) && bNoTrax;
}

bool Board::GetDisableStretch(bool bGrow) const
{
	if ( GetMirrored() ) return true;
	if ( ( m_groupMgr.GetNumUserComps() != 1 ) || ( GetCompMode() == COMPSMODE::OFF ) ) return true;
	const Component& comp = GetUserComponent();
	return !comp.CanStretch(bGrow);
}

bool Board::GetDisableStretchWidth(bool bGrow) const
{
	if ( GetMirrored() ) return true;
	if ( ( m_groupMgr.GetNumUserComps() != 1 ) || ( GetCompMode() == COMPSMODE::OFF ) ) return true;
	const Component& comp = GetUserComponent();
	return !comp.CanStretchWidth(bGrow);
}

bool Board::GetDisableChangeType() const
{
	if ( GetMirrored() ) return true;
	if ( ( m_groupMgr.GetNumUserComps() != 1 ) || ( GetCompMode() == COMPSMODE::OFF ) ) return true;
	const Component& comp	= GetUserComponent();
	const COMP		 eType	= comp.GetType();
	return !CompTypes::AllowTypeChange(eType, eType);
}

bool Board::GetDisableChangeCustom() const
{
	if ( GetMirrored() ) return true;
	if ( ( m_groupMgr.GetNumUserComps() != 1 ) || ( GetCompMode() == COMPSMODE::OFF ) ) return true;
	const Component& comp	= GetUserComponent();
	return !comp.GetAllowCustomPads();
}

bool Board::GetDisableWipe() const
{
	return GetTrackMode() == TRACKMODE::OFF;
}
