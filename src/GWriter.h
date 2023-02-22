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

#include <QTextStream>
#include <QFile>
#include "CurveList.h"

class QPointF;
class QPolygonF;
class QString;
class Board;
class GStream;

enum class	GPOLARITY	{ UNKNOWN = 0, DARK, CLEAR };
enum class	GFILE		{ GKO = 0, GBL, GBS, GBO, GTL, GTS, GTO, DRL };

Q_DECL_CONSTEXPR static const int NUM_STREAMS = 8;

struct GPenInfo
{
	GPenInfo(GPEN ePen = GPEN::NONE,
			 int iWidth = 0,
			 int iCode = 0,
			 const QString& comment = "",
			 bool bCustom = false)
	: m_ePen(ePen), m_iWidth(iWidth), m_iCode(iCode), m_comment(comment), m_bCustom(bCustom) {}
	GPenInfo(const GPenInfo& o) { *this = o; }
	GPenInfo& operator=(const GPenInfo& o)
	{
		m_ePen		= o.m_ePen;
		m_iWidth	= o.m_iWidth;
		m_iCode		= o.m_iCode;
		m_comment	= o.m_comment;
		m_bCustom	= o.m_bCustom;
		return *this;
	}
	bool operator==(const GPenInfo& o) const
	{
		return m_ePen		== o.m_ePen
			&& m_iWidth		== o.m_iWidth
			&& m_iCode		== o.m_iCode
			&& m_comment	== o.m_comment
			&& m_bCustom	== o.m_bCustom;
	}
	bool operator!=(const GPenInfo& o) const
	{
		return !(*this == o);
	}
	GPEN	m_ePen;
	int		m_iWidth;
	int		m_iCode;	// Aperture/Tool code	// e.g. "10" for D10 or T10
	QString	m_comment;
	bool	m_bCustom;
};

// Wrapper for a stream to a Gerber file
class GStream
{
public:
	GStream()	{ Clear(); }
	~GStream()	{ Close(); Clear(); }
	void Clear();
	void Close();
	bool Open(const QString& fileName, GFILE eType, bool bMetric, const Board& board, bool bVias, bool bConfirmEachFile);
	void WriteHeader(const QString& UTC);
	void WriteFooter();
	void Drill(const QPoint& pF);
	void SetPolarity(GPOLARITY ePolarity, bool bCheckOK = true);
	void AddPad(const QPointF& pF, GPEN ePen, int w = 0);			// Add to m_pads buffer
	void AddViaPad(const QPointF& pF, GPEN ePen);							// Add to m_viapads buffer
	void AddTrack(const QPolygonF& pF, GPEN ePen);						// Add to m_tracks buffer
	void AddVariTrack(const QPolygonF& pF, GPEN ePenHV, GPEN ePen);	// Add to m_tracks buffer
	void AddLoop(const QPolygonF& pF, GPEN ePen);							// Add to m_loops buffer
	void AddRegion(const QPolygonF& pF);										// Add to m_regions buffer
	void AddHole(const QPointF& pF, GPEN ePen, int w = 0);			// Add to m_holes buffer
	void ClearBuffers(bool bCheckOK = true);
	void DrawBuffers();
	// Methods to draw things immediately
//	void DrawPad(const QPointF& pF, GPEN ePen, int w)	{ ClearBuffers(); AddPad(pF, ePen, w);	DrawBuffers(); }
//	void DrawTrack(const QPolygonF& pF, GPEN ePen )		{ ClearBuffers(); AddTrack(pF, ePen);	DrawBuffers(); }
	void DrawLoop(const QPolygonF& pF, GPEN ePen)		{ ClearBuffers(); AddLoop(pF,ePen);		DrawBuffers(); }
	void DrawRegion(const QPolygonF& pF)				{ ClearBuffers(); AddRegion(pF);		DrawBuffers(); }
private:
	void MakeDrills();
	void MakeApertures();
	void LinearInterpolation();
	void Comment(const QString& str);
	void EndLine();
	void QtEndline();
	bool GetOK() const;
	void SetPen(GPEN ePen, int w);
	void Flash(const QPoint& p);
	void Move(const QPoint& p);
	void Draw(const QPoint& p);
	void Line(const QPoint& pA, const QPoint& pB);
	void Region(const Curve& curve)	;						// A filled curve (with zero width pen)
	void Polygon(const Curve& curve);						// Filled polygon (with non-zero width pen)
	void OutLine(const Curve& curve, bool bClose);	// Outline of a curve (can be closed)
	void WriteXY(const QPoint& p, bool bFullLine);
	void WriteDrillOrdinate(int iDeciMils);
	void GetQPoint(const QPointF& in, QPoint& out) const;		// Convert float to integer
	void GetQPolygon(const QPolygonF& in, QPolygon& out) const;	// Convert float to integer
	QString MilToInch(int iMil, bool bLZ = false) const;
	QString MilToMM( int iMil, bool bLZ = false) const;
	// Data
	GFILE				m_eType		= GFILE::GBL;	// GKO, GBL, GBS, GTL, GTS, GTO
	GPEN				m_ePen		= GPEN::NONE;
	int					m_penWidth	= 0;
	std::list<GPenInfo>	m_ePenList;
	GPOLARITY			m_ePolarity	= GPOLARITY::UNKNOWN;
	const Board*		m_pBoard	= nullptr;		// The board, so we can get dimensions and track sizes
	bool				m_bVias		= false;		// true ==> the board has vias
	int					m_iLastX	= INT_MAX;		// Last X used
	int					m_iLastY	= INT_MAX;		// Last Y used
	bool				m_bMetric	= false;		// true ==> use mm as units instead of inches
	// Buffers for optimising data before writing to file
	CurveList			m_pads;		// Pads
	CurveList			m_viapads;	// Via pads
	CurveList			m_tracks;	// Tracks (drawn with non-zero width pen).
	CurveList			m_loops;	// Loops (drawn with non-zero width pen).
	CurveList			m_regions;	// Drawn with zero width pen. For filling gaps between tracks.
	CurveList			m_holes;	// Pad/Via holes
	// The output stream
	QFile				m_file;
	QTextStream			m_os;
};

// Wrapper for handling a set of Gerber files
class GWriter
{
public:
	GWriter()	{}
	~GWriter()	{ Close(); }
	bool		Open(const QString& fileName, const Board& board, bool bVias, bool bTwoLayerGerber, bool bMetric, bool bConfirmEachFile);
	void		Close();
	GStream&	GetStream(GFILE eType);
private:
	GStream		m_os[NUM_STREAMS];	// Output file streams
};
