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

#include "Pin.h"
#include "TrackElement.h"

// The board is basically a Grid of "Element" objects.
// "Element" derives from "Pin" and therefore has a description
// of the surface at a location, and the pin index there (if any).
// Each Element in the grid is "glued" (i.e. has pointers to) it's
// 8 neighbours, and wires (jumpers) "glue" remote elements together.
// This makes all the routing/connectivity code tidy because
// each Element knows what it can be connected to without having
// to go through the parent Grid object.
//
// Note: It should be straightforward to extend this model to
// multi-layer routing by having multiple nodeIds per element (1 per layer).
// A component pin (or via or wire) at a location would force all nodeIds
// there to the same value.  Other elements can have different NodeIds on each layer.
// So within a layer there would still be 8 neighbours per element, but at pin element
// there would be 8*numlayer neighbours.

const int			TRAX_COMPID = -2;		// The component manager member m_trax has this ID
const int			BAD_COMPID  = -1;		// Invalid component ID
const unsigned int	BAD_ROUTEID = UINT_MAX;	// Invalid route (i.e. track section) ID
const unsigned int	BAD_MH		= UINT_MAX;	// "Infinite" MH distance

class Element;

// Quicker to use struct than a std::pair
struct ElementInt
{
	ElementInt(const Element* p, unsigned int i) : first(p), second(i) {}
	const Element*	first;
	unsigned int	second;
};

// Quicker to use a list than an unordered_map since list is typically small
typedef std::list<ElementInt> WIRELIST;	// Helper for chains of wires

class Element : public Pin, public TrackElement
{
public:
	Element() : Pin(), TrackElement() { ZeroConnectionPointers(); }
	Element(const Element& o) : Pin(o), TrackElement(o)	{ assert(0); *this = o; }	// The assert just shows this is never used
	~Element() {}
	void ZeroConnectionPointers()
	{
		memset(m_pNbr,	0, NUM_NBRS * sizeof(Element*));
		memset(m_pW,	0, 2 * sizeof(Element*));
	}
	Element& operator=(const Element& o)
	{
		Pin::operator=(o);			// Call operator= in base class
		TrackElement::operator=(o);	// Call operator= in base class
		m_bIsVia	= o.m_bIsVia;
		m_compId	= o.m_compId;
		m_compId2	= o.m_compId2;
		m_pinChar2	= o.m_pinChar2;
		m_bSolderR	= o.m_bSolderR;
//		m_iRoutable	= o.m_iRoutable;	// This should only be set by the Board::Glue() method
		m_MH		= o.m_MH;
		m_maxMH		= o.m_maxMH;
		m_routeId	= o.m_routeId;
		// Zero the connection pointers m_pNbr[] and m_pW[].
		// These should only be set by Board::GlueNbrs() and Board::GlueWires().
		// m_pW can also be modified by the methods Board::PutDown() and Board::TakeOff().
		ZeroConnectionPointers();
		return *this;
	}
	bool operator==(const Element& o) const	// Compare persisted info only
	{
		return	Pin::operator==(o)
			&&	TrackElement::operator==(o)
			&&	m_bIsVia	== o.m_bIsVia
			&&	m_compId	== o.m_compId
			&&	m_compId2	== o.m_compId2
			&&	m_pinChar2	== o.m_pinChar2;
	}
	bool operator!=(const Element& o) const
	{
		return !(*this == o);
	}
	void SetNodeId(const int& i)	// Only called via the parent board method Board::SetNodeId()
	{
		TrackElement::SetNodeId(i);

		// Update usage flags for connections emanating from "this" element.
		for (int iNbr = 0; iNbr < NUM_NBRS; iNbr++) UpdateUsed(iNbr);

		// Update usage flags for diagonals that cut across the LT,RT,LB,RB diagonals.
		// Call these LTX,RTX,LBX,RBX respectively.
		// The point of doing this is that if "this" element has a diagonal connection that
		// we've just cleared, then previously blocked diagonals may now be usable.
		GetNbr(NBR_L)->UpdateUsed(NBR_RT);	GetNbr(NBR_R)->UpdateUsed(NBR_LT);	// LTX, RTX
		GetNbr(NBR_L)->UpdateUsed(NBR_RB);	GetNbr(NBR_R)->UpdateUsed(NBR_LB);	// LBX, RBX
	}
	void SetIsVia(const bool& b)		{ m_bIsVia = b; }
	void SetCompId(const int& i)		{ m_compId = i; }
	void SetCompId2(const int& i)		{ m_compId2 = i; }
	void SetPinIndex2(const size_t& i)	{ m_pinChar2= ( i >= BAD_PINCHAR ) ? BAD_PINCHAR : static_cast<uchar> (i); }
	void SetSolderR(const bool& b)		{ m_bSolderR	= b; }
	void SetRoutable(const int& i)		{ m_iRoutable	= i; }
	void ResetMH()
	{
		m_routeId	= BAD_ROUTEID;	// Wipe RouteId
		m_MH		= BAD_MH;		// Set "infinite" MH distance.
		m_maxMH		= 0;			// Zero max MH parameter
	}
	void UpdateMH(const unsigned int& routeID, const unsigned int& iMH, unsigned int& iMaxMH)
	{
		assert( m_MH == BAD_MH );	// Should only ever write the MH once
		iMaxMH		= std::max(iMaxMH, iMH);	// Update iMaxMH for output before storing it
		m_routeId	= routeID;
		m_MH		= iMH;
		m_maxMH		= iMaxMH;
	}
	void SetNbr(const int& iNbr, Element* p)	{ m_pNbr[iNbr]	= p; }
	void ClearWires()							{ m_pW[0] = m_pW[1] = nullptr; }
	bool GetHasWire() const						{ return m_pW[0] != nullptr || m_pW[1] != nullptr; }
	int  GetNumWires() const
	{
		int i(0);
		if ( m_pW[0] != nullptr ) i++;
		if ( m_pW[1] != nullptr ) i++;
		return i;
	}
	int  GetUsedSlot() const
	{
		return	( GetCompId()  != BAD_COMPID ) ? 0 :
				( GetCompId2() != BAD_COMPID ) ? 1 : -1;
	}
	int  GetFreeSlot() const
	{
		return	( GetCompId()  == BAD_COMPID ) ? 0 :
				( GetCompId2() == BAD_COMPID ) ? 1 : -1;
	}
	int  GetSlotFromCompId(const int& compId)
	{
		assert( GetCompId() != GetCompId2() || GetCompId() == BAD_COMPID );
		if ( compId == GetCompId()  ) return 0;
		if ( compId == GetCompId2() ) return 1;
		assert(0);	// Error
		return -1;
	}
	void SetSlotInfo(const int& iSlot, const size_t& pinIndex, const int& compId)
	{
		switch( iSlot )
		{
			case 0:		SetPinIndex(pinIndex);	SetCompId(compId);	return;
			case 1:		SetPinIndex2(pinIndex);	SetCompId2(compId);	return;
			default:	assert(0);
		}
	}
	void GetSlotInfo(const int& iSlot, size_t& pinIndex, int& compId) const
	{
		switch( iSlot )
		{
			case 0:		pinIndex = GetPinIndex();	compId = GetCompId();	return;
			case 1:		pinIndex = GetPinIndex2();	compId = GetCompId2();	return;
			default:	pinIndex = BAD_PININDEX;	compId = BAD_COMPID;	assert(0);
		}
	}
	bool GetWireExists(Element* p) const
	{
		return p != nullptr && ( m_pW[0] == p || m_pW[1] == p );
	}
	bool GetCompExists(const int& compId) const
	{
		return compId != BAD_COMPID && ( m_compId == compId || m_compId2 == compId );
	}
	void SetW(const int& iSlot, Element* p)
	{
		assert( !GetWireExists(p) );	// No duplicates allowed
		assert(iSlot == 0 || iSlot == 1);
		m_pW[iSlot] = p;
	}
	const bool&			GetIsVia() const				{ return m_bIsVia; }
	const int&			GetCompId() const				{ return m_compId; }
	const int&			GetCompId2() const				{ return m_compId2; }
	int					GetNumCompIds() const			{ int i(0); if ( m_compId != BAD_COMPID ) i++; if ( m_compId2 != BAD_COMPID ) i++; return i; }
	bool				GetHasComp() const				{ return GetCompId() != BAD_COMPID || GetCompId2() != BAD_COMPID; }
	bool				GetHasPin() const				{ return GetIsPin() || m_pinChar2 != BAD_PINCHAR; }
	size_t				GetPinIndex2() const			{ return ( m_pinChar2 == BAD_PINCHAR ) ? BAD_PININDEX : m_pinChar2; }
	const bool&			GetSolderR() const				{ return m_bSolderR; }
	const int&			GetRoutable() const				{ return m_iRoutable; }
	const unsigned int&	GetRouteId() const				{ return m_routeId; }
	const unsigned int&	GetMH() const					{ return m_MH; }
	const unsigned int&	GetMaxMH() const				{ return m_maxMH; }
	Element*			GetNbr(const int& iNbr) const	{ return m_pNbr[iNbr]; }
	Element*			GetW(const int& i) const		{ return m_pW[i]; }

	// Helpers
	bool HaveNoBlankPins(const int& iNbr) const
	{
		Element* pNbr = GetNbr(iNbr);
		return	( !this->GetHasPin() || this->GetNodeId() != BAD_NODEID || this->GetHasWire() ) &&	// Only allow routing FROM blank pins if they are on wires
				( !pNbr->GetHasPin() || pNbr->GetNodeId() != BAD_NODEID || pNbr->GetHasWire() );	// Only allow routing  TO  blank pins if they are on wires
	}
	void GetWireList(WIRELIST& wireList) const
	{
		wireList.clear();
		return UpdateWireList(wireList, 0);
	}
	// Connectivity helpers
	void UpdateUsed(const int& iNbr)
	{
		const bool bUsed = GetNodeId() != BAD_NODEID
						&& GetNodeId() == GetNbr(iNbr)->GetNodeId()
						&& !IsBlocked(iNbr, GetNodeId());
		SetUsed(iNbr, bUsed);
		m_pNbr[iNbr]->SetUsed(Opposite(iNbr), bUsed);	// Keep consistent with nbr
	}
	void ToggleUsed(const int& iNbr)
	{
		ToggleCodeBit(iNbr, m_iCode);
		ToggleCodeBit(Opposite(iNbr), m_pNbr[iNbr]->m_iCode);	// Keep consistent with nbr
		// Toggles are only done by the user so set the flag accordingly
		Element* pNbr = GetNbr(iNbr);
		ClearFlagBits(AUTOSET|VEROSET);			SetFlagBits(USERSET);
		pNbr->ClearFlagBits(AUTOSET|VEROSET);	pNbr->SetFlagBits(USERSET);
		// Handle wire ends
		Element* pW = GetW(0);	if ( pW ) { pW->ClearFlagBits(AUTOSET|VEROSET);	pW->SetFlagBits(USERSET); }
		pW = GetW(1);			if ( pW ) { pW->ClearFlagBits(AUTOSET|VEROSET);	pW->SetFlagBits(USERSET); }
		pW = pNbr->GetW(0);		if ( pW ) { pW->ClearFlagBits(AUTOSET|VEROSET);	pW->SetFlagBits(USERSET); }
		pW = pNbr->GetW(1);		if ( pW ) { pW->ClearFlagBits(AUTOSET|VEROSET);	pW->SetFlagBits(USERSET); }
	}
	bool SwapDiagLinks()
	{
		// Take "this" to be the bottom right element in group of 4 squares
		// "LT"  is the diagonal from "this" to m_pLT
		// "LTX" is the diagonal that cuts across it (from m_pL to m_pT)

		// Swap (by inverting flags) if we have competing diagonals
		const bool bCanSwap = GetNodeId() == GetNbr(NBR_LT)->GetNodeId()				&&	// LT:  "this" and LT must have same nodeId
							  GetNbr(NBR_L)->GetNodeId() == GetNbr(NBR_T)->GetNodeId()	&&	// LTX: L and T must have same nodeId
							  GetNbr(NBR_L)->IsClash( GetNodeId() );						// L and "this" must have clashing nodeIds
		if ( !bCanSwap ) return false;
		ToggleUsed(NBR_LT);
		GetNbr(NBR_L)->ToggleUsed(NBR_RT);
		return true;
	}
	bool IsNbr(const Element* p) const
	{
		assert(p != nullptr);	// Sanity check
		for (int iNbr = 0; iNbr < NUM_NBRS; iNbr++)
			if ( GetNbr(iNbr) == p ) return true;
		return false;
	}			
	bool IsUselessWire(const int& iNbr, const int& nodeId) const	// Helper: true ==> painting nbr with nodeId is wasteful
	{
		const Element* pWA = GetNbr(iNbr);
		if ( pWA->GetHasWire() )
		{
			const Element* pWB0 = pWA->GetW(0);
			const Element* pWB1 = pWA->GetW(1);
			// pWA and pWB are opposite ends of a wire.
			// If these ends both neighbour a common element with the specified nodeId,
			// then it is wasteful to paint the wire with that nodeId too, since the
			// common element already provides a connection.
			for (int iNbr = 0; iNbr < NUM_NBRS; iNbr++)
			{
				const Element* p = pWA->GetNbr(iNbr);
				if ( p->GetNodeId() != nodeId ) continue;
				if ( pWB0 != nullptr && pWB0->IsNbr(p) ) return true;
				if ( pWB1 != nullptr && pWB1->IsNbr(p) ) return true;
			}
		}
		return false;
	}
	bool IsBlocked(const int& iNbr, const int& nodeId) const	// Helper: true ==> assiging nodeId to "this" blocks the iNbr direction
	{
		if ( !ReadCodeBit(iNbr, GetRoutable() ) ) return true;	// Block toroidal connections at board edges
		if ( GetNbr(iNbr)->IsClash(nodeId) ) return true;		// Check if nbr has a clashing nodeId assigned to it
		if ( GetNbr(iNbr)->GetIsHole() ) return true;			// Block connections to holes

		switch( iNbr )	// Then do additional checks for competing diagonals
		{
			case NBR_LT: return GetNbr(NBR_L)->IsClash(nodeId) && GetNbr(NBR_L)->GetUsed(NBR_RT);
			case NBR_RT: return GetNbr(NBR_R)->IsClash(nodeId) && GetNbr(NBR_R)->GetUsed(NBR_LT);
			case NBR_LB: return GetNbr(NBR_B)->IsClash(nodeId) && GetNbr(NBR_B)->GetUsed(NBR_LT);
			case NBR_RB: return GetNbr(NBR_B)->IsClash(nodeId) && GetNbr(NBR_B)->GetUsed(NBR_RT);
			default:	 return false;
		}
	}
	// Merge interface functions
	virtual void UpdateMergeOffsets(MergeOffsets& o) override
	{
		Pin::UpdateMergeOffsets(o);	// Does nothing
		TrackElement::UpdateMergeOffsets(o);
		if ( m_compId != BAD_COMPID && m_compId != TRAX_COMPID )
			o.deltaCompId = std::max(o.deltaCompId,  m_compId + 1);
		assert( m_compId2 != TRAX_COMPID );
		if ( m_compId2 != BAD_COMPID && m_compId2 != TRAX_COMPID )
			o.deltaCompId = std::max(o.deltaCompId,  m_compId2 + 1);
	}
	virtual void ApplyMergeOffsets(const MergeOffsets& o) override
	{
		Pin::ApplyMergeOffsets(o);	// Does nothing
		TrackElement::ApplyMergeOffsets(o);
		if ( m_compId != BAD_COMPID	&& m_compId != TRAX_COMPID)
			m_compId += o.deltaCompId;
		assert( m_compId2 != TRAX_COMPID );
		if ( m_compId2 != BAD_COMPID && m_compId2 != TRAX_COMPID)
			m_compId2 += o.deltaCompId;
	}
	void Merge(const Element& o)
	{
		Pin::Merge(o);
		TrackElement::Merge(o);
		m_bIsVia	= o.m_bIsVia;
		m_compId	= o.m_compId;
		m_compId2	= o.m_compId2;
		m_pinChar2	= o.m_pinChar2;
	}
	// Persist interface functions
	virtual void Load(DataStream& inStream) override
	{
		if ( inStream.GetVersion() < VRT_VERSION_25 )
		{
			Pin::Load(inStream);			// Load() base class
			inStream.Load(m_compId);
			TrackElement::Load(inStream);	// Load() base class
			inStream.Load(m_bIsVia);
		}
		else
		{
			Pin::Load(inStream);			// Load() base class
			TrackElement::Load(inStream);	// Load() base class
			inStream.Load(m_bIsVia);
			inStream.Load(m_compId);
		}
		m_compId2	= BAD_COMPID;
		m_pinChar2	= BAD_PINCHAR;
		if ( inStream.GetVersion() >= VRT_VERSION_27 )
		{
			inStream.Load(m_compId2);	// Added in VRT_VERSION_27
			inStream.Load(m_pinChar2);	// Added in VRT_VERSION_27
		}
	}
	virtual void Save(DataStream& outStream) override
	{
		Pin::Save(outStream);				// Save() base class
		TrackElement::Save(outStream);		// Save() base class
		outStream.Save(m_bIsVia);
		outStream.Save(m_compId);
		outStream.Save(m_compId2);		// Added in VRT_VERSION_27
		outStream.Save(m_pinChar2);		// Added in VRT_VERSION_27
	}
private:
	bool WireListHelper(WIRELIST& wireList, const Element* p, unsigned int iStep) const
	{
		for (auto& o : wireList)
		{
			if ( o.first != p ) continue;
			if ( iStep < o.second )	{ o.second = iStep; return true; } else return false;
		}
		wireList.push_back(ElementInt(p, iStep));
		return true;
	}
	void UpdateWireList(WIRELIST& wireList, unsigned int iStep) const
	{
		WireListHelper(wireList, this, iStep);
		bool bOK_0	= m_pW[0] != nullptr && WireListHelper(wireList, m_pW[0], iStep + 1);
		bool bOK_1	= m_pW[1] != nullptr && WireListHelper(wireList, m_pW[1], iStep + 1);

		if ( bOK_0 ) m_pW[0]->UpdateWireList(wireList, iStep + 1);
		if ( bOK_1 ) m_pW[1]->UpdateWireList(wireList, iStep + 1);
	}
private:
	// Persist info
	bool			m_bIsVia	= false;
	int				m_compId	= BAD_COMPID;	// For elements with a valid pinindex, this is the ID of the parent component
	int				m_compId2	= BAD_COMPID;	// Only used when we have 2 wires sharing a hole
	uchar			m_pinChar2	= BAD_PINCHAR;	// Only used when we have 2 wires sharing a hole

	// Working variables.	Don't persist.
	bool			m_bSolderR	= false;		// true ==> have blob of solder to right (for joining vero tracks)
	int				m_iRoutable	= 0;			// Set by Board::GlueNbrs().  Code bits used to enable/disable connections to neighbours
	unsigned int	m_routeId	= BAD_ROUTEID;	// For the routing algorithm.
	unsigned int	m_MH		= BAD_MH;		// Manhatten distance to another element.  For the routing/connectivity algorithm.
	unsigned int	m_maxMH		= 0;			// For the routing algorithm.
	// Connection pointers. Set by Board::GlueNbrs() and Board::GlueWires().	Don't persist.
	Element*		m_pNbr[(size_t)NUM_NBRS];	// 0 to 7 <==> NBR_L to NBR_LB
	Element*		m_pW[2];					// Up to 2 wires per element. These point to the other end of the wire(s).
};
