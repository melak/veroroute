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
#include <QPolygonF>
#include "CurveList.h"	// For GPEN

// A helper for calculating separations between tracks

struct MyPointF : public QPointF		// A point + the pen radius for drawing it
{
	MyPointF(qreal x = 0, qreal y = 0, qreal radius = 0) : QPointF(x,y), m_radius(radius) {}
	~MyPointF() {}
	MyPointF(const QPointF& p, const qreal& radius) : QPointF(p), m_radius(radius) {}
	MyPointF(const MyPointF& o) : QPointF(o), m_radius(o.m_radius) {}
	MyPointF& operator=(const MyPointF& o)	{ QPointF::operator=(o); m_radius = o.m_radius; return *this; }
	qreal	m_radius	= 0;		// Pen radius
};

struct MyPolygonF : public QPolygonF	// A polygon + the pen radii for drawing it
{
	MyPolygonF() {}
	~MyPolygonF() { m_bFatPoint.clear(); m_bFatEdge.clear(); }
	MyPolygonF(const QPolygonF& p, const GPEN& eTrkPen, const GPEN& ePadPen, const qreal& radiusTrk, const qreal& radiusPad, bool bClosed)
		: QPolygonF(p), m_eTrkPen(eTrkPen), m_ePadPen(ePadPen), m_radiusTrk(radiusTrk), m_radiusPad(radiusPad), m_bClosed(bClosed)
	{
		Process();
	}
	MyPolygonF(const MyPolygonF& o)
		: QPolygonF(o), m_eTrkPen(o.m_eTrkPen), m_ePadPen(o.m_ePadPen), m_radiusTrk(o.m_radiusTrk), m_radiusPad(o.m_radiusPad), m_bClosed(o.m_bClosed)
	{
		Process();
	}
	MyPolygonF& operator=(const MyPolygonF& o)
	{
		QPolygonF::operator=(o);
		m_eTrkPen	= o.m_eTrkPen;
		m_ePadPen	= o.m_ePadPen;
		m_radiusTrk	= o.m_radiusTrk;
		m_radiusPad	= o.m_radiusPad;
		m_bClosed	= o.m_bClosed;
		Process();
		return *this;
	}
	bool HaveVariTracks() const { return QPolygonF::size() > 1 && m_radiusPad > m_radiusTrk && m_radiusTrk > 0; }
	void Process(bool bForce = false) const
	{
		// Set flags to indicate if points and edges are fat or thin
		const int iSize = QPolygonF::size();
		if ( !bForce && m_bFatPoint.size() == (size_t) iSize ) return;
		m_bFatPoint.resize(iSize, false);
		m_bFatEdge.resize(iSize, false);
		if ( !HaveVariTracks() ) return;
		for (int i = 0, j = 1, iEnd = m_bClosed ? iSize : (iSize-1); i < iEnd; i++, j++)
		{
			if ( j == iSize ) j = 0;
			if ( operator[](i).x() == operator[](j).x() || operator[](i).y() == operator[](j).y() )
				m_bFatPoint[i] = m_bFatPoint[j] = m_bFatEdge[i] = true;
		}
	}
	// If both pens are set, then it means we are in fat tracks mode
	// and using the pad pen for the HV sections instead of the track pen
	GPEN	m_eTrkPen	= GPEN::NONE;	// TRK, TRK_GAP, or NONE
	GPEN	m_ePadPen	= GPEN::NONE;	// PAD, PAD_GAP, or NONE
	qreal	m_radiusTrk	= 0;			// Trk radius
	qreal	m_radiusPad	= 0;			// Pen radius
	bool	m_bClosed	= false;		// Flag to indicate closed polygon
	// A cache indicating the points and edges that are fat (i.e. have pad radius)
	mutable std::vector<bool> m_bFatPoint;	//
	mutable std::vector<bool> m_bFatEdge;	// [i] means edge from point with index i to point with index i+1
};

struct PolygonHelper
{
	PolygonHelper()		{ m_pWarn.clear(); }
	~PolygonHelper()	{ m_pWarn.clear(); }
	QPolygonF	m_pWarn;			// Set of warning points
	qreal		m_Dmin = DBL_MAX;	// The closest separation found

	inline void CalcSeparation(const MyPointF& X, const MyPointF& Y)
	{
		const qreal sum		= ( X.m_radius + Y.m_radius );
		const qreal semi	= ( X.m_radius - Y.m_radius ) * 0.5;
		Update(X, Y, sum, semi);
	}
	inline void CalcSeparation(const MyPointF& X, const MyPolygonF& P)
	{
		if ( P.empty() ) return;
		//P.Process();	// Not needed since m_bFatEdge already populated
		const bool  bVariTracks	= P.HaveVariTracks();
		const qreal sum			= ( X.m_radius + P.m_radiusTrk );
		const qreal semi		= ( X.m_radius - P.m_radiusTrk ) * 0.5;
		const qreal sumHV		= bVariTracks ? (   X.m_radius + P.m_radiusPad         ) : sum;
		const qreal semiHV		= bVariTracks ? ( ( X.m_radius - P.m_radiusPad ) * 0.5 ) : semi;

		const int iSize = P.size();
		if ( iSize == 1 ) return Update(X, P[0], sum, semi);

		for (int i = 0, j = 1, iEnd = P.m_bClosed ? iSize : (iSize-1); i < iEnd; i++, j++)
		{
			if ( j == iSize ) j = 0;
			if ( P.m_bFatEdge[i] )	Update(X, Closest(X, P[i], P[j]), sumHV, semiHV);
			else					Update(X, Closest(X, P[i], P[j]), sum, semi);
		}
	}
	inline void CalcSeparation(const MyPolygonF& P, const MyPolygonF& Q)
	{
		if ( P.empty() || Q.empty() ) return;
		//P.Process();	Q.Process();	// Not needed since m_bFatPoint already populated
		size_t i(0), j(0);
		for (auto& p : P) CalcSeparation(MyPointF(p, P.m_bFatPoint[i++] ? P.m_radiusPad : P.m_radiusTrk), Q);
		for (auto& q : Q) CalcSeparation(MyPointF(q, Q.m_bFatPoint[j++] ? Q.m_radiusPad : Q.m_radiusTrk), P);
	}
private:
	static inline QPointF Closest(const QPointF& X, const QPointF& A, const QPointF& B)	// Closest point to X on line segment A-B
	{
		if ( A == B ) return A;
		const QPointF	AB(B - A), AX(X - A);
		const qreal		lambda = std::max(0.0, std::min(1.0, QPointF::dotProduct(AB,AX) / QPointF::dotProduct(AB,AB)));
		return A + (AB * lambda);
	}
	inline void Update(const QPointF& X, const QPointF& Y, const qreal& sum, const qreal& semi)	// Sum of radii, and semi-diff of radii
	{
		const QPointF	L(Y - X);
		const qreal		l = Length(L);
		const qreal		D = round( std::max(0.0, l - sum) * 1000 ) * 0.001;	// 0.1 mil resolution
		if ( D > m_Dmin ) return;
		if ( D < m_Dmin ) m_pWarn.clear();
		m_Dmin = D;
		QPointF mid( (X + Y) * 0.5 );
		if ( semi != 0 && l != 0 ) mid += L * ( semi / l );
		m_pWarn.push_back( mid );
	}
	static inline qreal Length(const QPointF& p)
	{
		return !p.x() ? fabs(p.y()) : !p.y() ? fabs(p.x()) : sqrt( QPointF::dotProduct(p,p) );
	}
};
