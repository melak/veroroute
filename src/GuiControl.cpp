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

#include "GuiControl.h"
#include "PolygonHelper.h"

void GuiControl::CalcBlob(const qreal& W, const QPointF& pC, const QPointF& pCoffset, const int& iPerimeterCode, std::list<MyPolygonF>& out, const bool bHavePad, const bool bGap) const
{
	// Given a grid point (pC) and its perimeter code, this method populates "out" with a
	// description of the local track pattern at the grid point (or "blob").
	// The scale parameter W represents the width of a 100 mil grid square.

	out.clear();

	const bool	bMaxDiags		= GetDiagsMode() == DIAGSMODE::MAX;
	const qreal	C				= W * 0.5;	// Half square width
	const qreal	padWidth		= 0.01 * ( GetPAD_MIL()	  + 2 * ( bGap ? GetGAP_MIL() : 0 ) );
	const qreal	trkWidth		= 0.01 * ( GetTRACK_MIL() + 2 * ( bGap ? GetGAP_MIL() : 0 ) );
	const GPEN	padPen			= bGap ? GPEN::PAD_GAP : GPEN::PAD;
	const GPEN	trkPen			= bGap ? GPEN::TRK_GAP : GPEN::TRK;
	const bool&	bCurvedTracks	= GetCurvedTracks();
	const bool&	bFatTracks		= !bCurvedTracks && GetFatTracks();
	const bool	bLeg			= ( iPerimeterCode > 0 ) && pCoffset != pC;

	// Clockwise-ordered array of perimeter points around the square, starting at left...
	const QPointF p[8] = { pC+QPointF(-C,0), pC+QPointF(-C,-C), pC+QPointF(0,-C), pC+QPointF( C,-C),
						   pC+QPointF( C,0), pC+QPointF( C, C), pC+QPointF(0, C), pC+QPointF(-C, C) };
	// Clockwise-ordered array of perimeter point usage, starting at left...
	bool bUsed[8];
	for (int iNbr = 0; iNbr < 8; iNbr++) bUsed[iNbr] = ReadCodeBit(iNbr, iPerimeterCode);

	if ( bMaxDiags )	// For "max diagonals mode", force relevant corner perimeter points to be used
	{
		if ( bUsed[NBR_L] && bUsed[NBR_T] ) bUsed[NBR_LT] = true;
		if ( bUsed[NBR_R] && bUsed[NBR_T] ) bUsed[NBR_RT] = true;
		if ( bUsed[NBR_L] && bUsed[NBR_B] ) bUsed[NBR_LB] = true;
		if ( bUsed[NBR_R] && bUsed[NBR_B] ) bUsed[NBR_RB] = true;
	}

	// Construct a track polygon ("blob") based on used perimeter points
	MyPolygonF polygon;

	// Count used perimeter points and find the first
	int iFirst(-1), N(0);	// N ==> number of perimeter points
	for (int i = 0; i < 8; i++) if ( bUsed[i] ) { N++; if ( iFirst == -1 ) iFirst = i; }

	if		( N == 0 )	polygon << pC;				// Done making polygon
	else if ( N == 1 )	polygon << pC << p[iFirst];	// Done making polygon
	else if ( N == 2 )	// Check if second point is consecutive to first point
	{
		if		( bUsed[( 1 + iFirst ) % 8] )	polygon << pC << p[iFirst] << p[( 1 + iFirst ) % 8];	// Done making polygon
		else if	( bUsed[( 7 + iFirst ) % 8] )	polygon << pC << p[iFirst] << p[( 7 + iFirst ) % 8];	// Done making polygon
	}
	const bool bClosed = ( N > 2 ) || ( polygon.size() == 3 );	// true ==> closed polygon

	if ( N > 2 || ( N == 2 && !bClosed ) )	// If not done making polygon ...
	{
		int nCount(0);				// Perimeter point counter
		int iL(iFirst), iR(iFirst);	// Indexes of consecutive used perimeter points
		for (int ii = 1; ii <= 8 && nCount < N; ii++)	// A full clockwise loop around the perimeter back to the start
		{
			if ( !bClosed && ii == 8 ) break;
			const int jj = ( ii + iFirst ) % 8;
			if ( bUsed[jj] ) nCount++; else continue;
			iL = iR;	iR = jj;	// Update iL and iR
			const int  iDiff	= ( 8 + iR - iL ) % 8;
			const bool bOrtho	= ( iDiff == 2 || iDiff == 6 );	// Track section bends 90 degrees
			const bool bObtuse	= ( iDiff == 3 || iDiff == 5 );	// Track section bends < 90 degrees
			if ( bOrtho || bObtuse )	// Bend <= 90 degrees
			{
				if ( bCurvedTracks && !bHavePad )
				{
					// Make an N-point curve from L to R passing near central control point C
					// Current interpolation is quadratic.
					// Using higher order (e.g. 2.5) gives bends passing closer to C (hence sharper corners)
					static int		N = 10;
					static double	d = 1.0 / N;
					const QPointF	pLC(p[iL] - pC), pRC(p[iR] - pC);
					for (int i = 0; i <= N; i++)
					{
						const double t(i * d), u(1 - t);
						polygon << pC + pLC*(u*u) + pRC*(t*t);	// Bezier curve (quadratic interpolation)
					//	polygon << pC + pLC*pow(u,2.5) + pRC*pow(t,2.5);	// Sharper bends
					}
				}
				else if ( bOrtho && !bHavePad )	// Bend == 90 degrees (chosen to approximate the above curve)
				{
					static double r = 0.5;			// i.e. 2*t^2	when t = 0.5
				//	static double r = 0.25*sqrt(2);	// i.e. 2*t^2.5	when t = 0.5
					static double s = 1 - r;
					polygon << p[iL] << p[iL]*r + pC*s << p[iR]*r + pC*s << p[iR];	// Draw mitred corner instead of 90 degree bend for L-C-R
				}
				else
					polygon << p[iL] << pC << p[iR];	// Draw a sharp bend for L-C-R instead of a smooth curve
			}
			else
			{
				if ( iL == iFirst ) polygon << p[iL];	// Add "L" to the polygon if it's the first point
				if ( iR != iFirst ) polygon << p[iR];	// Add "R" to the polygon if it isn't the first point
			}
		}
	}

	// Set other polygon attributes, then copy the polygon to the output polygon list
	const bool bVariTracks = N > 0 && !bClosed && bFatTracks && padWidth > trkWidth;
	polygon.m_eTrkPen	= trkPen;
	polygon.m_ePadPen	= bVariTracks ? padPen : GPEN::NONE;
	polygon.m_radiusTrk	= trkWidth * 0.5;
	polygon.m_radiusPad	= bVariTracks ? ( padWidth * 0.5 ) : 0;
	polygon.m_bClosed	= bClosed;
	out.push_back(polygon);

	if ( bLeg )	// Track leg from offset pad to its grid origin
	{
		polygon.m_eTrkPen	= trkPen;
		polygon.m_ePadPen	= GPEN::NONE;
		polygon.m_radiusTrk	= trkWidth * 0.5;
		polygon.m_radiusPad	= 0;
		polygon.m_bClosed	= false;
		polygon.clear();
		polygon << pC << pCoffset;
		out.push_back(polygon);
	}
	if ( bFatTracks && padWidth > trkWidth )	// Widen H and V tracks to pad width (closed loops not handled by "VariTracks" approach)
	{
		// Create additional polygons for any fat H/V tracks, and copy them to the output polygon list
		polygon.m_eTrkPen	= GPEN::NONE;
		polygon.m_ePadPen	= padPen;
		polygon.m_radiusTrk	= 0;
		polygon.m_radiusPad	= padWidth * 0.5;
		polygon.m_bClosed	= false;
		for (int iNbr = 0; iNbr < 8; iNbr += 2)	// Loop non-diagonal perimeter points
		{
			if ( !bUsed[iNbr] ) continue;

			const int iNbrOpp = Opposite(iNbr);
			if ( bUsed[iNbrOpp] )	// If can go straight across, do so
			{
				if ( iNbr <= 2 )	// No overlay
				{
					polygon.clear();
					polygon << p[iNbr] << p[iNbrOpp];
					out.push_back(polygon);
				}
			}
			else
			{
				polygon.clear();
				polygon << pC << p[iNbr];
				out.push_back(polygon);
			}
		}
	}
}
