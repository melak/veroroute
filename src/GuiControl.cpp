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
#include "Component.h"

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
					Q_DECL_CONSTEXPR static const int		N = 10;
					Q_DECL_CONSTEXPR static const double	d = 1.0 / N;
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

void Bezier(MyPolygonF& polygon, const QPointF& pL, const QPointF& pC, const QPointF& pR)
{
	// Make an N-point curve from L to R passing near central control point C
	// Current interpolation is quadratic.
	// Using higher order (e.g. 2.5) gives bends passing closer to C (hence sharper corners)
	Q_DECL_CONSTEXPR static const int		N = 10;
	Q_DECL_CONSTEXPR static const double	d = 1.0 / N;
	const QPointF	pLC(pL - pC), pRC(pR - pC);
	for (int i = 0; i <= N; i++)
	{
		const double t(i * d), u(1 - t);
		polygon << pC + pLC*(u*u) + pRC*(t*t);	// Bezier curve (quadratic interpolation)
	//	polygon << pC + pLC*pow(u,2.5) + pRC*pow(t,2.5);	// Sharper bends
	}
}

void GuiControl::CalcSOIC(qreal W, const QPointF& pC, size_t pinIndex, const Component* pComp, std::list<MyPolygonF>& out, bool bSolderMask, bool bIsGnd, bool bGap) const
{
	assert(pComp);
	const size_t	numPins		= pComp->GetNumPins();
	const char		direction	= pComp->GetDirection();

	assert( !bGap || !bSolderMask );
	assert(numPins == 8 || numPins == 14 || numPins == 16 || numPins == 20 || numPins == 24 || numPins == 28);
	out.clear();

	// Given a grid point (pC) this method populates "out" with a description of an SOIC track from a "SOIC pin".
	// The scale parameter W represents the width of a 100 mil grid square.

	const qreal	T			= W * 0.025;	// (1/40 square width)  i.e. T = 2.5 mil
	const qreal	padWidth	= W * 0.01 * GetPAD_IC_MIL();
	const qreal	padHeight	= W * 0.01 * 90;	// Pad length = 90 mil
	const qreal	maskDelta	= W * 0.01 * GetMASK_MIL();
	const bool	bThinTrack	= ( numPins == 16 ) && ( pinIndex == 1 || pinIndex == 6 || pinIndex == 9 || pinIndex == 14 );
	const qreal	trkWidth	= 0.01 * ( ( bThinTrack ? GetMIN_IC_MIL() : GetTRACK_IC_MIL() )+ 2 * ( bGap ? GetGAP_MIL() : 0 ) );

	MyPolygonF polygonA;	// SOIC tracks
	if ( bThinTrack )
		polygonA.m_eTrkPen	= bGap ?  GPEN::MIN_IC_GAP : GPEN::MIN_IC;
	else
		polygonA.m_eTrkPen	= bGap ?  GPEN::TRK_IC_GAP : GPEN::TRK_IC;
	polygonA.m_radiusTrk	= trkWidth * 0.5;
	polygonA.clear();

	MyPolygonF polygonB;	// SOIC pads are drawn as a filled closed rectangle using GPEN::NONE
	polygonB.m_bClosed		= true;
	polygonB.clear();

	const bool bNoTrackGap	= bIsGnd && bGap;	// For tracks in the ground fill, don't draw a gap around them
	const bool bDoTrack		= !bSolderMask && !bNoTrackGap;
	const bool bDoPad		= !bSolderMask && !bGap;
	const bool bDoMask		=  bSolderMask && !bGap;

	const QPointF padLength(0, padHeight);
	const QPointF padLR(padWidth * 0.5, 0);
	const QPointF padLeg(0,4*T);
	const QPointF padGapLR(15*T,0), padGapTB(0,12*T);	// Gaps for the SOIC pads
	const QPointF padMaskLR(padWidth * 0.5 + maskDelta, 0), padMaskTB(0, maskDelta);
	QPointF padTop, padBot;	// Limits of SOIC pads

	// 28 pin =========================================================================
	if ( numPins == 28 )
	{
		// 7 basic curves, referenced by pins 21-27
		const int iRefPin = ( pinIndex <  7 ) ? 27 - pinIndex :
							( pinIndex < 14 ) ? 14 + pinIndex :
							( pinIndex < 21 ) ? 41 - pinIndex : pinIndex;
		assert(iRefPin >= 21 && iRefPin <= 27);

		switch(iRefPin)
		{
			case 27: padTop = pC+QPointF(50*T,-12*T); break;
			case 26: padTop = pC+QPointF(70*T,28*T); break;
			case 25: padTop = pC+QPointF(90*T,68*T); break;
			case 24: padTop = pC+QPointF(70*T,68*T); break;
			case 23: padTop = pC+QPointF(50*T,68*T); break;
			case 22: padTop = pC+QPointF(30*T,68*T); break;
			case 21: padTop = pC+QPointF(10*T,68*T); break;
		}
		padBot = padTop + padLength;

		if ( bDoTrack )
		{
			switch(iRefPin)
			{
				case 27: polygonA << pC << pC+QPointF(50*T,0); break;
				case 26: Bezier(polygonA, pC, pC+QPointF(60*T,3*T),	padTop); break;
				case 25: polygonA << pC << pC+QPointF(25*T,25*T);
						 Bezier(polygonA, pC+QPointF(25*T,25*T), pC+QPointF(81*T,38*T), padTop); break;
				case 24: polygonA << pC << pC+QPointF(10*T,10*T);
						 Bezier(polygonA, pC+QPointF(10*T,10*T), pC+QPointF(70*T,45*T), padTop); break;
				case 23: Bezier(polygonA, pC, pC+QPointF(50*T,35*T), padTop); break;
				case 22: Bezier(polygonA, pC, pC+QPointF(30*T,25*T), padTop); break;
				case 21: Bezier(polygonA, pC, pC+QPointF(10*T,20*T), padTop); break;
			}
		}
		if ( bDoPad )
			polygonB << padTop-padLR << padTop+padLR << padBot+padLR << padBot-padLR << padTop-padLR;
		if ( bDoMask )
		{
			padTop -= padMaskTB;	padBot += padMaskTB;
			polygonB << padTop-padMaskLR << padTop+padMaskLR << padBot+padMaskLR << padBot-padMaskLR << padTop-padMaskLR;
		}
		if ( bGap )
		{
			padTop -= padGapTB;	padBot += padGapTB;
			polygonB << padTop-padGapLR << padTop+padGapLR << padBot+padGapLR << padBot-padGapLR << padTop-padGapLR;
		}

		// Reflect the polygon based on the reference pin as necessary
		if ( pinIndex < 14 )					{ polygonA.flipV(pC); polygonB.flipV(pC); }
		if ( pinIndex >= 7 && pinIndex < 21 )	{ polygonA.flipH(pC); polygonB.flipH(pC); }
	}
	// 28 pin =========================================================================

	// 24 pin =========================================================================
	if ( numPins == 24 )
	{
		// 6 basic curves, referenced by pins 18-23
		const int iRefPin = ( pinIndex <  6 ) ? 23 - pinIndex :
							( pinIndex < 12 ) ? 12 + pinIndex :
							( pinIndex < 18 ) ? 35 - pinIndex : pinIndex;
		assert(iRefPin >= 18 && iRefPin <= 23);

		switch(iRefPin)
		{
			case 23: padTop = pC+QPointF(30*T,-12*T); break;
			case 22: padTop = pC+QPointF(50*T,28*T); break;
			case 21: padTop = pC+QPointF(70*T,68*T); break;
			case 20: padTop = pC+QPointF(50*T,68*T); break;
			case 19: padTop = pC+QPointF(30*T,68*T); break;
			case 18: padTop = pC+QPointF(10*T,68*T); break;
		}
		padBot = padTop + padLength;

		if ( bDoTrack )
		{
			switch(iRefPin)
			{
				case 23: polygonA << pC << pC+QPointF(30*T,0); break;
				case 22: Bezier(polygonA, pC, pC+QPointF(40*T, 5*T), padTop); break;
				case 21: polygonA << pC << pC+QPointF(25*T,25*T);
						 Bezier(polygonA, pC+QPointF(25*T,25*T), pC+QPointF(70*T,45*T), padTop); break;
				case 20: Bezier(polygonA, pC, pC+QPointF(50*T,35*T), padTop); break;
				case 19: Bezier(polygonA, pC, pC+QPointF(30*T,25*T), padTop); break;
				case 18: Bezier(polygonA, pC, pC+QPointF(10*T,20*T), padTop); break;
			}
		}
		if ( bDoPad )
			polygonB << padTop-padLR << padTop+padLR << padBot+padLR << padBot-padLR << padTop-padLR;
		if ( bDoMask )
		{
			padTop -= padMaskTB;	padBot += padMaskTB;
			polygonB << padTop-padMaskLR << padTop+padMaskLR << padBot+padMaskLR << padBot-padMaskLR << padTop-padMaskLR;
		}
		if ( bGap )
		{
			padTop -= padGapTB;	padBot += padGapTB;
			polygonB << padTop-padGapLR << padTop+padGapLR << padBot+padGapLR << padBot-padGapLR << padTop-padGapLR;
		}

		// Reflect the polygon based on the reference pin as necessary
		if ( pinIndex < 12 )					{ polygonA.flipV(pC); polygonB.flipV(pC); }
		if ( pinIndex >= 6 && pinIndex < 18 )	{ polygonA.flipH(pC); polygonB.flipH(pC); }
	}
	// 24 pin =========================================================================

	// 20 pin =========================================================================
	if ( numPins == 20 )
	{
		// 5 basic curves, referenced by pins 15-19
		const int iRefPin = ( pinIndex <  5 ) ? 19 - pinIndex :
							( pinIndex < 10 ) ? 10 + pinIndex :
							( pinIndex < 15 ) ? 29 - pinIndex : pinIndex;
		assert(iRefPin >= 15 && iRefPin <= 19);

		switch(iRefPin)
		{
			case 19: padTop = pC+QPointF(50*T,28*T); break;
			case 18: padTop = pC+QPointF(70*T,68*T); break;
			case 17: padTop = pC+QPointF(50*T,68*T); break;
			case 16: padTop = pC+QPointF(30*T,68*T); break;
			case 15: padTop = pC+QPointF(10*T,68*T); break;
		}
		padBot = padTop + padLength;

		if ( bDoTrack )
		{
			switch(iRefPin)
			{
				case 19: Bezier(polygonA, pC, pC+QPointF(40*T, 5*T), padTop); break;
				case 18: Bezier(polygonA, pC, pC+QPointF(70*T,45*T), padTop); break;
				case 17: Bezier(polygonA, pC, pC+QPointF(50*T,35*T), padTop); break;
				case 16: Bezier(polygonA, pC, pC+QPointF(30*T,25*T), padTop); break;
				case 15: Bezier(polygonA, pC, pC+QPointF(10*T,20*T), padTop); break;
			}
		}
		if ( bDoPad )
			polygonB << padTop-padLR << padTop+padLR << padBot+padLR << padBot-padLR << padTop-padLR;
		if ( bDoMask )
		{
			padTop -= padMaskTB;	padBot += padMaskTB;
			polygonB << padTop-padMaskLR << padTop+padMaskLR << padBot+padMaskLR << padBot-padMaskLR << padTop-padMaskLR;
		}
		if ( bGap )
		{
			padTop -= padGapTB;	padBot += padGapTB;
			polygonB << padTop-padGapLR << padTop+padGapLR << padBot+padGapLR << padBot-padGapLR << padTop-padGapLR;
		}

		// Reflect the polygon based on the reference pin as necessary
		if ( pinIndex < 10 )					{ polygonA.flipV(pC); polygonB.flipV(pC); }
		if ( pinIndex >= 5 && pinIndex < 15 )	{ polygonA.flipH(pC); polygonB.flipH(pC); }
	}
	// 20 pin =========================================================================

	// 16 pin =========================================================================
	if ( numPins == 16 )
	{
		// 4 basic curves, referenced by pins 12-15
		const int iRefPin = ( pinIndex <  4 ) ? 15 - pinIndex :
							( pinIndex <  8 ) ?  8 + pinIndex :
							( pinIndex < 12 ) ? 23 - pinIndex : pinIndex;
		assert(iRefPin >= 12 && iRefPin <= 15);

		switch(iRefPin)
		{
			case 15: padTop = pC+QPointF(30*T,-12*T); break;
			case 14: padTop = pC+QPointF(50*T,28*T); break;
			case 13: padTop = pC+QPointF(30*T,28*T); break;
			case 12: padTop = pC+QPointF(10*T,28*T); break;
		}
		padBot = padTop + padLength;

		if ( bDoTrack )
		{
			switch(iRefPin)
			{
				case 15: polygonA << pC << pC+QPointF(30*T,0); break;
				case 14: Bezier(polygonA, pC, pC+QPointF(0,11*T), padTop-padLeg);	polygonA << padTop; break;
				case 13: polygonA << pC << padTop-padLeg << padTop; break;
				case 12: polygonA << pC << padTop-padLeg << padTop; break;
			}
		}
		if ( bDoPad )
			polygonB << padTop-padLR << padTop+padLR << padBot+padLR << padBot-padLR << padTop-padLR;
		if ( bDoMask )
		{
			padTop -= padMaskTB;	padBot += padMaskTB;
			polygonB << padTop-padMaskLR << padTop+padMaskLR << padBot+padMaskLR << padBot-padMaskLR << padTop-padMaskLR;
		}
		if ( bGap )
		{
			padTop -= padGapTB;	padBot += padGapTB;
			polygonB << padTop-padGapLR << padTop+padGapLR << padBot+padGapLR << padBot-padGapLR << padTop-padGapLR;
		}

		// Reflect the polygon based on the reference pin as necessary
		if ( pinIndex < 8 )						{ polygonA.flipV(pC); polygonB.flipV(pC); }
		if ( pinIndex >= 4 && pinIndex < 12 )	{ polygonA.flipH(pC); polygonB.flipH(pC); }
	}
	// 16 pin =========================================================================

	// 14 pin =========================================================================
	if ( numPins == 14 )
	{
		// 4 basic curves, referenced by pins 10-13
		const int iRefPin = ( pinIndex <  4 ) ? 13 - pinIndex :
							( pinIndex <  7 ) ?  7 + pinIndex :
							( pinIndex < 10 ) ? 20 - pinIndex : pinIndex;
		assert(iRefPin >= 10 && iRefPin <= 13);

		switch(iRefPin)
		{
			case 13: padTop = pC+QPointF(20*T,-12*T); break;
			case 12: padTop = pC+QPointF(40*T,28*T); break;
			case 11: padTop = pC+QPointF(20*T,28*T); break;
			case 10: padTop = pC+QPointF(   0,28*T); break;
		}
		padBot = padTop + padLength;

		if ( bDoTrack )
		{
			switch(iRefPin)
			{
				case 13: polygonA << pC << pC+QPointF(20*T,0); break;
				case 12: Bezier(polygonA, pC, pC+QPointF(0,7*T), padTop-padLeg);	polygonA << padTop; break;
				case 11: polygonA << pC << padTop-padLeg << padTop; break;
				case 10: polygonA << pC << padTop; break;
			}
		}
		if ( bDoPad )
			polygonB << padTop-padLR << padTop+padLR << padBot+padLR << padBot-padLR << padTop-padLR;
		if ( bDoMask )
		{
			padTop -= padMaskTB;	padBot += padMaskTB;
			polygonB << padTop-padMaskLR << padTop+padMaskLR << padBot+padMaskLR << padBot-padMaskLR << padTop-padMaskLR;
		}
		if ( bGap )
		{
			padTop -= padGapTB;	padBot += padGapTB;
			polygonB << padTop-padGapLR << padTop+padGapLR << padBot+padGapLR << padBot-padGapLR << padTop-padGapLR;
		}

		// Reflect the polygon based on the reference pin as necessary
		if ( pinIndex < 7 )						{ polygonA.flipV(pC); polygonB.flipV(pC); }
		if ( pinIndex >= 4 && pinIndex < 10 )	{ polygonA.flipH(pC); polygonB.flipH(pC); }
	}
	// 14 pin =========================================================================

	// 8 pin ==========================================================================
	if ( numPins == 8 )
	{
		// 2 basic curves, referenced by pins 6-7
		const int iRefPin = ( pinIndex < 2 ) ?  7 - pinIndex :
							( pinIndex < 4 ) ?  4 + pinIndex :
							( pinIndex < 6 ) ? 11 - pinIndex : pinIndex;
		assert(iRefPin >= 6 && iRefPin <= 7);

		switch(iRefPin)
		{
			case 7: padTop = pC+QPointF(30*T,28*T); break;
			case 6: padTop = pC+QPointF(10*T,28*T); break;
		}
		padBot = padTop + padLength;

		if ( bDoTrack )
			polygonA << pC << padTop-padLeg << padTop; 
		if ( bDoPad )
			polygonB << padTop-padLR << padTop+padLR << padBot+padLR << padBot-padLR << padTop-padLR;
		if ( bDoMask )
		{
			padTop -= padMaskTB;	padBot += padMaskTB;
			polygonB << padTop-padMaskLR << padTop+padMaskLR << padBot+padMaskLR << padBot-padMaskLR << padTop-padMaskLR;
		}
		if ( bGap )
		{
			padTop -= padGapTB;	padBot += padGapTB;
			polygonB << padTop-padGapLR << padTop+padGapLR << padBot+padGapLR << padBot-padGapLR << padTop-padGapLR;
		}

		// Reflect the polygon based on the reference pin as necessary
		if ( pinIndex < 4 )						{ polygonA.flipV(pC); polygonB.flipV(pC); }
		if ( pinIndex >= 2 && pinIndex < 6 )	{ polygonA.flipH(pC); polygonB.flipH(pC); }
	}
	// 8 pin ==========================================================================

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
