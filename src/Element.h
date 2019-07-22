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

#include "CompElement.h"

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

class Element : public CompElement
{
public:
	Element()
	: CompElement()
	, m_bSolderR(false)
	, m_iRoutable(0)
	, m_MH(BAD_MH)
	, m_maxMH(0)
	, m_routeId(BAD_ROUTEID)
	{
		memset(m_pNbr, 0, 8 * sizeof(Element*));
		m_pW = nullptr;
	}
	Element(const Element& o) : CompElement(o)	{ assert(0); *this = o; }	// The assert just shows this is never used
	~Element() {}
	Element& operator=(const Element& o)
	{
		CompElement::operator=(o);	// Call operator= in base class
		m_bSolderR		= o.m_bSolderR;
		//m_iRoutable		= o.m_iRoutable;	// This should only be set by the Board::Glue() method
		m_MH			= o.m_MH;
		m_maxMH			= o.m_maxMH;
		m_routeId		= o.m_routeId;
		// Zero the connection pointers m_pNbr[] and m_pW.
		// These should only be set by Board::GlueNbrs() and Board::GlueWires().
		// m_pW can also be modified by the methods Board::PutDown() and Board::TakeOff().
		memset(m_pNbr, 0, 8 * sizeof(Element*));
		m_pW = nullptr;
		return *this;
	}
	bool operator==(const Element& o) const	// Compare persisted info
	{
		return CompElement::operator==(o);
	}
	bool operator!=(const Element& o) const
	{
		return !(*this == o);
	}
	void SetNodeId(const int& i)	// Only called via the parent board method Board::SetNodeId()
	{
		CompElement::SetNodeId(i);

		// Update usage flags for connections emanating from "this" element.
		for (int iNbr = 0; iNbr < 8; iNbr++) UpdateUsed(iNbr);

		// Update usage flags for diagonals that cut across the LT,RT,LB,RB diagonals.
		// Call these LTX,RTX,LBX,RBX respectively.
		// The point of doing this is that if "this" element has a diagonal connection that
		// we've just cleared, then previously blocked diagonals may now be usable.
		GetNbr(NBR_L)->UpdateUsed(NBR_RT);	GetNbr(NBR_R)->UpdateUsed(NBR_LT);	// LTX, RTX
		GetNbr(NBR_L)->UpdateUsed(NBR_RB);	GetNbr(NBR_R)->UpdateUsed(NBR_LB);	// LBX, RBX
	}
	void SetSolderR(const bool& b)				{ m_bSolderR	= b; }
	void SetRoutable(const int& i)				{ m_iRoutable	= i; }
	void SetMH(const unsigned int& i)			{ m_MH			= i; }
	void SetMaxMH(const unsigned int& i)		{ m_maxMH		= i; }
	void SetRouteId(const unsigned int& i)		{ m_routeId		= i; }
	void SetNbr(const int& iNbr, Element* p)	{ m_pNbr[iNbr]	= p; }
	void SetW(Element* p)						{ m_pW			= p; }

	const bool&			GetSolderR() const				{ return m_bSolderR; }
	const int&			GetRoutable() const				{ return m_iRoutable; }
	const unsigned int&	GetMH() const					{ return m_MH; }
	const unsigned int& GetMaxMH() const				{ return m_maxMH; }
	const unsigned int&	GetRouteId() const				{ return m_routeId; }
	Element*			GetNbr(const int& iNbr) const	{ return m_pNbr[iNbr]; }
	Element*			GetW() const					{ return m_pW; }

	// Helpers
	bool HaveNonBlankPins(const int& iNbr) const
	{
		Element* pNbr = GetNbr(iNbr);
		return 	( !this->GetIsPin() || this->GetNodeId() != BAD_NODEID || this->GetW() ) &&	// Only allow routing FROM blank pins if they are on wires
				( !pNbr->GetIsPin() || pNbr->GetNodeId() != BAD_NODEID || pNbr->GetW() );	// Only allow routing  TO  blank pins if they are on wires
	}
	// Connectivity helpers
	void UpdateUsed(const int& iNbr)
	{
		const bool bUsed = CheckUsed(iNbr);
		SetUsed(iNbr, bUsed);
		m_pNbr[iNbr]->SetUsed(Opposite(iNbr), bUsed);			// Keep consistent with nbr
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
		Element* pW = GetW();	if ( pW ) { pW->ClearFlagBits(AUTOSET|VEROSET);	pW->SetFlagBits(USERSET); }
		pW = pNbr->GetW();		if ( pW ) { pW->ClearFlagBits(AUTOSET|VEROSET);	pW->SetFlagBits(USERSET); }
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
		for (int iNbr = 0; iNbr < 8; iNbr++)
			if ( GetNbr(iNbr) == p ) return true;
		return false;
	}			
	bool IsUselessWire(const int& iNbr, const int& nodeId) const	// Helper: true ==> painting nbr with nodeId is wasteful
	{
		const Element* pW1 = GetNbr(iNbr);
		const Element* pW2 = pW1->GetW();
		if ( pW2 == nullptr ) return false;
		// pW1 and pW2 are opposite ends of a wire.
		// If these ends both neighbour a common element with the specified nodeId,
		// then it is wasteful to paint the wire with that nodeId too, since the
		// common element already provides a connection.
		for (int iNbr = 0; iNbr < 8; iNbr++)
		{
			const Element* p = pW1->GetNbr(iNbr);
			if ( p->GetNodeId() == nodeId && pW2->IsNbr(p) ) return true;
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
	// Persist interface functions
	virtual void Load(DataStream& inStream) override
	{
		CompElement::Load(inStream);	// Load() base class
	}
	virtual void Save(DataStream& outStream) override
	{
		CompElement::Save(outStream);	// Save() base class
	}
private:
	bool CheckUsed(const int& iNbr) const
	{
		return GetNodeId() != BAD_NODEID && GetNodeId() == GetNbr(iNbr)->GetNodeId() && !IsBlocked(iNbr, GetNodeId());
	}
private:
	// Working variables.	Don't persist.
	bool			m_bSolderR;		// true ==> have blob of solder to right (for joining vero tracks)
	int				m_iRoutable;	// Set by Board::GlueNbrs().  An 8-bit code used to enable/disable connections to the 8 neighbours
	unsigned int	m_MH;			// Manhatten distance to another element.  For the routing/connectivity algorithm.
	unsigned int	m_maxMH;		// For the routing algorithm.
	unsigned int	m_routeId;		// For the routing algorithm.
	// Connection pointers. Set by Board::GlueNbrs() and Board::GlueWires().	Don't persist.
	Element*		m_pNbr[8];		// 0 to 7 <==> NBR_L to NBR_LB
	Element*		m_pW;			// Element at other end of wire/jumper (if one end is here)
};
