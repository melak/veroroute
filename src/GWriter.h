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

enum class	GPEN		{UNKNOWN = 0, MIL10, PAD, TRACK, HOLE, PAD_GAP, TRACK_GAP, PAD_MASK, RELIEF};
enum class	GPOLARITY	{DARK = 0, CLEAR};
enum class	GFILE		{GKO = 0, GBL, GBS, GTL, GTS};	//TODO Add drill and silk screens

const int	NUM_STREAMS	= 1 + (int)(GFILE::GTS);
const bool	FULL_LINE	= false;	// Set to true to force each Gerber line to be written in long format

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
	GStream()  {}
	~GStream() {}
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
		m_ePen	 = GPEN::UNKNOWN;
		m_pBoard = &board;
		m_iLastX = INT_MAX;
		m_iLastY = INT_MAX;
		WriteHeader();
		MakeApertures();
		LinearInterpolation();
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

		(*this) << "%ADD17C,0.";					// D17 is a circle with diameter of a thermal relief hole
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
	void SetPen(const GPEN& ePen)
	{
		//(*this) << "G54";	// G54 (tool select) can be omitted
		if ( m_ePen == ePen ) return;
		m_ePen = ePen;
		switch( m_ePen )
		{
			case GPEN::UNKNOWN:		return;
			case GPEN::MIL10:		(*this) << "D10"; EndLine(); return;
			case GPEN::PAD:			(*this) << "D11"; EndLine(); return;
			case GPEN::TRACK:		(*this) << "D12"; EndLine(); return;
			case GPEN::HOLE:		(*this) << "D13"; EndLine(); return;
			case GPEN::PAD_GAP:		(*this) << "D14"; EndLine(); return;
			case GPEN::TRACK_GAP:	(*this) << "D15"; EndLine(); return;
			case GPEN::PAD_MASK:	(*this) << "D16"; EndLine(); return;
			case GPEN::RELIEF:		(*this) << "D17"; EndLine(); return;
		}
	}
	void Flash(const QPointF& p)
	{
		WriteXY(p, FULL_LINE);
		(*this) << "D03";		// Always specify D03 code
		EndLine();
	}
	void Move(const QPointF& p)
	{
		const int ix = (int) p.x();
		const int iy = m_pBoard->GetGRIDPIXELS() * m_pBoard->GetRows() - (int) p.y();	// Gerber y-axis goes up screen
		if ( m_iLastX == ix && m_iLastY == iy ) return;
		WriteXY(p, FULL_LINE);
		(*this) << "D02";		// Always specify D02 code
		EndLine();
	}
	void Draw(const QPointF& p)
	{
		WriteXY(p, FULL_LINE);
		 (*this) << "D01";		// Always specify D01 code
		EndLine();
	}
	void Line(const QPointF& pA, const QPointF& pB)
	{
		Move(pA);
		Draw(pB);
	}
	void Rect(const QPointF& pA, const QPointF& delta)
	{
		Move(pA);
		Draw(pA + QPointF(delta.x(), 0));
		Draw(pA + QPointF(delta.x(), delta.y()));
		Draw(pA + QPointF(0,         delta.y()));
		Draw(pA);
	}
	void RoundedRect(const QPointF& pA, const QPointF& delta, double d1)	//TODO
	{
		if ( d1 == 0 || d1 != 0 )
			Rect(pA, delta);
	}
	void Ellipse(const QPointF& pA, const QPointF& delta)	//TODO
	{
		Rect(pA, delta);
	}
	void Arc(const QPointF& pA, const QPointF& delta, double d1, double d2)	//TODO
	{
		if ( d1 == 0 || d1 != 0 || d2 == 0 )
		Rect(pA, delta);
	}
	void Chord(const QPointF& pA, const QPointF& delta, double d1, double d2)	//TODO
	{
		if ( d1 == 0 || d1 != 0 || d2 == 0 )
		Rect(pA, delta);
	}
	void DrawRegion(const QPolygonF& polygon)	// A filled polygon (with zero width pen)
	{
		if ( polygon.size() < 3 ) return;	// Region must have >= 3 points
		(*this) << "G36";	EndLine();		// "Begin region"
		DrawOutLine(polygon);
		(*this) << "G37";	EndLine();		// "End region"
	}
	void DrawPolygon(const QPolygonF& polygon)
	{
		DrawOutLine(polygon);	// Draw polygon outline (in the current pen)
		DrawRegion(polygon);	// Fill the polygon (using a zero width pen)
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
	void WriteXY(const QPointF& p, const bool& bFullLine)
	{
		const int ix = (int) p.x();
		const int iy = m_pBoard->GetGRIDPIXELS() * m_pBoard->GetRows() - (int) p.y();	// Gerber y-axis goes up screen
		if ( bFullLine || m_iLastX != ix ) (*this) << "X" << ix;
		m_iLastX = ix;
		if ( bFullLine || m_iLastY != iy ) (*this) << "Y" << iy;
		m_iLastY = iy;
	}
	void LinearInterpolation()		{ (*this) << "G01"; }
	void Comment(const char* sz)	{ (*this) << "G04 " << sz << " ";	EndLine(); }
	void EndLine()					{ (*this) << "*" << std::endl;	}
	// Data
	GFILE			m_eType		= GFILE::GBL;	// GKO, GBL, GBS, GTL, GTS
	GPEN			m_ePen		= GPEN::UNKNOWN;
	const Board*	m_pBoard	= nullptr;		// The board, so we can get dimensions and track sizes
	int				m_iLastX	= INT_MAX;		// Last X used
	int				m_iLastY	= INT_MAX;		// Last Y used
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
