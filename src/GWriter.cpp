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
const bool	XNC_FORMAT	= true;		// true ==> XNC Format / Excellon Format 2.		false ==> Excellon Format 1.

// Wrapper for a stream to a Gerber file
void GStream::Clear()
{
	m_eType		= GFILE::GBL;
	m_ePen		= GPEN::NONE;
	m_penWidth	= 0;
	m_ePenList.clear();
	m_ePolarity	= GPOLARITY::UNKNOWN;
	m_pBoard	= nullptr;
	m_bVias		= false;
	m_iLastX	= INT_MAX;
	m_iLastY	= INT_MAX;
	m_bMetric	= false;
	ClearBuffers(false);	// false ==> skip GetOK() checks
}
void GStream::Close()
{
	if ( !m_os.is_open() ) return;
	switch( m_eType )
	{
		case GFILE::DRL:	m_os << "M30";	EndLine();	return m_os.close();	// End of program
		default:			m_os << "M02";	EndLine();	return m_os.close();	// End of file
	}
}
bool GStream::Open(const char* fileName, const GFILE& eType, const bool& bMetric, const Board& board, const bool& bVias, const QString& UTC)
{
	Clear();
	m_eType		= eType;
	m_bMetric	= bMetric;
	m_pBoard	= &board;
	m_bVias		= bVias;

	std::string str(fileName);
	switch( m_eType )
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
	m_os.open(str.c_str(), std::ios::out);

	WriteHeader(UTC);
	LinearInterpolation();
	SetPolarity(GPOLARITY::DARK, false);	// false ==> skip GetOK() checks
	return m_os.is_open();
}
void GStream::WriteHeader(const QString& UTC)	// Write header for current stream
{
	if ( !m_os.is_open() ) return;
	assert( m_pBoard->GetGRIDPIXELS() == 1000 );	// ==> 4 decimal places per inch
	std::string	strLayer	= std::string("Layer: ");
	std::string	strProgram	= std::string("VeroRoute V") + std::string(szVEROROUTE_VERSION);
	std::string	strUTC		= UTC.toStdString();
	std::string	strGen		= std::string("Gerber Generator version 0.7");
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
		m_os << "M48";	EndLine();	// M48 is start of header
		if ( m_bMetric )
		{
			m_os << "METRIC,LZ,0000.000000";	EndLine();	// Millimetres.  Leading zeros INCLUDED.  4 integer and 6 decimal
		}
		else
		{
			m_os << "INCH,LZ,00.0000";			EndLine();	// Inches.  Leading zeros INCLUDED.  2 integer and 4 decimal
		}
		MakeDrills();
		m_os << "%";	EndLine();							// Rewind Stop.  Often used instead of M95.
		m_os << ( XNC_FORMAT ? "G05" : "G81" );	EndLine();	// Turn on drill
		m_os << "G90";	EndLine();							// Absolute mode
	}
	else
	{
		Comment("Scale: 100 percent, Rotated: No, Reflected: No");
		if ( m_bMetric )
		{
			Comment("Dimensions in mm");
			Comment("Leading zeros omitted, Absolute positions, 4 integer and 6 decimal");
			m_os << "%MOMM*%"		<< std::endl;	// MOMM ==> Millimetres
			m_os << "%FSLAX46Y46*%"	<< std::endl;
		}
		else
		{
			Comment("Dimensions in inches");
			Comment("Leading zeros omitted, Absolute positions, 2 integer and 4 decimal");
			m_os << "%MOIN*%"		<< std::endl;	// MOIN ==> Inches
			m_os << "%FSLAX24Y24*%"	<< std::endl;
		}
		MakeApertures();
	}
}
void GStream::MakeDrills()
{
	assert( m_os.is_open() && m_eType == GFILE::DRL );

	int				holeDefault;	// Default hole width
	std::list<int>	holes;	  m_pBoard->GetHoleWidths_MIL(holes, holeDefault);
	const int		viahole	= m_pBoard->GetVIAHOLE_MIL();

	// Build drill list
	int code = 1;	// Start with drill T01
	for (auto& hole : holes)
		m_ePenList.push_back( GPenInfo(GPEN::PAD_HLE, hole, code++, "Pad Hole = ", hole != holeDefault) );
	if ( m_bVias )
		m_ePenList.push_back( GPenInfo(GPEN::VIA_HLE, viahole, code++, "Via Hole = ") );

	// Write drill list to file
	const bool bLZ(true);	// Include leading zeros
	for (auto& o : m_ePenList)
	{
		std::string codeStr("T");
		if ( o.m_iCode < 10 ) codeStr += "0";	// Add leading zero
		codeStr += std::to_string(o.m_iCode);

		if ( m_bMetric )
		{
			m_os << ";" << o.m_comment << MilToMM(o.m_iWidth, bLZ) << " MM";	EndLine();
			m_os << codeStr << "C" << MilToMM(o.m_iWidth, bLZ);	EndLine();
		}
		else
		{
			m_os << ";" << o.m_comment << MilToInch(o.m_iWidth, bLZ) << " INCH";	EndLine();
			m_os << codeStr << "C" << MilToInch(o.m_iWidth, bLZ);	EndLine();
		}
	}
}
void GStream::MakeApertures()	// Make "pens" for current stream
{
	assert( m_os.is_open() && m_eType != GFILE::DRL);

	int				padDefault;	// Default pad width
	std::list<int>	pads;	  m_pBoard->GetPadWidths_MIL(pads, padDefault);
	const int		via		= m_pBoard->GetVIAPAD_MIL();
	const int		trk		= m_pBoard->GetTRACK_MIL();
	const int		gap		= m_pBoard->GetGAP_MIL();
	const int		msk		= m_pBoard->GetMASK_MIL();
	const int		slk		= m_pBoard->GetSILK_MIL();
	const int		gko		= 10;	// Draw border in 10 mil pen

	// Build aperture list
	int code = 10;	// Start with aperture D10
	switch( m_eType )
	{
		case GFILE::GKO:
			m_ePenList.push_back( GPenInfo(GPEN::GKO, gko, code++, "") );
			break;
		case GFILE::GBL:
		case GFILE::GTL:
			for (auto& pad : pads)
				m_ePenList.push_back( GPenInfo(GPEN::PAD, pad, code++, " is for pads", pad != padDefault) );
			if ( m_bVias )
				m_ePenList.push_back( GPenInfo(GPEN::VIA, via, code++, " is for via-pads") );
			if ( true )
				m_ePenList.push_back( GPenInfo(GPEN::TRK, trk, code++, " is for tracks") );
			if ( !m_pBoard->GetGroundFill() ) break;
			for (auto& pad : pads)
				m_ePenList.push_back( GPenInfo(GPEN::PAD_GAP, pad + 2 * gap, code++, " is for separating pads from fill", pad != padDefault) );
			if ( m_bVias )
				m_ePenList.push_back( GPenInfo(GPEN::VIA_GAP, via + 2 * gap, code++, " is for separating via-pads from fill") );
			if ( true )
				m_ePenList.push_back( GPenInfo(GPEN::TRK_GAP, trk + 2 * gap, code++, " is for separating tracks from fill") );
			break;
		case GFILE::GBS:
		case GFILE::GTS:
			for (auto& pad : pads)
				m_ePenList.push_back( GPenInfo(GPEN::PAD_MSK, pad + 2 * msk, code++, " is for pads", pad != padDefault) );
			if ( m_bVias )
				m_ePenList.push_back( GPenInfo(GPEN::VIA_MSK, via + 2 * msk, code++, " is for via-pads") );
			break;
		case GFILE::GTO:
		case GFILE::GBO:
			m_ePenList.push_back( GPenInfo(GPEN::SLK, slk, code++, "") );
			break;
		case GFILE::DRL:	break;
	}
	// Write aperture list to file
	for (auto& o : m_ePenList)
	{
		std::string codeStr("D");
		assert(o.m_iCode >= 10);
		if ( o.m_iCode < 10 ) codeStr += "0";	// Add leading zero
		codeStr += std::to_string(o.m_iCode);

		if ( !o.m_comment.empty() )
		{
			std::string str = std::string("Aperture ") + codeStr + o.m_comment;
			Comment( str.c_str() );
		}
		if ( m_bMetric )
			m_os << "%AD" << codeStr << "C," << MilToMM(o.m_iWidth) << "*%" << std::endl;
		else
			m_os << "%AD" << codeStr << "C," << MilToInch(o.m_iWidth) << "*%" << std::endl;
	}
}
void GStream::LinearInterpolation()
{
	if ( !m_os.is_open() || m_eType == GFILE::DRL ) return;
	m_os << "G01";
	EndLine();
}
void GStream::Comment(const char* sz)
{
	if ( !m_os.is_open() ) return;
	if ( m_eType == GFILE::DRL )
		m_os << ";" << sz;
	else
		m_os << "G04 " << sz << " ";
	EndLine();
}
void GStream::EndLine()
{
	if ( !m_os.is_open() ) return;
	if ( m_eType == GFILE::DRL )
		m_os << std::endl;
	else
		m_os << "*" << std::endl;
}
bool GStream::GetOK() const
{
	if ( m_pBoard == nullptr ) return false;
	switch( m_eType )
	{
		case GFILE::GKO: return m_pBoard->GetCurrentLayer() == 0;
		case GFILE::GBL: return m_pBoard->GetCurrentLayer() == 0 || m_pBoard->GetLyrs() == 1;
		case GFILE::GBS: return m_pBoard->GetCurrentLayer() == 0 || m_pBoard->GetLyrs() == 1;
		case GFILE::GBO: return m_pBoard->GetCurrentLayer() == 0 || m_pBoard->GetLyrs() == 1;
		case GFILE::GTL: return m_pBoard->GetCurrentLayer() == 1 || m_pBoard->GetLyrs() == 1;
		case GFILE::GTS: return m_pBoard->GetCurrentLayer() == 1 || m_pBoard->GetLyrs() == 1;
		case GFILE::GTO: return m_pBoard->GetCurrentLayer() == 1 || m_pBoard->GetLyrs() == 1;
		case GFILE::DRL: return m_pBoard->GetCurrentLayer() == 0;
		default:		 return false;
	}
}
void GStream::SetPolarity(const GPOLARITY& ePolarity, bool bCheckOK)
{
	if ( !m_os.is_open() || m_ePolarity == ePolarity || m_eType == GFILE::DRL ) return;
	if ( bCheckOK && !GetOK() ) return;
	m_ePolarity = ePolarity;
	switch( m_ePolarity )
	{
		case GPOLARITY::UNKNOWN:	return;
		case GPOLARITY::DARK:		m_os << "%LPD*%" << std::endl;	return;
		case GPOLARITY::CLEAR:		m_os << "%LPC*%" << std::endl;	return;
	}
}
void GStream::Drill(const QPoint& p)
{
	if ( !GetOK() || !m_os.is_open() || m_eType != GFILE::DRL ) return;
	m_os << "X";  WriteDrillOrdinate( p.x() );
	m_os << "Y";  WriteDrillOrdinate( p.y() );
	m_os << std::endl;
}
void GStream::AddPad(const QPointF& pF, const GPEN& ePen, const int& w)		// Add to m_pads buffer for later writing to file
{
	if ( !GetOK() ) return;
	QPoint p;
	GetQPoint(pF, p);
	m_pads.push_back( new Curve(p, ePen, w) );
}
void GStream::AddViaPad(const QPointF& pF, const GPEN& ePen)	// Add to m_viapads buffer for later writing to file
{
	if ( !GetOK() ) return;
	QPoint p;
	GetQPoint(pF, p);
	m_viapads.push_back( new Curve(p, ePen) );
}
void GStream::AddTrack(const QPolygonF& pF, const GPEN& ePen)	// Add to m_tracks buffer for later writing to file
{
	if ( !GetOK() ) return;
	QPolygon p;
	GetQPolygon(pF, p);
	m_tracks.push_back( new Curve(p, ePen) );
}
void GStream::AddVariTrack(const QPolygonF& pF, const GPEN& ePenHV, const GPEN& ePen)	// Add to m_tracks buffer for later writing to file
{
	if ( !GetOK() ) return;
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
		m_tracks.push_back( new Curve(temp, bHV ? ePenHV : ePen) );
	}
}
void GStream::AddLoop(const QPolygonF& pF, const GPEN& ePen)	// Add to m_loops buffer for later writing to file
{
	if ( !GetOK() ) return;
	assert( pF.size() >= 3 );
	if ( pF.size() < 3 ) return;	// Loop must have at least 3 points
	QPolygon p;
	GetQPolygon(pF, p);
	m_loops.push_back( new Curve(p, ePen) );
}
void GStream::AddRegion(const QPolygonF& pF)	// Add to m_regions buffer for later writing to file
{
	if ( !GetOK() ) return;
	assert( pF.size() >= 3 );
	if ( pF.size() < 3 ) return;	// Region must have at least 3 points
	QPolygon p;
	GetQPolygon(pF, p);
	m_regions.push_back( new Curve(p, GPEN::NONE) );
}
void GStream::AddHole(const QPointF& pF, const GPEN& ePen, const int& w)	// Add to m_holes buffer for later writing to file
{
	if ( !GetOK() ) return;
	QPoint p;
	GetQPoint(pF, p);
	m_holes.push_back( new Curve(p, ePen, w) );
}
void GStream::ClearBuffers(bool bCheckOK)
{
	if ( bCheckOK && !GetOK() ) return;
	m_pads.Clear();
	m_viapads.Clear();
	m_tracks.Clear();
	m_loops.Clear();
	m_regions.Clear();
	m_holes.Clear();
}
void GStream::DrawBuffers()
{
	if ( !GetOK() ) return;

	m_regions.Sort();
	m_loops.Sort();
	m_tracks.SpliceAll();	// Includes a Sort().  Only tracks (not loops) are spliced
	m_pads.Sort();
	m_viapads.Sort();
	m_holes.Sort();
	for (auto& o : m_regions)	Region(*o);
	for (auto& o : m_loops)		OutLine(*o, true);	// true  ==> closed
	for (auto& o : m_tracks)	OutLine(*o, false);	// false ==> not closed
	for (auto& o : m_pads)		OutLine(*o, false);	// false ==> not closed
	for (auto& o : m_viapads)	OutLine(*o, false);	// false ==> not closed
	for (auto& o : m_holes)		OutLine(*o, false);	// false ==> not closed
}
void GStream::Region(const Curve& curve)	// A filled closed curve (with zero width pen)
{
	if ( !m_os.is_open() || m_eType == GFILE::DRL ) return;
	assert( curve.m_ePen == GPEN::NONE );
	if ( curve.size() < 3 ) return;	// Region must have >= 3 points
	m_os << "G36";	EndLine();		// "Begin region"
	OutLine(curve, true);			// true ==> force close
	m_os << "G37";	EndLine();		// "End region"
}
void GStream::OutLine(const Curve& curve, bool bForceClose)	// Outline of a curve
{
	if ( curve.empty() ) return;
	SetPen(curve.m_ePen, curve.m_width);
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
void GStream::SetPen(const GPEN& ePen, const int& w)
{
	if ( !m_os.is_open() ) return;
	assert( m_pBoard );

	int penWidth(w);
	const bool bCustom = ( penWidth != 0 );
	if ( bCustom )
	{
		switch( ePen )
		{
			case GPEN::PAD:
			case GPEN::PAD_HLE:	break;
			case GPEN::PAD_GAP:	penWidth += 2 * m_pBoard->GetGAP_MIL();		break;
			case GPEN::PAD_MSK:	penWidth += 2 * m_pBoard->GetMASK_MIL();	break;
			default:	assert(0);	// Only pads support variable width at the moment
		}
	}

	if ( m_ePen == ePen && m_penWidth == penWidth ) return;	// No change
	m_ePen		= ePen;
	m_penWidth	= penWidth;

	if ( m_ePen == GPEN::NONE ) return;

	for (auto& o : m_ePenList)
	{
		if ( o.m_ePen == ePen && ( o.m_iWidth == penWidth || ( !bCustom && !o.m_bCustom ) ) )
		{
			m_os << ( m_eType == GFILE::DRL ? "T" : "D" );
			if ( o.m_iCode < 10 ) m_os << "0";	// Add leading zero
			m_os << o.m_iCode; EndLine();
			return;
		}
	}
	assert(0);	// No such pen
}
void GStream::Flash(const QPoint& p)
{
	if ( !m_os.is_open() ) return;
	if ( m_eType == GFILE::DRL ) return Drill(p);
	WriteXY(p, FULL_LINE);
	m_os << "D03";		// Always specify D03 code
	EndLine();
}
void GStream::Move(const QPoint& p)
{
	if ( !m_os.is_open() || m_eType == GFILE::DRL ) return;
	if ( m_iLastX == p.x() && m_iLastY == p.y() ) return;
	WriteXY(p, FULL_LINE);
	m_os << "D02";		// Always specify D02 code
	EndLine();
}
void GStream::Draw(const QPoint& p)
{
	if ( !m_os.is_open() || m_eType == GFILE::DRL ) return;
	if ( m_iLastX == p.x() && m_iLastY == p.y() ) return;
	WriteXY(p, FULL_LINE);
	m_os << "D01";		// Always specify D01 code
	EndLine();
}
void GStream::Line(const QPoint& pA, const QPoint& pB)
{
	Move(pA);
	Draw(pB);
}
void GStream::WriteXY(const QPoint& p,  const bool& bFullLine)
{
	// p has deciMil units (since GRIDPIXELS == 1000 for Gerber Export)
	if ( !m_os.is_open() ) return;
	const int ix = ( m_bMetric ) ? ( 2540 * p.x() ) : p.x();	// metric ==> convert deciMil to nanometres
	const int iy = ( m_bMetric ) ? ( 2540 * p.y() ) : p.y();	// metric ==> convert deciMil to nanometres
	if ( bFullLine || m_iLastX != ix ) m_os << "X" << ix;
	if ( bFullLine || m_iLastY != iy ) m_os << "Y" << iy;
	m_iLastX = p.x();
	m_iLastY = p.y();
}
void GStream::WriteDrillOrdinate(const int& iDeciMils)
{
	if ( !m_os.is_open() || m_eType != GFILE::DRL ) return;
	m_os << ( iDeciMils >= 0 ? "+" : "-" );	// Write sign
	if ( m_bMetric )	// Writes mm in format AAAABBBBBB
	{
		// Millimetres in 4.6 format, 1 deciMil = 0000.002540 mm
		const int iAbs = abs(2540 * iDeciMils);	// Absolute value in nanometres
		if ( iAbs < 1000000000 ) m_os << "0";
		if ( iAbs <  100000000 ) m_os << "0";
		if ( iAbs <   10000000 ) m_os << "0";
		if ( iAbs <    1000000 ) m_os << "0";
		if ( iAbs <     100000 ) m_os << "0";
		if ( iAbs <      10000 ) m_os << "0";
		if ( iAbs <       1000 ) m_os << "0";
		if ( iAbs <        100 ) m_os << "0";
		if ( iAbs <         10 ) m_os << "0";
		m_os << iAbs;
	}
	else			// Writes inches in format AABBBB
	{
		// Inches in 2.4 format, 1 deciMil = 00.0001 inches
		const int iAbs = abs(iDeciMils);		// Absolute value in deciMil
		if ( iAbs < 100000 ) m_os << "0";
		if ( iAbs <  10000 ) m_os << "0";
		if ( iAbs <   1000 ) m_os << "0";
		if ( iAbs <    100 ) m_os << "0";
		if ( iAbs <     10 ) m_os << "0";
		m_os << iAbs;
	}
}
std::string GStream::MilToInch(const int& iMil, const bool& bLZ) const	// Get inches in format AA.BBBB (for defining drills and apertures)
{
	std::string str;
	const int i		 = iMil * 10;				// deciMil
	const int inches = i / 10000;
	const int remain = i - 10000 * inches;
	assert( inches < 100 && remain < 10000 );	// i.e. AA.BBBB
	if ( bLZ && inches < 10 ) str += "0";		// bLZ ==> Add leading zeros
	str += std::to_string(inches) + ".";
	if ( remain < 1000 ) str += "0";
	if ( remain < 100  ) str += "0";
	if ( remain < 10   ) str += "0";
	str += std::to_string(remain);
	return str;
}
std::string GStream::MilToMM(const int& iMil, const bool& bLZ) const	// Get mm in format AAAA.BBBBBB (for defining drills and apertures)
{
	std::string str;
	const int i		 = iMil * 25400;			// nanometres
	const int mm	 = i / 1000000;
	const int remain = i - 1000000 * mm;
	assert( mm < 10000 && remain < 1000000 );	// i.e. AAAA.BBBBBB
	if ( bLZ && mm < 1000 ) str += "0";			// bLZ ==> Add leading zeros
	if ( bLZ && mm < 100  ) str += "0";			// bLZ ==> Add leading zeros
	if ( bLZ && mm < 10   ) str += "0";			// bLZ ==> Add leading zeros
	str += std::to_string(mm) + ".";
	if ( remain < 100000 ) str += "0";
	if ( remain < 10000  ) str += "0";
	if ( remain < 1000   ) str += "0";
	if ( remain < 100    ) str += "0";
	if ( remain < 10     ) str += "0";
	str += std::to_string(remain);
	return str;
}
void GStream::GetQPoint(const QPointF& in, QPoint& out) const
{
	// GRIDPIXELS == 100 means each integer ordinate is 1 mil
	assert( m_pBoard && m_pBoard->GetGRIDPIXELS() == 1000 );	// Confirm each integer ordinate == 0.0001 inches
	const double dEdge = m_pBoard->GetEdgeWidth();	// Add/subtract this offset so bottom-left corner of board outline is at (0,0)
	out.setX( (int)(in.x()+dEdge) );
	out.setY( m_pBoard->GetGRIDPIXELS() * m_pBoard->GetRows() - (int) (in.y()-dEdge) ); // Gerber y-axis goes up screen
}
void GStream::GetQPolygon(const QPolygonF& in, QPolygon& out) const
{
	out.clear();
	out.resize( in.size() );
	for (int i = 0, iSize = in.size(); i < iSize; i++) GetQPoint(in[i], out[i]);
}

// Wrapper for handling a set of Gerber files
bool GWriter::Open(const char* fileName, const Board& board, const bool& bVias, const bool& bTwoLayerGerber, const bool& bMetric)
{
	QDateTime	local(QDateTime::currentDateTime());
	QString		UTC = local.toTimeSpec(Qt::UTC).toString(Qt::ISODate);

	// Open all Gerber files for writing
	bool bOK(fileName != nullptr);
	for (int i = 0; i < NUM_STREAMS && bOK; i++)
	{
		if ( GFILE(i) == GFILE::GTL && !bTwoLayerGerber ) continue;
		if ( GFILE(i) == GFILE::GTS && !bTwoLayerGerber ) continue;
		if ( GFILE(i) == GFILE::GBO ) continue;	// Don't write this layer yet
		bOK = m_os[i].Open(fileName, GFILE(i), bMetric, board, bVias, UTC);
	}
	if ( !bOK ) Close();
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
