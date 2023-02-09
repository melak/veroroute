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

#include "Common.h"
#include <QPoint>
#include <QPolygon>

// Bits used to construct the GPEN enum
static const int	BIT_GKO(1),	 BIT_PAD(2), BIT_VIA(4), BIT_TRK(8), BIT_TAG(16),
					BIT_SLK(32), BIT_GAP(64), BIT_MSK(128), BIT_HLE(256);

// The GPEN enum defines "pen" types for writing to Gerber/Excellon files
enum class GPEN
{
	NONE	= 0,
	GKO		= BIT_GKO,				// Used for board outline
	PAD		= BIT_PAD,				// Used for pad
	VIA		= BIT_VIA,				// Used for via
	TRK		= BIT_TRK,				// Used for track
	TAG		= BIT_TAG,				// Used for thermal relief tags
	SLK		= BIT_SLK,				// Used for silkscreen
	PAD_GAP	= BIT_PAD | BIT_GAP,	// Used for gap around a pad
	VIA_GAP	= BIT_VIA | BIT_GAP,	// Used for gap around a via
	TRK_GAP	= BIT_TRK | BIT_GAP,	// Used for gap around a track
	PAD_MSK	= BIT_PAD | BIT_MSK,	// Used for solder mask at a pad
	VIA_MSK	= BIT_VIA | BIT_MSK,	// Used for solder mask at a via
	PAD_HLE	= BIT_PAD | BIT_HLE,	// Used for drill hole at a pad
	VIA_HLE	= BIT_VIA | BIT_HLE		// Used for drill hole at a via
};

// A class describing a curve as a set of points, with functionality for combining curves.
// Used for processing data before writing to Gerber file.

class Curve : public std::list<QPoint>	// A curve drawn in a fixed size pen
{
public:
	Curve() {}
	Curve(const QPoint& p, GPEN ePen, int width = 0);
	Curve(const QPolygon& polygon, GPEN ePen, int width = 0);
	~Curve() { clear(); }
	void Compress();		// Removes redundant points
	bool Splice(Curve* pB);	// Tries to splice curve B to this
	struct HasLargerPen		// Predicate for sorting
	{
		bool operator() (const Curve* p1, const Curve* p2) const
		{
			assert( static_cast<int>(GPEN::PAD) < static_cast<int>(GPEN::VIA) );
			if ( p1->m_width != p2->m_width ) return p1->m_width > p2->m_width;
			return static_cast<int>(p1->m_ePen) < static_cast<int>(p2->m_ePen);	// Pads before Vias if equal size
		}
	};
	GPEN	m_ePen	= GPEN::NONE;
	int		m_width	= 0;
};

class CurveList : public std::list<Curve*>
{
public:
	CurveList()		{}
	~CurveList()	{ Clear(); }
	void Clear();
	void Sort();
	void SpliceAll();
};
