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

#include <QPolygonF>
#include "Version.h"
#include "Board.h"

// Wrapper for writing to a Gerber file

enum class GPEN {PAD = 0, TRACK, HOLE, PAD_GAP, TRACK_GAP};
enum class GPOLARITY {DARK = 0, CLEAR};

const bool FULL_LINE = false;	// Set to true to force each Gerber line to be written in long format

class GWriter
{
public:
	GWriter()	{}
	~GWriter()	{ Close(); }
	bool Open(const char* fileName, const Board& board)
	{
		if ( fileName == nullptr ) return false;
		m_os.open(fileName, std::ios::out);	// Open file
		if ( !m_os.is_open() ) return false;
		m_pBoard = &board;
		WriteHeader();
		MakeApertures();
		return true;
	}
	void Close()
	{
		if ( !m_os.is_open() ) return;
		m_os << "M00";	EndLine();
		m_os << "M02";	EndLine();
		m_os.close();
	}
	void WriteHeader()
	{
		assert( m_pBoard->GetGRIDPIXELS() == 1000 );	// ==> 4 decimal places per inch

		const std::string strProgram = std::string("VeroRoute V") + std::string(szVEROROUTE_VERSION);
		Comment("Layer: BottomLayer"); //TODO Use correct layer name.  Maybe filename minus suffix
		Comment(strProgram.c_str());
		Comment("Gerber Generator version 0.1");
		Comment("Scale: 100 percent, Rotated: No, Reflected: No");
		Comment("Dimensions in inches");
		Comment("Leading zeros omitted, Absolute positions, 2 integer and 4 decimal");
		m_os << "%FSLAX24Y24*%"	<< std::endl;
		m_os << "%MOIN*%"		<< std::endl;	// MOIN/MOCM ==> Inches/cm
		m_os << "G90";		EndLine();			// G90/G91   ==> Absolute/relative coords
		m_os << "G70D02";	EndLine();			// G70/G71   ==> in/mm
		m_iLastG = 70;
		m_iLastD = 2;
	}
	void MakeApertures()	// Make "Pens"
	{
		const int pad		= m_pBoard->GetPAD_PERCENT();	assert(pad   > 0 && pad   < 100);
		const int track		= m_pBoard->GetTRACK_PERCENT();	assert(track > 0 && track < 100);
		const int hole		= m_pBoard->GetHOLE_PERCENT();	assert(hole  > 0 && hole  < 100);
		const int gap		= m_pBoard->GetGAP_PERCENT();	assert(gap   > 0 && gap  < 100);
		const int padgap	= pad   + 2 * gap;	// Gap is the radius increase
		const int trackgap	= track + 2 * gap;	// Gap is the radius increase

		m_os << "%ADD10C,0.";	// Aperture Define:  D10 is a circle with diameter of a pad
		if ( pad < 100 ) m_os << "0";
		if ( pad < 10  ) m_os << "0";
		m_os << pad << "*%" << std::endl;

		m_os << "%ADD11C,0.";	// Aperture Define:  D11 is a circle with diameter of a track
		if ( track < 100 ) m_os << "0";
		if ( track < 10  ) m_os << "0";
		m_os << track << "*%" << std::endl;

		m_os << "%ADD12C,0.";	// Aperture Define:  D12 is a circle with diameter of a hole
		if ( hole < 100 ) m_os << "0";
		if ( hole < 10  ) m_os << "0";
		m_os << hole << "*%" << std::endl;

		m_os << "%ADD13C,0.";	// Aperture Define:  D13 is a circle with diameter of a (pad + gap)
		if ( padgap < 100 ) m_os << "0";
		if ( padgap < 10  ) m_os << "0";
		m_os << padgap << "*%" << std::endl;

		m_os << "%ADD14C,0.";	// Aperture Define:  D14 is a circle with diameter of a (track + gap)
		if ( trackgap < 100 ) m_os << "0";
		if ( trackgap < 10  ) m_os << "0";
		m_os << trackgap << "*%" << std::endl;
	}
	void SetPolarity(const GPOLARITY& eType)
	{
		switch( eType )
		{
			case GPOLARITY::DARK:	m_os << "%LPD*%" << std::endl;	return;
			case GPOLARITY::CLEAR:	m_os << "%LPC*%" << std::endl;	return;
		}
	}
	void SetPen(const GPEN& eType)
	{
		// G54 ==> tool select.  D10,D11,D12,D13,D14 ==> PAD,TRACK,HOLE,PAD_GAP,TRACK_GAP
		switch( eType )
		{
			case GPEN::PAD:			m_os << "G54D10"; EndLine(); m_iLastG = 54; m_iLastD = 10; return;
			case GPEN::TRACK:		m_os << "G54D11"; EndLine(); m_iLastG = 54; m_iLastD = 11; return;
			case GPEN::HOLE:		m_os << "G54D12"; EndLine(); m_iLastG = 54; m_iLastD = 12; return;
			case GPEN::PAD_GAP:		m_os << "G54D13"; EndLine(); m_iLastG = 54; m_iLastD = 13; return;
			case GPEN::TRACK_GAP:	m_os << "G54D14"; EndLine(); m_iLastG = 54; m_iLastD = 14; return;
		}
	}
	void Flash(const QPointF& p)
	{
		LinearInterpolation();
		WriteXY(p, true);	// Always specify X and Y for a flash
		m_os << "D03";
		EndLine();	m_iLastD = 3;
	}
	void Move(const QPointF& p)
	{
		LinearInterpolation();
		WriteXY(p, FULL_LINE);
		if ( FULL_LINE || m_iLastD != 2 ) m_os << "D02";
		EndLine();	m_iLastD = 2;
	}
	void Draw(const QPointF& p)
	{
		LinearInterpolation();
		WriteXY(p, FULL_LINE);
		if ( FULL_LINE || m_iLastD != 1 ) m_os << "D01";
		EndLine();	m_iLastD = 1;
	}
	void Line(const QPointF& pA, const QPointF& pB)
	{
		Move(pA);
		Draw(pB);
	}
	void DrawRegion(const QPolygonF& polygon)	// A filled polygon (with zero width pen)
	{
		if ( polygon.size() < 3 ) return;			// Polygon must have >=3 points
		m_os << "G36";	EndLine();	m_iLastG = 36;	// "Begin region"
		DrawOutLine(polygon);
		m_os << "G37";	EndLine();	m_iLastG = 37;	// "End region"
	}
	void DrawPolygon(const QPolygonF& polygon, const bool& bFill)
	{
		DrawOutLine(polygon);		// Draw polygon outline (in the current pen)
		if ( bFill )
			DrawRegion(polygon);	// Fill the polygon (using a zero width pen)
	}
private:
	void EndLine()					{ m_os << "*" << std::endl;	}
	void Comment(const char* sz)	{ m_os << "G04 " << sz << " ";	EndLine();	m_iLastG = 4;  }
	void LinearInterpolation()
	{
		if ( FULL_LINE || m_iLastG != 1 ) m_os << "G01";
		m_iLastG = 1;
	}
	void WriteXY(const QPointF& p, const bool& bFullLine)
	{
		const int ix = (int) p.x();
		const int iy = m_pBoard->GetGRIDPIXELS() * m_pBoard->GetRows() - (int) p.y();	// Gerber y-axis goes up screen
		if ( bFullLine || m_iLastX != ix ) m_os << "X" << ix;
		m_iLastX = ix;
		if ( bFullLine || m_iLastY != iy ) m_os << "Y" << iy;
		m_iLastY = iy;
	}
	void DrawOutLine(const QPolygonF& polygon)	// Outline of a closed shape
	{
		const int N = polygon.size();
		if ( N == 1 ) return Flash( polygon[0] );
		if ( N == 2 ) return Line(polygon[0], polygon[1]);
		Move( polygon[0] );
		for (int i = 1; i < N; i++)
			if ( polygon[i] != polygon[i-1] ) Draw( polygon[i] );
		if ( polygon[0] != polygon[N-1] ) Draw( polygon[0] );	// Force closed polygon
	}
private:
	const Board*	m_pBoard = nullptr;	// The board, so we can get info like dimensions and track sizes
	std::ofstream	m_os;				// Output file stream
	// The following allow for smaller Gerber files by not repeating co-ordinates and commands
	int				m_iLastX = 0;		// Last X used
	int				m_iLastY = 0;		// Last Y used
	int				m_iLastG = -1;		// Last G-code used
	int				m_iLastD = -1;		// Last D-code used
};
