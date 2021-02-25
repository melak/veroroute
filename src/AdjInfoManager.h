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

#include "AdjInfo.h"

// Manager class to record the adjacency info for all NodeIds on the board (NOT floating components)

class AdjInfoManager
{
public:
	AdjInfoManager() {}
	~AdjInfoManager() { DeAllocate(); }
	void DeAllocate()
	{
		for (auto& o : m_mapIdtoAdjInfo) delete o.second;
		m_mapIdtoAdjInfo.clear();
	}
	AdjInfo* GetAdjInfo(int nodeId) const
	{
		auto iter = m_mapIdtoAdjInfo.find(nodeId);
		return ( iter != m_mapIdtoAdjInfo.end() ) ? iter->second : nullptr;
	}
	bool GetNodeIdExists(const int& nodeId) const	// true ==> the nodeId exists on the board
	{
		return GetAdjInfo(nodeId) != nullptr;
	}
	void InitCounts(Element* p)	// Pretend we're assigning "p" for the first time
	{
		const int& newNodeId = p->GetNodeId();
		if ( newNodeId == BAD_NODEID ) return;

		AdjInfo* pNew = GetAdjInfo(newNodeId);

		if ( !pNew )								// If no info on newNodeId ...
		{
			pNew = new AdjInfo(newNodeId);
			m_mapIdtoAdjInfo[ newNodeId ] = pNew;	// ... add it
		}
		// Modify adjacency info for newNodeId
		for (int iNbr = 0; iNbr < NUM_NBRS; iNbr++)
		{
			if ( p->GetNbr(iNbr) && ReadCodeBit(iNbr, p->GetRoutable()) )
				ModifyCount(nullptr, pNew, p->GetNbr(iNbr)->GetNodeId());
		}
		// Don't do p->GetW() !! That will be handled by painting the other wire end
	}
	void UpdateCounts(Element* p, int newNodeId)	// Only called by Board::SetNodeId()
	{
		const int& oldNodeId = p->GetNodeId();
		if ( oldNodeId == newNodeId ) return;		// No change in nodeId for element

		// Search for adjacency info on oldNodeId and newNodeId
		AdjInfo* pOld = GetAdjInfo(oldNodeId);
		AdjInfo* pNew = GetAdjInfo(newNodeId);

		if ( !pNew && newNodeId != BAD_NODEID )		// If no info on newNodeId ...
		{
			pNew = new AdjInfo(newNodeId);
			m_mapIdtoAdjInfo[ newNodeId ] = pNew;	// ... add it
		}
		// Modify adjacency info for oldNodeId and newNodeId
		assert( pNew == nullptr || pNew->GetNodeId() == newNodeId );	// Sanity check.
		for (int iNbr = 0; iNbr < NUM_NBRS; iNbr++)
		{
			if ( p->GetNbr(iNbr) && ReadCodeBit(iNbr, p->GetRoutable()) )
				ModifyCount(pOld, pNew, p->GetNbr(iNbr)->GetNodeId());
		}
		// Don't do p->GetW() !! That will be handled on painting the other wire end
	}
	void GetBoardNodeIds(std::vector<int>& out) const
	{
		out.resize( m_mapIdtoAdjInfo.size() );
		size_t i(0);
		for (auto& o : m_mapIdtoAdjInfo) out[i++] = o.first;
		std::stable_sort(out.begin(), out.end());	// Sort nodeIds to help keep coloring consistent
	}
private:
	void ModifyCount(AdjInfo* pOld, AdjInfo* pNew, const int& nbrNodeId)
	{
		if ( nbrNodeId == BAD_NODEID ) return;
		if ( pOld && pNew && pOld->GetNodeId() == pNew->GetNodeId() ) return;	// No change in nodeId

		AdjInfo* pNbr = GetAdjInfo(nbrNodeId);

		if ( pOld && pNbr && pOld->GetNodeId() != pNbr->GetNodeId() )
		{
			pOld->DecCount(pNbr->GetNodeId());
			pNbr->DecCount(pOld->GetNodeId());
		}
		if ( pNew && pNbr && pNew->GetNodeId() != pNbr->GetNodeId())
		{
			pNew->IncCount(pNbr->GetNodeId());
			pNbr->IncCount(pNew->GetNodeId());
		}
	}
private:
	std::unordered_map<int, AdjInfo*>	m_mapIdtoAdjInfo;
};
