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

const int			TRAX_COMPID = -2;		// The component manager member m_trax has this ID
const int			BAD_COMPID  = -1;		// Invalid component ID
const int			BAD_NODEID  =  0;		// Invalid node (i.e. netlist) ID
const unsigned int	BAD_ROUTEID =  0;		// Invalid route (i.e. track section) ID
const unsigned int	BAD_MH		= UINT_MAX;	// "Infinite" MH distance

// Flag is a bitfield describing the status of the nodeId at an element.
// USERSET elements will not have their nodeId modified during the auto-routing.
// The algorithm will change the flag from USERSET to AUTOKEPT if it thinks the element is useful.
// On hitting "Paste+Tidy", only the AUTOKEPT and AUTOSET points are kept, and the USERSET
// points will be wiped (if they are not component pins).
const char USERSET	= 1;					// ==> user assigned the nodeId
const char AUTOSET	= 2;					// ==> routing algorithm assigned the nodeId
const char AUTOKEPT	= (USERSET | AUTOSET);	// ==> user assigned the nodeId, and routing algorithm agrees it is useful
const char VEROSET	= 4;					// ==> auto assigned to create Vero strips
const char RECTSET	= 8;					// ==> is within a user-defined rect

// Indexes for the eight neighbour elements, starting on the left and going clockwise
const int	NBR_L(0), NBR_LT(1), NBR_T(2), NBR_RT(3),		// Left,  Left-Top,     Top,    Right-Top,
			NBR_R(4), NBR_RB(5), NBR_B(6), NBR_LB(7);		// Right, Right-Bottom, Bottom, Left-Bottom
static int	Opposite(int NBR) { return ( NBR + 4 ) % 8; }	// Helper to get opposite neighbour index

// Functions for mapping NBR indices to "code bits" and manipulating them
static bool ReadCodeBit(const int& NBR, const int& iCode)	{ return ( iCode & (1<<NBR) ) != 0; }
static void SetCodeBit(const int& NBR, int& iCode)			{ iCode |=  (1<<NBR); }
static void ClearCodeBit(const int& NBR, int& iCode)		{ iCode &= ~(1<<NBR); }
static void ToggleCodeBit(const int& NBR, int& iCode)		{ iCode ^=  (1<<NBR); }

class CompElement : public Pin
{
	friend class Element;
public:
	CompElement()
	: Pin()
	, m_compId(BAD_COMPID)
	, m_nodeId(BAD_NODEID)
	, m_iCode(0)
	, m_flag(USERSET)
	, m_bIsVia(false)
	{
	}
	CompElement(const CompElement& o) : Pin(o)	{ *this = o; }
	~CompElement() {}
	CompElement& operator=(const CompElement& o)
	{
		Pin::operator=(o);	// Call operator= in base class
		m_compId	= o.m_compId;
		m_nodeId	= o.m_nodeId;
		m_iCode		= o.m_iCode;
		m_flag		= o.m_flag;
		m_bIsVia	= o.m_bIsVia;
		return *this;
	}
	bool operator==(const CompElement& o) const	// Compare persisted info
	{
		return	Pin::operator==(o)
			&&	m_compId	== o.m_compId
			&&	m_nodeId	== o.m_nodeId
			&&	m_iCode		== o.m_iCode
			&&	m_flag		== o.m_flag
			&&	m_bIsVia	== o.m_bIsVia;
	}
	bool operator!=(const CompElement& o) const
	{
		return !(*this == o);
	}
	void SetNodeId(const int& i)				{ m_nodeId		= i; }
	void SetCompId(const int& i)				{ m_compId		= i; }
	void SetCode(const int& i)					{ m_iCode		= i; }
	void SetFlag(const char& i)					{ m_flag		= i; }
	void SetIsVia(const bool& b)				{ m_bIsVia		= b; }
	void SetUsed(const int& iNbr, const bool& b){ if ( b ) SetCodeBit(iNbr, m_iCode); else ClearCodeBit(iNbr, m_iCode); }

	const int&	GetNodeId() const				{ return m_nodeId; }
	const int&	GetCompId() const				{ return m_compId; }
	const int&	GetCode() const					{ return m_iCode; }
	const char&	GetFlag() const					{ return m_flag; }
	const bool&	GetIsVia() const				{ return m_bIsVia; }
	bool		GetUsed(const int& iNbr) const	{ return ReadCodeBit(iNbr, m_iCode); }

	// Flag Helpers
	bool ReadFlagBits(const char& i) const		{ return ( m_flag & i ) != 0; }
	void SetFlagBits(const char& i)				{ m_flag |=  i; }
	void ClearFlagBits(const char& i)			{ m_flag &= ~i; }

	// Connectivity helpers
	bool IsClash(const int& nodeId) const
	{
		return nodeId != BAD_NODEID && m_nodeId != BAD_NODEID && nodeId != m_nodeId;
	}
	int GetPerimeterCode(const bool& bDiagsOK, const bool& bMinDiags)	const // Helper for the GUI "blobs"
	{
		int iCode = GetCode();	// Take a copy of the connection code

		// Enforce any restrictions on diagonal connections acccording to the GUI options on diagonals
		const bool	bL( ReadCodeBit(NBR_L, iCode) ),	bT( ReadCodeBit(NBR_T, iCode) ),
					bR( ReadCodeBit(NBR_R, iCode) ),	bB( ReadCodeBit(NBR_B, iCode) );
		if ( ( !bDiagsOK && !(bL && bT) ) || ( bMinDiags && (bL != bT) ) ) ClearCodeBit(NBR_LT, iCode);
		if ( ( !bDiagsOK && !(bR && bT) ) || ( bMinDiags && (bR != bT) ) ) ClearCodeBit(NBR_RT, iCode);
		if ( ( !bDiagsOK && !(bL && bB) ) || ( bMinDiags && (bL != bB) ) ) ClearCodeBit(NBR_LB, iCode);
		if ( ( !bDiagsOK && !(bR && bB) ) || ( bMinDiags && (bR != bB) ) ) ClearCodeBit(NBR_RB, iCode);
		return iCode;
	}
	// Merge interface functions
	virtual void UpdateMergeOffsets(MergeOffsets& o) override
	{
		Pin::UpdateMergeOffsets(o);	// Does nothing
		if ( m_compId != BAD_COMPID &&
			 m_compId != TRAX_COMPID ) o.deltaCompId = std::max(o.deltaCompId,  m_compId + 1);
		if ( m_nodeId != BAD_NODEID  ) o.deltaNodeId = std::max(o.deltaNodeId,  m_nodeId + 1);
	}
	virtual void ApplyMergeOffsets(const MergeOffsets& o) override
	{
		Pin::ApplyMergeOffsets(o);	// Does nothing
		if ( m_compId != BAD_COMPID
		&&	 m_compId != TRAX_COMPID) m_compId += o.deltaCompId;
		if ( m_nodeId != BAD_NODEID ) m_nodeId += o.deltaNodeId;
	}
	void Merge(const CompElement& o)
	{
		*this = o;
	}
	// Persist interface functions
	virtual void Load(DataStream& inStream) override
	{
		Pin::Load(inStream);	// Load() base class
		inStream.Load(m_compId);
		inStream.Load(m_nodeId);
		inStream.Load(m_iCode);
		inStream.Load(m_flag);
		inStream.Load(m_bIsVia);
		if ( inStream.GetVersion() < VRT_VERSION_11 )
			ConvertLegacyFlag();
	}
	virtual void Save(DataStream& outStream) override
	{
		Pin::Save(outStream);	// Save() base class
		outStream.Save(m_compId);
		outStream.Save(m_nodeId);
		outStream.Save(m_iCode);
		outStream.Save(m_flag);
		outStream.Save(m_bIsVia);
	}
private:
	void ConvertLegacyFlag()	// Map legacy flag values to new bitfield based values
	{
		switch (m_flag)
		{
			case 0:	m_flag = USERSET;	break;
			case 1: m_flag = AUTOKEPT;	break;
			case 2: m_flag = AUTOSET;	break;
			case 3: m_flag = VEROSET;	break;
			default: assert(0);
		}
	}
private:
	int		m_compId;	// For elements with a valid pinindex, this is the ID of the parent component
	int		m_nodeId;	// The netlist value assigned to the element (or BAD_NODEID if not set)
	int		m_iCode;	// An 8-bit code describing connections to the 8 neighbours.  Bit set ==> connection used
	char	m_flag;		// For routing:  USERSET/AUTOKEPT/AUTOSET/VEROSET
	bool	m_bIsVia;
};
