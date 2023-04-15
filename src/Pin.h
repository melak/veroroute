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

#include "Persist.h"

//	"Pin" is a base class for "CompElement" and "Element".
//	A component's spatial layout (or "Footprint") is a two-dimensional array of "CompElement" objects.
//	The "Board" object used for designing a circuit is a two-dimensional array of "Element" objects.
//
//	There are 4 parts to the description of a "Pin":
//
//	1)	The pin character "m_pinChar" is just a pinIndex in the range 0 to 254.
//		Places with no pin (e.g. the middle of a resistor) have m_pinChar of 255 and an invalid pinIndex.
//
//	2)	The surface occupancy "m_surface" models the interaction between a component and the board surface.
//
//		SURFACE_FREE		==> the board surface is not occupied.
//		SURFACE_GAP			==> the gap between IC pins.
//		SURFACE_WIRE_END	==> a wire (endpoint).
//		SURFACE_WIRE		==> a wire (not endpoint).
//		SURFACE_PLUG		==> e.g. resistor/diode/pad etc.  Can be placed in a "GAP".
//		SURFACE_FULL		==> the board surface is fully occupied.  Nothing else can be placed there.
//		SURFACE_NOPAINT		==> If this bit is set, then no paint can be applied.
//		SURFACE_HOLE		==> SURFACE_FULL + SURFACE_NOPAINT
//
//	3)	The hole occupancy "m_holeUse" models the interaction between a component and the board holes.
//
//		HOLE_FREE			==> the hole is not occupied.
//		HOLE_WIRE			==> the hole is occupied by one wire.
//		HOLE_FULL			==> the hole is fully occupied.  (By a regular component pin, or by 2 wires).
//
//	4)	SOIC info.  We store this info on the base layer of the grid, but it refers to whatever
//		layer displays the SOIC pattern (i.e. "the SOIC layer").
//
//		SOIC_NO			==> No SOIC pattern 
//		SOIC_YES		==> Have SOIC pattern (so not point is not paintable on the SOIC layer)

Q_DECL_CONSTEXPR static const uchar  BAD_PINCHAR		= 255;
Q_DECL_CONSTEXPR static const size_t BAD_PININDEX		= static_cast<size_t>(-1);

Q_DECL_CONSTEXPR static const uchar  SURFACE_FREE		= 0;
Q_DECL_CONSTEXPR static const uchar  SURFACE_GAP		= 1;
Q_DECL_CONSTEXPR static const uchar  SURFACE_WIRE_END	= 2;	// Hence: "SURFACE_WIRE_END + SURFACE_WIRE_END == SURFACE_WIRE"
Q_DECL_CONSTEXPR static const uchar  SURFACE_WIRE		= 4;	// Hence: "SURFACE_WIRE + SURFACE_WIRE == SURFACE_PLUG"
Q_DECL_CONSTEXPR static const uchar  SURFACE_PLUG		= 8;
Q_DECL_CONSTEXPR static const uchar  SURFACE_FULL		= 9;	// Hence: "SURFACE_PLUG + SURFACE_GAP == SURFACE_FULL"
Q_DECL_CONSTEXPR static const uchar  SURFACE_NOPAINT	= 16;	// Should only be used as part of SURFACE_HOLE
Q_DECL_CONSTEXPR static const uchar  SURFACE_HOLE		= 25;	// Hence: "SURFACE_FULL + SURFACE_NOPAINT = SURFACE_HOLE"

Q_DECL_CONSTEXPR static const uchar  HOLE_FREE			= 0;
Q_DECL_CONSTEXPR static const uchar  HOLE_WIRE			= 1;	// Hence: "HOLE_WIRE + HOLE_WIRE == HOLE_FULL"
Q_DECL_CONSTEXPR static const uchar  HOLE_FULL			= 2;

Q_DECL_CONSTEXPR static const uchar  SOIC_NO			= 0;
Q_DECL_CONSTEXPR static const uchar  SOIC_YES			= 1;


Q_DECL_CONSTEXPR static size_t GetPinIndexFromLegacyPinChar(uchar c)	// Legacy VRT format had messy mapping of pinChar to pinIndex
{
	return ( c <  '1' ) ? BAD_PININDEX		// Handles the '.', '-', '+' characters used by GetMakeInstructions()
		 : ( c <= '9' ) ? (     c - '1')	// Char '1' to '9' ==> Index 0  to 8
		 : ( c <= '@' ) ? (61 + c - ':')	// Char ':' to '@' ==> Index 61 to 67
		 : ( c <= 'Z' ) ? (9  + c - 'A')	// Char 'A' to 'Z' ==> Index 9  to 34
		 : ( c <= '`' ) ? (68 + c - '[')	// Char '[' to '`' ==> Index 68 to 73
		 : ( c <= 'z' ) ? (35 + c - 'a')	// Char 'a' to 'z' ==> Index 35 to 60
						: (74 + c - '{');	// Char '{' to 255 ==> Index 74 to 206
}

static uchar GetSurfaceFromLegacySurfaceChar(uchar c)
{
	// The following are the old SURFACE codes before the introduction of SURFACE_WIRE_END and SURFACE_WIRE
	switch( c )
	{
		case 0:		return SURFACE_FREE;
		case 1:		return SURFACE_GAP;
		case 2:		return SURFACE_PLUG;
		case 3:		return SURFACE_FULL;
		case 4:		return SURFACE_NOPAINT;
		case 7:		return SURFACE_HOLE;
		default:	assert(0); return SURFACE_FREE;
	}
}

class Pin : public Persist, public Merge
{
public:
	Pin(uchar pinChar = BAD_PINCHAR, uchar surface = SURFACE_FREE, uchar holeUse = HOLE_FREE, uchar soicChar = SOIC_NO)
	: m_pinChar(pinChar)
	, m_surface(surface)
	, m_holeUse(holeUse)
	, m_soicChar(soicChar)
	{}
	Pin(const Pin& o) { *this = o; }
	virtual ~Pin() {}
	Pin& operator=(const Pin& o)
	{
		m_pinChar	= o.m_pinChar;
		m_surface	= o.m_surface;
		m_holeUse	= o.m_holeUse;
		m_soicChar	= o.m_soicChar;
		return *this;
	}
	bool operator==(const Pin& o) const	// Compare persisted info
	{
		return m_pinChar	== o.m_pinChar
			&& m_surface	== o.m_surface
			&& m_holeUse	== o.m_holeUse
			&& m_soicChar	== o.m_soicChar; 
	}
	bool operator!=(const Pin& o) const
	{
		return !(*this == o);
	}
	void SetPinIndex(size_t i)	{ m_pinChar = ( i >= BAD_PINCHAR ) ? BAD_PINCHAR : static_cast<uchar> (i); }
	void SetSurface(uchar c)	{ m_surface = c; }
	void SetHoleUse(uchar c)	{ m_holeUse = c; }
	void SetSoicChar(uchar c)	{ m_soicChar = c; }
	void SetOccupancy(bool bWire)	// Helper
	{
		if ( bWire )
		{
			SetSurface( GetIsPin() ? SURFACE_WIRE_END	: SURFACE_WIRE );	// Set surface occupancy for pins/non-pins
			SetHoleUse( GetIsPin() ? HOLE_WIRE			: HOLE_FREE );		// Set hole occupancy for pins/non-pins
		}
		else
		{
			SetHoleUse( GetIsPin() ? HOLE_FULL			: HOLE_FREE );		// Set hole occupancy for pins/non-pins
		}
	}
	size_t		 GetPinIndex() const		{ return ( m_pinChar == BAD_PINCHAR ) ? BAD_PININDEX : m_pinChar; }
	const uchar& GetSurface() const			{ return m_surface; }
	const uchar& GetHoleUse() const			{ return m_holeUse; }
	const uchar& GetSoicChar() const		{ return m_soicChar; }
	bool		 GetIsPin() const			{ return m_pinChar != BAD_PINCHAR; }
	bool		 GetIsHole() const			{ return m_surface == SURFACE_HOLE; }

	static const std::map<uchar, std::string>& GetMapSurfaceStrings()
	{
		static std::map<uchar, std::string>	mapSurfaceToStr;	// Mapping of SURFACE to strings for Component Editor
		if ( mapSurfaceToStr.empty() )
		{
			mapSurfaceToStr[SURFACE_FULL]	= "Full";
			mapSurfaceToStr[SURFACE_FREE]	= "Free";
			mapSurfaceToStr[SURFACE_HOLE]	= "Hole";
		}
		return mapSurfaceToStr;
	}
	static const std::list<std::string>& GetListSurfaceStrings()
	{
		static std::list<std::string>	listSurfaceStr;	// Use for populating combo boxes in Component Editor
		if ( listSurfaceStr.empty() )
		{
			listSurfaceStr.push_back("Full");
			listSurfaceStr.push_back("Free");
			listSurfaceStr.push_back("Hole");
		}
		return listSurfaceStr;
	}

	// Merge interface functions
	virtual void UpdateMergeOffsets(MergeOffsets&) override {}
	virtual void ApplyMergeOffsets(const MergeOffsets&) override {}
	void Merge(const Pin& o) { *this = o; }
	// Persist interface functions
	virtual void Load(DataStream& inStream) override
	{
		inStream.Load(m_pinChar);
		inStream.Load(m_surface);
		if ( inStream.GetVersion() < VRT_VERSION_4 )	// Remap m_pinChar if file is older than VRT_VERSION_4
			SetPinIndex( GetPinIndexFromLegacyPinChar(m_pinChar) );
		if ( inStream.GetVersion() < VRT_VERSION_26 )	// Remap m_surface if file is older than VRT_VERSION_26
			SetSurface( GetSurfaceFromLegacySurfaceChar(m_surface) );
		m_holeUse = GetIsPin() ? HOLE_FULL : HOLE_FREE;
		if ( inStream.GetVersion() >= VRT_VERSION_26 )
			inStream.Load(m_holeUse);	// Added in VRT_VERSION_26
		if ( inStream.GetVersion() <= VRT_VERSION_39 )
			if ( GetIsPin() && m_holeUse == HOLE_FREE ) m_holeUse = HOLE_FULL;	// Bug-fix non-wire hole-use
		m_soicChar = SOIC_NO;
		if ( inStream.GetVersion() >= VRT_VERSION_55 )
			inStream.Load(m_soicChar);	// Added in VRT_VERSION_55
	}
	virtual void Save(DataStream& outStream) override
	{
		outStream.Save(m_pinChar);	// New mapping from VRT_VERSION_4
		outStream.Save(m_surface);	// New mapping from VRT_VERSION_26
		outStream.Save(m_holeUse);	// Added in VRT_VERSION_26
		outStream.Save(m_soicChar);	// Added in VRT_VERSION_55
	}
private:
	// Data
	uchar	m_pinChar;
	uchar	m_surface;
	uchar	m_holeUse;
	uchar	m_soicChar;
};
