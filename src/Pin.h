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

//	"Pin" is a base class for "CompElement" which in turn is a base class for "Element".
//	A component's spatial layout (or "Footprint") is a two-dimensional array of "CompElement" objects.
//	The "Board" object used for designing a circuit is a two-dimensional array of "Element" objects.
//
//	There are 2 parts to the description of a "Pin":
//
//	1)	The surface description "m_surface" models the interaction between a component and the board surface.
//
//		SURFACE_FREE	==> the board surface is not occupied.
//		SURFACE_GAP		==> the gap between IC pins.  We may place a "PLUG" component there.
//		SURFACE_PLUG	==> e.g. a wire/resistor/diode/pad/via etc.  Can be placed in a "GAP".
//		SURFACE_FULL	==> the board surface is occupied.  Nothing else can be placed there.
//		SURFACE_NOPAINT ==> If this bit is set, then no paint can be applied.
//		SURFACE_HOLE	==> SURFACE_FULL + SURFACE_NOPAINT
//
//	2)	The pin character "m_pinChar" is just a pinIndex in the range 0 to 254.
//		Places with no pin (e.g. the middle of a resistor) have m_pinChar of 255 and an invalid pinIndex.

const uchar  SURFACE_FREE		= 0;
const uchar  SURFACE_GAP		= 1;
const uchar  SURFACE_PLUG		= 2;
const uchar  SURFACE_FULL		= 3;	// Hence: "SURFACE_GAP + SURFACE_PLUG = SURFACE_FULL"
const uchar  SURFACE_NOPAINT	= 4;	// Should only be used as part of SURFACE_HOLE.
const uchar  SURFACE_HOLE		= 7;	// Hence: "SURFACE_FULL + SURFACE_NOPAINT = SURFACE_HOLE"

const uchar	 BAD_PINCHAR	= 255;
const size_t BAD_PININDEX	= -1;

static size_t GetPinIndexFromLegacyPinChar(const uchar& c)	// Legacy VRT format had messy mapping of pinChar to pinIndex
{
	if ( c <  '1' )	return BAD_PININDEX;	// Handles the '.', '-', '+' characters used by GetMakeInstructions()
	if ( c <= '9' )	return      c - '1';	// Char '1' to '9' ==> Index 0  to 8
	if ( c <= '@' )	return 61 + c - ':';	// Char ':' to '@' ==> Index 61 to 67
	if ( c <= 'Z' )	return 9  + c - 'A';	// Char 'A' to 'Z' ==> Index 9  to 34
	if ( c <= '`' )	return 68 + c - '[';	// Char '[' to '`' ==> Index 68 to 73
	if ( c <= 'z' )	return 35 + c - 'a';	// Char 'a' to 'z' ==> Index 35 to 60
	else			return 74 + c - '{';	// Char '{' to 255 ==> Index 74 to 206
}

class Pin : public Persist, public Merge
{
public:
	Pin(uchar pinChar = BAD_PINCHAR, uchar surface = SURFACE_FREE)
	: m_pinChar(pinChar)
	, m_surface(surface)
	{}
	Pin(const Pin& o) { *this = o; }
	~Pin() {}
	Pin& operator=(const Pin& o)
	{
		m_pinChar	= o.m_pinChar;
		m_surface	= o.m_surface;
		return *this;
	}
	bool operator==(const Pin& o) const	// Compare persisted info
	{
		return m_pinChar == o.m_pinChar
			&& m_surface == o.m_surface;
	}
	bool operator!=(const Pin& o) const
	{
		return !(*this == o);
	}
	void		 Clear()						{ SetSurface(SURFACE_FREE); SetPinIndex(BAD_PININDEX); }
	void		 SetSurface(const uchar& c)		{ m_surface = c; }
	void		 SetPinIndex(const size_t& i)	{ m_pinChar = ( i >= BAD_PINCHAR ) ? BAD_PINCHAR : static_cast<uchar> (i); }
	const uchar& GetSurface() const				{ return m_surface; }
	size_t		 GetPinIndex() const			{ return ( m_pinChar == BAD_PINCHAR ) ? BAD_PININDEX : m_pinChar; }
	bool		 GetIsPin() const				{ return m_pinChar != BAD_PINCHAR; }
	bool		 GetIsHole() const				{ return m_surface == SURFACE_HOLE; }

	// Merge interface functions
	virtual void UpdateMergeOffsets(MergeOffsets&) override
	{
	}
	virtual void ApplyMergeOffsets(const MergeOffsets&) override
	{
	}
	// Persist interface functions
	virtual void Load(DataStream& inStream) override
	{
		inStream.Load(m_pinChar);
		inStream.Load(m_surface);
		if ( inStream.GetVersion() < VRT_VERSION_4 )	// Remap m_pinChar if file is older than VRT_VERSION_4
			SetPinIndex( GetPinIndexFromLegacyPinChar(m_pinChar) );
	}
	virtual void Save(DataStream& outStream) override
	{
		outStream.Save(m_pinChar);	// New mapping from VRT_VERSION_4
		outStream.Save(m_surface);
	}
private:
	// Data
	uchar	m_pinChar;
	uchar	m_surface;
};
