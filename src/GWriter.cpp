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

#include "Version.h"
#include "Board.h"
#include "GWriter.h"
#include <QPolygonF>
#include <QTimeZone>

const bool	FULL_LINE	= false;	// Set to true to force each Gerber line to be written in long format

// Wrapper for a stream to a Gerber file
void GStream::Close()
{
	if ( !is_open() ) return;
	if ( m_eType == GFILE::DRL )
	{
		(*this) << "M30";	EndLine();	// End of program
	}
	else
	{
		if ( m_eType == GFILE::GKO )	// Not sure if this is really needed at the end
		{
			(*this) << "%LPD*%";	EndLine();
		}
		(*this) << "M00";	EndLine();	// Program stop
		(*this) << "M02";	EndLine();	// End of file
	}
	close();
}
void GStream::Initialise(const GFILE& eType, const Board& board, const QString& UTC)
{
	m_eType	 = eType;
	m_ePen	 = GPEN::UNKNOWN;
	m_pBoard = &board;
	m_iLastX = INT_MAX;
	m_iLastY = INT_MAX;
	WriteHeader(UTC);
	MakeApertures();
	LinearInterpolation();
	if ( m_eType != GFILE::DRL && m_eType != GFILE::GKO )	// Not sure if this is really needed at the start
	{
		(*this) << "%LPD*%";	EndLine();
	}
}
void GStream::WriteHeader(const QString& UTC)	// Write header for current stream
{
	assert( m_pBoard->GetGRIDPIXELS() == 1000 );	// ==> 4 decimal places per inch
	std::string	strLayer	= std::string("Layer: ");
	std::string	strProgram	= std::string("VeroRoute V") + std::string(szVEROROUTE_VERSION);
	std::string	strUTC		= UTC.toStdString();
	std::string	strGen		= std::string("Gerber Generator version 0.1");
	switch(m_eType)
	{
		case GFILE::GKO: strLayer += "BoardOutline";			break;
		case GFILE::DRL: strLayer += "Drill_PTH";				break;
		case GFILE::GBL: strLayer += "BottomLayer";				break;
		case GFILE::GBS: strLayer += "BottomSolderMaskLayer";	break;
		case GFILE::GTL: strLayer += "TopLayer";				break;
		case GFILE::GTS: strLayer += "TopSolderMaskLayer";		break;
	}
	Comment(strLayer.c_str());
	Comment(strProgram.c_str());
	Comment(strUTC.c_str());
	Comment(strGen.c_str());

	if ( m_eType == GFILE::DRL )
	{
		const int hole		= m_pBoard->GetHOLE_PERCENT();

		(*this) << "M48";				EndLine();	// M48 is start of header
		(*this) << "INCH,LZ,00.0000";	EndLine();	// Inches.  Leading zeros INCLUDED.  2 integer and 4 decimal

		// Comment about hole size:		";Holesize 1 = 0.032 INCH"
		(*this) << ";Holesize 1 = 0.";
		if ( hole < 100 ) (*this) << "0";
		if ( hole < 10  ) (*this) << "0";
		(*this) << hole << " INCH";	EndLine();

		// Define Tool 1:				"T01C0.032" ==> 0.032 inch diameter
		(*this) << "T01C0.";
		if ( hole < 100 ) (*this) << "0";
		if ( hole < 10  ) (*this) << "0";
		(*this) << hole;	EndLine();

	//	(*this) << "M95";	EndLine();	// M95 End of the header
		(*this) << "%";		EndLine();	// Rewind Stop.  Often used instead of M95.
		(*this) << "G05";	EndLine();	// Turn on drill mode (Format 2 command)
		(*this) << "G81";	EndLine();	// Turn on drill mode (Format 1 command)
		(*this) << "G90";	EndLine();	// Absolute mode
		(*this) << "T01";	EndLine();	// Select Tool 1
	}
	else
	{
		Comment("Scale: 100 percent, Rotated: No, Reflected: No");
		Comment("Dimensions in inches");
		Comment("Leading zeros omitted, Absolute positions, 2 integer and 4 decimal");
		(*this) << "%FSLAX24Y24*%"	<< std::endl;
		(*this) << "%MOIN*%"		<< std::endl;	// MOIN/MOCM ==> Inches/cm
		(*this) << "G90";		EndLine();			// G90/G91   ==> Absolute/relative coords
		(*this) << "G70D02";	EndLine();			// G70/G71   ==> in/mm
	}
}
void GStream::MakeApertures()	// Make "pens" for current stream
{
	if ( m_eType == GFILE::DRL ) return;
	const int pad		= m_pBoard->GetPAD_PERCENT();
	const int track		= m_pBoard->GetTRACK_PERCENT();
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

	(*this) << "%ADD13C,0.";					// D13 is a circle with diameter of a (pad + gap)
	if ( padgap < 100 ) (*this) << "0";
	if ( padgap < 10  ) (*this) << "0";
	(*this) << padgap << "*%" << std::endl;

	(*this) << "%ADD14C,0.";					// D14 is a circle with diameter of a (track + gap)
	if ( trackgap < 100 ) (*this) << "0";
	if ( trackgap < 10  ) (*this) << "0";
	(*this) << trackgap << "*%" << std::endl;

	(*this) << "%ADD15C,0.";					// D15 is a circle with diameter of a (track + mask)
	if ( padmask < 100 ) (*this) << "0";
	if ( padmask < 10  ) (*this) << "0";
	(*this) << padmask << "*%" << std::endl;

	(*this) << "%ADD16C,0.";					// D16 is a circle with diameter of a thermal relief hole
	if ( relief < 100 ) (*this) << "0";
	if ( relief < 10  ) (*this) << "0";
	(*this) << relief << "*%" << std::endl;
}
void GStream::SetPolarity(const GPOLARITY& ePolarity)
{
	if ( m_eType == GFILE::DRL ) return;
	switch( ePolarity )
	{
		case GPOLARITY::DARK:	(*this) << "%LPD*%" << std::endl;	return;
		case GPOLARITY::CLEAR:	(*this) << "%LPC*%" << std::endl;	return;
	}
}
void GStream::SetPen(const GPEN& ePen)
{
	if ( m_eType == GFILE::DRL ) return;
	//(*this) << "G54";	// G54 (tool select) can be omitted
	if ( m_ePen == ePen ) return;
	m_ePen = ePen;
	switch( m_ePen )
	{
		case GPEN::UNKNOWN:		return;
		case GPEN::MIL10:		(*this) << "D10"; EndLine(); return;
		case GPEN::PAD:			(*this) << "D11"; EndLine(); return;
		case GPEN::TRACK:		(*this) << "D12"; EndLine(); return;
		case GPEN::PAD_GAP:		(*this) << "D13"; EndLine(); return;
		case GPEN::TRACK_GAP:	(*this) << "D14"; EndLine(); return;
		case GPEN::PAD_MASK:	(*this) << "D15"; EndLine(); return;
		case GPEN::RELIEF:		(*this) << "D16"; EndLine(); return;
	}
}
void GStream::Drill(const QPointF& p)
{
	if ( m_eType != GFILE::DRL ) return;
	const int ix = (int) p.x();
	const int iy = m_pBoard->GetGRIDPIXELS() * m_pBoard->GetRows() - (int) p.y();	// Gerber y-axis goes up screen
	(*this) << "X";  WriteDrillValue(ix);
	(*this) << "Y";  WriteDrillValue(iy);
	(*this) << std::endl;
}
void GStream::WriteDrillValue(const int& iMil)
{
	if ( m_eType != GFILE::DRL ) return;
	const int	iAbs	= abs(iMil);
	assert(iMil > 0);	// All veroRoute grid points are >= 0
	(*this) << ( iMil >= 0 ? "+" : "-" );
	if ( iAbs < 100000 ) (*this) << "0";
	if ( iAbs <  10000 ) (*this) << "0";
	if ( iAbs <   1000 ) (*this) << "0";
	if ( iAbs <    100 ) (*this) << "0";
	if ( iAbs <     10 ) (*this) << "0";
	(*this) << iAbs;
}
void GStream::Flash(const QPointF& p)
{
	if ( m_eType == GFILE::DRL ) return;
	WriteXY(p, FULL_LINE);
	(*this) << "D03";		// Always specify D03 code
	EndLine();
}
void GStream::Move(const QPointF& p)
{
	if ( m_eType == GFILE::DRL ) return;
	const int ix = (int) p.x();
	const int iy = m_pBoard->GetGRIDPIXELS() * m_pBoard->GetRows() - (int) p.y();	// Gerber y-axis goes up screen
	if ( m_iLastX == ix && m_iLastY == iy ) return;
	WriteXY(p, FULL_LINE);
	(*this) << "D02";		// Always specify D02 code
	EndLine();
}
void GStream::Draw(const QPointF& p)
{
	if ( m_eType == GFILE::DRL ) return;
	WriteXY(p, FULL_LINE);
	 (*this) << "D01";		// Always specify D01 code
	EndLine();
}
void GStream::Line(const QPointF& pA, const QPointF& pB)
{
	Move(pA);
	Draw(pB);
}
void GStream::Rect(const QPointF& pA, const QPointF& delta)
{
	Move(pA);
	Draw(pA + QPointF(delta.x(), 0));
	Draw(pA + QPointF(delta.x(), delta.y()));
	Draw(pA + QPointF(0,         delta.y()));
	Draw(pA);
}
void GStream::RoundedRect(const QPointF& pA, const QPointF& delta, double d1)	//TODO
{
	if ( d1 == 0 || d1 != 0 )
		Rect(pA, delta);
}
void GStream::Ellipse(const QPointF& pA, const QPointF& delta)	//TODO
{
	Rect(pA, delta);
}
void GStream::Arc(const QPointF& pA, const QPointF& delta, double d1, double d2)	//TODO
{
	if ( d1 == 0 || d1 != 0 || d2 == 0 )
	Rect(pA, delta);
}
void GStream::Chord(const QPointF& pA, const QPointF& delta, double d1, double d2)	//TODO
{
	if ( d1 == 0 || d1 != 0 || d2 == 0 )
	Rect(pA, delta);
}
void GStream::DrawRegion(const QPolygonF& polygon)	// A filled polygon (with zero width pen)
{
	if ( polygon.size() < 3 ) return;	// Region must have >= 3 points
	(*this) << "G36";	EndLine();		// "Begin region"
	DrawOutLine(polygon);
	(*this) << "G37";	EndLine();		// "End region"
}
void GStream::DrawPolygon(const QPolygonF& polygon)	// Filled polygon (with non-zero width pen)
{
	DrawOutLine(polygon);	// Draw polygon outline (in the current pen)
	DrawRegion(polygon);	// Fill the polygon (using a zero width pen)
}
void GStream::DrawOutLine(const QPolygonF& polygon)	// Outline of a closed shape
{
	const int N = polygon.size();
	if ( N == 1 ) return Flash( polygon[0] );
	if ( N == 2 ) return Line(polygon[0], polygon[1]);
	Move( polygon[0] );
	for (int i = 1; i < N; i++)
		if ( polygon[i] != polygon[i-1] ) Draw( polygon[i] );
	if ( polygon[0] != polygon[N-1] ) Draw( polygon[0] );	// Force closed polygon
}
void GStream::WriteXY(const QPointF& p, const bool& bFullLine)
{
	const int ix = (int) p.x();
	const int iy = m_pBoard->GetGRIDPIXELS() * m_pBoard->GetRows() - (int) p.y();	// Gerber y-axis goes up screen
	if ( bFullLine || m_iLastX != ix ) (*this) << "X" << ix;
	m_iLastX = ix;
	if ( bFullLine || m_iLastY != iy ) (*this) << "Y" << iy;
	m_iLastY = iy;
}
void GStream::LinearInterpolation()
{
	if ( m_eType == GFILE::DRL ) return;
	(*this) << "G01";
	EndLine();
}
void GStream::Comment(const char* sz)
{
	if ( m_eType == GFILE::DRL )
		(*this) << ";" << sz;
	else
		(*this) << "G04 " << sz << " ";
	EndLine();
}
void GStream::EndLine()
{
	if ( m_eType == GFILE::DRL )
		(*this) << std::endl;
	else
		(*this) << "*" << std::endl;
}

// Wrapper for handling a set of Gerber files
bool GWriter::Open(const char* fileName, const Board& board)
{
	QDateTime	local(QDateTime::currentDateTime());
	QString		UTC = local.toTimeSpec(Qt::UTC).toString(Qt::ISODate);

	// Open all Gerber files for writing
	bool bOK(fileName != nullptr);
	for (int i = 0; i < NUM_STREAMS && bOK; i++)
	{
		std::string str(fileName);
		switch( GFILE(i) )
		{
			case GFILE::GKO: str += ".GKO";	break;
			case GFILE::DRL: str += ".DRL";	break;
			case GFILE::GBL: str += ".GBL";	break;
			case GFILE::GBS: str += ".GBS";	break;
			case GFILE::GTL: str += ".GTL";	break;
			case GFILE::GTS: str += ".GTS";	break;
		}
		m_os[i].open(str.c_str(), std::ios::out);
		bOK = m_os[i].is_open();
		if ( bOK )
			m_os[i].Initialise(GFILE(i), board, UTC);
	}
	if ( !bOK )
		for (int i = 0; i < NUM_STREAMS; i++)
			if ( m_os[i].is_open() ) m_os[i].close();
	return bOK;
}
void GWriter::Close()	// Close all file streams
{
	for (int i = 0; i < NUM_STREAMS; i++) m_os[i].Close();
}
GStream& GWriter::GetStream(const GFILE& eType)
{
	return m_os[(size_t)(eType)];
}
