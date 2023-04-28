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

void GuiControl::CalcBlob(qreal W, const QPointF& pC, const QPointF& pCoffset,
						  int iPadWidthMIL, int iPerimeterCode, int iTagCode,
						  std::list<MyPolygonF>& out,
						  bool bHavePad, bool bHaveSoic, bool bIsGnd, bool bGap) const
{
	// Given a grid point (pC) and its perimeter code, this method populates "out" with a
	// description of the local track pattern at the grid point (or "blob").
	// The scale parameter W represents the width of a 100 mil grid square.

	out.clear();

	const bool	bMaxDiags		= GetDiagsMode() == DIAGSMODE::MAX;
	const qreal	C				= W * 0.5;	// Half square width
	const qreal	padWidth		= 0.01 * ( GetPAD_MIL()	  + 2 * ( bGap ? GetGAP_MIL() : 0 ) );
	const qreal	trkWidth		= 0.01 * ( GetTRACK_MIL() + 2 * ( bGap ? GetGAP_MIL() : 0 ) );
	const qreal	tagWidth		= 0.01 * ( GetTAG_MIL()	  + 2 * ( bGap ? GetGAP_MIL() : 0 ) );
	const GPEN	padPen			= bGap ? GPEN::PAD_GAP : GPEN::PAD;
	const GPEN	trkPen			= bGap ? GPEN::TRK_GAP : bIsGnd ? GPEN::TAG : GPEN::TRK;
	const bool&	bCurvedTracks	= GetCurvedTracks();
	const bool&	bFatTracks		= !bCurvedTracks && !bIsGnd && GetFatTracks();
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
	bool bClosed = ( polygon.size() == 3 );	// true ==> closed polygon
	if ( !bClosed && N > 2 )
	{
		if ( !bHavePad && !bHaveSoic )
			bClosed = true;
		else
		{
			// If we have a pad, and no connections to consecutive perimeter points,
			// then we have a polygon with zero area (e.g.  L -> C -> T -> B -> C -> L).
			// We must set bClosed to false in this case so we draw a "loop" (using a non-zero width pen)
			for (int i = 0; i < 8 && !bClosed; i++)
				bClosed = bUsed[i] && bUsed[(i+1)%8];	// Consecutive perimeter points used ==> closed
		}
	}

	if ( N > 2 || ( N == 2 && !bClosed ) )	// If not done making polygon ...
	{
		int nCount(0);		// Perimeter point counter
		int iL, iR(iFirst);	// Indexes of consecutive used perimeter points
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
				if ( bCurvedTracks && !bHavePad && !bHaveSoic )
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
				else if ( bOrtho && !bHavePad && !bHaveSoic )	// Bend == 90 degrees (chosen to approximate the above curve)
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

	const bool bGndPad = ( N == 0 && (bHavePad || bHaveSoic) && bIsGnd );
	if ( !bGndPad )	// Don't draw the blob for an isolated pad in the ground-fill
	{
		// Set other polygon attributes, then copy the polygon to the output polygon list
		const bool bVariTracks = N > 0 && !bClosed && bFatTracks && padWidth > trkWidth;
		polygon.m_eTrkPen	= trkPen;
		polygon.m_ePadPen	= bVariTracks ? padPen : GPEN::NONE;
		polygon.m_radiusTrk	= ( bIsGnd ? tagWidth : trkWidth ) * 0.5;
		polygon.m_radiusPad	= bVariTracks ? ( padWidth * 0.5 ) : 0;
		polygon.m_bClosed	= bClosed;
		out.push_back(polygon);
	}

	if ( bLeg )	// Track leg from offset pad to its grid origin
	{
		polygon.m_eTrkPen	= trkPen;
		polygon.m_ePadPen	= GPEN::NONE;
		polygon.m_radiusTrk	= ( bIsGnd ? tagWidth : trkWidth ) * 0.5;
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
	if ( iTagCode > 0 && ( !bLeg || GetXthermals() ) )	// Only draw extra thermal relief tags if we don't have an offset pad, or are forcing X-shaped tags
	{
		assert( bIsGnd );

		polygon.m_eTrkPen	= GPEN::NONE;
		polygon.m_ePadPen	= GPEN::NONE;
		polygon.m_radiusTrk	= 0;
		polygon.m_radiusPad	= 0;
		polygon.m_bClosed	= true;

		const int	i = ( ( iPadWidthMIL == 0 ) ? GetPAD_MIL() : iPadWidthMIL ) + ( GetGAP_MIL() << 1 );
		const qreal X(W * 0.005 * (i + 1));	// Using (i+1) instead of (i) increases tag length by 0.5 mil.  Avoids short tags from rounding errors.
		const qreal T(W * 0.005 * GetTAG_MIL());
		const qreal f(sqrt(0.5));
		const qreal x(f * X), t(f * T);	// Scale for diagonal diretions
		const qreal p(x + t), q(x - t); // Transform for diagonal connections

		for (int iNbr = 0; iNbr < 8; iNbr++)
		{
			if ( !ReadCodeBit(iNbr, iTagCode) ) continue;

			polygon.clear();
			switch( iNbr)
			{
				case NBR_L: 	polygon	<< pCoffset + QPointF(-X,  T) << pCoffset + QPointF(-X, -T) << pCoffset + QPointF( 0, -T) << pCoffset + QPointF( 0,  T);	break;
				case NBR_LT:	polygon	<< pCoffset + QPointF(-p, -q) << pCoffset + QPointF(-q, -p) << pCoffset + QPointF( t, -t) << pCoffset + QPointF(-t,  t);	break;
				case NBR_T:		polygon	<< pCoffset + QPointF(-T,  0) << pCoffset + QPointF(-T, -X) << pCoffset + QPointF( T, -X) << pCoffset + QPointF( T,  0);	break;
				case NBR_RT:	polygon	<< pCoffset + QPointF( q, -p) << pCoffset + QPointF( p, -q) << pCoffset + QPointF( t,  t) << pCoffset + QPointF(-t, -t);	break;
				case NBR_R:		polygon	<< pCoffset + QPointF( X, -T) << pCoffset + QPointF( X,  T) << pCoffset + QPointF( 0,  T) << pCoffset + QPointF( 0, -T);	break;
				case NBR_RB:	polygon	<< pCoffset + QPointF( p,  q) << pCoffset + QPointF( q,  p) << pCoffset + QPointF(-t,  t) << pCoffset + QPointF( t, -t);	break;
				case NBR_B:		polygon	<< pCoffset + QPointF( T,  0) << pCoffset + QPointF( T,  X) << pCoffset + QPointF(-T,  X) << pCoffset + QPointF(-T,  0);	break;
				case NBR_LB:	polygon	<< pCoffset + QPointF(-q,  p) << pCoffset + QPointF(-p,  q) << pCoffset + QPointF(-t, -t) << pCoffset + QPointF( t,  t);	break;
			}
			if ( !polygon.empty() )
				out.push_back(polygon);
		}
	}
}

#ifdef _TEST_SOIC
void Bezier(MyPolygonF& polygon, const QPointF& pL, const QPointF& pC, const QPointF& pR)
{
	// Make an N-point curve from L to R passing near central control point C
	// Current interpolation is quadratic.
	// Using higher order (e.g. 2.5) gives bends passing closer to C (hence sharper corners)
	static int		N = 10;
	static double	d = 1.0 / N;
	const QPointF	pLC(pL - pC), pRC(pR - pC);
	for (int i = 0; i <= N; i++)
	{
		const double t(i * d), u(1 - t);
		polygon << pC + pLC*(u*u) + pRC*(t*t);	// Bezier curve (quadratic interpolation)
	//	polygon << pC + pLC*pow(u,2.5) + pRC*pow(t,2.5);	// Sharper bends
	}
}

void GuiControl::CalcSOIC(qreal W, const QPointF& pC, size_t pinIndex, char direction, std::list<MyPolygonF>& out, bool bSolderMask, bool bIsGnd, bool bGap) const
{
	out.clear();

	// Given a grid point (pC) this method populates "out" with a description of an SOIC track from a "SOIC pin".
	// The scale parameter W represents the width of a 100 mil grid square.

	const qreal	C			= W * 0.5;	// 1/2 square width
	const qreal	Q			= W * 0.25;	// 1/4 square width
	const qreal	padWidth	= 0.01 * ( GetPAD_IC_MIL() + 2 * ( bSolderMask ? GetMASK_MIL() : 0) );
	const qreal	trkWidth	= 0.01 * ( GetTRACK_IC_MIL() + 2 * ( bGap ? GetGAP_MIL() : 0 ) );

	MyPolygonF polygonA;	// SOIC tracks
	polygonA.m_eTrkPen		= bGap ?  GPEN::TRK_IC_GAP : GPEN::TRK_IC;
	polygonA.m_ePadPen		= GPEN::NONE;
	polygonA.m_radiusTrk	= trkWidth * 0.5;
	polygonA.m_radiusPad	= 0;
	polygonA.m_bClosed		= false;
	polygonA.clear();

	MyPolygonF polygonB;	// SOIC pads
	polygonB.m_eTrkPen		= GPEN::NONE;
	polygonB.m_ePadPen		= bGap ? GPEN::NONE : bSolderMask ? GPEN::PAD_IC_MSK : GPEN::PAD_IC;
	polygonB.m_radiusTrk	= 0;
	polygonB.m_radiusPad	= bGap ? 0 : padWidth * 0.5;
	polygonB.m_bClosed		= bGap;
	polygonB.clear();

	// If we're doing the gap then instead of showing a small gap around each SOIC pad strip,
	// blank out a large area across the IC
	if ( bGap )
	{
		assert(!bSolderMask);
		const QPointF p = pC + QPointF(C,C);
		polygonB << p << p+QPointF(W,0) << p+QPointF(W,2.25*W) << p+QPointF(0,2.25*W) << p;
	}

	const bool bNoTrackGap = bIsGnd && bGap;	// For tracks in the ground fill, don't draw a gap around them

	// 7 basic curves.  Start by repeating the curves for pins 21-27
	switch(pinIndex)
	{
		case 27:	case  0:	case 13:	case 14:
			if ( !bSolderMask && !bNoTrackGap )
				polygonA << pC << pC+QPointF(5*Q,0);
			if ( !bGap )
				polygonB << pC+QPointF(5*Q,-Q) << pC+QPointF(5*Q,1.8*Q);
			else 
				polygonB.clear();
			break;
		case 26:	case  1:	case 12:	case 15:
			if ( !bSolderMask && !bNoTrackGap )
				Bezier(polygonA, pC, pC+QPointF(6*Q,0.5*Q), pC+QPointF(7*Q,3*Q));
			if ( !bGap )
				polygonB << pC+QPointF(7*Q,3*Q) << pC+QPointF(7*Q,5.8*Q);
			else 
				polygonB.clear();
			break;
		case 25:	case  2:	case 11:	case 16:
			if ( !bSolderMask && !bNoTrackGap )
			{
				polygonA << pC << pC+QPointF(2.5*Q,2.5*Q);
				Bezier(polygonA, pC+QPointF(2.5*Q,2.5*Q), pC+QPointF(8.1*Q,3.8*Q), pC+QPointF(9*Q,7*Q) );
			}
			if ( !bGap )
				polygonB << pC+QPointF(9*Q,7*Q) << pC+QPointF(9*Q,9.8*Q);
			break;
		case 24:	case  3:	case 10:	case 17:
			if ( !bSolderMask && !bNoTrackGap )
				Bezier(polygonA, pC, pC+QPointF(7*Q,4.5*Q), pC+QPointF(7*Q,7*Q));
			if ( !bGap )
				polygonB << pC+QPointF(7*Q,7*Q) << pC+QPointF(7*Q,9.8*Q);
			break;
		case 23:	case  4:	case  9:	case 18:
			if ( !bSolderMask && !bNoTrackGap )
				Bezier(polygonA, pC, pC+QPointF(5*Q,3.5*Q), pC+QPointF(5*Q,7*Q));
			if ( !bGap )
				polygonB << pC+QPointF(5*Q,7*Q) << pC+QPointF(5*Q,9.8*Q);
			break;
		case 22:	case  5:	case  8:	case 19:
			if ( !bSolderMask && !bNoTrackGap )
				Bezier(polygonA, pC, pC+QPointF(3*Q,2.5*Q), pC+QPointF(3*Q,7*Q));
			if ( !bGap )
				polygonB << pC+QPointF(3*Q,7*Q) << pC+QPointF(3*Q,9.8*Q);
			break;
		case 21:	case  6:	case  7:	case 20:
			if ( !bSolderMask && !bNoTrackGap )
				Bezier(polygonA, pC, pC+QPointF(Q,2*Q), pC+QPointF(Q,7*Q) );
			if ( !bGap )
				polygonB << pC+QPointF(Q,7*Q) << pC+QPointF(Q,9.8*Q);
			break;
	}

	// Then reflect as necessary
	if ( pinIndex < 14 )					{ polygonA.flipV(pC); polygonB.flipV(pC); } 
	if ( pinIndex >= 7 && pinIndex < 21 )	{ polygonA.flipH(pC); polygonB.flipH(pC); }

	// Handle component rotation
	int numRotations(0);
	switch( direction )
	{
		case 'N':	numRotations = 1;	break;
		case 'E':	numRotations = 2;	break;
		case 'S':	numRotations = 3;	break;
	}
	while (numRotations) { polygonA.rotateCW(pC); polygonB.rotateCW(pC); numRotations--; } 

	out.push_back(polygonA);
	out.push_back(polygonB);
}
#endif
