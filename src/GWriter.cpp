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
GStream::~GStream()
{
	ClearBuffers();
}
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
	ClearBuffers();

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
		case GFILE::GBL: strLayer += "BottomLayer";				break;
		case GFILE::GBS: strLayer += "BottomSolderMaskLayer";	break;
		case GFILE::GBO: strLayer += "BottomSilkLayer";			break;
		case GFILE::GTL: strLayer += "TopLayer";				break;
		case GFILE::GTS: strLayer += "TopSolderMaskLayer";		break;
		case GFILE::GTO: strLayer += "TopSilkLayer";			break;
		case GFILE::DRL: strLayer += ( m_pBoard->GetHoleType() == HOLETYPE::PTH ) ? "Drill_PTH" : "Drill_NPTH";	break;
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
		(*this) << ";Holesize 1 = " << MilToInch(hole) << " INCH";	EndLine();

		// Define Tool 1:				"T01C0.032" ==> 0.032 inch diameter
		(*this) << "T01C" << MilToInch(hole);	EndLine();

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
	const int silk		= m_pBoard->GetSILK_PERCENT();
	const int padgap	= pad   + 2 * gap;	// Gap  is the radius increase
	const int trackgap	= track + 2 * gap;	// Gap  is the radius increase
	const int padmask	= pad   + 2 * mask;	// Mask is the radius increase

	switch( m_eType )
	{
		case GFILE::GKO:
			(*this) << "%ADD10C," << MilToInch(10)		 << "*%" << std::endl;	// D10 is a circle with diameter of 10 mil
			break;
		case GFILE::GBL:
		case GFILE::GTL:
			(*this) << "%ADD11C," << MilToInch(pad)		 << "*%" << std::endl;	// D11 is a circle with diameter of a pad
			(*this) << "%ADD12C," << MilToInch(track)	 << "*%" << std::endl;	// D12 is a circle with diameter of a track
			(*this) << "%ADD13C," << MilToInch(padgap)	 << "*%" << std::endl;	// D13 is a circle with diameter of a (pad + gap)
			(*this) << "%ADD14C," << MilToInch(trackgap) << "*%" << std::endl;	// D14 is a circle with diameter of a (track + gap)
//			(*this) << "%ADD15C," << MilToInch(via)		 << "*%" << std::endl;	// D15 is a circle with diameter of a via-pad
			break;
		case GFILE::GBS:
		case GFILE::GTS:
			(*this) << "%ADD16C," << MilToInch(padmask)	 << "*%" << std::endl;	// D16 is a circle with diameter of a (pad + mask)
			break;
		case GFILE::GTO:
		case GFILE::GBO:
			(*this) << "%ADD17C," << MilToInch(silk)	 << "*%" << std::endl;	// D17 is a circle with diameter of the silk screen pen
			break;
		case GFILE::DRL:
			break;
	}
}
void GStream::Drill(const QPointF& pF)
{
	if ( m_eType != GFILE::DRL ) return;

	QPoint p;
	GetQPoint(pF, p);

	const int& ix = p.x();
	const int& iy = p.y();
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
void GStream::SetPolarity(const GPOLARITY& ePolarity)
{
	if ( m_eType == GFILE::DRL ) return;
	switch( ePolarity )
	{
		case GPOLARITY::DARK:	(*this) << "%LPD*%" << std::endl;	return;
		case GPOLARITY::CLEAR:	(*this) << "%LPC*%" << std::endl;	return;
	}
}
void GStream::AddPad(const GPEN& ePen, const QPointF& pF)		// Adds to m_pads buffer for later writing to file
{
	QPoint p;
	GetQPoint(pF, p);
	m_pads.push_back( new Curve(ePen, p) );
}
void GStream::AddTrack(const GPEN& ePen, const QPolygonF& pF)	// Adds to m_tracks buffer for later writing to file
{
	QPolygon p;
	GetQPolygon(pF, p);
	m_tracks.push_back( new Curve(ePen, p) );
}
void GStream::AddVariTrack(const GPEN& ePenHV, const GPEN& ePen, const QPolygonF& pF)	// Adds to m_tracks buffer for later writing to file
{
	QPolygon p;
	GetQPolygon(pF, p);

	QPolygon temp;
	auto A = p.begin();
	auto B = A; ++B;
	for (; B != p.end(); A++, B++)
	{
		temp.clear();
		temp << *A << *B;
		const bool bHV = ( A->x() == B->x() || A->y() == B->y() );
		m_tracks.push_back( new Curve(bHV ? ePenHV : ePen, temp) );
	}
}
void GStream::AddLoop(const GPEN& ePen, const QPolygonF& pF)	// Adds to m_loops buffer for later writing to file
{
	assert(pF.size() >= 3);
	if ( pF.size() < 3 ) return;	// Loop must have at least 3 points
	QPolygon p;
	GetQPolygon(pF, p);
	m_loops.push_back( new Curve(ePen, p) );
}
void GStream::AddRegion(const QPolygonF& pF)	// Adds to m_regions buffer for later writing to file
{
	assert(pF.size() >= 3);
	if ( pF.size() < 3 ) return;	// Region must have at least 3 points
	QPolygon p;
	GetQPolygon(pF, p);
	m_regions.push_back( new Curve(GPEN::UNKNOWN, p) );
}
void GStream::ClearBuffers()
{
	m_regions.clear();
	m_loops.clear();
	m_tracks.clear();
	m_pads.clear();
}
void GStream::DrawBuffers()
{
	m_tracks.SpliceAll();	// Only tracks (not loops) are spliced

	for (auto& o : m_regions ) Region(*o);
	for (auto& o : m_loops   ) OutLine(*o, true);	// true  ==> closed
	for (auto& o : m_tracks  ) OutLine(*o, false);	// false ==> not closed
	for (auto& o : m_pads    ) OutLine(*o, false);	// false ==> not closed
}
void GStream::Region(const Curve& curve)	// A filled closed curve (with zero width pen)
{
	assert(curve.m_pen == GPEN::UNKNOWN);
	if ( curve.size() < 3 ) return;	// Region must have >= 3 points
	(*this) << "G36";	EndLine();	// "Begin region"
	OutLine(curve, true);			// true ==> force close
	(*this) << "G37";	EndLine();	// "End region"
}
void GStream::OutLine(const Curve& curve, bool bForceClose)	// Outline of a curve
{
	if ( curve.empty() ) return;
	SetPen(curve.m_pen);
	const size_t N = curve.size();
	if	( N == 1 ) return Flash( curve.front() );
	if	( N == 2 ) return Line( curve.front(), curve.back() );
	auto& front = curve.front();
	Move(front);	// Move pen to start of curve
	auto iter = curve.begin(); ++iter;
	for (auto iterEnd = curve.end(); iter != iterEnd; ++iter)
		Draw(*iter);	// Draw line to next point in curve
	if ( bForceClose && front != curve.back() ) Draw( front );
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
		case GPEN::VIA_PAD:		(*this) << "D15"; EndLine(); return;
		case GPEN::PAD_MASK:	(*this) << "D16"; EndLine(); return;
		case GPEN::SILK:		(*this) << "D17"; EndLine(); return;
	}
}
void GStream::Flash(const QPoint& p)
{
	if ( m_eType == GFILE::DRL ) return;
	WriteXY(p, FULL_LINE);
	(*this) << "D03";		// Always specify D03 code
	EndLine();
}
void GStream::Move(const QPoint& p)
{
	if ( m_eType == GFILE::DRL ) return;
	const int& ix = p.x();
	const int& iy = p.y();
	if ( m_iLastX == ix && m_iLastY == iy ) return;
	WriteXY(p, FULL_LINE);
	(*this) << "D02";		// Always specify D02 code
	EndLine();
}
void GStream::Draw(const QPoint& p)
{
	if ( m_eType == GFILE::DRL ) return;
	WriteXY(p, FULL_LINE);
	(*this) << "D01";		// Always specify D01 code
	EndLine();
}
void GStream::Line(const QPoint& pA, const QPoint& pB)
{
	Move(pA);
	Draw(pB);
}
void GStream::WriteXY(const QPoint& p,  const bool& bFullLine)
{
	const int& ix = p.x();
	const int& iy = p.y();
	if ( bFullLine || m_iLastX != ix ) (*this) << "X" << ix;
	m_iLastX = ix;
	if ( bFullLine || m_iLastY != iy ) (*this) << "Y" << iy;
	m_iLastY = iy;
}
void GStream::GetQPoint(const QPointF& in, QPoint& out) const
{
	out.setX( (int) in.x() );
	out.setY( m_pBoard->GetGRIDPIXELS() * m_pBoard->GetRows() - (int) in.y() ); // Gerber y-axis goes up screen
}
void GStream::GetQPolygon(const QPolygonF& in, QPolygon& out) const
{
	out.clear();
	out.resize( in.size() );
	for (auto i = 0; i < in.size(); i++) GetQPoint(in[i], out[i]);
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
std::string GStream::MilToInch(const int& iMil) const	// Just for pen sizes
{
	assert(iMil >= 0 && iMil < 1000);	//TODO Generalise this
	std::string str("0.");
	if ( iMil < 100 ) str += "0";
	if ( iMil < 10  ) str += "0";
	str += std::to_string(iMil);
	return str;
}

// Wrapper for handling a set of Gerber files
bool GWriter::Open(const char* fileName, const Board& board, const bool& bLongGerber)
{
	QDateTime	local(QDateTime::currentDateTime());
	QString		UTC = local.toTimeSpec(Qt::UTC).toString(Qt::ISODate);

	// Open all Gerber files for writing
	bool bOK(fileName != nullptr);
	for (int i = 0; i < NUM_STREAMS && bOK; i++)
	{
		std::string str(fileName);
		if ( bLongGerber )
		{
			switch( GFILE(i) )
			{
				case GFILE::GKO: str += ".boardoutline.ger";		break;
				case GFILE::GBL: str += ".bottomlayer.ger";			break;
				case GFILE::GBS: str += ".bottomsoldermask.ger";	break;
				case GFILE::GBO: str += ".bottomsilkscreen.ger";	break;
				case GFILE::GTL: str += ".toplayer.ger";			break;
				case GFILE::GTS: str += ".topsoldermask.ger";		break;
				case GFILE::GTO: str += ".topsilkscreen.ger";		break;
				case GFILE::DRL: str += ( board.GetHoleType() == HOLETYPE::PTH )
									  ? ".drills_pth.xln"
									  : ".drills_npth.xln";			break;
			}
		}
		else
		{
			switch( GFILE(i) )
			{
				case GFILE::GKO: str += ".GKO";	break;
				case GFILE::GBL: str += ".GBL";	break;
				case GFILE::GBS: str += ".GBS";	break;
				case GFILE::GBO: str += ".GBO";	break;
				case GFILE::GTL: str += ".GTL";	break;
				case GFILE::GTS: str += ".GTS";	break;
				case GFILE::GTO: str += ".GTO";	break;
				case GFILE::DRL: str += ".DRL";	break;
			}
		}
		if ( GFILE(i) == GFILE::GTL || GFILE(i) == GFILE::GTS || GFILE(i) == GFILE::GBO ) continue;	// Don't write these layers yet
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
	for (int i = 0; i < NUM_STREAMS; i++) if ( m_os[i].is_open() ) m_os[i].Close();
}
GStream& GWriter::GetStream(const GFILE& eType)
{
	return m_os[(size_t)(eType)];
}
