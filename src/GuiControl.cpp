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
	//TODO Not entirely sure of the need to pass two flags for pads (bHavePad and bHaveSoic)

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
		if ( !bHavePad )
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

	const bool bGndPad = ( N == 0 && bHavePad && bIsGnd );
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

void GuiControl::CalcSOIC(qreal W, const QPointF& pLT, /*size_t pinIndex,*/ std::list<MyPolygonF>& out, bool bGap) const
{
	// Really need to give two bits of info here.  A pin index and pattern type (maybe some COMP type for SOICs).
	// The pattern info should all be in the component.

	// Given a grid point (pLT) this method populates "out" with a description of a/ SOIC track pattern.
	// The scale parameter W represents the width of a 100 mil grid square.

	// We have Gerber pen widths tied to the polygons but nodeId info.
	// So that either needs to change, or we have to tag each polygon track/pad with a pin number
	// so the rendering code in Color mode can choose colours for eaxh track/pad

	out.clear();

	const qreal	Q			= W * 0.25;	// 1/4 square width
	const qreal	padWidth	= 0.01 * GetPAD_IC_MIL();
	const qreal	trkWidth	= 0.01 * ( GetTRACK_IC_MIL() + 2 * ( bGap ? GetGAP_MIL() : 0 ) );

	const QPointF pC = pLT + QPointF(4.5*W,4*W);	// Centre of the shape

	MyPolygonF polygon;

	if ( bGap )
	{
		polygon.m_eTrkPen	= GPEN::NONE;
		polygon.m_ePadPen	= GPEN::NONE;
		polygon.m_radiusTrk	= 0;
		polygon.m_radiusPad	= 0;
		polygon.m_bClosed	= true;
		polygon << pC + QPointF(-4.5*W, -4*W) 
				<< pC + QPointF( 4.5*W, -4*W)
				<< pC + QPointF( 4.5*W,  4*W)
				<< pC + QPointF(-4.5*W,  4*W)
				<< pC + QPointF(-4.5*W, -4*W);
		out.push_back(polygon);
	}

	// Split description into two parts.  One starting at pin and ending at pad.  Other for the pad
	// Units of Q should suffice. If W = 100 then Q is 25 mil.
	// So just set Q = 25 in values below to get mil
	
	

 	//  Coords w.r.t. pin x-> y down
	// L ==> line   B==> Bezier
	/*
	enum class LINE_TYPE { LINEAR , BEZIER };
	
	struct Track
	{
		Track(const std::vector<qreal>& A, const std::vector<qreal>& B)
		{
			pointsA.resize(3);
			for (size_t i = 0, k = 0; k < 3; k++, i += 2)
				pointsA[k] = QPointF(A[i], A[i+1]);

			pointsB.resize(3);
			for (size_t i = 0, k = 0; k < 2; k++, i += 2)
				pointsB[k] = QPointF(B[i], B[i+1]);
		}
		Track& operator=(const Track& o)
		{
			lineTypeA = o.lineTypeA;
			pointsA.resize(o.pointsA.size());
			pointsB.resize(o.pointsB.size());
			std::copy(o.pointsA.begin(), o.pointsA.end(), pointsA.begin());
			std::copy(o.pointsB.begin(), o.pointsB.end(), pointsB.begin());
			return *this;
		}
		void flipV()
		{
			for (auto& o : pointsA) o.setY( -o.y() );
			for (auto& o : pointsB) o.setY( -o.y() );
		}
		void flipH()
		{
			for (auto& o : pointsA) o.setX( -o.x() );
			for (auto& o : pointsB) o.setX( -o.x() );
		}
		void AddPolygonA(MyPolygonF& polygon, std::list<MyPolygonF>& out)
		{
			polygon.clear();
			if ( lineTypeA == LINE_TYPE::LINEAR )
				polygon << pointsA[0] << pointsA[1] <<  pointsA[2];
			else
				Bezier(polygon, pointsA[0], pointsA[1], pointsA[2]);
			out.push_back(polygon);	
		}
		void AddPolygonB(MyPolygonF& polygon, std::list<MyPolygonF>& out)
		{
			polygon.clear();
			polygon << pointsB[0] << pointsB[1];
			out.push_back(polygon);	
		}
		// Data
		LINE_TYPE	lineTypeA = LINE_TYPE::BEZIER;	// LineTypeB is always linear
		std::vector<QPointF> pointsA;				// For track starting at pin
		std::vector<QPointF> pointsB;				// For pad at end of track
	};

	std::vector<Track> track; track.resize(28);	// indexed by pinIndex

	track[27]	= Track( {0,0,100,0,125,0},			{125,-25,125,150} );	track[27].lineTypeA = LINE_TYPE::LINEAR;
	track[0]	= track[27];	track[0].flipV();
	track[13]	= track[0];		track[12].flipH();
	track[14]	= track[13];	track[14].flipV();
	track[26]	= Track( {0,0,150,12.5,175,75},		{0,75,175,250} );
	track[1]	= track[26];	track[1].flipV();
	track[12]	= track[1];		track[12].flipH();
	track[15]	= track[12];	track[15].flipV();
	track[25]	= Track( {0,0,225,135,225,175},		{225,175,225,350} );
	track[2]	= track[25];	track[2].flipV();
	track[11]	= track[2];		track[11].flipH();
	track[16]	= track[11];	track[16].flipV();
	track[24]	= Track( {0,0,175,112.5,175,175},	{175,175,175,350} );
	track[3]	= track[24];	track[3].flipV();
	track[10]	= track[3];		track[10].flipH();
	track[17]	= track[10];	track[17].flipV();
	track[23]	= Track( {0,0,125,87.5,125,175},	{125,175,125,350} );
	track[4]	= track[23];	track[3].flipV();
	track[9]	= track[4];		track[10].flipH();
	track[18]	= track[9];		track[17].flipV();
	track[22]	= Track( {0,0,75,62.5,75,175},		{75,175,75,350} );
	track[5]	= track[22];	track[5].flipV();
	track[8]	= track[5];		track[8].flipH();
	track[19]	= track[8];		track[19].flipV();
	track[21]	= Track( {0,0,25,50,25,175},		{25,175,25,350} );
	track[6]	= track[21];	track[6].flipV();
	track[7]	= track[6];		track[7].flipH();
	track[20]	= track[7];		track[20].flipV();

	
	// SOIC tracks -------------------------------------------------------------------------
	polygon.m_eTrkPen	= bGap ?  GPEN::TRK_IC_GAP : GPEN::TRK_IC;
	polygon.m_ePadPen	= GPEN::NONE;
	polygon.m_radiusTrk	= trkWidth * 0.5;
	polygon.m_radiusPad	= 0;
	polygon.m_bClosed	= false;
	track[iPinIndex].AddPolygonA(polygon, out);

	// SOIC Pads ----------------------------------------------------------------------------
	if ( !bGap )
	{
		polygon.m_eTrkPen	= GPEN::NONE;
		polygon.m_ePadPen	= GPEN::PAD_IC;
		polygon.m_radiusTrk	= 0;
		polygon.m_radiusPad	= padWidth * 0.5;
		polygon.m_bClosed	= false;
		track[iPinIndex].AddPolygonB(polygon, out);
	}
	*/
	
	// SOIC Pads ----------------------------------------------------------------------------
	if ( !bGap )
	{
		polygon.m_eTrkPen	= GPEN::NONE;
		polygon.m_ePadPen	= GPEN::PAD_IC;
		polygon.m_radiusTrk	= 0;
		polygon.m_radiusPad	= padWidth * 0.5;
		polygon.m_bClosed	= false;
		
		for (int iPinIndex = 0; iPinIndex < 28; iPinIndex++)
		{
			polygon.m_pinIndex = iPinIndex;
			polygon.clear();
		
			const bool bLeft = iPinIndex < 14;
			const qreal x    = Q * ( - 13 + 2 * ( bLeft ? iPinIndex : (27-iPinIndex) ) );
			const qreal yLo	 = Q * ( bLeft ? 2 : -9 );
			const qreal yHi	 = Q * ( bLeft ? 9 : -2 );
			polygon.clear();
			
			polygon << pC + QPointF(x, yLo) << pC  + QPointF(x , yHi);	out.push_back(polygon);
		}
	}

	// SOIC tracks -------------------------------------------------------------------------
	polygon.m_eTrkPen	= bGap ?  GPEN::TRK_IC_GAP : GPEN::TRK_IC;
	polygon.m_ePadPen	= GPEN::NONE;
	polygon.m_radiusTrk	= trkWidth * 0.5;
	polygon.m_radiusPad	= 0;
	polygon.m_bClosed	= false;

	polygon.clear();
	polygon.m_pinIndex = 27;	polygon << pC+QPointF(-18*Q,-8*Q) << pC+QPointF(-13*Q,-8*Q);out.push_back(polygon);
	polygon.m_pinIndex =  0;	polygon.flipV();	polygon.translate( QPointF(0,4*W) );	out.push_back(polygon);
	polygon.m_pinIndex = 13;	polygon.flipH();	polygon.translate( QPointF(9*W,0) );	out.push_back(polygon);
	polygon.m_pinIndex = 14;	polygon.flipV();	polygon.translate( QPointF(0,-4*W) );	out.push_back(polygon);

	polygon.clear();
	polygon.m_pinIndex = 26;	Bezier(polygon, pC+QPointF(-18*Q,-12*Q), pC+QPointF(-12*Q,-11.5*Q), pC+QPointF(-11*Q,-9*Q));	out.push_back(polygon);
	polygon.m_pinIndex =  1;	polygon.flipV();	polygon.translate( QPointF(0,6*W) );	out.push_back(polygon);
	polygon.m_pinIndex = 12;	polygon.flipH();	polygon.translate( QPointF(9*W,0) );	out.push_back(polygon);
	polygon.m_pinIndex = 15;	polygon.flipV();	polygon.translate( QPointF(0,-6*W) );	out.push_back(polygon);

	polygon.clear();
	polygon.m_pinIndex = 25;	Bezier(polygon, pC+QPointF(-18*Q,-16*Q), pC+QPointF(-9*Q,-10.6*Q), pC+QPointF(-9*Q,-9*Q) );		out.push_back(polygon);
	polygon.m_pinIndex =  2;	polygon.flipV();	polygon.translate( QPointF(0,8*W) );	out.push_back(polygon);
	polygon.m_pinIndex = 11;	polygon.flipH();	polygon.translate( QPointF(9*W,0) );	out.push_back(polygon);
	polygon.m_pinIndex = 16;	polygon.flipV();	polygon.translate( QPointF(0,-8*W) );	out.push_back(polygon);

	polygon.clear();
	polygon.m_pinIndex = 24;	Bezier(polygon, pC+QPointF(-14*Q,-16*Q), pC+QPointF(-7*Q,-11.5*Q), pC+QPointF(-7*Q,-9*Q));		out.push_back(polygon);
	polygon.m_pinIndex =  3;	polygon.flipV();	polygon.translate( QPointF(0,8*W) );	out.push_back(polygon);
	polygon.m_pinIndex = 10;	polygon.flipH();	polygon.translate( QPointF(7*W,0) );	out.push_back(polygon);
	polygon.m_pinIndex = 17;	polygon.flipV();	polygon.translate( QPointF(0,-8*W) );	out.push_back(polygon);

	polygon.clear();
	polygon.m_pinIndex = 23;	Bezier(polygon, pC+QPointF(-10*Q,-16*Q), pC+QPointF(-5*Q,-12.5*Q), pC+QPointF(-5*Q,-9*Q));	out.push_back(polygon);
	polygon.m_pinIndex =  4;	polygon.flipV();	polygon.translate( QPointF(0,8*W) );	out.push_back(polygon);
	polygon.m_pinIndex =  9;	polygon.flipH();	polygon.translate( QPointF(5*W,0) );	out.push_back(polygon);
	polygon.m_pinIndex = 18;	polygon.flipV();	polygon.translate( QPointF(0,-8*W) );	out.push_back(polygon);

	polygon.clear(); 
	polygon.m_pinIndex = 22;	Bezier(polygon, pC+QPointF(-6*Q,-16*Q), pC+QPointF(-3*Q,-13.5*Q), pC+QPointF(-3*Q,-9*Q));	out.push_back(polygon);
	polygon.m_pinIndex =  5;	polygon.flipV();	polygon.translate( QPointF(0,8*W) );	out.push_back(polygon);
	polygon.m_pinIndex =  8;	polygon.flipH();	polygon.translate( QPointF(3*W,0) );	out.push_back(polygon);
	polygon.m_pinIndex = 19;	polygon.flipV();	polygon.translate( QPointF(0,-8*W) );	out.push_back(polygon);

	polygon.clear();
	polygon.m_pinIndex = 21;	Bezier(polygon, pC+QPointF(-2*Q,-16*Q), pC+QPointF(-Q,-14*Q), pC+QPointF(-Q,-9*Q) );	out.push_back(polygon);
	polygon.m_pinIndex =  6;	polygon.flipV();	polygon.translate( QPointF(0,8*W) );	out.push_back(polygon);
	polygon.m_pinIndex =  7;	polygon.flipH();	polygon.translate( QPointF(W,0) );		out.push_back(polygon);
	polygon.m_pinIndex = 20;	polygon.flipV();	polygon.translate( QPointF(0,-8*W) );	out.push_back(polygon);
}
#endif
