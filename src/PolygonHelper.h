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

// A set of methods for calculating distances between tracks and pads

struct MyPointF : public QPointF		// A point + the pen radius for drawing it
{
	MyPointF(qreal x = 0, qreal y = 0, qreal radius = 0) : QPointF(x,y), m_radius(radius) {}
	~MyPointF()	{}
	MyPointF(const QPointF& p, const qreal& radius) : QPointF(p), m_radius(radius) {}
	MyPointF(const MyPointF& o) : QPointF(o), m_radius(o.m_radius) {}
	MyPointF& operator=(const MyPointF& o)	{ QPointF::operator=(o); m_radius = o.m_radius; return *this; }
	qreal	m_radius	= 0;		// Pen radius
};

struct MyPolygonF : public QPolygonF	// A polygon + the pen radius for drawing it
{
	MyPolygonF() {}
	~MyPolygonF()	{}
	MyPolygonF(const QPolygonF& p, const qreal& radius, bool bClosed) : QPolygonF(p), m_radius(radius), m_bClosed(bClosed) {}
	MyPolygonF(const MyPolygonF& o) : QPolygonF(o), m_radius(o.m_radius), m_bClosed(o.m_bClosed) {}
	MyPolygonF& operator=(const MyPolygonF& o) { QPolygonF::operator=(o); m_radius = o.m_radius; m_bClosed = o.m_bClosed; return *this; }
	qreal	m_radius	= 0;		// Pen radius
	bool	m_bClosed	= false;	// Flag to indicate closed polygon
};

struct PolygonHelper
{
private:
	static QPointF Closest(const QPointF& X, const QPointF& A, const QPointF& B)	// Closest point to X on line segment A-B
	{
		if ( A == B ) return A;
		const QPointF	AB(B-A), AX(X-A);
		const qreal		lambda = std::max(0.0, std::min(1.0, QPointF::dotProduct(AB,AX) / QPointF::dotProduct(AB,AB)));
		return A + (AB * lambda);
	}
	static qreal Distance(const QPointF& A, const QPointF& B)	// Distance from A to B
	{
		const QPointF r(B-A);	return sqrt( QPointF::dotProduct(r,r) );
	}
public:
	static void UpdateClosest(const MyPointF& X, const MyPointF& Y,	// For calculating separation between 2 points.
							  QPolygonF& pWarn, qreal& Dmin)		// Updates Dmin, and the set of warning points pWarn.
	{
		const qreal radii	= X.m_radius + Y.m_radius;
		const qreal w		= Distance(X,Y);
		const qreal D		= std::max(0.0, w - radii);
		if ( D < Dmin ) pWarn.clear();
		if ( D <= Dmin )
		{
			QPointF	XY(Y-X);
			QPointF	Xt	= ( w == 0 ) ? X : ( X + XY*(X.m_radius / w) );
			QPointF	Yt	= ( w == 0 ) ? Y : ( Y - XY*(Y.m_radius / w) );
			pWarn.push_back(0.5*(Xt+Yt));
			Dmin = D;
		}
	}
	static void UpdateClosest(const MyPointF& X, const MyPolygonF& P,	// For calculating separation between a point and a polygon.
							  QPolygonF& pWarn, qreal& Dmin)			// Updates Dmin, and the set of warning points pWarn.
	{
		if ( P.empty() ) return;

		const qreal radii = X.m_radius + P.m_radius;
		const int iSize = P.size();
		if ( iSize == 1 )
		{
			QPointF		Y = P[0];
			const qreal w = Distance(X,Y);
			const qreal D = std::max(0.0, w - radii);
			if ( D < Dmin ) pWarn.clear();
			if ( D <= Dmin )
			{
				QPointF	XY(Y-X);
				QPointF	Xt	= ( w == 0 ) ? X : ( X + XY*(X.m_radius / w) );
				QPointF	Yt	= ( w == 0 ) ? Y : ( Y - XY*(P.m_radius / w) );
				pWarn.push_back(0.5*(Xt+Yt));
				Dmin = D;
			}
			return;
		}
		for (int i = 0, j = 1, iEnd = P.m_bClosed ? iSize : (iSize-1); i < iEnd; i++, j++)
		{
			if ( j == iSize ) j = 0;
			QPointF		Y = Closest(X, P[i], P[j]);	// Get closest point on line segment P[i]-P[j]
			const qreal w = Distance(X,Y);
			const qreal D = std::max(0.0, w - radii);
			if ( D < Dmin ) pWarn.clear();
			if ( D <= Dmin )
			{
				QPointF	XY(Y-X);
				QPointF	Xt	= ( w == 0 ) ? X : ( X + XY*(X.m_radius / w) );
				QPointF	Yt	= ( w == 0 ) ? Y : ( Y - XY*(P.m_radius / w) );
				pWarn.push_back(0.5*(Xt+Yt));
				Dmin = D;
			}
		}
	}
	static void UpdateClosest(const MyPolygonF& P, const MyPolygonF& Q,	// For calculating separation between 2 polygons.
							  QPolygonF& pWarn, qreal& Dmin)			// Updates Dmin, and the set of warning points pWarn.
	{
		if ( P.empty() || Q.empty() ) return;
		for (auto& q : Q) UpdateClosest(MyPointF(q, Q.m_radius), P, pWarn, Dmin);
		for (auto& p : P) UpdateClosest(MyPointF(p, P.m_radius), Q, pWarn, Dmin);
	}
	PolygonHelper() { assert( true || PreventBuildWarnings() ); }
private:
	bool PreventBuildWarnings() const
	{
		MyPolygonF	P;
		QPolygonF	W;
		qreal		dummy;
		UpdateClosest(P, P, W, dummy);
		return true;
	}
};
