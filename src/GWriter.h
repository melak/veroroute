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

class QPolygonF;
class CBoard;

enum class	GPEN		{UNKNOWN = 0, MIL10, PAD, TRACK, PAD_GAP, TRACK_GAP, PAD_MASK, RELIEF};
enum class	GPOLARITY	{DARK = 0, CLEAR};
enum class	GFILE		{GKO = 0, DRL, GBL, GBS, GTL, GTS};	//TODO Add silk screens

const int	NUM_STREAMS	= 1 + (int)(GFILE::GTS);

// Wrapper for a stream to a Gerber file
struct GStream : public std::ofstream
{
	GStream()  {}
	~GStream() {}
	void Close();
	void Initialise(const GFILE& eType, const Board& board, const QString& UTC);
	void WriteHeader(const QString& UTC);
	void MakeApertures();
	void SetPolarity(const GPOLARITY& ePolarity);
	void SetPen(const GPEN& ePen);
	void Drill(const QPointF& p);
	void WriteDrillValue(const int& iMil);
	void Flash(const QPointF& p);
	void Move(const QPointF& p);
	void Draw(const QPointF& p);
	void Line(const QPointF& pA, const QPointF& pB);
	void Rect(const QPointF& pA, const QPointF& delta);
	void RoundedRect(const QPointF& pA, const QPointF& delta, double d1);
	void Ellipse(const QPointF& pA, const QPointF& delta);
	void Arc(const QPointF& pA, const QPointF& delta, double d1, double d2);
	void Chord(const QPointF& pA, const QPointF& delta, double d1, double d2);
	void DrawRegion(const QPolygonF& polygon);	// A filled polygon (with zero width pen)
	void DrawPolygon(const QPolygonF& polygon);	// Filled polygon (with non-zero width pen)
	void DrawOutLine(const QPolygonF& polygon);	// Outline of a closed shape
private:
	void WriteXY(const QPointF& p, const bool& bFullLine);
	void LinearInterpolation();
	void Comment(const char* sz);
	void EndLine();
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
	GWriter()	{}
	~GWriter()	{ Close(); }
	bool		Open(const char* fileName, const Board& board);
	void		Close();
	GStream&	GetStream(const GFILE& eType);
private:
	GStream		m_os[NUM_STREAMS];	// Output file streams
};
