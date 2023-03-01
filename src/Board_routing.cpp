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

//#define ROUTE_ALGORITHM_TIMER

#include "Board.h"

// MH_LRTB = Manhatten "distance" for horizontally/vertically adjacent grid points.
// MH_DIAG = Manhatten "distance" for diagonally adjacent grid points.
// MH_WIRE = Manhatten "distance" for wires, regardless of their length.
// MH_LPIN = Manhatten "distance" for changing layers at a pin (of a component or wire)
// MH_LVIA = Manhatten "distance" for changing layers at a via (i.e. place with no pin)

// Routing algorithm assumes:  MH_LVIA >= MH_DIAG > MH_LRTB > MH_WIRE >= MH_LPIN

// Increasing MH_LVIA will try harder to avoid making vias but makes the algorithm much slower.
// For maximum speed put MH_LVIA = 3.

#define MH_LPIN 1
#define MH_WIRE 1
#define MH_LRTB 2
#define MH_DIAG 3
#define MH_LVIA 6

// Routing methods

void Board::WipeAutoSetPoints(int nodeId, bool bHavePlacedWires)
{
	WIRELIST wireList;	// Helper for chains of wires

	const bool bWipeAll = ( nodeId == BAD_NODEID );
	for (int i = 0, iSize = GetSize(); i < iSize; i++)
	{
		Element* const p = GetAt(i);
		if ( !bWipeAll && p->GetNodeId() != nodeId ) continue;	// Skip points with wrong nodeId
		const bool bAllLyrs = p->GetHasPin();
		bool bWipe = p->ReadFlagBits(AUTOSET) && !p->ReadFlagBits(USERSET);
		if ( bHavePlacedWires && p->GetHasWire() )
		{
			p->GetWireList(wireList);	// Get list containing p and its wired points ...
			for (const auto& o : wireList)	// ... and disable wipe if any of them are USERSET
			{
				const Element* const pW = o.first;
				if ( pW == p ) continue;	// Skip p
				bWipe &= ( !pW->ReadFlagBits(USERSET) );
				if ( !bWipe ) break;
			}
			for (const auto& o : wireList)
			{
				Element* const pW = const_cast<Element*> (o.first);
				if ( bWipe ) SetNodeId(pW, BAD_NODEID, bAllLyrs);
				WipeFlagBits(pW, AUTOSET, bAllLyrs);
				MarkFlagBits(pW, USERSET, bAllLyrs);
			}
		}
		else
		{
			if ( bWipe ) SetNodeId(p, BAD_NODEID, bAllLyrs);
			WipeFlagBits(p, AUTOSET, bAllLyrs);
			MarkFlagBits(p, USERSET, bAllLyrs);
		}
	}
}

void Board::BuildTargetPins(int nodeId)
{
	// Populate m_targetPins with all (non-wire) component pins with the specified NodeId.
	// These are the things on the board that the routing algorithm will try and connect together.

	assert( nodeId != BAD_NODEID );
	m_targetPins.clear();
	for (int i = 0, iSize = ( GetLyrs() == 1 ) ? GetSize() : ( GetSize() / 2 ); i < iSize; i++)	// Use layer 0 only for pins
	{
		Element* const p = GetAt(i);
		if ( p->GetHasPin() && p->GetNodeId() == nodeId && !(m_bHavePlacedWires && p->GetHasWire()) )
			m_targetPins.push_back(p);
	}
}

void Board::Route(bool bMinimal)
{
#ifdef ROUTE_ALGORITHM_TIMER
	const auto start = std::chrono::steady_clock::now();
#endif

	SetHavePlacedWires();	// Set up m_bHavePlacedWires at very start of routing

	m_bRouteMinimal = bMinimal;

	// When routing is enabled,  this method will build tracks and update the cost in each NodeInfo.
	// When routing is disabled, this method will update each NodeInfo cost without building new tracks.
	if ( GetRoutingEnabled() ) WipeAutoSetPoints(BAD_NODEID, m_bHavePlacedWires);

	m_nodeInfoMgr.SortByLowestDifficulty(m_compMgr);

	const bool bRipUpEnabled = GetRoutingMethod() == 1;

	const size_t numNodes = m_nodeInfoMgr.GetSize();

	for (size_t i = 0; i < numNodes; i++)
		m_nodeInfoMgr.GetAt(i)->SetCost(UINT_MAX);	// i.e. Mark all nodesIds as unrouted

	int iPasses(0);
	bool bImproved(true), bAllowRipUp( bRipUpEnabled && GetRoutingEnabled() );
	while ( bImproved )
	{
		bImproved = false;	// Gets set true if we manage to lower any route costs on this pass

		// Allowing multiple passes fixes some simplistic cases but kills speed. So only do one pass.
		// The code for multiple passes has been left in place for future tests.
		iPasses++; if ( iPasses > 1 ) break;

		for (size_t i = 0; i < numNodes; i++)	// Loop all nodeIds used by components
		{
			NodeInfo* const pI = m_nodeInfoMgr.GetAt(i);
			if ( pI->GetCost() == 0 ) continue;	// Skip if fully routed

			const int& nodeIdI = pI->GetNodeId();

			// Flood with MH values, starting from the m_targetPins
			const unsigned int costI = ( nodeIdI != BAD_NODEID ) ? Flood(nodeIdI) : UINT_MAX;
			if ( costI < pI->GetCost() )
			{
				pI->SetCost( costI );
				bImproved = true;
			}

			if ( bAllowRipUp && nodeIdI != BAD_NODEID && pI->GetCost() > 0 && i > 0 )
			{
				TrackElementGrid Ibest, Iripped;

				CopyTo(Ibest);

				WipeAutoSetPoints(nodeIdI, m_bHavePlacedWires);	// Rip-up I

				CopyTo(Iripped);

				size_t j(i-1);	// Loop j through previously routed nodeIds
				while( pI->GetCost() > 0 )
				{
					NodeInfo* const pJ = m_nodeInfoMgr.GetAt(j);
					const int& nodeIdJ = pJ->GetNodeId();
					if ( pJ->GetCost() == 0 )	// Only consider J if it is fully routed
					{
						WipeAutoSetPoints(nodeIdJ, m_bHavePlacedWires);	// Rip-up J

						const unsigned int costI = Flood(nodeIdI);	// Route I ...
						if ( costI < pI->GetCost() )				// ... and if I improved
						{
							const unsigned int costJ = Flood(nodeIdJ);	// Route J ...
							if ( costJ == 0 )							// ... and if J is still fully routed
							{
								bImproved = true;

								pI->SetCost( costI );	// Update cost I

								if ( costI > 0 )		// If we've not solved I ...
									CopyTo(Ibest);		// ... log the improved route (it's the best so far)
							}
						}
						if ( pI->GetCost() > 0 )
							CopyFrom(Iripped);	// Revert to ripped-up I
					}
					if ( j == 0 ) break; else j--;
				}
				if ( pI->GetCost() > 0 )
					CopyFrom(Ibest);
			}
		}
		if ( !bAllowRipUp ) break;
	}

	RebuildAdjacencies();

#ifdef ROUTE_ALGORITHM_TIMER
	const auto elapsed = std::chrono::steady_clock::now() - start;
	const auto duration_ms	= std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
	std::cout << "Time : " << duration_ms << std::endl;
#endif
}

void Board::UpdateVias()	// Sets the via flag to true on all candidate vias
{
	m_bRouteMinimal	= true;
	m_bHasVias = false;	// Reset the flag indicating if the board has routed vias (as opposed to "wires-as-tracks" vias)

	if ( GetViasEnabled() )
	{
		// If the ends of a candidate via can be connected through a pin on the board then it is not a via.
		const bool bRoutingEnabled	= GetRoutingEnabled();	// Log routing state
		SetRoutingEnabled(false);	// Don't build tracks while performing test
		SetViasEnabled(false);		// Disable routing through vias to perform test

		// Store existing MH values and route IDs (used for showing connected areas) because Flood() will wipe them
		std::vector<unsigned int> iMH;	iMH.resize( GetSize() );
		std::vector<unsigned int> iRID;	iRID.resize( GetSize() );
		for (int i = 0, iSize = GetSize(); i < iSize; i++)
		{
			Element* const p = GetAt(i);
			iMH[i]	= p->GetMH();
			iRID[i]	= p->GetRouteId();
		}

		SetHavePlacedWires();	// Set up m_bHavePlacedWires before calling Flood() in loop below

		for (int i = 0, iSize = ( GetLyrs() == 1 ) ? GetSize() : ( GetSize() / 2 ); i < iSize; i++)	// Loop layer 0 only
		{
			Element* const p = GetAt(i);
			Element* const q = p->GetNbr(NBR_X);
			bool bIsVia(false);
			if ( q && !p->GetHasPin() && p->GetNodeId() == q->GetNodeId() && p->GetNodeId() != BAD_NODEID )	// If candidate via ...
			{
				m_targetPins.clear();
				m_targetPins.push_back(p);
				m_targetPins.push_back(q);
				bIsVia = Flood() > 0;
				m_bHasVias |= bIsVia;	// Update m_bHasVias
			}
			p->SetIsVia(bIsVia);
		}

		// Restore MH values and route IDs
		for (int i = 0, iSize = GetSize(); i < iSize; i++)
		{
			Element* const p = GetAt(i);
			p->SetMH( iMH[i] );
			p->SetRouteId( iRID[i] );
		}

		SetViasEnabled(true);				// Restore vias state
		SetRoutingEnabled(bRoutingEnabled);	// Restore routing state
	}
	else
	{
		// No test needed.  Just wipe the via flag on the base elements
		for (int i = 0, iSize = ( GetLyrs() == 1 ) ? GetSize() : ( GetSize() / 2 ); i < iSize; i++)	// Loop layer 0 only
			GetAt(i)->SetIsVia(false);
	}
	CalcMIN_SEPARATION();
}

unsigned int Board::Flood(int iFloodNodeId)
{
	BuildTargetPins(iFloodNodeId);	// Populate m_targetPins
	return Flood();
}

unsigned int Board::Flood(bool bSingleRoute)
{
	// Flood the board with MH values, starting from the m_targetPins.
	// The return value is a cost that shows how unconnected the pins are.
	// Zero cost means the pins are all inter-connected.

	const size_t N = m_targetPins.size();	assert( bSingleRoute == ( N == 1) );

	if ( !bSingleRoute && N < 2 ) return 0;	// Return cost of zero

	// Allocate the connection matrix to indicate which pairs of targetPins are connected
	m_connectionMatrix.Allocate( N );

	// All target pins that support "flying wires" are connected to each other
	for (size_t j = 0; j < N; j++)
		if ( GetAllowFlyWire( m_targetPins[j] ) )
			for (size_t k = j+1; k < N; k++)
				if ( GetAllowFlyWire( m_targetPins[k] ) )
					m_connectionMatrix.Connect(j, k);	// Make j-k connection and enforce transitivity

	if ( m_bRouteMinimal )	// For minimal routing, first do a preliminary flood to see which pins are connected
		Flood_Helper(false);	// false ==> don't build new tracks

	if ( GetRoutingEnabled() )
		Flood_Helper(true);	// true ==> build new tracks

	// Give connected target pins the same route ID (so we can draw a limited set of air-wires).
	// We don't care about route IDs at other elements, so leave them as they are.
	for (size_t j = 0; j < N; j++)
	for (size_t k = j+1; k < N; k++)
		if ( m_connectionMatrix.GetAreConnected(j, k) )
			m_targetPins[k]->SetRouteId( m_targetPins[j]->GetRouteId() );

	m_connectionMatrix.DeAllocate();
	return m_connectionMatrix.GetCost();
}

void Board::Flood_Helper(const bool bBuildTracks)
{
	WIRELIST wireList;	// Helper for chains of wires

	for (int i = 0, iSize = GetSize(); i < iSize; i++)	// Loop all grid points
		GetAt(i)->ResetMH();	// Wipe RouteId. Set "infinite" MH distance.  Zero max MH parameter.

	m_tmpVec.resize(static_cast<size_t>(GetSize()), nullptr);
	m_tmpVecSize = 0;			// Clear the set of visited points

	const size_t N = m_targetPins.size();
	m_growingRoutes.resize(N);	// Allocate flags to track route growth from pins

	unsigned int iMH(0), iMaxMH(0), iMHlastGrowthCheck(0);

	// Add each target pin to the set of visited points, with a unique routeId, and MH value of zero
	unsigned int iRouteID(BAD_ROUTEID);
	for (auto p : m_targetPins)
	{
		iRouteID++;	assert( iRouteID < BAD_ROUTEID );	// Should be safely < UINT_MAX in practice
		UpdateMH(p, iRouteID, iMH, iMaxMH);				// Add p to set of visited points
	}

	const bool			bMultiLayer	 = GetLyrs() > 1;
	const bool			bViasEnabled = bMultiLayer && GetViasEnabled();
	const int&			iFloodNodeId = m_targetPins[0]->GetNodeId();
	const bool			bDiagsOK	 = ( GetDiagsMode() != DIAGSMODE::OFF );
	const unsigned int	iMaxDeltaMH	 = ( bViasEnabled ) ? MH_LVIA : bDiagsOK ? MH_DIAG : MH_LRTB;	// The max MH increment in single-layer mode depends on if diagonals are allowed

	// If any of the target pins are wires, we have to handle those first
	for (size_t n = 0; n < N && m_bHavePlacedWires; n++)
	{
		Element* const p = m_targetPins[n];
		if ( !p->GetHasWire() ) continue;
		const int iRID = p->GetRouteId();
		p->GetWireList(wireList);	// Get list of p and its wired points
		for (const auto& o : wireList)
		{
			Element* const pW = const_cast<Element*> (o.first);
			if ( pW == p ) continue;	// Skip p
			assert( p->GetNodeId() == pW->GetNodeId() );			// Sanity check
			if ( pW->GetMH() != BAD_MH ) continue;					// Don't overwrite visited points (even if MH is improved)
			const unsigned int iOtherMH = iMH + MH_WIRE * o.second;	// Each wire increases MH by MH_WIRE
			UpdateMH(pW, iRID, iOtherMH, iMaxMH);					// Add pW to set of visited points
		}
	}

	std::fill(m_growingRoutes.begin(), m_growingRoutes.end(), false);	// Clear flags tracking route growth from pins

	size_t jjStart(0);

	if ( !bMultiLayer ) iMH = MH_LRTB - 1;	// Set iMH so it's incremented to MH_LRTB on loop entry

	bool bDone(false);
	while( !bDone )
	{
		iMH++;	// Increase MH (think of this as distance from start points).

		// Now see what visited points have an MH value that is "one step away" from this target value.
		// For visited wires, both wires-ends are in the set of visited points.
		// Therefore we only need to consider the 8 neighbours for each visited point.

		if ( iMH == BAD_MH ) break;					// Quit if iMH reaches "infinity"
		if ( iMH > iMaxMH + iMaxDeltaMH ) break;	// Can't reach out further from the set of visited points

		const size_t jjSize = m_tmpVecSize;	// m_tmpVecSize gets modified in loop so take a copy
		for (size_t jj = jjStart; jj < jjSize && !bDone; jj++)	// Loop through visited points
		{
			Element* const pJ = m_tmpVec[jj];

			if ( pJ->GetMaxMH() + iMaxDeltaMH < iMH )	// If pJ (and all previous points) are too far from the flood boundary
			{
				jjStart = jj + 1;						// ... then skip them from now on (since iMH only ever increases)
				continue;
			}

			const int iTypeMin(bMultiLayer ? 0 : 1), iTypeMax(bViasEnabled ? 2 : 1);
			for (int iType = iTypeMin; iType <= iTypeMax && !bDone; iType++)
			{
				switch( iType )
				{
					case 0:	// Type 0 ==> Change layer at a pin
						if ( pJ->GetMH() + MH_LPIN == iMH && pJ->GetHasPin() )
							Flood_Grow(iFloodNodeId, pJ, NBR_X, bBuildTracks, iMH, iMaxMH, bDone);
						break;
					case 1:	// Type 1 ==> Move within layer
						for (int iDiag = 0, iDiagMax = ( bDiagsOK ) ? 2 : 1; iDiag < iDiagMax && !bDone; iDiag++)	// First pass ==> Non-diagonal nbrs.  Second pass diagonal nbrs
						{
							const unsigned int iDeltaMH = ( iDiag ) ? MH_DIAG : MH_LRTB;
							if ( pJ->GetMH() + iDeltaMH != iMH ) continue;	// pJ has wrong MH for connection

							for (int iNbr = iDiag; iNbr < 8 && !bDone; iNbr += 2)	// Even/Odd iNbr ==> Non-diagonal/Diagonal
								Flood_Grow(iFloodNodeId, pJ, iNbr, bBuildTracks, iMH, iMaxMH, bDone);
						}
						break;
					case 2:	// Type 2 ==> Change layer at a via
						if ( pJ->GetMH() + MH_LVIA == iMH && !pJ->GetHasPin() )
							Flood_Grow(iFloodNodeId, pJ, NBR_X, bBuildTracks, iMH, iMaxMH, bDone);
						break;
				}
			}
		}

		// Periodically (every sufficiently large MH increase) examine which routes have grown.
		// If all growing routes are connected to each other then we're done.
		if ( N > 1 && !bDone && iMH >= iMHlastGrowthCheck + iMaxDeltaMH + (m_bHavePlacedWires ? MH_WIRE : 0) )
		{
			bDone = true;
			for (size_t i = 0; i < N && bDone; i++)
				if ( m_growingRoutes[i] )
					for (size_t j = i+1; j < N && bDone; j++)
						if ( m_growingRoutes[j] )
							bDone = m_connectionMatrix.GetAreConnected(i,j);
			if ( !bDone )
			{
				iMHlastGrowthCheck = iMH;
				std::fill(m_growingRoutes.begin(), m_growingRoutes.end(), false);	// Clear flags tracking route growth from pins
			}
		}
	}
}

void Board::Flood_Grow(int iFloodNodeId, Element* const pJ, int iNbr, bool bBuildTracks, unsigned int& iMH, unsigned int& iMaxMH, bool& bDone)
{
	WIRELIST wireList;	// Helper for chains of wires

	Element* const pK = pJ->GetNbr(iNbr);	assert( pK );
	if ( pK == nullptr ) return;

	const bool			bOK	= pJ->GetNodeId() == iFloodNodeId;	// true ==> pJ already painted with correct NodeId
	const unsigned int&	j	= pJ->GetRouteId();
	const unsigned int&	k	= pK->GetRouteId();

	const bool bDirOK = ( bOK && pJ->GetUsed(iNbr) ) ||	// i.e. if already painted with correct nodeId
						( bBuildTracks && pJ->HaveNoBlankPins(iNbr) && !pJ->IsBlocked(iNbr, iFloodNodeId) && !(m_bHavePlacedWires && pJ->IsUselessWire(iNbr, iFloodNodeId)) );
	if ( !bDirOK ) return;

	if ( pK->GetMH() == BAD_MH ) // Grow route with ID j (from pJ to pK)
	{
		const int& nodeId = pK->GetNodeId();
		if ( nodeId == iFloodNodeId || nodeId == BAD_NODEID )
		{
			UpdateMH(pK, j, iMH, iMaxMH);	// Add pK to set of visited points

			const bool bWire = m_bHavePlacedWires && pK->IsLayer0() && pK->GetHasWire();	// Constrain wire-routing to layer 0
			if ( bWire )
			{
				pK->GetWireList(wireList);	// Get list of pK and its wired points
				for (const auto& o : wireList)	// Ideally want these in order of increasing MH
				{
					Element* const pW = const_cast<Element*> (o.first);
					if ( pW == pK ) continue;	// Skip pK
					assert( pK->GetNodeId() == pW->GetNodeId() );			// Sanity check
					if ( pW->GetMH() != BAD_MH ) continue;					// Don't overwrite visited points (even if MH is improved)
					const unsigned int iOtherMH = iMH + MH_WIRE * o.second;	// Each wire increases MH by MH_WIRE
					UpdateMH(pW, j, iOtherMH, iMaxMH);						// Add pW to set of visited points
				}
			}
		}
		return;
	}
	if ( m_connectionMatrix.GetAreConnected(j,k) ) return;

	// Routes with IDs j and k have met and don't have a connection yet ...
	if ( bBuildTracks )	// If building tracks ...
	{
		Backtrace(pJ, iFloodNodeId);					// ... trace pJ back to its source, painting iFloodNodeId along the way
		Element* const pKprev = Backtrace(pK, iFloodNodeId);	// ... trace pK back to its source, painting iFloodNodeId along the way
		// pKprev is the first backtraced element from pK.
		// If diagonal connections are allowed, and if pJ and pKprev are diagonal neighbours,
		// then remove pK from the backtrace.  This will give a direct diagonal connection between pJ and pKprev.
		const bool bDiagsOK = ( GetDiagsMode() != DIAGSMODE::OFF );
		if ( bDiagsOK && pKprev && pKprev->IsDiagNbr(pJ) )
			BacktraceErase(pK);
	}

	m_connectionMatrix.Connect(j, k);	// Make j-k connection and enforce transitivity

	if ( m_targetPins.size() > 1 )
		bDone = m_connectionMatrix.GetCost() == 0;	// Zero cost ==> done
}

Element* Board::Backtrace(Element* const pEnd, int nodeId)
{
	// Backtrace route from pEnd to point with MH = 0
	Element* pOut = nullptr;	// We will return the first traced element from pEnd (if there is one)

	Element* p = pEnd;
	if ( p->GetMH() == BAD_MH ) return pOut;

	const bool bMultiLayer	= GetLyrs() > 1;
	const bool bViasEnabled	= bMultiLayer && GetViasEnabled();
	const bool bDiagsOK		= ( GetDiagsMode() != DIAGSMODE::OFF );

	int iLastDirection(-1);	// To avoid ping-pong between layers

	unsigned int MH = p->GetMH();
	while ( true )	// Backtrace
	{
		assert( !p->GetIsHole() );

		Element* const pW0 = m_bHavePlacedWires ? p->GetW(0) : nullptr;
		Element* const pW1 = m_bHavePlacedWires ? p->GetW(1) : nullptr;
		const bool bWire	= (pW0 || pW1) && p->IsLayer0();	// Constrain wire-routing to layer 0
		const bool bHasPin	= bWire || p->GetHasPin();
		if ( !bHasPin || bWire ) // For non-pins and wires
			BacktracePaint(p, nodeId, bHasPin, bWire);	// Paint element p

		if ( !pOut && p != pEnd ) pOut = p;
		if ( MH == 0 ) return pOut;	// We're done backtracing once MH == 0

		bool bOK(false);	// Will be set true after a successful backtrace (with modified p and MH)
		// Now decide where to back trace to.

		// Check wires first...
		if ( m_bHavePlacedWires )
		{
			bOK = ( pW0 && pW0->GetMH() == MH - MH_WIRE );
			if ( bOK ) { p = pW0; MH -= MH_WIRE; continue; }
			bOK = ( pW1 && pW1->GetMH() == MH - MH_WIRE );
			if ( bOK ) { p = pW1; MH -= MH_WIRE; continue; }
		}

		for (int iLoop = 0; iLoop < 2 && !bOK; iLoop++)	// First pass to give preference to nbrs that are not wire ends
		{
			const int iTypeMin(bMultiLayer ? 0 : 1), iTypeMax(bViasEnabled ? 2 : 1);
			for (int iType = iTypeMin; iType <= iTypeMax && !bOK; iType++)
			{
				switch( iType )
				{
					case 0:	// Type 0 ==> Change layer at a pin if multi-layer routing
						if ( bMultiLayer && bHasPin && iLastDirection != NBR_X )
						{
							bOK = BacktraceHelper(p, MH, nodeId, MH_LPIN, NBR_X, iLoop);
							if ( bOK ) iLastDirection = NBR_X;
						}
						break;
					case 1:	// Type 1 ==> Move within layer
						for (int iDiag = 0, iDiagMax = ( bDiagsOK ) ? 2 : 1; iDiag < iDiagMax && !bOK; iDiag++)	// Loop for diagonal/non-diagonal directions
						{
							const unsigned int iDeltaMH = ( iDiag ) ? MH_DIAG : MH_LRTB;	// First pass ==> Non-diagonal nbrs.  Second pass diagonal nbrs
							for (int iNbr = iDiag; iNbr < 8 && !bOK; iNbr += 2)	// Even/Odd iNbr ==> Non-diagonal/Diagonal
							{
								bOK = BacktraceHelper(p, MH, nodeId, iDeltaMH, iNbr, iLoop);
								if ( bOK ) iLastDirection = iNbr;
							}
						}
						break;
					case 2:	// Type 2 ==> Change layer at a via
						if ( !bHasPin && iLastDirection != NBR_X )
						{
							bOK = BacktraceHelper(p, MH, nodeId, MH_LVIA, NBR_X, iLoop);
							if ( bOK ) iLastDirection = NBR_X;
						}
						break;
				}
			}
		}
		if ( !bOK ) break;
	}
	assert(0);	// Oh dear. Something went badly wrong !!!
	return pOut;
}

void Board::BacktracePaint(Element* const p, int nodeId, bool bHasPin, bool bWire)
{
	const bool bPaintNodeId = ( p->GetNodeId() == BAD_NODEID );	// Set NodeId if not set yet.
	if ( !bPaintNodeId && !p->ReadFlagBits(USERSET) ) return;

	assert( bPaintNodeId || p->GetNodeId() == nodeId );	// If USERSET then nodeId must be OK

	if ( bPaintNodeId )
	{
		SetNodeId(p, nodeId, bHasPin);
		WipeFlagBits(p, USERSET, bHasPin);
	}
	MarkFlagBits(p, AUTOSET, bHasPin);

	if ( bWire )
	{
		WIRELIST wireList;			// Helper for chains of wires
		p->GetWireList(wireList);	// Get list of p and its wired points
		for (const auto& o : wireList)
		{
			Element* const pW = const_cast<Element*> (o.first);
			if ( pW == p ) continue;	// Skip p
			if ( bPaintNodeId )
			{
				SetNodeId(pW, nodeId, bHasPin);
				WipeFlagBits(pW, USERSET, bHasPin);
			}
			MarkFlagBits(pW, AUTOSET, bHasPin);
		}
	}
}

void Board::BacktraceErase(Element* const p)
{
	assert( !p->GetIsHole() );
	assert( p->GetNodeId() != BAD_NODEID );

	Element* const pW0 = m_bHavePlacedWires ? p->GetW(0) : nullptr;
	Element* const pW1 = m_bHavePlacedWires ? p->GetW(1) : nullptr;
	const bool bWire		= (pW0 || pW1) && p->IsLayer0();	// Constrain wire-routing to layer 0
	const bool bHasPin		= bWire || p->GetHasPin();
	const bool bWipeNodeId	= !p->ReadFlagBits(USERSET);

	if ( bWipeNodeId )
	{
		SetNodeId(p, BAD_NODEID, bHasPin);
		WipeFlagBits(p, USERSET, bHasPin);
	}
	WipeFlagBits(p, AUTOSET, bHasPin);

	if ( bWire )
	{
		WIRELIST wireList;			// Helper for chains of wires
		p->GetWireList(wireList);	// Get list of p and its wired points
		for (const auto& o : wireList)
		{
			Element* const pW = const_cast<Element*> (o.first);
			if ( pW == p ) continue;	// Skip p
			if ( bWipeNodeId )
			{
				SetNodeId(pW, BAD_NODEID, bHasPin);
				WipeFlagBits(pW, USERSET, bHasPin);
			}
			WipeFlagBits(pW, AUTOSET, bHasPin);
		}
	}
}

bool Board::BacktraceHelper(Element*& p, unsigned int& MH, int nodeId, unsigned int iDeltaMH, int iNbr, int iLoop)
{
	Element* const pNbr = p->GetNbr(iNbr);
	if ( pNbr->GetRouteId() != p->GetRouteId() ) return false;	// Skip if nbr has wrong routeId
	const bool bWire = m_bHavePlacedWires && pNbr->IsLayer0() && pNbr->GetHasWire();	// Constrain wire-routing to layer 0
	if ( iLoop == 0 &&  bWire ) return false;					// Skip if nbr is a wire
	if ( iLoop == 1 && !bWire ) return false;					// Skip if nbr is a non-wire
	if ( p->IsBlocked(iNbr, nodeId) ) return false;				// Skip if blocked
	if ( pNbr->GetMH() != MH - iDeltaMH ) return false;			// Skip if wrong MH change
	p = pNbr;	MH -= iDeltaMH;
	return true;	// We backtraced OK, and have modified p and MH
}

void Board::Manhatten(Element* p, bool bSingleRoute)
{
	const int iTraceNodeId = p->GetNodeId();	// The NodeID to trace
	if ( iTraceNodeId == BAD_NODEID ) return;	// Don't trace invalid NodeID

	const bool bRouteMinimal	= m_bRouteMinimal;		// Log m_bRouteMinimal state
	const bool bRoutingEnabled	= GetRoutingEnabled();	// Log routing state

	SetHavePlacedWires();	// Set up m_bHavePlacedWires so we can skip calls to GetHasWire() here, and in Flood()

	m_bRouteMinimal	= true;
	SetRoutingEnabled(false);	// Don't build tracks

	m_targetPins.clear();

	if ( p->GetHasPin() )	// p could be a wire end
		m_targetPins.push_back( p->IsLayer0() ? p : p->GetNbr(NBR_X) );	// Always want target pins on layer 0
	else
		m_targetPins.push_back(p);

	// If we haven't specified bSingleRoute, then build additional routes from all true component pins
	for (int i = 0, iSize = ( GetLyrs() == 1 ) ? GetSize() : ( GetSize() / 2 ); i < iSize && !bSingleRoute; i++)	// Use layer 0 only for pins
	{
		Element* const q = GetAt(i);
		if ( q == p ) continue;	// Skip self
		if ( q && q->GetHasPin() && !(m_bHavePlacedWires && q->GetHasWire()) && q->GetNodeId() == iTraceNodeId )	// Skip wires
			m_targetPins.push_back(q);
	}

	Flood(bSingleRoute);

	SetRoutingEnabled(bRoutingEnabled);	// Restore routing state
	m_bRouteMinimal	= bRouteMinimal;	// Restore m_bRouteMinimal state

	// Update m_iConnPin and m_iConnRID.
	for (int i = 0, iSize = GetSize(); i < iSize && m_iConnPin == -1; i++)
	{
		Element* const q = GetAt(i);
		if ( q->GetMH() != BAD_MH && q->GetHasPin() && !(m_bHavePlacedWires && q->GetHasWire()) )
		{
			m_iConnPin = i;
			m_iConnRID = q->GetRouteId();
		}
	}
}

Element* Board::GetConnPin()
{
	return ( m_iConnPin >= 0 && m_iConnPin < GetSize() ) ? GetAt(m_iConnPin) : nullptr;
}

unsigned int Board::GetConnRID() const
{
	return ( m_iConnPin >= 0 && m_iConnPin < GetSize() ) ? m_iConnRID : BAD_ROUTEID;
}

void Board::CheckAllComplete()
{
	assert( !GetRoutingEnabled() );	// If routing is enabled, use the "RoutedOK" flags instead of the "Complete" flags.

	// Calling Route() when routing is not enabled sets the cost info without building new tracks.
	// So we can use that to set the "Complete" flags
	Route(true);
	for (size_t n = 0, nSize = m_nodeInfoMgr.GetSize(); n < nSize; n++)
	{
		NodeInfo* const pNodeInfo = m_nodeInfoMgr.GetAt(n);
		pNodeInfo->SetComplete( pNodeInfo->GetCost() == 0 );
	}
	m_nodeInfoMgr.SortByLowestDifficulty(m_compMgr);
}

void Board::PasteTracks(bool bTidy)
{
	assert( GetRoutingEnabled() != bTidy );

	if ( bTidy )
	{
		SetRoutingEnabled(true);
		Route(false);	// false ==> non minimal routing
	}

	size_t	iPinIndex;
	int		tmpCompId;

	// If we are doing a paste (not a tidy) and have a placed trax component, only paste within it then destroy the trax component
	Component&	trax		= m_compMgr.GetTrax();
	const bool	bRestrict	= !bTidy && ( trax.GetSize() > 0 && trax.GetIsPlaced() );
	const int	lyrs		= bRestrict ? trax.GetLyr() + 1  : GetLyrs();
	const int	rows		= bRestrict ? trax.GetCompRows() : GetRows();
	const int	cols		= bRestrict ? trax.GetCompCols() : GetCols();

	for (int k = bRestrict ? trax.GetLyr() : 0;				k < lyrs; k++)
	for (int j = 0, jRow = bRestrict ? trax.GetRow() : 0;	j < rows; j++, jRow++)
	for (int i = 0, iCol = bRestrict ? trax.GetCol() : 0;	i < cols; i++, iCol++)
	{
		if ( bRestrict && !trax.GetCompElement(j,i)->ReadFlagBits(RECTSET) ) continue;	// Skip points outside grey area

		Element* const	p		= Get(k, jRow, iCol);
		const bool		bHasPin	= p->GetHasPin();

		// Tidy clears all non-pins and wires that are USER_SET ...
		if ( bTidy && ( !bHasPin || p->GetHasWire() ) && p->ReadFlagBits(USERSET) && !p->ReadFlagBits(AUTOSET|VEROSET) )
		{
			SetNodeId(p, BAD_NODEID, bHasPin);
			for (int iSlot = 0; iSlot < 2; iSlot++)
			{
				Element* const pW = p->GetW(iSlot);
				if ( pW ) SetNodeId(pW, BAD_NODEID, bHasPin);
			}
		}

		// Make the point USERSET
		WipeFlagBits(p, AUTOSET|VEROSET, bHasPin);
		MarkFlagBits(p, USERSET, bHasPin);

		// For wires, the "Paste" operation either paints the board at the wire-ends or wipes it.
		// Fix-up the nodeId info on any wire components ...
		const int& nodeId = p->GetNodeId();
		for (int iSlot = 0; iSlot < 2; iSlot++)
		{
			Element* const pW = p->GetW(iSlot);
			if ( pW == nullptr ) continue;

			p->GetSlotInfo(iSlot, iPinIndex, tmpCompId);
			Component& comp = m_compMgr.GetComponentById( tmpCompId );
			assert( comp.GetType() != COMP::INVALID );
			for (size_t i = 0, iSize = comp.GetNumPins(); i < iSize; i++)
			{
				comp.SetNodeId(i, nodeId);
				for (int lyr = 0; lyr < 2; lyr++) comp.SetOrigId(lyr, i, nodeId);
			}
		}
	}
	if ( bRestrict )
	{
		m_compMgr.ClearTrax();
		m_rectMgr.Clear();
	}
	if ( !bRestrict )
		SetRoutingEnabled(false);
}

void Board::WipeTracks()
{
	FloatAllComps();	// Float all components

	// If we have a placed trax component, only wipe the board within it then destroy the trax component
	Component&	trax		= m_compMgr.GetTrax();
	const bool	bRestrict	= ( trax.GetSize() > 0 && trax.GetIsPlaced() );
	const int	lyrs		= bRestrict ? trax.GetLyr() + 1  : GetLyrs();
	const int	rows		= bRestrict ? trax.GetCompRows() : GetRows();
	const int	cols		= bRestrict ? trax.GetCompCols() : GetCols();

	for (int k = bRestrict ? trax.GetLyr() : 0;				k < lyrs; k++)
	for (int j = 0, jRow = bRestrict ? trax.GetRow() : 0;	j < rows; j++, jRow++)
	for (int i = 0, iCol = bRestrict ? trax.GetCol() : 0;	i < cols; i++, iCol++)
	{
		if ( bRestrict && !trax.GetCompElement(j,i)->ReadFlagBits(RECTSET) ) continue;	// Skip points outside grey area
		Element* const p = Get(k, jRow, iCol);
		assert( !p->GetHasPin() && !p->GetIsHole() && !p->GetHasComp() );	// Sanity check

		SetNodeId(p, BAD_NODEID, !bRestrict);
		p->SetSurface(SURFACE_FREE);
		WipeFlagBits(p, bRestrict ? (AUTOSET|VEROSET|RECTSET) : (AUTOSET|VEROSET), !bRestrict);
		MarkFlagBits(p, USERSET, !bRestrict);
	}
	if ( bRestrict )
	{
		m_compMgr.ClearTrax();
		m_rectMgr.Clear();
	}
	if ( !bRestrict )
		SetRoutingEnabled(false);
	PlaceFloaters();	// Unfloat components
}
