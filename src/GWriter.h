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

#include "CurveList.h"

class QPointF;
class QPolygonF;
class CBoard;
class GStream;

enum class	GPOLARITY	{DARK = 0, CLEAR};
enum class	GFILE		{GKO = 0, GBL, GBS, GTL, GTS, GTO, DRL};
const int	NUM_STREAMS	= 7;

// Wrapper for a stream to a Gerber file
class GStream : public std::ofstream
{
public:
	GStream() {}
	~GStream();
	void Close();
	void Initialise(const GFILE& eType, const Board& board, const QString& UTC);
	void WriteHeader(const QString& UTC);
	void MakeApertures();
	void Drill(const QPointF& pF);
	void WriteDrillValue(const int& iMil);
	void SetPolarity(const GPOLARITY& ePolarity);
	void AddPad(const GPEN& ePen, const QPointF& pF);		// Add to m_pads buffer
	void AddTrack(const GPEN& ePen, const QPolygonF& pF);	// Add to m_tracks buffer
	void AddVariTrack(const GPEN& ePenHV, const GPEN& ePen, const QPolygonF& pF);	// Add to m_tracks buffer
	void AddLoop(const GPEN& ePen, const QPolygonF& pF);	// Add to m_loops buffer
	void AddRegion(const QPolygonF& pF);					// Add to m_regions buffer
	void ClearBuffers();
	void DrawBuffers();
	// Methods to draw things immediately
	void DrawPad(const GPEN& ePen, const QPointF& pF)		{ ClearBuffers(); AddPad(ePen, pF);		DrawBuffers(); }
	void DrawTrack(const GPEN& ePen, const QPolygonF& pF)	{ ClearBuffers(); AddTrack(ePen, pF);	DrawBuffers(); }
	void DrawLoop(const GPEN& ePen, const QPolygonF& pF)	{ ClearBuffers(); AddLoop(ePen, pF);	DrawBuffers(); }
	void DrawRegion(const QPolygonF& pF)					{ ClearBuffers(); AddRegion(pF);		DrawBuffers(); };
private:
	void SetPen(const GPEN& ePen);
	void Flash(const QPoint& p);
	void Move(const QPoint& p);
	void Draw(const QPoint& p);
	void Line(const QPoint& pA, const QPoint& pB);
	void Region(const Curve& curve)	;				// A filled curve (with zero width pen)
	void Polygon(const Curve& curve);				// Filled polygon (with non-zero width pen)
	void OutLine(const Curve& curve, bool bClose);	// Outline of a curve (can be closed)
	void WriteXY(const QPoint& p,   const bool& bFullLine);
	void GetQPoint(const QPointF& in, QPoint& out) const;		// Convert float to integer
	void GetQPolygon(const QPolygonF& in, QPolygon& out) const;	// Convert float to integer
	void LinearInterpolation();
	void Comment(const char* sz);
	void EndLine();
	// Data
	GFILE			m_eType		= GFILE::GBL;	// GKO, GBL, GBS, GTL, GTS, GTO
	GPEN			m_ePen		= GPEN::UNKNOWN;
	const Board*	m_pBoard	= nullptr;		// The board, so we can get dimensions and track sizes
	int				m_iLastX	= INT_MAX;		// Last X used
	int				m_iLastY	= INT_MAX;		// Last Y used
	// Buffers for optimising data before writing to file
	CurveList		m_pads;		// The pads and nothing else.
	CurveList		m_tracks;	// Tracks (drawn with non-zero width pen).
	CurveList		m_loops;	// Loops (drawn with non-zero width pen).
	CurveList		m_regions;	// Drawn with zero width pen. For filling gaps between tracks.
};

// Wrapper for handling a set of Gerber files
class GWriter
{
public:
	GWriter()	{}
	~GWriter()	{ Close(); }
	bool		Open(const char* fileName, const Board& board);
	void		Close();
	GStream&	GetStream(const GFILE& eType);
private:
	GStream		m_os[NUM_STREAMS];	// Output file streams
};
