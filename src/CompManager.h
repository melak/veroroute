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

#include "Component.h"

// Manager class for components on the board

// Quicker to use struct than a std::pair
struct WireInfo
{
	WireInfo() {}
	WireInfo(const WireInfo& o) { *this = o; }
	WireInfo& operator=(const WireInfo& o)
	{
		m_iShift = o.m_iShift;
		m_iCross = o.m_iCross;
		return *this;
	}
	bool operator==(const WireInfo& o)
	{
		return m_iShift == o.m_iShift
			&& m_iCross == o.m_iCross;
	}
	bool operator!=(const WireInfo& o)
	{
		return !(*this == o);
	}
	int	m_iShift = 0;	// For drawing overlaid wires
	int	m_iCross = 0;	// Number of crossing wires
};

class CompManager : public Persist, public Merge
{
	friend class Board;
public:
	CompManager() { ClearTrax(); }
	virtual ~CompManager() {}
	CompManager(const CompManager& o) { *this = o; }
	void Clear()
	{
		m_mapIdToComp.clear();
		m_mapWireToInfo.clear();
		ClearFind();
		ClearTrax();
	}
	CompManager& operator=(const CompManager& o)
	{
		Clear();
		for (const auto& mapObj : o.m_mapIdToComp) m_mapIdToComp[ mapObj.first ] = mapObj.second;
		m_trax = o.m_trax;
		// Don't copy m_mapWireToInfo (it's just a helper)
		// Don't copy m_foundId       (it's just a helper)
		return *this;
	}
	bool operator==(const CompManager& o) const	// Compare persisted info
	{
		if ( m_mapIdToComp.size() != o.m_mapIdToComp.size() ) return false;
		for (const auto& mapObj : m_mapIdToComp)
		{
			const Component& compA = mapObj.second;
			const auto iterOther = o.m_mapIdToComp.find( compA.GetId() );
			if ( iterOther == o.m_mapIdToComp.end() ) return false;
			const Component& compB = iterOther->second;
			if ( !compA.IsEqual(compB) ) return false;
		}
		return m_trax == o.m_trax;
	}
	bool operator!=(const CompManager& o) const
	{
		return !(*this == o);
	}
	int GetComponentIdFromName(const std::string& nameStr) const
	{
		for (const auto& mapObj : m_mapIdToComp)
		{
			const Component& comp = mapObj.second;
			if ( nameStr == comp.GetNameStr() ) return comp.GetId();
		}
		return BAD_COMPID;
	}
	const Component& GetComponentById(const int& compId) const
	{
		return LookupCompById(compId);
	}
	Component& GetComponentById(const int& compId)
	{
		return const_cast<Component&> ( LookupCompById(compId) );
	}
	bool GetAllowFlyWire(const int& compId) const
	{
		auto iter = m_mapIdToComp.find(compId);
		return ( iter != m_mapIdToComp.end() ) ? iter->second.GetAllowFlyWire() : false;
	}
	const std::unordered_map<int, Component>& GetMapIdToComp() const
	{
		return m_mapIdToComp;
	}
	void GetSortedComps(std::vector<const Component*>& comps) const	// Sorted by render order
	{
		comps.clear();
		for (const auto& mapObj : m_mapIdToComp) comps.push_back(&mapObj.second);
		std::sort(comps.begin(), comps.end(), HasLowerRenderOrder());
	}
	int CreateComp(const Component& tmp, const bool& bUsePCBshapes)	// Creates a copy of tmp and returns its compId
	{
		assert(tmp.GetType() != COMP::INVALID);
		assert(tmp.GetType() != COMP::TRACKS);
		// Find the first unused compId.
		int compId(0);
		while ( GetComponentExists(compId) && compId != INT_MAX ) compId++;
		if ( compId == INT_MAX ) return BAD_COMPID;	// Reached component limit !!!

		// tmp is ***copied*** into the map, and the map component then has its compId set
		Component& comp = m_mapIdToComp[compId] = tmp;
		comp.SetId(compId);
		if ( CompTypes::AllowCustomPCBshapes(comp.GetType()) ) comp.SetDefaultShapes(bUsePCBshapes);
		return compId;
	}
	void DestroyComp(Component& comp)
	{
		const auto iterF = m_foundId.find(comp.GetId());
		if ( iterF != m_foundId.end() )
			m_foundId.erase(iterF);
		const auto iter = m_mapIdToComp.find(comp.GetId());
		if ( iter != m_mapIdToComp.end() )
			m_mapIdToComp.erase(iter);
		else
			assert(0);	// Should never hit this
	}
	void GetBadCompIds(std::set<int>& badCompIds)	// Helper to fix corrupted boards
	{
		badCompIds.clear();
		for (const auto& mapObj : m_mapIdToComp)
			if ( mapObj.second.GetType() == COMP::INVALID ) badCompIds.insert(mapObj.first);
	}
	bool GetIsEmpty() const
	{
		return m_mapIdToComp.empty() && m_trax.GetSize() == 0;
	}
	Rect GetBounding() const
	{
		Rect bounding;
		for (const auto& mapObj : m_mapIdToComp) bounding |= mapObj.second.GetFootprintRect();
		if ( m_trax.GetSize() > 0 ) bounding |= m_trax.GetFootprintRect();	// If have a trax pattern
		return bounding;
	}
	bool GetHavePlacedWires() const
	{
		for (const auto& mapObj : m_mapIdToComp)
		{
			const Component& comp = mapObj.second;
			if ( comp.GetType() == COMP::WIRE && comp.GetIsPlaced() ) return true;
		}
		return false;
	}
	void GetPadWidths(std::list<int>& o, const int& iDefaultWidth) const
	{
		o.clear();
		for (const auto& mapObj : m_mapIdToComp)
		{
			const Component& comp = mapObj.second;
			if ( !comp.GetIsPlaced() ) continue;
			const int& iWidth = comp.GetCustomPads() ? comp.GetPadWidth() : iDefaultWidth;
			const auto iterFind = std::find(o.begin(), o.end(), iWidth);
			if ( iterFind == o.end() ) o.push_back( iWidth );
		}
	}
	void GetHoleWidths(std::list<int>& o, const int& iDefaultWidth) const
	{
		o.clear();
		for (const auto& mapObj : m_mapIdToComp)
		{
			const Component& comp = mapObj.second;
			if ( !comp.GetIsPlaced() ) continue;
			const int& iWidth = comp.GetCustomPads() ? comp.GetHoleWidth() : iDefaultWidth;
			const auto iterFind = std::find(o.begin(), o.end(), iWidth);
			if ( iterFind == o.end() ) o.push_back( iWidth );
		}
	}
	void CalculateWireInfo()	// Calculate wire shifts and crossing flags
	{
		m_mapWireToInfo.clear();

		std::vector<const Component*> wiresH, wiresV;	// Lists of placed wires in H and V directions
		for (const auto& mapObj : m_mapIdToComp)
		{
			const Component& comp = mapObj.second;
			if ( comp.GetType() != COMP::WIRE || !comp.GetIsPlaced() ) continue;
			if ( comp.GetCompRows() == 1 )
				wiresH.push_back(&comp);
			else
				wiresV.push_back(&comp);
		}
		std::sort(wiresH.begin(), wiresH.end(), IsEarlierWire());
		std::sort(wiresV.begin(), wiresV.end(), IsEarlierWire());

		const Component* pPrev(nullptr);	// The previous wire on the line
		const Component* pLast(nullptr);	// The wire that reaches most along the line
		int i(0);
		for (const auto& p : wiresH)
		{
			if ( pPrev == nullptr || p->GetRow() != pPrev->GetRow() )	// Reset all if new row
			{
				m_mapWireToInfo[p].m_iShift = 0;
				pLast = nullptr;
			}
			else if ( p->GetCol() == pLast->GetLastCol() )		// If touches pLast ...
				m_mapWireToInfo[p].m_iShift = m_mapWireToInfo[pLast].m_iShift;	// ... give same shift as pLast
			else if ( p->GetCol() > pLast->GetLastCol() )		// If beyond pLast ...
				m_mapWireToInfo[p].m_iShift = 0;				// ... set zero shift
			else if ( p->GetCol() < pPrev->GetLastCol() )		// If overlap pPrev ...
			{
				if ( m_mapWireToInfo[pPrev].m_iShift == 0 )		// ... shift pPrev if necessary
				{
					m_mapWireToInfo[pPrev].m_iShift = -1;
					for (int j = i - 2; j >= 0; j--)			// ... and also back along its chain
					{
						const size_t J = static_cast<size_t>(j);
						if ( wiresH[J]->GetRow()	 != wiresH[J+1]->GetRow() ||
							 wiresH[J]->GetLastCol() != wiresH[J+1]->GetCol() ) break;
						m_mapWireToInfo[ wiresH[J] ].m_iShift = -1;
					}
				}
				m_mapWireToInfo[p].m_iShift = -m_mapWireToInfo[pPrev].m_iShift;	// ... give this opposite shift to pPrev
			}
			else												// If no overlap ...
				m_mapWireToInfo[p].m_iShift = m_mapWireToInfo[pPrev].m_iShift;	// ... give this same shift as pPrev
			pPrev = p;
			if ( pLast == nullptr || p->GetLastCol() > pLast->GetLastCol() )
				pLast = p;
			i++;
		}
		pPrev = pLast = nullptr;
		i = 0;
		for (const auto& p : wiresV)
		{
			if ( pPrev == nullptr || p->GetCol() != pPrev->GetCol() )	// Reset all if new col
			{
				m_mapWireToInfo[p].m_iShift = 0;
				pLast = nullptr;
			}
			else if ( p->GetRow() == pLast->GetLastRow() )		// If touches pLast ...
				m_mapWireToInfo[p].m_iShift = m_mapWireToInfo[pLast].m_iShift;	// ... give same shift as pLast
			else if ( p->GetRow() > pLast->GetLastRow() )		// If beyond pLast ...
				m_mapWireToInfo[p].m_iShift = 0;				// ... set zero shift
			else if ( p->GetRow() < pPrev->GetLastRow() )		// If overlap pPrev ...
			{
				if ( m_mapWireToInfo[pPrev].m_iShift == 0 )		// ... shift pPrev if necessary
					m_mapWireToInfo[pPrev].m_iShift = -1;
				for (int j = i - 2; j >= 0; j--)				// ... and also back along its chain
				{
					const size_t J = static_cast<size_t>(j);
					if ( wiresV[J]->GetCol()	 != wiresV[J+1]->GetCol() ||
						 wiresV[J]->GetLastRow() != wiresV[J+1]->GetRow() ) break;
					m_mapWireToInfo[ wiresV[J] ].m_iShift = -1;
				}
				m_mapWireToInfo[p].m_iShift = -m_mapWireToInfo[pPrev].m_iShift;	// ... give this opposite shift to pPrev
			}
			else												// If no overlap ...
				m_mapWireToInfo[p].m_iShift = m_mapWireToInfo[pPrev].m_iShift;	// ... give this same shift as pPrev
			pPrev = p;
			if ( pLast == nullptr || p->GetLastRow() > pLast->GetLastRow() )
				pLast = p;
			i++;
		}

		for (const auto& pH : wiresH)
			for (const auto& pV : wiresV)
				if ( pH->GetRow() > pV->GetRow() &&
					 pH->GetRow() < pV->GetLastRow() &&
					 pV->GetCol() > pH->GetCol() &&
					 pV->GetCol() < pH->GetLastCol() )
				{
					auto& infoH = m_mapWireToInfo[pH];
					auto& infoV = m_mapWireToInfo[pV];
					// If one wire is overlaid (i.e. shifted) but the other is not then don't consider this a cross.
					// The point being that we could still convert the other wire to a track.
					if ( ( infoH.m_iShift == 0 && infoV.m_iShift != 0 ) ||
						 ( infoV.m_iShift == 0 && infoH.m_iShift != 0 ) )continue;
					infoH.m_iCross++;
					infoV.m_iCross++;
				};
	}
	int GetWireShift(const Component* pWire) const
	{
		auto iter = m_mapWireToInfo.find( pWire );
		return ( iter != m_mapWireToInfo.end() ) ? iter->second.m_iShift : 0;
	}
	bool GetWireCanBeTrack(const Component* pWire) const	// true ==> wire can be turned into a top-surface track
	{
		assert( pWire->GetType() == COMP::WIRE && pWire->GetIsPlaced() );	// Should have already checked for this
		// Wire must be non-stacked, and either horizontal or not crossing another
		auto iter = m_mapWireToInfo.find( pWire );
		const bool bH = pWire->GetDirection() == 'W' || pWire->GetDirection() == 'E';
		return ( iter != m_mapWireToInfo.end() ) ? ( iter->second.m_iShift == 0  && (bH || iter->second.m_iCross == 0) ) : false;
	}
	void CustomPCBshapes(const bool bUsePCBshapes)
	{
		for (auto& mapObj : m_mapIdToComp)
		{
			Component& comp = mapObj.second;
			if ( CompTypes::AllowCustomPCBshapes(comp.GetType()) ) comp.SetDefaultShapes(bUsePCBshapes);
		}
	}
	void ClearFind()
	{
		m_foundId.clear();
	}
	void Find(const bool bUseName, const bool bExact, const std::string& str)
	{
		if ( str.empty() ) return;	// Do nothing if passed an empty string
		for (const auto& mapObj : m_mapIdToComp)
		{
			const Component&	comp	= mapObj.second;
			const COMP&			eType	= comp.GetType();
			if ( eType == COMP::WIRE || eType == COMP::MARK || eType == COMP::VERO_NUMBER || eType == COMP::VERO_LETTER ) continue;
			const std::string& compStr = bUseName ? comp.GetNameStr() : comp.GetValueStr();
			const bool bFound = bExact ? ( compStr == str )
									   : ( compStr.find(str) != std::string::npos );
			if ( bFound ) m_foundId.insert( mapObj.first );
		}
	}
	bool GetFound(const int& compId) const
	{
		return m_foundId.find(compId) != m_foundId.end();
	}

	// Merge interface functions
	virtual void UpdateMergeOffsets(MergeOffsets& o) override
	{
		for (auto& mapObj : m_mapIdToComp)
			mapObj.second.UpdateMergeOffsets(o);
		if ( m_trax.GetSize() > 0 )
			m_trax.UpdateMergeOffsets(o);
	}
	virtual void ApplyMergeOffsets(const MergeOffsets& o) override
	{
		for (auto& mapObj : m_mapIdToComp)
			mapObj.second.ApplyMergeOffsets(o);
		if ( m_trax.GetSize() > 0 )
			m_trax.ApplyMergeOffsets(o);
	}
	void Merge(CompManager& o)
	{
		for (auto& mapObj : o.m_mapIdToComp)
		{
			Component& comp = mapObj.second;
			bool bNameExists = GetComponentIdFromName( comp.GetNameStr() ) != BAD_COMPID;
			if ( bNameExists )
			{
				// Try and produce a simple unique Name for the part if possible
				std::string nameStr;
				char buffer[256] = {'\0'};
				const std::string prefixStr = comp.GetPrefixStr();	// e.g. "C" for capacitors
				for (int iSuffix = 1; iSuffix < INT_MAX && bNameExists; iSuffix++)
				{
					sprintf(buffer,"%s%d", prefixStr.c_str(), iSuffix);
					nameStr = buffer;	// e.g. "C1"
					bNameExists = GetComponentIdFromName(nameStr) != BAD_COMPID;
				}
				comp.SetNameStr(nameStr);
			}
			assert( !GetComponentExists(comp.GetId()) );	// Must have unique component IDs
			m_mapIdToComp[ comp.GetId() ] = comp;
		}
		m_trax = o.m_trax;	// Replace m_trax with the one in 'o'
	}
	// Persist interface functions
	virtual void Load(DataStream& inStream) override
	{
		// Call Load() on all the components
		unsigned int numComps(0);
		inStream.Load(numComps);
		m_mapIdToComp.clear();
		for (unsigned int i = 0; i < numComps; i++)
		{
			Component tmp;
			tmp.Load(inStream);

			assert( !GetComponentExists(tmp.GetId()) );	// Must have unique component IDs
			m_mapIdToComp[ tmp.GetId() ] =  tmp;
		}
		if ( inStream.GetVersion() >= VRT_VERSION_11 )
			m_trax.Load(inStream);
	}
	virtual void Save(DataStream& outStream) override
	{
		const unsigned int numComps = static_cast<unsigned int>( m_mapIdToComp.size() );
		outStream.Save(numComps);
		for (auto& mapObj : m_mapIdToComp) mapObj.second.Save(outStream);
		m_trax.Save(outStream);	// Added in VRT_VERSION_11
	}
	Component&	GetTrax()	{ return m_trax; }
	void		ClearTrax()	{ m_trax.DeAllocate(); m_trax.SetType(COMP::TRACKS); m_trax.SetId(TRAX_COMPID); m_trax.SetIsPlaced(false); m_trax.SetRow(0); m_trax.SetCol(0); }
	void		BuildTrax(const RectManager& rectMgr, const ElementGrid& grid, const int& nLyr, const int& nRowMin, const int& nRowMax, const int& nColMin, const int& nColMax)
	{
		m_trax = Component(this, rectMgr, grid, nLyr, nRowMin, nRowMax, nColMin, nColMax);
		m_trax.SetId(TRAX_COMPID);
		m_trax.SetIsPlaced(true);
	}
private:
	bool GetComponentExists(const int& compId) const
	{
		return m_mapIdToComp.find(compId) != m_mapIdToComp.end();
	}
	const Component& LookupCompById(const int& compId) const
	{
		if ( compId == TRAX_COMPID ) return m_trax;

		auto iter = m_mapIdToComp.find(compId);
		if ( iter != m_mapIdToComp.end() ) return iter->second;

		// Should not really get here!!!  Use assert() to check the returned component type is valid.
		Component& comp = m_mapIdToComp[compId];	// This creates a blank component and puts it in the map
		comp.SetId(compId);	// Even a blank component should have the correct compId
		return comp;
	}
	struct IsEarlierWire
	{
		bool operator()(const Component* pA, const Component* pB) const
		{
			if ( pA->GetCompRows() == 1 )	// Horizontal
			{
				if ( pA->GetRow() != pB->GetRow() ) return pA->GetRow() < pB->GetRow();
				return pA->GetCol() < pB->GetCol();
			}
			else							// Vertical
			{
				if ( pA->GetCol() != pB->GetCol() ) return pA->GetCol() < pB->GetCol();
				return pA->GetRow() < pB->GetRow();
			}
		}
	};
	struct HasLowerRenderOrder
	{
		bool operator()(const Component* pA, const Component* pB) const
		{
			const COMP& eTypeA = pA->GetType();
			const COMP& eTypeB = pB->GetType();
			if ( pA->GetIsPlaced() != pB->GetIsPlaced() ) return pA->GetIsPlaced();	// Render floating components last
			if ( CompTypes::IsPlug(eTypeA) != CompTypes::IsPlug(eTypeB) ) return CompTypes::IsPlug(eTypeB);	// Render "plug" components last
			return static_cast<int>(eTypeA) < static_cast<int>(eTypeB);
		}
	};
private:
	mutable std::unordered_map<int, Component>		m_mapIdToComp;		// The components (indexed by compId).  Mutable because of LookupCompById()
	Component										m_trax;				// The "trax" component
	// Helpers. Don't persist.
	std::unordered_map<const Component*, WireInfo>	m_mapWireToInfo;	// For handling overlaid / crossing wires
	std::set<int>									m_foundId;			// Set of compId's produced by Find()
};
