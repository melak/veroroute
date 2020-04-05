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

#include <list>

class QPoint;
class QPolygon;

const int	BIT_GKO(1),	 BIT_PAD(2), BIT_VIA(4), BIT_TRK(8),
			BIT_SLK(16), BIT_GAP(32), BIT_MSK(64), BIT_HLE(128);

enum class GPEN
{
	NONE	= 0,
	GKO		= BIT_GKO,
	PAD		= BIT_PAD,
	VIA		= BIT_VIA,
	TRK		= BIT_TRK,
	SLK		= BIT_SLK,
	PAD_GAP	= BIT_PAD | BIT_GAP,
	VIA_GAP	= BIT_VIA | BIT_GAP,
	TRK_GAP	= BIT_TRK | BIT_GAP,
	PAD_MSK	= BIT_PAD | BIT_MSK,
	VIA_MSK	= BIT_VIA | BIT_MSK,
	PAD_HLE	= BIT_PAD | BIT_HLE,
	VIA_HLE	= BIT_VIA | BIT_HLE
};

// A class describing a curve as a set of points, with functionality for combining curves.
// Used for processing data before writing to Gerber file.

class Curve : public std::list<QPoint>	// A curve drawn in a fixed size pen
{
public:
	Curve() {}
	Curve(const QPoint& p, const GPEN& ePen, const int& width = 0);
	Curve(const QPolygon& polygon, const GPEN& ePen, const int& width = 0);
	~Curve() { clear(); }
	void Compress();		// Removes redundant points
	bool Splice(Curve* pB);	// Tries to splice curve B to this
	struct HasSmallerPen	// Predicate for sorting
	{
		bool operator() (const Curve* p1, const Curve* p2) const
		{
			if ( p1->m_ePen != p2->m_ePen ) return (int)(p1->m_ePen) < (int)(p2->m_ePen);
			return p1->m_width < p2->m_width;
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
	void Clear()	{ for (auto& p : *this) p->clear(); clear(); }
	void SpliceAll();
};
