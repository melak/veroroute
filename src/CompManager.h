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

class CompManager : public Persist, public Merge
{
	friend class Board;
public:
	CompManager()	{ Clear(); }
	~CompManager()	{ Clear(); }
	CompManager(const CompManager& o) { *this = o; }
	void Clear()
	{
		m_mapIdToComp.clear();
		m_mapWireToShift.clear();
		ClearTrax();
	}
	CompManager& operator=(const CompManager& o)
	{
		Clear();
		for (const auto& mapObj : o.m_mapIdToComp)
		{
			const Component& comp = mapObj.second;
			m_mapIdToComp[ comp.GetId() ] = comp;
		}
		m_trax = o.m_trax;
		// Don't copy m_mapWireToShift (it's just a helper)
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
	Component& GetComponentById(const int& compId)
	{
		if ( compId == TRAX_COMPID ) return m_trax;
		assert( GetComponentExists(compId) );
		return m_mapIdToComp[compId];
	}
	const std::unordered_map<int, Component>& GetMapIdToComp() const
	{
		return m_mapIdToComp;
	}
	int CreateComp(const Component& tmp)	// Creates a copy of tmp and returns its compId
	{
		// Find the first unused compId.
		int compId(0);
		while ( GetComponentExists(compId) && compId != INT_MAX ) compId++;
		if ( compId == INT_MAX ) return BAD_COMPID;	// Reached component limit !!!

		// tmp is ***copied*** into the map, and the map component then has its compId set
		Component& comp = m_mapIdToComp[compId] = tmp;
		comp.SetId(compId);
		return compId;
	}
	void DestroyComp(Component& comp)
	{
		const auto iter = m_mapIdToComp.find(comp.GetId());
		if ( iter != m_mapIdToComp.end() )
			m_mapIdToComp.erase(iter);
		else
		assert(0);	// Should never hit this
	}
	bool GetIsEmpty() const
	{
		return m_mapIdToComp.empty() && m_trax.GetSize() == 0;
	}
	Rect GetBounding() const
	{
		Rect bounding;
		for (const auto& mapObj : m_mapIdToComp)
		{
			const Component& comp = mapObj.second;
			bounding |= comp.GetFootprintRect();
		}
		if ( m_trax.GetSize() > 0 )	// If have a trax pattern
			bounding |= m_trax.GetFootprintRect();
		return bounding;
	}
	void CalculateWireShifts()
	{
		m_mapWireToShift.clear();

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

		const Component* pLast(nullptr);
		for (auto& p : wiresH)
		{
			if ( pLast == nullptr || p->GetRow() != pLast->GetRow() )	// Reset if new row
				m_mapWireToShift[p] = 0;
			else if ( p->GetCol() < pLast->GetCol() + pLast->GetCompCols() - 1 )	// If have overlap ...
			{
				if ( m_mapWireToShift[pLast] == 0 )				// ... shift last wire if necessary
					m_mapWireToShift[pLast] = -1;
				m_mapWireToShift[p] = -m_mapWireToShift[pLast];	// ... give this wire opposite shift
			}
			else												// If no overlap ...
				m_mapWireToShift[p] = m_mapWireToShift[pLast];	// ... give this wire same shift
			pLast = p;
		}
		pLast = nullptr;
		for (auto& p : wiresV)
		{
			if ( pLast == nullptr || p->GetCol() != pLast->GetCol() )	// Reset if new col
				m_mapWireToShift[p] = 0;
			else if ( p->GetRow() < pLast->GetRow() + pLast->GetCompRows() - 1 )	// If have overlap ...
			{
				if ( m_mapWireToShift[pLast] == 0 )				// ... shift last wire if necessary
					m_mapWireToShift[pLast] = -1;
				m_mapWireToShift[p] = -m_mapWireToShift[pLast];	// ... give this wire opposite shift
			}
			else												// If no overlap ...
				m_mapWireToShift[p] = m_mapWireToShift[pLast];	// ... give this wire same shift
			pLast = p;
		}
	}
	int GetWireShift(const Component* pWire) const
	{
		auto iter = m_mapWireToShift.find( pWire );
		return ( iter != m_mapWireToShift.end() ) ? iter->second : 0;
	}
	// Merge interface functions
	virtual void UpdateMergeOffsets(MergeOffsets& o) override
	{
		for (auto& mapObj : m_mapIdToComp)
		{
			Component& comp = mapObj.second;
			comp.UpdateMergeOffsets(o);
		}
		if ( m_trax.GetSize() > 0 )
			m_trax.UpdateMergeOffsets(o);
	}
	virtual void ApplyMergeOffsets(const MergeOffsets& o) override
	{
		for (auto& mapObj : m_mapIdToComp)
		{
			Component& comp = mapObj.second;
			comp.ApplyMergeOffsets(o);
		}
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
		for (auto& mapObj : m_mapIdToComp)
		{
			Component& comp = mapObj.second;
			comp.Save(outStream);
		}
		m_trax.Save(outStream);	// Added in VRT_VERSION_11
	}
	Component&	GetTrax()	{ return m_trax; }
	void		ClearTrax()	{ m_trax.DeAllocate(); m_trax.SetType(COMP::TRACKS); m_trax.SetId(TRAX_COMPID); m_trax.SetIsPlaced(false); m_trax.SetRow(0); m_trax.SetCol(0); }
	void		BuildTrax(const RectManager& rectMgr, const ElementGrid& grid, const int& nRowMin, const int& nRowMax, const int& nColMin, const int& nColMax)
	{
		m_trax = Component(this, rectMgr, grid, nRowMin, nRowMax, nColMin, nColMax);
		m_trax.SetId(TRAX_COMPID);
		m_trax.SetIsPlaced(true);
	}
private:
	bool GetComponentExists(const int& compId) const
	{
		return m_mapIdToComp.find(compId) != m_mapIdToComp.end();
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
private:
	std::unordered_map<int, Component>			m_mapIdToComp;		// The components (indexed by compId)
	Component									m_trax;				// The "trax" component
	// Helpers. Don't persist.
	std::unordered_map<const Component*, int>	m_mapWireToShift;	// For stacking wires
};
