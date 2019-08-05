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

#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::DestroyPixmapCache()
{
	if ( m_ppPixmapPad )	for (int i = 0; i <     NUM_PIXMAP_COLORS; i++) delete m_ppPixmapPad[i];	delete[] m_ppPixmapPad;		m_ppPixmapPad	= nullptr;
	if ( m_ppPixmapDiag )	for (int i = 0; i < 2 * NUM_PIXMAP_COLORS; i++) delete m_ppPixmapDiag[i];	delete[] m_ppPixmapDiag;	m_ppPixmapDiag	= nullptr;
	if ( m_ppPixmapBlob )	for (int i = 0; i < 256; i++)					delete m_ppPixmapBlob[i];	delete[] m_ppPixmapBlob;	m_ppPixmapBlob	= nullptr;
}

void MainWindow::CreatePixmapCache(const GuiControl& guiCtrl, ColorManager& colorMgr)
{
	colorMgr.SetSaturation( guiCtrl.GetSaturation() );	// Must do this BEFORE making pixmaps

	if ( m_ppPixmapPad ) return;	// Cache exists

	grabMouse(Qt::WaitCursor);

	// The set of used connections for a grid point are represented by an 8-bit "perimeter code".
	// There are 256 possible ways for a grid point to have connections to its 8 neigbours.
	// Each way produces a different local track pattern (or "blob") for the grid point.
	// The pixmap cache provides a way of quickly mapping the "perimeter code" to a "blob" pixmap.

	const int&	W	= guiCtrl.GetGRIDPIXELS();		// Square width size in pixels
	const int	C	= W / 2;						// Half square width in pixels
	const int	D	= guiCtrl.GetHalfPadWidth();	// Half pad width in pixels
	const int	H	= (int) ceil(1.414 * guiCtrl.GetHalfTrackWidth());
	m_ppPixmapPad	= new QPixmap*[NUM_PIXMAP_COLORS];
	m_ppPixmapDiag	= new QPixmap*[2 * NUM_PIXMAP_COLORS];
	m_ppPixmapBlob	= new QPixmap*[256];

	QPainter painter;
	for (int i = 0; i < NUM_PIXMAP_COLORS; i++)
	{
		int R(0), G(0), B(0);
		colorMgr.GetPixmapRGB(i, R, G, B);
		const QColor color(R, G, B, 255);

		m_ppPixmapPad[i] = new QPixmap(2*D, 2*D);
		m_ppPixmapPad[i]->fill(Qt::transparent);

		painter.begin(m_ppPixmapPad[i]);
		PaintPad(guiCtrl, painter, color, QPointF(D,D));
		painter.end();

		for (int jDiagCode = 0; jDiagCode < 2; jDiagCode++)	// 0 ==> LT, 1 ==> RT
		{
			const int ii = i + jDiagCode * NUM_PIXMAP_COLORS;

			m_ppPixmapDiag[ii] = new QPixmap(2*H, 2*H);
			m_ppPixmapDiag[ii]->fill(Qt::transparent);

			painter.begin(m_ppPixmapDiag[ii]);
			PaintDiag(guiCtrl, painter, color, QPointF(H,H), H, jDiagCode == 0);
			painter.end();
		}
	}
	const QColor backgroundColor = GetBackgroundColor();
	for (int i = 0; i < 256; i++)	// Loop all possible perimeter codes
	{
		m_ppPixmapBlob[i] = new QPixmap(2*C, 2*C);
		m_ppPixmapBlob[i]->fill(backgroundColor);

		painter.begin(m_ppPixmapBlob[i]);
		PaintBlob(guiCtrl, painter, Qt::black, QPointF(C,C), i);
		painter.end();

		// Now turn the black blob area transparent, so we can overlay it over colored nodes.
		QBitmap mask = m_ppPixmapBlob[i]->createMaskFromColor(backgroundColor, Qt::MaskOutColor);
		m_ppPixmapBlob[i]->setMask(mask);
	}

	releaseMouse();
}

void MainWindow::PaintPad(const GuiControl& guiCtrl, QPainter& painter, const QColor& color, const QPointF& pC, const bool bGap)
{
	const int	gapWidth	= ( bGap ) ? guiCtrl.GetGapWidth() : 0;
	const int	padWidth	= ( guiCtrl.GetHalfPadWidth() + gapWidth ) << 1;	// Pad width in pixels

	static QPen	pen(Qt::black, 2, Qt::SolidLine);
	pen.setColor(color);
	pen.setJoinStyle(Qt::RoundJoin);
	pen.setCapStyle(Qt::RoundCap);
	pen.setWidth(padWidth);
	painter.setPen(pen);
	painter.setBrush(Qt::NoBrush);
	painter.drawPoint(pC);
}

void MainWindow::PaintDiag(const GuiControl& guiCtrl, QPainter& painter, const QColor& color, const QPointF& pCorner, const int& H, bool bLT)
{
	const int	trackWidth	= guiCtrl.GetHalfTrackWidth() << 1;	// Track width in pixels

	static QPen	pen(Qt::black, 2, Qt::SolidLine);
	pen.setColor(color);
	pen.setJoinStyle(Qt::RoundJoin);
	pen.setCapStyle(Qt::RoundCap);
	pen.setWidth(trackWidth);
	painter.setPen(pen);
	painter.setBrush(Qt::NoBrush);
	if ( bLT )
		painter.drawLine(pCorner + QPoint(-H,-H), pCorner + QPoint(H, H));
	else // bRT
		painter.drawLine(pCorner + QPoint(-H, H), pCorner + QPoint(H,-H));
}

void MainWindow::PaintBlob(const GuiControl& guiCtrl, QPainter& painter, const QColor& color, const QPointF& pC, const int& iPerimeterCode, const bool bGap)
{
	const bool		bMaxDiags		= ( guiCtrl.GetDiagsMode() == DIAGSMODE::MAX );
	const int&		W				= guiCtrl.GetGRIDPIXELS();	// Square width in pixels
	const int		C				= W / 2;					// Half square width in pixels
	const int		gapWidth		= ( bGap ) ? guiCtrl.GetGapWidth() : 0;
	const int		padWidth		= ( guiCtrl.GetHalfPadWidth()   + gapWidth ) << 1;	// Pad width in pixels
	const int		trackWidth		= ( guiCtrl.GetHalfTrackWidth() + gapWidth ) << 1;	// Track width in pixels
	const bool&		bCurvedTracks	= guiCtrl.GetCurvedTracks();
	QPolygonF		polygon;
	static QPen		pen(Qt::black, 2, Qt::SolidLine);
	static QBrush	brush(Qt::black,  Qt::SolidPattern);
	pen.setColor(color);
	pen.setJoinStyle(Qt::RoundJoin);
	pen.setCapStyle(Qt::RoundCap);
	brush.setColor(color);

	// Clockwise-ordered array of perimeter points around the square, starting at left...
	const QPointF p[8] = { pC+QPointF(-C,0), pC+QPointF(-C,-C), pC+QPointF(0,-C), pC+QPointF(C,-C),
						   pC+QPointF( C,0), pC+QPointF( C, C), pC+QPointF(0, C), pC+QPointF(-C,C) };
	// Clockwise-ordered array of perimeter point usage, starting at left...
	bool bUsed[8];
	for (int iNbr = 0; iNbr < 8; iNbr++) bUsed[iNbr] = ReadCodeBit(iNbr, iPerimeterCode);

	if ( bMaxDiags )	// For "max diagonals mode", force relevant corner perimeter points to be used
	{
		if ( bUsed[NBR_L] && bUsed[NBR_T] ) bUsed[NBR_LT] = true;
		if ( bUsed[NBR_R] && bUsed[NBR_T] ) bUsed[NBR_RT] = true;
		if ( bUsed[NBR_L] && bUsed[NBR_B] ) bUsed[NBR_LB] = true;
		if ( bUsed[NBR_R] && bUsed[NBR_B] ) bUsed[NBR_RB] = true;
	}

	pen.setWidth(trackWidth);
	painter.setPen(pen);
	painter.setBrush(brush);

	// Draw a polygon based on the used perimeter points
	polygon.clear();

	// Find first used perimeter point
	int iFirst(-1);
	for (int i = 0; i < 8 && iFirst == -1; i++) if ( bUsed[i] ) iFirst = i;

	if ( iFirst != -1 )
	{
		polygon << p[iFirst];	// Add first point to polygon

		int iL, iR(iFirst);		// Indexes of consecutive used perimeter points
		for (int ii = 1; ii <= 8; ii++)	// We want a full clockwise loop around the perimeter back to the start
		{
			const int jj = ( ii + iFirst ) % 8;
			if ( !bUsed[jj] ) continue;
			iL = iR;	iR = jj;	// Update iL and iR
			const int iDiff = ( 8 + iR - iL ) % 8;
			if ( iDiff == 2 || iDiff == 3 || iDiff == 5 || iDiff == 6 )	// If the path L-C-R is bent ...
			{
				if ( bCurvedTracks )
				{
					// Make an N-point Bezier curve from L to R (via the central control point C)
					const int		N = 10;
					const double	d = 1.0 / N;
					for (int i = 0; i <= N; i++)
					{
						const double t(i * d), u(1 - t);
						polygon << p[iL]*(u*u) + pC*(2.0*t*u) + p[iR]*(t*t);
					}
				}
				else
				{
					// Draw a sharp bend for L-C-R instead of a smooth curve
					polygon << p[iL] << pC << p[iR];
				}
			}
			else if ( iR != iFirst )	// L-C-R is not bent, so simply add "R" to the polygon if it isn't the first point
				polygon << p[iR];
		}
	}
	if ( polygon.size() < 3 ) polygon << pC;	// Add centre point if necessary
	if ( polygon.size() > 1 ) painter.drawPolygon(polygon);
	else
	{
		// Isolated node drawn as a pad
		pen.setWidth(padWidth);
		painter.setPen(pen);
		painter.drawPoint(pC);
	}

	if ( !bCurvedTracks && padWidth > trackWidth )	// Widen H and V tracks to pad width
	{
		pen.setWidth(padWidth);
		painter.setPen(pen);
		painter.setBrush(brush);
		for (int iNbr = 0; iNbr < 8; iNbr += 2)	// Loop non-diagonal perimeter points
			if ( bUsed[iNbr] ) painter.drawLine(pC, p[iNbr]);	// Draw track from centre to perimeter point
	}
}

void MainWindow::paintEvent(QPaintEvent*)
{
	if ( !m_bRepaint ) return;

	if ( m_board.GetCompEdit() )
		PaintCompDefiner();
	else
		PaintBoard();

	// Tidy up
	m_label->setPixmap(m_mainPixmap);
	m_scrollArea->setVisible(true);
	m_scrollArea->setWidgetResizable(false);
	m_label->adjustSize();

	m_bRepaint = false;
}

void MainWindow::PaintCompDefiner()	// The paint method in "component editor mode"
{
	Board&			board		= m_board;
	CompDefiner&	def			= board.GetCompDefiner();
	const PinGrid&	grid		= def.GetGrid();

	const int&		 W			= board.GetGRIDPIXELS();	// Square width in pixels
	const int		 C			= W / 2;					// Half square width in pixels
	const double	 dTextScale	= W / 24.0;					// For scaling text when zooming

	// Shift comp to near grid centre
	const int ROWS( def.GetScreenRows() );
	const int COLS( def.GetScreenCols() );

	const Rect rect(def.GetGridRowMin(), def.GetGridRowMax(), def.GetGridColMin(), def.GetGridColMax());

	// Get footprint grid centre
	double dCX(0), dCY(0);
	def.GetGridCentre(dCY, dCX);	// Footprint centre w.r.t. screen

	QPainter painter;
	const int reqWidth  = W * COLS;
	const int reqHeight = W * ROWS;
	if ( m_mainPixmap.width() != reqWidth || m_mainPixmap.height() != reqHeight )
	{
		m_mainPixmap = QPixmap(reqWidth, reqHeight);
		m_mainPixmap.setDevicePixelRatio(1.0);
	}
	painter.begin(&m_mainPixmap);	// Paint to main pixmap

	SetQuality(painter);

	const QColor backgroundColor = GetBackgroundColor();
	m_backgroundPen.setColor(backgroundColor);
	m_backgroundBrush.setColor(backgroundColor);

	painter.fillRect(m_XGRIDOFFSET, m_YGRIDOFFSET, reqWidth, reqHeight, backgroundColor);

	m_blackPen.setWidth(0);
	m_whitePen.setWidth(0);
	m_varBrush.setColor(QColor(192,192,255,128));	// Light blue
	painter.setPen(m_blackPen);
	painter.setBrush(Qt::NoBrush);

	int X(0), Y(0), L(0), R(0), T(0), B(0);

	// Draw rect around whole board area =========================================================
	int dummy;
	GetLRTB(board, 110, 0, 0, L, dummy, T, dummy);				// 110% size square
	GetLRTB(board, 110, ROWS-1, COLS-1, dummy, R, dummy, B);	// 110% size square
	painter.drawRect(L, T, R-L, B-T);

	// Draw grid points ==========================================================================
	if ( board.GetShowGrid() )
	{
		for (int j = 0; j < ROWS; j++)	for (int i = 0; i < COLS; i++)
		{
			GetXY(board, j, i, X, Y);
			painter.drawPoint(X, Y);
		}
	}

	// Draw dashed rect around footprint boundary and along central axes =========================
	GetLRTB(board, rect, L, R, T, B);
	L -= C; R += C; T -= C; B += C;
	painter.setPen(m_dashPen);
	painter.setBrush(Qt::NoBrush);
	painter.drawRect(L, T, R-L, B-T);
	painter.setPen(m_dotPen);
	painter.drawLine(0, (T+B)/2, reqWidth, (T+B)/2);
	painter.drawLine((L+R)/2, 0, (L+R)/2, reqHeight);

	// Draw pins =================================================================================
	QFont pinsFont = painter.font();	// Copy of current font
	pinsFont.setPointSize( m_board.GetTextSizePins() );
	painter.setFont(pinsFont);

	int iPinId(0);
	for (int j = 0; j < grid.GetRows(); j++) for(int i = 0; i < grid.GetCols(); i++, iPinId++)
	{
		auto p = grid.Get(j,i);
		GetXY(board, def.GetGridRowMin() + j, def.GetGridColMin() + i, X, Y );

		// Write pin labels
		painter.save();
		painter.translate(X, Y);

		painter.setPen(Qt::NoPen);
		painter.setBrush(p->GetSurface() == SURFACE_FREE ? Qt::NoBrush :
						 p->GetSurface() == SURFACE_HOLE ? m_darkBrush : m_varBrush);	//TODO Handle SURFACE_GAP, SURFACE PLUG in future
		painter.drawRect(-C,-C,W,W);
		if ( iPinId == def.GetCurrentPinId() )
		{
			painter.setPen(m_redPen);
			painter.drawEllipse(-C,-C,W,W);
		}
		if ( p->GetIsPin() )
		{
			painter.setPen(m_blackPen);
			painter.rotate(270);
			painter.scale(dTextScale, dTextScale);
			painter.drawText(0,0,0,0, Qt::TextDontClip | Qt::AlignCenter, GetDefaultPinLabel(p->GetPinIndex()).c_str());
		}
		painter.restore();
	}

	// Draw shapes ===============================================================================
	painter.save();
	painter.translate(dCX * W, dCY * W);
	m_varBrush.setColor(QColor(0,255,255,255));	// Cyan

	for (const auto& mapObj : def.GetShapes())
	{
		const Shape& s = mapObj.second;
		GetXY(board, s.GetY1(), s.GetX1(), L, T);
		GetXY(board, s.GetY2(), s.GetX2(), R, B);

		const bool bCurrentShape = ( mapObj.first == def.GetCurrentShapeId() );
		m_blackPen.setWidth( bCurrentShape ? 3 : 2 );
		painter.setPen(m_blackPen);

		switch( s.GetType() )
		{
			case SHAPE::LINE:			painter.drawLine(L, T, R, B);	break;
			case SHAPE::RECT:			painter.drawRect(L, T, R-L, B-T);	break;
			case SHAPE::ROUNDED_RECT:	painter.drawRoundedRect(L, T, R-L, B-T, 0.35 * W, 0.35 * W);	break;
			case SHAPE::ELLIPSE:		painter.drawEllipse(L, T, R-L, B-T);	break;
			case SHAPE::ARC:			painter.drawArc(L, T, R-L, B-T, s.GetA1() * 16, s.GetAlen() * 16);	break;
			case SHAPE::CHORD:			painter.drawChord(L, T, R-L, B-T, s.GetA1() * 16, s.GetAlen() * 16);	break;
			default: assert(0);	// Unhandled shape
		}

		if ( bCurrentShape )
		{
			// Show the base rect/ellipse for line/arc/chord
			painter.setPen(m_dotPen);
			switch( s.GetType() )
			{
				case SHAPE::LINE:	painter.drawRect(L, T, R-L, B-T);		break;
				case SHAPE::ARC:
				case SHAPE::CHORD:	painter.drawEllipse(L, T, R-L, B-T);	break;
				default:			break;
			}
		}
	}
	painter.restore();
	//============================================================================================

	painter.end();
}

void MainWindow::PaintBoard()	// The paint method in "circuit layout mode"
{
	Board& board = m_board;

	CompManager&	compMgr			= board.GetCompMgr();
	ColorManager&	colorMgr		= board.GetColorMgr();

	CreatePixmapCache(board, colorMgr);	// Sets color saturation, then builds pixmaps if the cache is empty

	const TRACKMODE& trackMode		= board.GetTrackMode();
	const COMPSMODE& compMode		= board.GetCompMode();
	const bool&		 bVero			= board.GetVeroTracks();
	const bool		 bDiagsOK		= ( board.GetDiagsMode() != DIAGSMODE::OFF );
	const bool		 bMinDiags		= ( board.GetDiagsMode() == DIAGSMODE::MIN );
	const bool		 bGroundFill	= !bVero && ( trackMode == TRACKMODE::MONO ) && board.GetGroundFill();
	const bool		 bPixmapCache	= !bVero && !m_bWritePDF && !bGroundFill;
	const int&		 W				= board.GetGRIDPIXELS();		// Square width in pixels
	const int		 C				= W / 2;						// Half square width in pixels
	const int		 D				= board.GetHalfPadWidth();		// Half pad width in pixels
	const int		 H				= (int) ceil(1.414 * board.GetHalfTrackWidth());
	const int		 iHalfGap		= std::max(1, W / 12);			// For vero only
	const int		 iGap			= iHalfGap + iHalfGap;			// For vero only
	const int		 iWirePenWidth	= D / 4;						// For wires with no NodeID
	const int		 iWireBoxWidth	= 3 * iWirePenWidth;			// For wires with no NodeID
	const double	 dTextScale		= ( m_bWritePDF ) ? (48.0 / W) : (W / 24.0);	// For scaling text when zooming

	if ( bVero && trackMode != TRACKMODE::OFF ) board.CalcSolder();	// Calculate positions of solder blobs for stripboard builds

	board.CalculateColors();	// Work out best way to color things

	// Get bounds to minimise looping
	int minRow, minCol, maxRow,  maxCol;
	board.GetBounds(minRow, minCol, maxRow, maxCol);

	QPainter painter;

	QPdfWriter*	pdfWriter = nullptr;
	if ( m_bWritePDF )
	{
		pdfWriter = new QPdfWriter(m_pdfFileName);
		pdfWriter->setCreator("VeroRoute");
		pdfWriter->setPageSize(QPagedPaintDevice::A4);
		pdfWriter->setPageOrientation(QPageLayout::Landscape);
		pdfWriter->setResolution(1200);
		painter.begin(pdfWriter);	// Paint to PDF file
	}
	else
	{
		const int reqWidth  = W * board.GetCols();
		const int reqHeight = W * board.GetRows();
		if ( m_mainPixmap.width() != reqWidth || m_mainPixmap.height() != reqHeight )
		{
			m_mainPixmap = QPixmap(reqWidth, reqHeight);
			m_mainPixmap.setDevicePixelRatio(1.0);
		}
		painter.begin(&m_mainPixmap);	// Paint to main pixmap
	}

	SetQuality(painter);

	if ( board.GetFlipH() )
	{
		painter.translate(2*m_XGRIDOFFSET + W * board.GetCols(), 0);
		painter.scale(-1, 1);	// Mirror L-R
	}
	if ( board.GetFlipV() )
	{
		painter.translate(0, 2*m_YGRIDOFFSET + W * board.GetRows());
		painter.scale(1, -1);	// Mirror T-B
	}

	const QColor backgroundColor = ( m_bWritePDF ) ? Qt::white : GetBackgroundColor();
	m_backgroundPen.setColor(backgroundColor);
	m_backgroundBrush.setColor(backgroundColor);

	painter.fillRect(m_XGRIDOFFSET, m_YGRIDOFFSET, W * board.GetCols(), W * board.GetRows(), bGroundFill ? Qt::black : backgroundColor);

	m_blackPen.setWidth(0);
	m_whitePen.setWidth(0);
	painter.setPen(m_blackPen);
	painter.setBrush(Qt::NoBrush);

	int X(0), Y(0), L(0), R(0), T(0), B(0), cR(0), cG(0), cB(0);

	// Draw rect around whole board area =========================================================
	int dummy;
	GetLRTB(board, 110, 0, 0, L, dummy, T, dummy);	// 110% size square
	GetLRTB(board, 110, board.GetRows()-1, board.GetCols()-1, dummy, R, dummy, B);	// 110% size square
	painter.drawRect(L, T, R-L, B-T);

	// Draw grid points ==========================================================================
	if ( board.GetShowGrid() )
	{
		for (int j = 0; j < board.GetRows(); j++)	for (int i = 0; i < board.GetCols(); i++)
		{
			const Element* pC = board.Get(j,i);
			if ( trackMode == TRACKMODE::OFF || ( !pC->GetHasPin() && pC->GetNodeId() == BAD_NODEID ) )
			{
				GetXY(board, j, i, X, Y);
				painter.drawPoint(X, Y);
			}
		}
	}

	// Draw tracks ===============================================================================
	if ( trackMode != TRACKMODE::OFF )
	{
		painter.save();

		const int numLoops = ( bPixmapCache || bGroundFill ) ? 2 : 1;
		// bGroundFill		==> 1st pass draws fat tracks in white, 2nd pass draws tracks
		// bPixmapCache 	==> 1st pass draws the pixmaps,			2nd pass fixes up diagonals
		for (int iLoop = 0; iLoop < numLoops; iLoop++)
		{
			for (int j = minRow; j <= maxRow; j++)
			for (int i = minCol; i <= maxCol; i++)
			{
				const Element*	pC				= board.Get(j,i);
				const int&		nodeId			= pC->GetNodeId();
				const int		colorId			= colorMgr.GetColorId(nodeId);
				const bool		bPin			= pC->GetHasPin();	// true ==> real pin
				const int		iPerimeterCode	= pC->GetPerimeterCode(bDiagsOK, bMinDiags);	// 0 to 255

				if ( colorId == BAD_COLORID && !pC->GetHasWire() ) continue;	// Usually don't color places with no NodeID assigned unless they are wire ends

				// Use GetPixmapRGB for pixmaps.  It can handle MY_GREY, MY_BLACK as special cases
				const bool		bInvalidColor	=  colorId == BAD_COLORID ||
												  ( trackMode == TRACKMODE::MONO && nodeId != GetCurrentNodeId() );
				const int		iEffColorId		= ( bInvalidColor ) ? MY_BLACK :
												  ( nodeId == GetCurrentNodeId() ) ? MY_GREY : ( colorId % MYNUMCOLORS );

				colorMgr.GetPixmapRGB(iEffColorId, cR, cG, cB);
				const QColor color(cR, cG, cB, 255);

				GetLRTB(board, 100, j, i, L, R, T, B);	// 100% size square
				const int X((L+R)/2), Y((T+B)/2);
				const QPointF pCentre(X,Y);

				// Common special case: Draw blank wire-ends as squares (so we can easily see them)
				if ( colorId == BAD_COLORID && pC->GetHasWire() )
				{
					QPen& wirePen = ( bGroundFill ) ? m_whitePen : m_blackPen;
					wirePen.setWidth(iWirePenWidth);
					painter.setPen(wirePen);
					painter.setBrush(Qt::NoBrush);
					painter.drawRect(X-iWireBoxWidth, Y-iWireBoxWidth, iWireBoxWidth*2, iWireBoxWidth*2);
					wirePen.setWidth(0);		
					continue;	// Next grid square
				}

				// Note that bVero, bPixmapCache, bGroundFill are mutually exclusive

				if ( bVero ) // Vero shows squares and strips with holes
				{
					assert(iLoop == 0);

					const bool bVertical = board.GetVerticalStrips();
					if ( bVertical )
					{
						L += iHalfGap;
						R -= iHalfGap;
					}
					else
					{
						T += iHalfGap;
						B -= iHalfGap;
					}
					painter.setBrush(color);
					painter.setPen(Qt::NoPen);
					painter.drawRect(L, T, R-L, B-T);
					m_backgroundPen.setWidth(std::max(1, W/4));
					painter.setPen(m_backgroundPen);
					painter.setBrush(Qt::NoBrush);
					painter.drawPoint(pCentre);

					// Emphasize strip breaks
					painter.setPen(Qt::NoPen);
					painter.setBrush(m_backgroundBrush);
					if ( bVertical )
					{
						if ( j > minRow && pC->GetNbr(NBR_T)->IsClash(nodeId) ) painter.drawRect(L, T-iHalfGap, R-L, iGap);
						if ( j < maxRow && pC->GetNbr(NBR_B)->IsClash(nodeId) ) painter.drawRect(L, B-iHalfGap, R-L, iGap);
					}
					else
					{
						if ( i > minCol && pC->GetNbr(NBR_L)->IsClash(nodeId) ) painter.drawRect(L-iHalfGap, T, iGap, B-T);
						if ( i < maxCol && pC->GetNbr(NBR_R)->IsClash(nodeId) ) painter.drawRect(R-iHalfGap, B, iGap, B-T);
					}
					continue;	// Next grid square
				}
				else if ( bPixmapCache )	// Draw track "blobs" and pads using pre-calculated pixmaps for speed
				{
					if ( iLoop == 0 )
					{
						// Draw background square first in relevant color
						painter.setPen(Qt::NoPen);
						painter.setBrush(color);
						painter.drawRect(L, T, R-L, B-T);

						// Set the area that is not in the "blob" to the background color
						painter.drawPixmap(L, T,*(m_ppPixmapBlob[iPerimeterCode]));

						// Draw pad
						if ( bPin ) painter.drawPixmap(L+C-D, T+C-D,*(m_ppPixmapPad[iEffColorId]));
					}
					else	// iLoop == 1
					{
						// Read flags for LT and RT so we can fill diagonal gaps produced on previous iLoop
						const bool bUsedLT = ReadCodeBit(NBR_LT, iPerimeterCode);
						const bool bUsedRT = ReadCodeBit(NBR_RT, iPerimeterCode);
						if ( bUsedLT ) painter.drawPixmap(L-H, T-H,*(m_ppPixmapDiag[iEffColorId]));
						if ( bUsedRT ) painter.drawPixmap(R-H, T-H,*(m_ppPixmapDiag[iEffColorId + NUM_PIXMAP_COLORS]));
					}
					continue;
				}
				else if ( bGroundFill )	// Draw track "blobs" and pads directly
				{
					if ( iLoop == 0 )
					{
						if ( nodeId != board.GetGroundNodeId() )	// Only the non-ground tracks have a "white" surround
						{
							PaintBlob(board, painter, backgroundColor, pCentre, iPerimeterCode, true);	// Draw fat "white" track blob
							if ( bPin ) PaintPad(board, painter, backgroundColor, pCentre, true);		// Draw fat "white" pad
						}
					}
					else // iLoop == 1	// Draw track "blobs" and pads directly
					{
						PaintBlob(board, painter, color, pCentre, iPerimeterCode);	// Draw track blob
						if ( bPin ) PaintPad(board, painter, color, pCentre);		// Draw pad
					}
					continue;	// Next grid square
				}
				else					// Draw track "blobs" and pads directly
				{
					assert(iLoop == 0);
					PaintBlob(board, painter, color, pCentre, iPerimeterCode);	// Draw track blob
					if ( bPin ) PaintPad(board, painter, color, pCentre);		// Draw pad
				}
			}
		}
		painter.restore();
	}

	// Draw target board area ====================================================================
	if ( m_board.GetShowTarget() && trackMode != TRACKMODE::MONO )
	{
		const int targetT = ( board.GetRows() - m_board.GetTargetRows() ) / 2;
		const int targetB = targetT + m_board.GetTargetRows() - 1;
		const int targetL = ( board.GetCols() - m_board.GetTargetCols() ) / 2;
		const int targetR = targetL + m_board.GetTargetCols() - 1;

		int l(0), r(0), t(0), b(0);
		GetXY(board, 0, 0, L, T);
		GetXY(board, board.GetRows()-1, board.GetCols()-1, R, B);
		GetXY(board, targetT, targetL, l, t);
		GetXY(board, targetB, targetR, r, b);
		L -= C; l -= C; T -= C; t -= C;
		R += C; r += C; B += C; b += C;

		painter.save();
		m_varBrush.setColor( QColor(0,128,128,32) );	// Very transparent light cyan tint
		painter.setPen(Qt::NoPen);
		painter.setBrush(m_varBrush);
		painter.drawRect(L, T, R-L, t-T);
		painter.drawRect(L, t, l-L, b-t);
		painter.drawRect(r, t, R-r, b-t);
		painter.drawRect(L, b, R-L, B-b);
		painter.restore();
	}

	// Draw hatched lines ========================================================================
	if ( trackMode != TRACKMODE::OFF )
	{
		if ( board.GetRoutingEnabled() || GetCurrentNodeId() != BAD_NODEID )
		{
			painter.save();
			m_yellowPen.setWidth(W / 8);
			m_backgroundPen.setWidth(0);
			painter.setBrush(Qt::NoBrush);
			for (int j = minRow; j <= maxRow; j++)
			for (int i = minCol; i <= maxCol; i++)
			{
				const Element* pC = board.Get(j,i);
				if ( pC->GetNodeId() == BAD_NODEID ) continue;
				GetLRTB(board, 100, j, i, L, R, T, B);	// 100% size square

				if ( pC->ReadFlagBits(AUTOSET) && !pC->ReadFlagBits(USERSET) )
				{
					painter.setPen(m_backgroundPen);
					painter.drawLine(L, T, R, B);		// Draw "\" (hatched) line
					painter.drawLine(L+C, T, R, B-C);	// Draw "\" (hatched) line
					painter.drawLine(L, T+C, R-C, B);	// Draw "\" (hatched) line
					painter.drawLine(L, B, R, T);		// Draw "/" (hatched) line
					painter.drawLine(L+C, B, R, T+C);	// Draw "/" (hatched) line
					painter.drawLine(L, B-C, R-C, T);	// Draw "/" (hatched) line
				}
				if ( pC->GetNodeId() == GetCurrentNodeId() && pC->GetMH() == BAD_MH )
				{
					painter.setPen(m_yellowPen);
					painter.drawLine(L, B, R, T);		// Draw "/" (hatched) line
				}
			}
			painter.restore();
		}
	}

	// Draw solder ===============================================================================
	if ( bVero && trackMode != TRACKMODE::OFF )
	{
		const bool bVertical = board.GetVerticalStrips();
		painter.save();
		painter.setPen(Qt::NoPen);
		painter.setBrush(m_darkBrush);
		for (int j = minRow; j <= maxRow; j++)
		for (int i = minCol; i <= maxCol; i++)
		{
			const Element* pC = board.Get(j,i);
			if ( !pC->GetSolderR() ) continue;
			GetLRTB(board, 100, j, i, L, R, T, B);	// 100% size square
			if ( bVertical )
				painter.drawEllipse(R-(W/3), (T+B)/2 - (W/6), (2*W)/3, W/3);
			else
				painter.drawEllipse((L+R)/2 - (W/6), B-(W/3), W/3, (2*W)/3);
		}
		painter.restore();
	}

	// Draw Component outlines and pins ==========================================================
	QPen& penPlaced	= ( trackMode == TRACKMODE::MONO ) ? m_lightBluePen : m_blackPen;	// For placed (non-floating) components

	if ( compMode != COMPSMODE::OFF || trackMode == TRACKMODE::MONO )	// Mono (i.e. "PCB") mode still needs pin holes drawn
	{
		QFont pinsFont = painter.font();	// Copy of current font
		pinsFont.setPointSize( m_board.GetTextSizePins() );
		painter.setFont(pinsFont);

		compMgr.CalculateWireShifts();

		for (const auto& mapObj : compMgr.GetMapIdToComp())	// Iterate components
		{
			const Component& comp			= mapObj.second;
			const COMP&		 compType		= comp.GetType();
			const char&		 compDirection	= comp.GetDirection();
			const bool		 bVia			= compType == COMP::VIA;
			const bool		 bPinLabels		= (comp.GetPinFlags() & PIN_LABELS) > 0;
			const bool		 bRectPins		= (comp.GetPinFlags() & PIN_RECT)   > 0;

			const bool	bHighlightComp = board.GetGroupMgr().GetIsUserComp( comp.GetId() );

			// Draw component pins first
			const int jComp = comp.GetRow();
			const int iComp = comp.GetCol();

			painter.save();
			if ( trackMode == TRACKMODE::MONO )
			{
				m_backgroundPen.setWidth(0);
				painter.setPen(m_backgroundPen);
				painter.setBrush(m_backgroundBrush);
			}
			else
			{
				penPlaced.setWidth(0);		// For pin labels
				m_redPen.setWidth(0);		// For pin labels
				m_darkGreyPen.setWidth(0);	// For pins
				painter.setPen(m_darkGreyPen);
				painter.setBrush(m_clearBrush);
			}

			if ( bVia )	// Vias are a special case since they don't actually have a pin !!!
			{
				if ( trackMode == TRACKMODE::MONO )  // Only draw vias as pins in MONO (i.e. PCB) mode
				{
					GetLRTB(board, board.GetHOLE_PERCENT(), jComp, iComp, L, R, T, B);
					painter.drawEllipse(L, T, R-L, B-T);	// A pin is drawn with a circle
				}
			}
			else		// Regular components/pads/wires ...
			{
				for (int jj = 0; jj < comp.GetCompRows(); jj++)
				for (int ii = 0; ii < comp.GetCompCols(); ii++)
				{
					const int j = jComp + jj;
					const int i = iComp + ii;

					const size_t iPinIndex = comp.GetCompElement(jj,ii)->GetPinIndex();
					if ( iPinIndex == BAD_PININDEX ) continue;

					if ( !comp.GetIsPlaced() )	// Color pins of floating components
					{
						const int&	nodeId	= comp.GetNodeId(iPinIndex);
						int			colorId	= colorMgr.GetColorId(nodeId);

						if ( colorId != BAD_COLORID && nodeId == GetCurrentNodeId() )
							colorId = MY_GREY;

						int cR, cG, cB;
						colorMgr.GetPixmapRGB(colorId, cR, cG, cB);

						const QColor color(cR, cG, cB, 255);
						m_varBrush.setColor(color);
						painter.setBrush(m_varBrush);
					}
					const int iPinSize = ( trackMode == TRACKMODE::MONO || comp.GetIsPlaced() ) ? board.GetHOLE_PERCENT() :
														  std::min(3*board.GetHOLE_PERCENT()/2, board.GetPAD_PERCENT());
					GetLRTB(board, iPinSize, j, i, L, R, T, B);
					// Stop pins vanishing if zoomed too far out
					if ( L == R ) { L--, R++; }
					if ( T == B ) { T--, B++; }

					if ( bPinLabels && trackMode != TRACKMODE::MONO && board.GetShowPinLabels() )
					{
						// Write pin labels
						painter.save();

						painter.translate((L+R)/2, (T+B)/2);

						// Set text orientation
						switch( compDirection )
						{
							case 'W':
							case 'E':	painter.rotate(270);	break;
						}

						// Use special alignment for DIP pin labels
						int iFlag = Qt::TextDontClip | Qt::AlignCenter;
						if ( compType == COMP::DIP || compType == COMP::DIP_RECTIFIER )
						{
							const bool bLow	= ( iPinIndex < comp.GetNumPins() / 2 );
							switch( compDirection )
							{
								case 'E':
								case 'S':	iFlag = Qt::TextDontClip | Qt::AlignVCenter | ( bLow ? Qt::AlignRight : Qt::AlignLeft );
											painter.translate(bLow ? C/2 : -C/2, 0);
											break;
								default:	iFlag = Qt::TextDontClip | Qt::AlignVCenter | ( bLow ? Qt::AlignLeft  : Qt::AlignRight );
											painter.translate(bLow ? -C/2 : C/2, 0);
							}
						}

						painter.scale(dTextScale, dTextScale);
						painter.setPen( comp.GetIsPlaced() ? penPlaced : m_redPen);
						painter.drawText(0,0,0,0, iFlag, comp.GetPinLabel(iPinIndex).c_str());
						painter.restore();
					}
					else if ( bRectPins && trackMode != TRACKMODE::MONO )	// Draw switch pins as rectangles
					{
						const int d = std::max(1, static_cast<int>(iPinSize * W * 0.005));
						switch( compDirection )
						{
							case 'W':
							case 'E':	L -= d; R += d;	break;
							case 'N':
							case 'S':	T -= d; B += d;	break;
						}
						painter.drawRect(L, T, R-L, B-T);
					}
					else
						painter.drawEllipse(L, T, R-L, B-T);	// A regular pin is drawn as a circle
				}
			}
			painter.restore();


			// Draw component outlines
			if ( compMode == COMPSMODE::OFF ) continue;	// Skip if we've forbidden them
			if ( comp.GetShapes().empty() )	continue;	// Component has no shapes assigned

			// Set pen width.  Selected component shown thicker than normal components
			if ( comp.GetIsPlaced() )
			{
				penPlaced.setWidth( bHighlightComp ? 3 : bVia ? 1 : 2 );
				painter.setPen(penPlaced);
			}
			else
			{
				m_redPen.setWidth(4);	// Make floating components stand out in red
				painter.setPen(m_redPen);
			}

			painter.setBrush(m_clearBrush);

			GetXY(board, comp, X, Y);	// Get footprint centre

			// Implement wire shift
			if ( compType == COMP::WIRE && comp.GetIsPlaced() )
			{
				if ( comp.GetCompRows() == 1 )	// Horizontal
					Y += compMgr.GetWireShift( &comp ) * 0.1 * W;
				else
					X += compMgr.GetWireShift( &comp ) * 0.1 * W;
			}

			painter.save();
			painter.translate(X, Y);	// Shape coordinates are relative to footprint centre
			switch( compDirection )
			{
				case 'W':	break;
				case 'E':	painter.rotate(180);	break;
				case 'N':	painter.rotate(90);		break;
				case 'S':	painter.rotate(270);	break;
			}
			const size_t numShapes = comp.GetNumShapes();
			for (size_t i = 0; i < numShapes; i++)
			{
				const Shape& s = comp.GetShape(i);
				switch( s.GetType() )
				{
					case SHAPE::LINE:			painter.drawLine(s.GetX1() * W, s.GetY1() * W, s.GetX2() * W, s.GetY2() * W);	break;
					case SHAPE::RECT:			painter.drawRect(s.GetX1() * W, s.GetY1() * W, s.GetXlen() * W, s.GetYlen() * W);	break;
					case SHAPE::ROUNDED_RECT:	painter.drawRoundedRect(s.GetX1() * W, s.GetY1() * W, s.GetXlen() * W, s.GetYlen() * W, 0.35 * W, 0.35 * W);	break;
					case SHAPE::ELLIPSE:		painter.drawEllipse(s.GetX1() * W, s.GetY1() * W, s.GetXlen() * W, s.GetYlen() * W);	break;
					case SHAPE::ARC:			painter.drawArc(s.GetX1() * W, s.GetY1() * W, s.GetXlen() * W, s.GetYlen() * W,	s.GetA1() * 16, s.GetAlen() * 16);	break;
					case SHAPE::CHORD:			painter.drawChord(s.GetX1() * W, s.GetY1() * W, s.GetXlen() * W, s.GetYlen() * W, s.GetA1() * 16, s.GetAlen() * 16);	break;
					default: assert(0);	// Unhandled shape
				}
			}
			painter.restore();
		}
	}

	// Draw Component Text =======================================================================
	if ( compMode != COMPSMODE::OFF )
	{
		QFont compFont = painter.font();	// Copy of current font
		compFont.setPointSize( m_board.GetTextSizeComp() );
		painter.setFont(compFont);

		m_redPen.setWidth(0);	// Use red text for floating components
		penPlaced.setWidth(0);	// Use this for placed components

		for (const auto& mapObj : compMgr.GetMapIdToComp())	// Iterate components
		{
			const Component& comp			= mapObj.second;
			const COMP&		 compType		= comp.GetType();
			const char&		 compDirection	= comp.GetDirection();
			if ( compType == COMP::VIA || compType == COMP::WIRE ) continue;

			GetXY(board, comp, X, Y);	// Get footprint centre

			X += W * 0.0625 * comp.GetLabelOffsetCol(); // Offset for text is 1/16 of a grid square
			Y += W * 0.0625 * comp.GetLabelOffsetRow(); // Offset for text is 1/16 of a grid square

			painter.save();
			painter.translate(X, Y);
			if ( compDirection == 'N' || compDirection == 'S' )
				painter.rotate(270);

			const std::string& myStr = ( compMode == COMPSMODE::NAME )  ? comp.GetNameStr() :
									   ( compMode == COMPSMODE::VALUE ) ? comp.GetValueStr() : "";
			painter.scale(dTextScale, dTextScale);
			painter.setPen( comp.GetIsPlaced() ? penPlaced : m_redPen );
			painter.drawText(0,0,0,0, Qt::AlignCenter | Qt::TextDontClip, myStr.c_str());
			painter.restore();
		}
	}

	// Draw the User-defined "trax" component ====================================================
	if ( compMode != COMPSMODE::OFF || trackMode != TRACKMODE::OFF )
	{
		Component& trax = compMgr.GetTrax();
		if ( trax.GetSize() > 0 )
		{
			if ( trax.GetIsPlaced() )
				m_varBrush.setColor( QColor(128,128,128,128) );	// light grey
			else
				m_varBrush.setColor( QColor(192,128,192,128) );	// magenta tint
			painter.setPen(Qt::NoPen);
			painter.setBrush(m_varBrush);

			const int&	rowTL		= trax.GetRow();
			const int&	colTL		= trax.GetCol();
			const int&	compCols	= trax.GetCompCols();
			const int&	compRows	= trax.GetCompRows();
			int jRow(rowTL);
			for (int j = 0; j < compRows; j++, jRow++)
			{
				int iCol(colTL);
				for (int i = 0; i < compCols; i++, iCol++)
				{
					if ( trax.GetCompElement(j, i)->ReadFlagBits(RECTSET) )
					{
						GetLRTB(board, 100, jRow, iCol, L, R, T, B);	// 100% size square
						painter.drawRect(L,T,R-L,B-T);
					}
				}
			}
		}
	}

	// Draw User-defined labels ==================================================================
	if ( board.GetShowText() )
	{
		QFont font = painter.font();	// Copy of current font
		TextManager& textMgr = m_board.GetTextMgr();
		for (const auto& mapObj : textMgr.GetMapIdToText())
		{
			const TextRect& rect = mapObj.second;
			if ( !rect.GetIsValid() ) continue;

			GetXY(board, rect.m_rowMin, rect.m_colMin, L, T);
			GetXY(board, rect.m_rowMax, rect.m_colMax, R, B);
			L -= C; T -= C; R += C; B += C;

			font.setBold( rect.GetStyle() & TEXT_BOLD );
			font.setItalic( rect.GetStyle() & TEXT_ITALIC );
			font.setUnderline( rect.GetStyle() & TEXT_UNDERLINE );
			font.setPointSize( rect.GetSize() );
			painter.setFont(font);

			m_varPen.setColor( QColor(rect.GetR(),rect.GetG(),rect.GetB(),255) );
			painter.setPen(m_varPen);
			painter.setBrush(Qt::NoBrush);
			painter.save();
			painter.translate(L, T);
			painter.scale(dTextScale, dTextScale);
			painter.drawText(0,0,(R-L)/dTextScale,(B-T)/dTextScale, Qt::TextWordWrap | rect.GetFlags(), QString::fromStdString(rect.GetStr()));
			painter.restore();

			if ( mapObj.first == GetCurrentTextId() )
			{
				painter.setPen(m_dotPen);
				painter.drawRect(L, T, R-L, B-T);
				painter.drawRect(R-C, B-C, C, C);
			}
		}
	}

	painter.end();

	delete pdfWriter;
}

void MainWindow::GetXY(const GuiControl& guiCtrl, double row, double col, int& X, int& Y) const
{
	// For rendering.
	// Takes a point in the Board and returns coordinates in the drawn image.
	const int& W = guiCtrl.GetGRIDPIXELS();	// Square width in pixels
	const int  C = W / 2;					// Half square width in pixels
	X = m_XGRIDOFFSET + C + col * W;
	Y = m_YGRIDOFFSET + C + row * W;
}

void MainWindow::GetLRTB(const GuiControl& guiCtrl, double percent, double row, double col, int& L, int& R, int& T, int& B) const
{
	// For rendering.
	// Takes a point in the Board and returns bounding box coordinates in the drawn image.
	// Setting percent to 100 will make the bounding box the same size as the grid square.
	const int& W = guiCtrl.GetGRIDPIXELS();	// Square width in pixels
	int X(0), Y(0);
	GetXY(guiCtrl, row, col, X, Y);
	const int S = ( W * 0.005 * percent );
	L = X - S;	T = Y - S;
	R = X + S;	B = Y + S;
}

void MainWindow::GetLRTB(const GuiControl& guiCtrl, const Component& comp, int& L, int& R, int& T, int& B) const
{
	// For rendering.
	// Takes a component in the Board and returns bounding box coordinates in the drawn image.
	GetXY(guiCtrl, comp.GetRow(), comp.GetCol(), L, T);
	GetXY(guiCtrl, comp.GetLastRow(), comp.GetLastCol(), R, B);
}

void MainWindow::GetLRTB(const GuiControl& guiCtrl, const Rect& rect, int& L, int& R, int& T, int& B) const
{
	// For rendering.
	// Takes a Rect returns bounding box coordinates in the drawn image.
	GetXY(guiCtrl, rect.m_rowMin, rect.m_colMin, L, T);
	GetXY(guiCtrl, rect.m_rowMax, rect.m_colMax, R, B);
}

void MainWindow::GetXY(const GuiControl& guiCtrl, const Component& comp, int& X, int& Y) const
{
	// For rendering.
	// Takes a component in the Board and returns the footprint centre in the drawn image.
	int L, R, T, B;
	GetLRTB(guiCtrl, comp, L, R, T, B);
	X = ( L + R ) / 2;
	Y = ( T + B ) / 2;
}
