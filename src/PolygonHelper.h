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

struct MyPolygonF : public QPolygonF	// A polygon + the pen radius for drawing it
{
	MyPolygonF() {}
	~MyPolygonF() {}
	MyPolygonF(const QPolygonF& p, const qreal& radius, bool bClosed) : QPolygonF(p), m_radius(radius), m_bClosed(bClosed) {}
	MyPolygonF(const MyPolygonF& o) : QPolygonF(o), m_radius(o.m_radius), m_bClosed(o.m_bClosed) {}
	MyPolygonF& operator=(const MyPolygonF& o) { QPolygonF::operator=(o); m_radius = o.m_radius; m_bClosed = o.m_bClosed; return *this; }
	qreal	m_radius	= 0;		// Pen radius
	bool	m_bClosed	= false;	// Flag to indicate closed polygon
};

struct PolygonHelper
{
	PolygonHelper()		{ m_pWarn.clear(); }
	~PolygonHelper()	{ m_pWarn.clear(); }
	QPolygonF	m_pWarn;			// Set of warning points
	qreal		m_Dmin = DBL_MAX;	// The closest separation found

	inline void CalcSeparation(const MyPointF& X, const MyPointF& Y)
	{
		const qreal radii	= X.m_radius + Y.m_radius;
		const qreal semi	= 0.5 * ( X.m_radius - Y.m_radius );
		Update(X, Y, radii, semi);
	}
	inline void CalcSeparation(const MyPointF& X, const MyPolygonF& P)
	{
		if ( P.empty() ) return;
		const qreal radii	= X.m_radius + P.m_radius;
		const qreal semi	= 0.5 * ( X.m_radius - P.m_radius );

		const int iSize = P.size();
		if ( iSize == 1 ) return Update(X, P[0], radii, semi);

		for (int i = 0, j = 1, iEnd = P.m_bClosed ? iSize : (iSize-1); i < iEnd; i++, j++)
		{
			if ( j == iSize ) j = 0;
			Update(X, Closest(X, P[i], P[j]), radii, semi);
		}
	}
	inline void CalcSeparation(const MyPolygonF& P, const MyPolygonF& Q)
	{
		if ( P.empty() || Q.empty() ) return;
		for (auto& q : Q) CalcSeparation(MyPointF(q, Q.m_radius), P);
		for (auto& p : P) CalcSeparation(MyPointF(p, P.m_radius), Q);
	}
private:
	static inline QPointF Closest(const QPointF& X, const QPointF& A, const QPointF& B)	// Closest point to X on line segment A-B
	{
		if ( A == B ) return A;
		const QPointF	AB(B - A), AX(X - A);
		const qreal		lambda = std::max(0.0, std::min(1.0, QPointF::dotProduct(AB,AX) / QPointF::dotProduct(AB,AB)));
		return A + (AB * lambda);
	}
	inline void Update(const QPointF& X, const QPointF& Y, const qreal& radii, const qreal& semi)	// Sum of radii, and semi-diff of radii
	{
		const QPointF	L(Y - X);
		const qreal		l = Length(L);
		const qreal		D = round( std::max(0.0, l - radii) * 1000 ) * 0.001;	// 0.1 mil resolution
		if ( D > m_Dmin ) return;
		if ( D < m_Dmin ) m_pWarn.clear();
		m_Dmin = D;
		QPointF mid( (X + Y) * 0.5 );
		if ( semi != 0 && l != 0 ) mid += L * ( semi / l );
		m_pWarn.push_back( mid );
	}
	Q_DECL_CONSTEXPR static inline qreal Length(const QPointF& p)
	{
		return !p.x() ? fabs(p.y()) : !p.y() ? fabs(p.x()) : sqrt( QPointF::dotProduct(p,p) );
	}
};
