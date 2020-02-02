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

enum class GPEN			{MIL10 = 0, PAD, TRACK, HOLE, PAD_GAP, TRACK_GAP, PAD_MASK, RELIEF};
enum class GPOLARITY	{DARK = 0, CLEAR};
enum class GFILE		{GKO = 0, GBL, GBS, GTL, GTS};	//TODO Add drill and silk screens

const int NUM_STREAMS = 1 + (int)(GFILE::GTS);

const bool FULL_LINE = false;	// Set to true to force each Gerber line to be written in long format

static void AppendFileType(const GFILE& eType, std::string& outStr)
{
	switch(eType)
	{
		case GFILE::GKO:	outStr += "BoardOutline";			return;
		case GFILE::GBL:	outStr += "BottomLayer";			return;
		case GFILE::GBS:	outStr += "BottomSolderMaskLayer";	return;
		case GFILE::GTL:	outStr += "TopLayer";				return;
		case GFILE::GTS:	outStr += "TopSolderMaskLayer";		return;
	}
}

static void AppendFileSuffix(const GFILE& eType, std::string& outStr)
{
	switch(eType)
	{
		case GFILE::GKO:	outStr += ".GKO";	return;
		case GFILE::GBL:	outStr += ".GBL";	return;
		case GFILE::GBS:	outStr += ".GBS";	return;
		case GFILE::GTL:	outStr += ".GTL";	return;
		case GFILE::GTS:	outStr += ".GTS";	return;
	}
}

// Wrapper for a stream to a Gerber file
struct GStream : public std::ofstream
{
	// Methods
	void Close()
	{
		if ( !is_open() ) return;
		if ( m_eType == GFILE::GKO )	// Not sure if this is really needed at the end
		{
			(*this) << "%LPD*%";	EndLine();
		}
		(*this) << "M00";	EndLine();
		(*this) << "M02";	EndLine();
		close();
	}
	void Initialise(const GFILE& eType, const Board& board)
	{
		m_eType	 = eType;
		m_pBoard = &board;
		WriteHeader();
		MakeApertures();
		if ( m_eType != GFILE::GKO )	// Not sure if this is really needed at the start
		{
			(*this) << "%LPD*%";	EndLine();
		}
	}
	void WriteHeader()	// Write header for current stream
	{
		assert( m_pBoard->GetGRIDPIXELS() == 1000 );	// ==> 4 decimal places per inch
		std::string	strProgram = std::string("VeroRoute V") + std::string(szVEROROUTE_VERSION);
		std::string	strLayer   = std::string("Layer: ");	AppendFileType(m_eType, strLayer);
		Comment(strLayer.c_str());
		Comment(strProgram.c_str());
		Comment("Gerber Generator version 0.1");
		Comment("Scale: 100 percent, Rotated: No, Reflected: No");
		Comment("Dimensions in inches");
		Comment("Leading zeros omitted, Absolute positions, 2 integer and 4 decimal");
		(*this) << "%FSLAX24Y24*%"	<< std::endl;
		(*this) << "%MOIN*%"		<< std::endl;	// MOIN/MOCM ==> Inches/cm
		(*this) << "G90";			EndLine();		// G90/G91   ==> Absolute/relative coords
		(*this) << "G70D02";		EndLine();		// G70/G71   ==> in/mm
		m_iLastG = 70;
		m_iLastD = 2;
	}
	void MakeApertures()	// Make "pens" for current stream
	{
		const int pad		= m_pBoard->GetPAD_PERCENT();
		const int track		= m_pBoard->GetTRACK_PERCENT();
		const int hole		= m_pBoard->GetHOLE_PERCENT();
		const int gap		= m_pBoard->GetGAP_PERCENT();
		const int mask		= m_pBoard->GetMASK_PERCENT();
		const int relief	= m_pBoard->GetRELIEF_PERCENT();
		const int padgap	= pad   + 2 * gap;	// Gap  is the radius increase
		const int trackgap	= track + 2 * gap;	// Gap  is the radius increase
		const int padmask	= pad   + 2 * mask;	// Mask is the radius increase

		(*this) << "%ADD10C,0.010*%" << std::endl;	// D10 is a circle with diameter of 10 mil

		(*this) << "%ADD11C,0.";					// D11 is a circle with diameter of a pad
		if ( pad < 100 ) (*this) << "0";
		if ( pad < 10  ) (*this) << "0";
		(*this) << pad << "*%" << std::endl;

		(*this) << "%ADD12C,0.";					// D12 is a circle with diameter of a track
		if ( track < 100 ) (*this) << "0";
		if ( track < 10  ) (*this) << "0";
		(*this) << track << "*%" << std::endl;

		(*this) << "%ADD13C,0.";					// D13 is a circle with diameter of a hole
		if ( hole < 100 ) (*this) << "0";
		if ( hole < 10  ) (*this) << "0";
		(*this) << hole << "*%" << std::endl;

		(*this) << "%ADD14C,0.";					// D14 is a circle with diameter of a (pad + gap)
		if ( padgap < 100 ) (*this) << "0";
		if ( padgap < 10  ) (*this) << "0";
		(*this) << padgap << "*%" << std::endl;

		(*this) << "%ADD15C,0.";					// D15 is a circle with diameter of a (track + gap)
		if ( trackgap < 100 ) (*this) << "0";
		if ( trackgap < 10  ) (*this) << "0";
		(*this) << trackgap << "*%" << std::endl;

		(*this) << "%ADD16C,0.";					// D16 is a circle with diameter of a (track + mask)
		if ( padmask < 100 ) (*this) << "0";
		if ( padmask < 10  ) (*this) << "0";
		(*this) << padmask << "*%" << std::endl;

		(*this) << "%ADD17C,0.";					// D16 is a circle with diameter of a thermal relief hole
		if ( relief < 100 ) (*this) << "0";
		if ( relief < 10  ) (*this) << "0";
		(*this) << relief << "*%" << std::endl;
	}
	void SetPolarity(const GPOLARITY& eType)
	{
		switch( eType )
		{
			case GPOLARITY::DARK:	(*this) << "%LPD*%" << std::endl;	return;
			case GPOLARITY::CLEAR:	(*this) << "%LPC*%" << std::endl;	return;
		}
	}
	void SetPen(const GPEN& eType)
	{
		// G54 ==> tool select.  D10 - D17 ==> MIL10, PAD, TRACK, HOLE, PAD_GAP, TRACK_GAP, PAD_MASK, RELIEF
		switch( eType )
		{
			case GPEN::MIL10:		(*this) << "G54D10"; EndLine(); m_iLastG = 54; m_iLastD = 10; return;
			case GPEN::PAD:			(*this) << "G54D11"; EndLine(); m_iLastG = 54; m_iLastD = 11; return;
			case GPEN::TRACK:		(*this) << "G54D12"; EndLine(); m_iLastG = 54; m_iLastD = 12; return;
			case GPEN::HOLE:		(*this) << "G54D13"; EndLine(); m_iLastG = 54; m_iLastD = 13; return;
			case GPEN::PAD_GAP:		(*this) << "G54D14"; EndLine(); m_iLastG = 54; m_iLastD = 14; return;
			case GPEN::TRACK_GAP:	(*this) << "G54D15"; EndLine(); m_iLastG = 54; m_iLastD = 15; return;
			case GPEN::PAD_MASK:	(*this) << "G54D16"; EndLine(); m_iLastG = 54; m_iLastD = 16; return;
			case GPEN::RELIEF:		(*this) << "G54D17"; EndLine(); m_iLastG = 54; m_iLastD = 17; return;
		}
	}
	void Flash(const QPointF& p)
	{
		WriteXY(p, true);	// Always specify X and Y for a flash //TODO test false
		(*this) << "D03";	// Always specify a flash
		EndLine();	m_iLastD =3;
	}
	void Move(const QPointF& p)
	{
		WriteXY(p, FULL_LINE);
		if ( FULL_LINE || m_iLastD != 2 ) (*this) << "D02";
		EndLine();	m_iLastD = 2;
	}
	void Draw(const QPointF& p)
	{
		WriteXY(p, FULL_LINE);
		if ( FULL_LINE || m_iLastD != 1 ) (*this) << "D01";
		EndLine();	m_iLastD = 1;
	}
	void Line(const QPointF& pA, const QPointF& pB)
	{
		Move(pA);
		Draw(pB);
	}
	void DrawRegion(const QPolygonF& polygon)	// A filled polygon (with zero width pen)
	{
		if ( polygon.size() < 3 ) return;	// Region must have >= 3 points
		(*this) << "G36";	EndLine();	m_iLastG = 36;	// "Begin region"
		DrawOutLine(polygon);
		(*this) << "G37";	EndLine();	m_iLastG = 37;	// "End region"
	}
	void DrawPolygon(const QPolygonF& polygon, const bool& bFill)
	{
		DrawOutLine(polygon);		// Draw polygon outline (in the current pen)
		if ( bFill )
			DrawRegion(polygon);	// Fill the polygon (using a zero width pen)
	}
private:
	void WriteXY(const QPointF& p, const bool& bFullLine)
	{
		if ( FULL_LINE || m_iLastG != 1 ) (*this) << "G01";	// Linear interpolation
		m_iLastG = 1;

		const int ix = (int) p.x();
		const int iy = m_pBoard->GetGRIDPIXELS() * m_pBoard->GetRows() - (int) p.y();	// Gerber y-axis goes up screen
		if ( bFullLine || m_iLastX != ix ) (*this) << "X" << ix;
		m_iLastX = ix;
		if ( bFullLine || m_iLastY != iy ) (*this) << "Y" << iy;
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
	void Comment(const char* sz)	{ (*this) << "G04 " << sz << " ";	EndLine();	m_iLastG = 4;  }
	void EndLine()					{ (*this) << "*" << std::endl;	}
	// Data
	GFILE			m_eType		= GFILE::GBL;	// GKO, GBL, GBS, GTL, GTS
	const Board*	m_pBoard	= nullptr;		// The board, so we can get dimensions and track sizes
	int				m_iLastX	= INT_MAX;		// Last X used
	int				m_iLastY	= INT_MAX;		// Last Y used
	int				m_iLastG	= INT_MAX;		// Last G-code used
	int				m_iLastD	= INT_MAX;		// Last D-code used
};

// Wrapper for handling a set of Gerber files
class GWriter
{
public:
	GWriter() {}
	~GWriter()	{ Close(); }
	bool Open(const char* fileName, const Board& board)
	{
		// Open all Gerber files for writing
		bool bOK(fileName != nullptr);
		for (int i = 0; i < NUM_STREAMS && bOK; i++)
		{
			std::string str(fileName);
			AppendFileSuffix(GFILE(i), str);

			m_os[i].open(str.c_str(), std::ios::out);
			bOK = m_os[i].is_open();
			if ( bOK )
				m_os[i].Initialise(GFILE(i), board);
		}
		if ( !bOK )
			for (int i = 0; i < NUM_STREAMS; i++)
				if ( m_os[i].is_open() ) m_os[i].close();
		return bOK;
	}
	void Close()	// Close all Gerber files
	{
		for (int i = 0; i < NUM_STREAMS; i++) m_os[i].Close();
	}
	GStream&	GetStream(const GFILE& eType)	{ return m_os[(size_t)(eType)]; }
private:
	GStream		m_os[NUM_STREAMS];	// Output streams
};
