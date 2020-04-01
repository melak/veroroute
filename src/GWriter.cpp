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
GStream::~GStream()
{
	Close();
}
void GStream::Close()
{
	ClearBuffers(false);	// false ==> skip GetOK() checks
	if ( !m_os.is_open() ) return;
	switch( m_eType )
	{
		case GFILE::DRL:	m_os << "M30";	EndLine();	return m_os.close();	// End of program
		default:			m_os << "M00";	EndLine();							// Program stop
							m_os << "M02";	EndLine();	return m_os.close();	// End of file
	}
}
bool GStream::Open(const char* fileName, const GFILE& eType, const Board& board, const QString& UTC)
{
	m_eType	 = eType;
	m_ePen	 = GPEN::UNKNOWN;
	m_pBoard = &board;
	m_iLastX = INT_MAX;
	m_iLastY = INT_MAX;

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

	ClearBuffers(false);	// false ==> skip GetOK() checks
	WriteHeader(UTC);
	MakeApertures();
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
	std::string	strGen		= std::string("Gerber Generator version 0.3");
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
		const int viahole	= m_pBoard->GetVIAHOLE_PERCENT();

		m_os << "M48";				EndLine();	// M48 is start of header
		m_os << "INCH,LZ,00.0000";	EndLine();	// Inches.  Leading zeros INCLUDED.  2 integer and 4 decimal

		// Comment about hole size:		";Holesize 1 = 0.032 INCH"
		// Define Tool 1:				"T01C0.032" ==> 0.032 inch diameter

		m_os << ";Holesize 1 = " << MilToInch(hole) << " INCH";	EndLine();
		m_os << "T01C" << MilToInch(hole);		EndLine();

		m_os << ";Holesize 2 = " << MilToInch(viahole) << " INCH";	EndLine();
		m_os << "T02C" << MilToInch(viahole);	EndLine();

	//	m_os << "M95";	EndLine();							// M95 End of the header
		m_os << "%";	EndLine();							// Rewind Stop.  Often used instead of M95.
		m_os << ( XNC_FORMAT ? "G05" : "G81" );	EndLine();	// Turn on drill
		m_os << "G90";	EndLine();							// Absolute mode
	}
	else
	{
		Comment("Scale: 100 percent, Rotated: No, Reflected: No");
		Comment("Dimensions in inches");
		Comment("Leading zeros omitted, Absolute positions, 2 integer and 4 decimal");
		m_os << "%FSLAX24Y24*%"	<< std::endl;
		m_os << "%MOIN*%"		<< std::endl;	// MOIN/MOCM ==> Inches/cm
		m_os << "G90";		EndLine();			// G90/G91   ==> Absolute/relative coords
		m_os << "G70D02";	EndLine();			// G70/G71   ==> in/mm
	}
}
void GStream::MakeApertures()	// Make "pens" for current stream
{
	if ( !m_os.is_open() ) return;

	const int pad		= m_pBoard->GetPAD_PERCENT();
	const int via		= m_pBoard->GetVIAPAD_PERCENT();
	const int track		= m_pBoard->GetTRACK_PERCENT();
	const int gap		= m_pBoard->GetGAP_PERCENT();
	const int mask		= m_pBoard->GetMASK_PERCENT();
	const int silk		= m_pBoard->GetSILK_PERCENT();
	const int padgap	= pad	+ 2 * gap;	// Gap  is the radius increase
	const int viagap	= via	+ 2 * gap;	// Gap  is the radius increase
	const int trackgap	= track	+ 2 * gap;	// Gap  is the radius increase
	const int padmask	= pad	+ 2 * mask;	// Mask is the radius increase
	const int viamask	= via	+ 2 * mask;	// Mask is the radius increase

	switch( m_eType )
	{
		case GFILE::GKO:
			m_os << "%ADD10C," << MilToInch(10)			<< "*%" << std::endl;	// D10 ==> GPEN::MIL10
			break;
		case GFILE::GBL:
		case GFILE::GTL:
			m_os << "%ADD11C," << MilToInch(pad)		<< "*%" << std::endl;	// D11 ==> GPEN::PAD
			m_os << "%ADD12C," << MilToInch(via)		<< "*%" << std::endl;	// D12 ==> GPEN::VIA
			m_os << "%ADD13C," << MilToInch(track)		<< "*%" << std::endl;	// D13 ==> GPEN::TRACK
			m_os << "%ADD14C," << MilToInch(padgap)		<< "*%" << std::endl;	// D14 ==> GPEN::PAD_GAP
			m_os << "%ADD15C," << MilToInch(viagap)		<< "*%" << std::endl;	// D15 ==> GPEN::VIA_GAP
			m_os << "%ADD16C," << MilToInch(trackgap)	<< "*%" << std::endl;	// D16 ==> GPEN::TRACK_GAP
			break;
		case GFILE::GBS:
		case GFILE::GTS:
			m_os << "%ADD17C," << MilToInch(padmask)	<< "*%" << std::endl;	// D17 ==> GPEN::PAD_MASK
			m_os << "%ADD18C," << MilToInch(viamask)	<< "*%" << std::endl;	// D18 ==> GPEN::VIA_MASK
			break;
		case GFILE::GTO:
		case GFILE::GBO:
			m_os << "%ADD19C," << MilToInch(silk)		<< "*%" << std::endl;	// D19 ==> GPEN::SILK
			break;
		case GFILE::DRL:
			break;
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
	m_os << "X";  WriteDrillValue( p.x() );
	m_os << "Y";  WriteDrillValue( p.y() );
	m_os << std::endl;
}
void GStream::AddPad(const GPEN& ePen, const QPointF& pF)		// Add to m_pads buffer for later writing to file
{
	if ( !GetOK() ) return;
	QPoint p;
	GetQPoint(pF, p);
	m_pads.push_back( new Curve(ePen, p) );
}
void GStream::AddViaPad(const GPEN& ePen, const QPointF& pF)	// Add to m_viapads buffer for later writing to file
{
	if ( !GetOK() ) return;
	QPoint p;
	GetQPoint(pF, p);
	m_viapads.push_back( new Curve(ePen, p) );
}
void GStream::AddTrack(const GPEN& ePen, const QPolygonF& pF)	// Add to m_tracks buffer for later writing to file
{
	if ( !GetOK() ) return;
	QPolygon p;
	GetQPolygon(pF, p);
	m_tracks.push_back( new Curve(ePen, p) );
}
void GStream::AddVariTrack(const GPEN& ePenHV, const GPEN& ePen, const QPolygonF& pF)	// Add to m_tracks buffer for later writing to file
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
		m_tracks.push_back( new Curve(bHV ? ePenHV : ePen, temp) );
	}
}
void GStream::AddLoop(const GPEN& ePen, const QPolygonF& pF)	// Add to m_loops buffer for later writing to file
{
	if ( !GetOK() ) return;
	assert( pF.size() >= 3 );
	if ( pF.size() < 3 ) return;	// Loop must have at least 3 points
	QPolygon p;
	GetQPolygon(pF, p);
	m_loops.push_back( new Curve(ePen, p) );
}
void GStream::AddRegion(const QPolygonF& pF)	// Add to m_regions buffer for later writing to file
{
	if ( !GetOK() ) return;
	assert( pF.size() >= 3 );
	if ( pF.size() < 3 ) return;	// Region must have at least 3 points
	QPolygon p;
	GetQPolygon(pF, p);
	m_regions.push_back( new Curve(GPEN::UNKNOWN, p) );
}
void GStream::AddPadHole(const GPEN& ePen, const QPointF& pF)	// Add to m_padholes buffer for later writing to file
{
	if ( !GetOK() ) return;
	QPoint p;
	GetQPoint(pF, p);
	m_padholes.push_back( new Curve(ePen, p) );
}
void GStream::AddViaHole(const GPEN& ePen, const QPointF& pF)	// Add to m_viaholes buffer for later writing to file
{
	if ( !GetOK() ) return;
	QPoint p;
	GetQPoint(pF, p);
	m_viaholes.push_back( new Curve(ePen, p) );
}
void GStream::ClearBuffers(bool bCheckOK)
{
	if ( bCheckOK && !GetOK() ) return;
	m_pads.Clear();
	m_viapads.Clear();
	m_tracks.Clear();
	m_loops.Clear();
	m_regions.Clear();
	m_padholes.Clear();
	m_viaholes.Clear();
}
void GStream::DrawBuffers()
{
	if ( !GetOK() ) return;
	m_tracks.SpliceAll();	// Only tracks (not loops) are spliced
	for (auto& o : m_regions)	Region(*o);
	for (auto& o : m_loops)		OutLine(*o, true);	// true  ==> closed
	for (auto& o : m_tracks)	OutLine(*o, false);	// false ==> not closed
	for (auto& o : m_pads)		OutLine(*o, false);	// false ==> not closed
	for (auto& o : m_viapads)	OutLine(*o, false);	// false ==> not closed
	for (auto& o : m_padholes)	OutLine(*o, false);	// false ==> not closed
	for (auto& o : m_viaholes)	OutLine(*o, false);	// false ==> not closed
}
void GStream::Region(const Curve& curve)	// A filled closed curve (with zero width pen)
{
	if ( !m_os.is_open() || m_eType == GFILE::DRL ) return;
	assert( curve.m_pen == GPEN::UNKNOWN );
	if ( curve.size() < 3 ) return;	// Region must have >= 3 points
	m_os << "G36";	EndLine();		// "Begin region"
	OutLine(curve, true);			// true ==> force close
	m_os << "G37";	EndLine();		// "End region"
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
	if ( !m_os.is_open() || m_ePen == ePen ) return;
	m_ePen = ePen;
	switch( m_ePen )
	{
		case GPEN::UNKNOWN:		return;
		case GPEN::MIL10:		m_os << "D10"; EndLine(); return;
		case GPEN::PAD:			m_os << "D11"; EndLine(); return;
		case GPEN::VIA:			m_os << "D12"; EndLine(); return;
		case GPEN::TRACK:		m_os << "D13"; EndLine(); return;
		case GPEN::PAD_GAP:		m_os << "D14"; EndLine(); return;
		case GPEN::VIA_GAP:		m_os << "D15"; EndLine(); return;
		case GPEN::TRACK_GAP:	m_os << "D16"; EndLine(); return;
		case GPEN::PAD_MASK:	m_os << "D17"; EndLine(); return;
		case GPEN::VIA_MASK:	m_os << "D18"; EndLine(); return;
		case GPEN::SILK:		m_os << "D19"; EndLine(); return;
		case GPEN::PAD_HOLE:	m_os << "T01"; EndLine(); return;
		case GPEN::VIA_HOLE:	m_os << "T02"; EndLine(); return;
	}
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
	const int& ix = p.x();
	const int& iy = p.y();
	if ( m_iLastX == ix && m_iLastY == iy ) return;
	WriteXY(p, FULL_LINE);
	m_os << "D02";		// Always specify D02 code
	EndLine();
}
void GStream::Draw(const QPoint& p)
{
	if ( !m_os.is_open() || m_eType == GFILE::DRL ) return;
	const int& ix = p.x();
	const int& iy = p.y();
	if ( m_iLastX == ix && m_iLastY == iy ) return;
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
	if ( !m_os.is_open() ) return;
	const int& ix = p.x();
	const int& iy = p.y();
	if ( bFullLine || m_iLastX != ix ) m_os << "X" << ix;
	m_iLastX = ix;
	if ( bFullLine || m_iLastY != iy ) m_os << "Y" << iy;
	m_iLastY = iy;
}
void GStream::WriteDrillValue(const int& iMil)
{
	if ( !m_os.is_open() || m_eType != GFILE::DRL ) return;
	const int iAbs = abs(iMil);
	assert( iMil > 0 );	// All veroRoute grid points are >= 0
	m_os << ( iMil >= 0 ? "+" : "-" );
	if ( iAbs < 100000 ) m_os << "0";
	if ( iAbs <  10000 ) m_os << "0";
	if ( iAbs <   1000 ) m_os << "0";
	if ( iAbs <    100 ) m_os << "0";
	if ( iAbs <     10 ) m_os << "0";
	m_os << iAbs;
}
void GStream::GetQPoint(const QPointF& in, QPoint& out) const
{
	if ( m_pBoard == nullptr ) { out = QPoint(0,0); return; }
	out.setX( (int) in.x() );
	out.setY( m_pBoard->GetGRIDPIXELS() * m_pBoard->GetRows() - (int) in.y() ); // Gerber y-axis goes up screen
}
void GStream::GetQPolygon(const QPolygonF& in, QPolygon& out) const
{
	out.clear();
	out.resize( in.size() );
	for (auto i = 0; i < in.size(); i++) GetQPoint(in[i], out[i]);
}
std::string GStream::MilToInch(const int& iMil) const	// Just for pen sizes
{
	assert(iMil >= 0);
	std::string str;
	const int inches = iMil / 1000;
	const int remain = iMil - 1000 * inches;
	str += std::to_string(inches) + ".";
	if ( remain < 100 ) str += "0";
	if ( remain < 10  ) str += "0";
	str += std::to_string(remain);
	return str;
}

// Wrapper for handling a set of Gerber files
bool GWriter::Open(const char* fileName, const Board& board, const bool& bTwoLayerGerber)
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
		bOK = m_os[i].Open(fileName, GFILE(i), board, UTC);
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
