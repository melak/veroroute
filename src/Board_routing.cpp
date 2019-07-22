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

#define WIRE_MH 1

// Routing methods

void Board::WipeAutoSetPoints()
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

void Board::BuildTargetPins(const int& nodeId)
{
	// Populate m_targetPins with all (non-wire) component pins with the specified NodeId;
	// These are the things on the board that the routing algorithm will try and connect together.

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

void Board::Route()
{
	WipeAutoSetPoints();

	m_nodeInfoMgr.SortByLowestDifficulty(m_compMgr);

	const size_t numNodes = m_nodeInfoMgr.GetSize();
	for (size_t ii = 0; ii < numNodes; ii++)	// Loop all nodeIds used by components
	{
		const int& nodeId = m_nodeInfoMgr.GetAt(ii)->GetNodeId();
		if ( nodeId != BAD_NODEID )
			RouteNodeId(nodeId);	// Try route all pins of the specified nodeId
	}
}

void Board::RouteNodeId(const int& iTraceNodeId)	// Try route all pins of the specified nodeId
{
	assert(iTraceNodeId != BAD_NODEID);

	BuildTargetPins(iTraceNodeId);	// Populate m_targetPins
	if ( m_targetPins.size() < 2 ) return;

	const int iSize = GetSize();
	for (int i = 0; i < iSize; i++)	// Loop all grid points
	{
		Element* p = GetAt(i);
		p->SetRouteId(BAD_ROUTEID);		// Wipe RouteId at point
		p->SetMH(BAD_MH);				// Set "infinite" MH distance
		p->SetMaxMH(0);					// Zero max MH algorithm parameter
	}

	m_tmpVec.resize(iSize, nullptr);	// Clear the set of visited points
	m_tmpVecSize = 0;

	// Add each target pin to the set of visited points, with a unique routeId, and MH value of zero
	unsigned int RID(BAD_ROUTEID);
	for (auto p : m_targetPins)
	{
		m_tmpVec[m_tmpVecSize++] = p;	// Add p to set of visited points
		RID++;	assert(RID < BAD_ROUTEID);	// Should be safely < UINT_MAX in practice
		p->SetRouteId(RID);
		p->SetMH(0);
	}

	const unsigned int numRIDs = RID + 1;	assert( m_targetPins.size() == (size_t) numRIDs );

	// Set up the connection matrix ppConn[][] to indicate which pairs of targetPins are connected
	const size_t connSize = numRIDs * numRIDs;
	bool*	pConn	= new bool[connSize];
	bool**	ppConn	= new bool*[numRIDs];
	memset(pConn, 0, connSize * sizeof(bool));
	for (size_t i = 0; i < numRIDs; i++) ppConn[i] = pConn + i * numRIDs;
	for (size_t i = 0; i < numRIDs; i++) ppConn[i][i] = true;	// Each pin is connected to itself

	typedef std::pair<unsigned int, unsigned int> CONNECTION;
	std::list<CONNECTION> list;	// Helper for updating the connection matrix

	const bool bDiagsOK = ( GetDiagsMode() != DIAGSMODE::OFF );
	const unsigned int iMaxDeltaMH = ( bDiagsOK ) ? 3 : 2;	// The max MH increment depends on if diagonals are allowed

	size_t jjStart(0);
	unsigned int iMH(0), iMaxMH(0);

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
			Element* pJ = m_tmpVec[jj];
			const unsigned int& j = pJ->GetRouteId();

			if ( pJ->GetMaxMH() + iMaxDeltaMH < iMH )	// If pJ (and all previous points) are too far from the flood boundary
			{
				jjStart = jj + 1;						// ... then skip them from now on (since iMH only ever increases)
				continue;
			}

			const bool bOK = pJ->GetNodeId() == iTraceNodeId;	// true ==> pJ already painted with correct NodeId

			const int iDiagMax = ( bDiagsOK ) ? 2 : 1;		// Diags allowed ==> 2 passes
			for (int iDiag = 0; iDiag < iDiagMax && !bDone; iDiag++)	// First pass ==> Non-diagonal nbrs.  Second pass diagonal nbrs
			{
				const int iDeltaMH = 2 + iDiag;
				if ( pJ->GetMH() + iDeltaMH != iMH ) continue;	// pJ has wrong MH for (Non-diagonal/Diagonal) connection

				// Visit pJ's neighbours
				for (int iNbr = iDiag; iNbr < 8 && !bDone; iNbr += 2)	// Even/Odd iNbr ==> Non-diagonal/Diagonal
				{
					Element* pK = pJ->GetNbr(iNbr);
					const unsigned int& k = pK->GetRouteId();

					const bool bDirOK = ( bOK && pJ->GetUsed(iNbr) ) ||
										( pJ->HaveNonBlankPins(iNbr) && !pJ->IsBlocked(iNbr, iTraceNodeId) && !pJ->IsUselessWire(iNbr, iTraceNodeId) );
					if ( !bDirOK ) continue;

					if ( pK->GetMH() == BAD_MH ) // Grow route with RID j (from pJ to pK)
					{
						const int& nodeId = pK->GetNodeId();
						if ( nodeId == iTraceNodeId || nodeId == BAD_NODEID )
						{
							m_tmpVec[m_tmpVecSize++] = pK;	// Add pK to set of visited points
							iMaxMH = std::max(iMaxMH, iMH);	// Update iMaxMH
							pK->SetRouteId(j);
							pK->SetMH(iMH);
							pK->SetMaxMH(iMaxMH);
							
							Element* pW = pK->GetW();	// The other end of the wire (if any)
							if ( pW )
							{
								assert( pK->GetNodeId() == pW->GetNodeId() );	// Sanity check
								const unsigned int iOtherMH = iMH + WIRE_MH;	// Wires always increase MH by WIRE_MH
								m_tmpVec[m_tmpVecSize++] = pW;					// Add pW to set of visited points
								iMaxMH = std::max(iMaxMH, iOtherMH);			// Update iMaxMH
								pW->SetRouteId(j);
								pW->SetMH(iOtherMH);
								pW->SetMaxMH(iMaxMH);
							}
						}
					}
					else if ( j != k )	// Routes with RIDs j and k have met ...
					{
						if ( !ppConn[j][k] )	// If no j-k connection yet ...
						{
							Backtrace(pJ, iTraceNodeId);	// Trace pJ back to its source, painting iTraceNodeId along the way
							Backtrace(pK, iTraceNodeId);	// Trace pK back to its source, painting iTraceNodeId along the way

							// Make j-k connection and enforce transitivity
							assert( list.empty() );
							list.push_back( CONNECTION(j,k) );
							while ( !list.empty() )
							{
								auto iter = list.begin();	// Read info from first list entry ...
								const auto a = iter->first;
								const auto b = iter->second;
								list.erase( iter );			// ... then remove the list entry

								if ( !ppConn[a][b] )	// If no a-b connection ...
								{
									ppConn[a][b] = ppConn[b][a] = true;	// Make a-b connection ...

									for (unsigned int c = 0; c < numRIDs; c++)	// Update 1st-order transitive relations
									{
										if ( c == a || c == b ) continue;
										if ( ppConn[a][c] ) list.push_back( CONNECTION(b,c) );	// a-c connection ==> b-c connection
										if ( ppConn[b][c] ) list.push_back( CONNECTION(a,c) );	// b-c connection ==> a-c connection
									}
								}
							}

							// If the connection matrix is totally filled, then we're done
							bDone = true;
							for (size_t i = 0; i < numRIDs && bDone; i++)
								for (size_t j = i + 1; j < numRIDs && bDone; j++)
									bDone = ppConn[i][j];
						}
					}
				}
			}
		}
	}
	// Deallocate connection matrix
	delete[] ppConn;
	delete[] pConn;
}

// Backtrace route from pEnd to point with MH = 0
void Board::Backtrace(Element* pEnd, const int& nodeId)
{
	Element* p = pEnd;
	if ( p->GetMH() == BAD_MH ) return;

	const bool bDiagsOK = ( GetDiagsMode() != DIAGSMODE::OFF );

	unsigned int MH = p->GetMH();
	while ( true )	// Backtrace
	{
		Element* pW = p->GetW();
		assert( !p->GetIsHole() );

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
			const int iDiagMax = ( bDiagsOK ) ? 2 : 1;		// Diags allowed ==> 2 passes
			for (int iDiag = 0; iDiag < iDiagMax; iDiag++)	// First pass ==> Non-diagonal nbrs.  Second pass diagonal nbrs
			{
				const int iDeltaMH = 2 + iDiag;
				for (int iNbr = iDiag; iNbr < 8 && !bOK; iNbr += 2)	// Even/Odd iNbr ==> Non-diagonal/Diagonal
				{
					Element* pNbr = p->GetNbr(iNbr);
					if ( pNbr->GetRouteId() != p->GetRouteId() ) continue;	// Skip if nbr has wrong routeId
					if ( iLoop == 0 &&  pNbr->GetW() ) continue;			// Skip if nbr is a wire
					if ( iLoop == 1 && !pNbr->GetW() ) continue;			// Skip if nbr is a non-wire
					if ( !p->IsBlocked(iNbr, nodeId) && pNbr->GetMH() == MH - iDeltaMH )
					{
						p = pNbr;	MH -= iDeltaMH;	bOK = true;
					}
				}
			}
		}
		if ( bOK ) continue;

		assert(0);	// Oh dear. Something went badly wrong !!!
		break;
	}
}

unsigned int Board::Manhatten(Element* p)
{
	// Populate the grid with connected Manhatten-style "distances" to p.
	// Horizontally/Vertically adjacent grid points give a "distance" of 2.
	// Diagonally adjacent grid points give a "distance" of 3.
	// Wires give a "distance" of WIRE_MH regardless of their length.

	const int iTraceNodeId = p->GetNodeId();			// The NodeID to trace
	if ( iTraceNodeId == BAD_NODEID ) return BAD_MH;	// Don't trace invalid NodeID

	const int iSize = GetSize();
	for (int i = 0; i < iSize; i++)	// Loop all grid points
	{
		Element* p = GetAt(i);
		p->SetMH(BAD_MH);	// Set "infinite" MH distance
		p->SetMaxMH(0);		// Zero max MH algorithm parameter
	}

	m_tmpVec.resize(iSize, nullptr);	// Clear the set of visited points
	m_tmpVecSize = 0;

	const bool bDiagsOK = ( GetDiagsMode() != DIAGSMODE::OFF );
	const unsigned int iMaxDeltaMH = ( bDiagsOK ) ? 3 : 2;	// The max MH increment depends on if diagonals are allowed

	size_t jjStart(0);
	unsigned int iMH(0), iMaxMH(0);

	// Add p to set of visited points, with MH value of zero
	m_tmpVec[m_tmpVecSize++] = p;
	p->SetMH(0);
	Element* pW = p->GetW();	// ... and the other end of the wire (if any)
	if ( pW )
	{
		assert( p->GetNodeId() == pW->GetNodeId() );	// Sanity check
		const unsigned int iOtherMH = iMH + WIRE_MH;	// Wires always increase MH by WIRE_MH
		m_tmpVec[m_tmpVecSize++] = pW;					// Add pW to set of visited points
		iMaxMH = std::max(iMaxMH, iOtherMH);			// Update iMaxMH
		pW->SetMH(iOtherMH);
		pW->SetMaxMH(iMaxMH);
	}

	while ( true )
	{
		iMH++;	// Increase MH (think of this as distance from start point).

		// Now see what visited points have an MH value that is "one step away" from this target value.
		// For visited wires, both wires-ends are in the set of visited points.
		// Therefore we only need to consider the 8 neighbours for each visited point.

		if ( iMH == BAD_MH ) break;					// Quit if iMH reaches "infinity"
		if ( iMH > iMaxMH + iMaxDeltaMH ) break;	// Can't reach out further from the set of visited points

		const size_t jjSize = m_tmpVecSize;	// m_tmpVecSize gets modified in loop so take a copy
		for (size_t jj = jjStart; jj < jjSize; jj++)	// Loop through visited points
		{
			const Element* pJ = m_tmpVec[jj];
			if ( pJ->GetNodeId() != iTraceNodeId ) continue;	// Wrong nodeId

			if ( pJ->GetMaxMH() + iMaxDeltaMH < iMH )	// If pJ (and all previous points) are too far from the flood boundary
			{
				jjStart = jj + 1;						// ... then skip them from now on (since iMH only ever increases)
				continue;
			}

			const int iDiagMax = ( bDiagsOK ) ? 2 : 1;		// Diags allowed ==> 2 passes
			for (int iDiag = 0; iDiag < iDiagMax; iDiag++)	// First pass ==> Non-diagonal nbrs.  Second pass diagonal nbrs
			{
				const int iDeltaMH = 2 + iDiag;
				if ( pJ->GetMH() + iDeltaMH != iMH ) continue;	// pJ has wrong MH for connection

				// Visit pJ's neighbours
				for (int iNbr = iDiag; iNbr < 8; iNbr += 2)	// Even/Odd iNbr ==> Non-diagonal/Diagonal
				{
					Element* pK = pJ->GetNbr(iNbr);
					if ( pJ->GetUsed(iNbr) && pK->GetMH() == BAD_MH )
					{
						m_tmpVec[m_tmpVecSize++] = pK;	// Add pK to set of visited points
						iMaxMH = std::max(iMaxMH, iMH);	// Update iMaxMH
						pK->SetMH(iMH);
						pK->SetMaxMH(iMaxMH);

						Element* pW = pK->GetW();	// The other end of the wire (if any)
						if ( pW )
						{
							assert( pK->GetNodeId() == pW->GetNodeId() );	// Sanity check
							const unsigned int iOtherMH = iMH + WIRE_MH;	// Wires always increase MH by WIRE_MH
							m_tmpVec[m_tmpVecSize++] = pW;					// Add pW to set of visited points
							iMaxMH = std::max(iMaxMH, iOtherMH);			// Update iMaxMH
							pW->SetMH(iOtherMH);
							pW->SetMaxMH(iMaxMH);
						}
					}
				}
			}
		}
	}
	return BAD_MH;
}

void Board::CheckAllComplete()
{
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

			Manhatten(pI);

			for (auto iterJ = iterI; iterJ != iterEnd && bComplete; ++iterJ)
			{
				Element* pJ = *iterJ;
				bComplete = ( pJ->GetMH() != BAD_MH );
			}
		}
		pNodeInfo->SetIsComplete(bComplete);
	}
}

void Board::PasteTracks(bool bTidy)
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

void Board::WipeTracks()
{
	FloatAllComps();	// Float all components

	// If we have a placed trax component, only wipe the board within it then destroy it
	Component& trax = m_compMgr.GetTrax();
	if ( trax.GetSize() > 0 && trax.GetIsPlaced() )
	{
		const int	rowTL		= trax.GetRow();
		const int	colTL		= trax.GetCol();
		const int&	compCols	= trax.GetCompCols();
		const int&	compRows	= trax.GetCompRows();

		int jRow(rowTL);
		for (int j = 0; j < compRows; j++, jRow++)
		{
			int iCol(colTL);
			for (int i = 0; i < compCols; i++, iCol++)
			{
				if ( !trax.GetCompElement(j,i)->ReadFlagBits(RECTSET) ) continue;
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
