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
#include "padoffsetdialog.h"
#include "GPainter.h"
#include "SpanningTreeHelper.h"

void MainWindow::DestroyPixmapCache()
{
#ifdef USE_PIXMAP_CACHE
	if ( m_ppPixmapPad )	for (int i = 0; i <     NUM_PIXMAP_COLORS; i++)	delete m_ppPixmapPad[i];
	if ( m_ppPixmapVia )	for (int i = 0; i <     NUM_PIXMAP_COLORS; i++)	delete m_ppPixmapVia[i];
	if ( m_ppPixmapDiag )	for (int i = 0; i < 2 * NUM_PIXMAP_COLORS; i++)	delete m_ppPixmapDiag[i];
	if ( m_ppPixmapBlob )	for (int i = 0; i < 256; i++)					delete m_ppPixmapBlob[i];
	delete[] m_ppPixmapPad;		m_ppPixmapPad	= nullptr;
	delete[] m_ppPixmapVia;		m_ppPixmapVia	= nullptr;
	delete[] m_ppPixmapDiag;	m_ppPixmapDiag	= nullptr;
	delete[] m_ppPixmapBlob;	m_ppPixmapBlob	= nullptr;
	delete   m_pPixmapDiagLT;	m_pPixmapDiagLT = nullptr;
	delete   m_pPixmapDiagRT;	m_pPixmapDiagRT = nullptr;
#endif
}

#ifdef USE_PIXMAP_CACHE
void MainWindow::CreatePixmapCache(const GuiControl& guiCtrl, ColorManager& colorMgr)
{
	if ( m_ppPixmapPad ) return;	// Cache exists

	grabMouse(Qt::WaitCursor);

	// The set of used connections for a grid point are represented by an 8-bit "perimeter code".
	// There are 256 possible ways for a grid point to have connections to its 8 neigbours.
	// Each way produces a different local track pattern (or "blob") for the grid point.
	// The pixmap cache provides a way of quickly mapping the "perimeter code" to a "blob" pixmap.

	m_radPixmapPad	= guiCtrl.GetHalfPixelsFromMIL( guiCtrl.GetPAD_MIL() );		// Half pad width in pixels
	m_radPixmapVia	= guiCtrl.GetHalfPixelsFromMIL( guiCtrl.GetVIAPAD_MIL() );	// Half via width in pixels
	m_radPixmapDiag	= static_cast<int>( ceil(1.414 * guiCtrl.GetHalfPixelsFromMIL( guiCtrl.GetTRACK_MIL() )) );
	m_radPixmapBlob	= guiCtrl.GetGRIDPIXELS() >> 1;	// Half grid-square width in pixels

	m_ppPixmapPad	= new QPixmap*[NUM_PIXMAP_COLORS];
	m_ppPixmapVia	= new QPixmap*[NUM_PIXMAP_COLORS];
	m_ppPixmapDiag	= new QPixmap*[2 * NUM_PIXMAP_COLORS];
	m_ppPixmapBlob	= new QPixmap*[256];

	m_pPixmapDiagLT = new QPixmap(2*m_radPixmapDiag, 2*m_radPixmapDiag);
	m_pPixmapDiagLT->fill(Qt::transparent);
	m_pPixmapDiagRT	= new QPixmap(2*m_radPixmapDiag, 2*m_radPixmapDiag);
	m_pPixmapDiagRT->fill(Qt::transparent);

	QPainter painter;
	for (int i = 0; i < NUM_PIXMAP_COLORS; i++)
	{
		const QColor color = colorMgr.GetPixmapColor(i);

		m_ppPixmapPad[i] = new QPixmap(2*m_radPixmapPad, 2*m_radPixmapPad);
		m_ppPixmapPad[i]->fill(Qt::transparent);

		painter.begin(m_ppPixmapPad[i]);
		PaintPad(guiCtrl, painter, color, QPointF(m_radPixmapPad, m_radPixmapPad));
		painter.end();

		m_ppPixmapVia[i] = new QPixmap(2*m_radPixmapVia, 2*m_radPixmapVia);
		m_ppPixmapVia[i]->fill(Qt::transparent);

		painter.begin(m_ppPixmapVia[i]);
		PaintVia(guiCtrl, painter, color, QPointF(m_radPixmapVia, m_radPixmapVia));
		painter.end();

		for (int jDiagCode = 0; jDiagCode < 2; jDiagCode++)	// 0 ==> LT, 1 ==> RT
		{
			const int ii = i + jDiagCode * NUM_PIXMAP_COLORS;

			m_ppPixmapDiag[ii] = new QPixmap(2*m_radPixmapDiag, 2*m_radPixmapDiag);
			m_ppPixmapDiag[ii]->fill(Qt::transparent);

			painter.begin(m_ppPixmapDiag[ii]);
			PaintDiag(guiCtrl, painter, color, QPointF(m_radPixmapDiag, m_radPixmapDiag), jDiagCode == 0);
			painter.end();
		}
	}
	const QColor backgroundColor = GetBackgroundColor();
	const bool bHavePad(false), bIsGnd(false);
	const int iTagCode(0), iPadWidthMIL(0);
	for (int i = 0; i < 256; i++)	// Loop all possible perimeter codes
	{
		m_ppPixmapBlob[i] = new QPixmap(2*m_radPixmapBlob, 2*m_radPixmapBlob);
		m_ppPixmapBlob[i]->fill(backgroundColor);

		painter.begin(m_ppPixmapBlob[i]);
		const QPointF pC(m_radPixmapBlob, m_radPixmapBlob);
		PaintBlob(guiCtrl, painter, Qt::black, pC, pC, iPadWidthMIL, i, iTagCode, bHavePad, bIsGnd);
		painter.end();

		// Now turn the black blob area transparent, so we can overlay it over colored nodes.
		QBitmap mask = m_ppPixmapBlob[i]->createMaskFromColor(backgroundColor, Qt::MaskOutColor);
		m_ppPixmapBlob[i]->setMask(mask);
	}

	releaseMouse();
}

void MainWindow::PaintDiag(const GuiControl& guiCtrl, QPainter& painter, const QColor& color, const QPointF& pCorner, bool bLT)
{
	// Only use this method for pixmap based track rendering
	const int&	H			= m_radPixmapDiag;
	const int	trackWidth	= guiCtrl.GetHalfPixelsFromMIL( guiCtrl.GetTRACK_MIL() ) << 1;	// Track width in pixels

	static QPen	pen(Qt::black, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	pen.setColor(color);
	pen.setWidth(trackWidth);
	painter.setPen(pen);
	painter.setBrush(Qt::NoBrush);
	if ( bLT )
		painter.drawLine(pCorner + QPointF(-H,-H), pCorner + QPointF(H, H));
	else // bRT
		painter.drawLine(pCorner + QPointF(-H, H), pCorner + QPointF(H,-H));
}
#endif

void MainWindow::PaintViaGrey(const GuiControl& guiCtrl, QPainter& painter, const QPointF& pC)
{
	assert(!m_bWriteGerber);
	const int width = guiCtrl.GetHalfPixelsFromMIL( guiCtrl.GetVIAPAD_MIL() ) << 1;
	static QPen	pen(QColor(200,200,200,255), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	pen.setWidth(width);
	painter.setPen(pen);
	painter.setBrush(Qt::NoBrush);
	painter.drawPoint(pC);
}

void MainWindow::PaintPadGrey(const GuiControl& guiCtrl, QPainter& painter, QPen& pen, const QPointF& pC, const int& iPadWidthMIL)
{
	assert(!m_bWriteGerber);

	const int w		= ( iPadWidthMIL == 0 ) ? guiCtrl.GetPAD_MIL() : iPadWidthMIL;
	const int width	= guiCtrl.GetHalfPixelsFromMIL( w ) << 1;
	pen.setWidth(width);
	painter.setPen(pen);
	painter.setBrush(Qt::NoBrush);
	painter.drawPoint(pC);
}

void MainWindow::PaintVia(const GuiControl& guiCtrl, QPainter& painter, const QColor& color, const QPointF& pC, const bool& bGap)
{
	if ( m_bWriteGerber )
	{
		m_gWriter.GetStream(GFILE::GTL).AddViaPad(pC, bGap ? GPEN::VIA_GAP : GPEN::VIA);	// Top    copper layer
		m_gWriter.GetStream(GFILE::GBL).AddViaPad(pC, bGap ? GPEN::VIA_GAP : GPEN::VIA);	// Bottom copper layer
		if ( !bGap )
		{
			m_gWriter.GetStream(GFILE::GTS).AddViaPad(pC, GPEN::VIA_MSK);	// Top    solder mask layer
			m_gWriter.GetStream(GFILE::GBS).AddViaPad(pC, GPEN::VIA_MSK);	// Bottom solder mask layer
			m_gWriter.GetStream(GFILE::DRL).AddHole(pC,   GPEN::VIA_HLE);	// Drill file
		}
	}
	else
	{
		const int gapWidth = bGap ? guiCtrl.GetPixelsFromMIL( guiCtrl.GetGAP_MIL() ) : 0;
		const int padWidth = ( guiCtrl.GetHalfPixelsFromMIL( guiCtrl.GetVIAPAD_MIL() ) + gapWidth ) << 1;
		static QPen	pen(Qt::black, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
		pen.setColor(color);
		pen.setWidth(padWidth);
		painter.setPen(pen);
		painter.setBrush(Qt::NoBrush);
		painter.drawPoint(pC);
	}
}

void MainWindow::PaintPad(const GuiControl& guiCtrl, QPainter& painter, const QColor& color, const QPointF& pC, const int& iPadWidthMIL, const int& iHoleWidthMIL, const bool& bGap)
{
	if ( m_bWriteGerber )
	{
		m_gWriter.GetStream(GFILE::GTL).AddPad(pC, bGap ? GPEN::PAD_GAP : GPEN::PAD, iPadWidthMIL); // Top    copper layer
		m_gWriter.GetStream(GFILE::GBL).AddPad(pC, bGap ? GPEN::PAD_GAP : GPEN::PAD, iPadWidthMIL); // Bottom copper layer
		if ( !bGap )
		{
			m_gWriter.GetStream(GFILE::GTS).AddPad(pC,  GPEN::PAD_MSK, iPadWidthMIL);	// Top    solder mask layer
			m_gWriter.GetStream(GFILE::GBS).AddPad(pC,  GPEN::PAD_MSK, iPadWidthMIL);	// Bottom solder mask layer
			m_gWriter.GetStream(GFILE::DRL).AddHole(pC, GPEN::PAD_HLE, iHoleWidthMIL);	// Drill file
		}
	}
	else
	{
		const int gapWidth	= bGap ? guiCtrl.GetPixelsFromMIL( guiCtrl.GetGAP_MIL() ) : 0;
		const int w			= ( iPadWidthMIL == 0 ) ? guiCtrl.GetPAD_MIL() : iPadWidthMIL;
		const int padWidth	= ( guiCtrl.GetHalfPixelsFromMIL( w ) + gapWidth ) << 1;
		static QPen	pen(Qt::black, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
		pen.setColor(color);
		pen.setWidth(padWidth);
		painter.setPen(pen);
		painter.setBrush(Qt::NoBrush);
		painter.drawPoint(pC);
	}
}

void MainWindow::PaintBlob(const GuiControl& guiCtrl, QPainter& painter, const QColor& color, const QPointF& pC, const QPointF& pCoffset,
						   const int& iPadWidthMIL, const int& iPerimeterCode, const int& iTagCode,
						   const bool bHavePad, const bool bIsGnd, const bool bGap)
{
	std::list<MyPolygonF> polygonList;
	guiCtrl.CalcBlob(guiCtrl.GetGRIDPIXELS(), pC, pCoffset, iPadWidthMIL, iPerimeterCode, iTagCode, polygonList, bHavePad, bIsGnd, bGap);	// Populate polygonList

	if ( m_bWriteGerber )	// Write to Gerber
	{
		for (int k = 0; k < m_board.GetLyrs(); k++)
		{
			GStream& os = m_gWriter.GetStream(k == 0 ? GFILE::GBL : GFILE::GTL);	// Bottom/Top copper layer

			for (auto& polygon : polygonList)
			{
				const bool bPad	= polygon.m_ePadPen != GPEN::NONE;
				const bool bTrk	= polygon.m_eTrkPen != GPEN::NONE;
				if ( !bTrk && !bPad ) continue;

				const GPEN& ePen = bTrk ? polygon.m_eTrkPen : polygon.m_ePadPen;

				if ( polygon.m_bClosed )
				{
					os.AddLoop(polygon, ePen);			// Closed polygon outline
					if ( !bGap ) os.AddRegion(polygon);	// Only non-Gap polygon needs filling
				}
				else if ( bPad && bTrk )	// Fat tracks with diagonals
					os.AddVariTrack(polygon, polygon.m_ePadPen, polygon.m_eTrkPen);
				else
					os.AddTrack(polygon, ePen);
			}
		}
	}
	else	// Draw to pixmap
	{
		const int	gapWidth	= bGap ? guiCtrl.GetPixelsFromMIL( guiCtrl.GetGAP_MIL() ) : 0;
		const int	padWidth	= ( guiCtrl.GetHalfPixelsFromMIL( guiCtrl.GetPAD_MIL() )   + gapWidth ) << 1;	// Pad width in pixels
		const int	trackWidth	= ( guiCtrl.GetHalfPixelsFromMIL( bIsGnd ? guiCtrl.GetTAG_MIL() : guiCtrl.GetTRACK_MIL() ) + gapWidth ) << 1;	// Track width in pixels

		static QPen		pen(Qt::black, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
		static QBrush	brush(Qt::black,  Qt::SolidPattern);
		pen.setColor(color);
		brush.setColor(color);
		painter.setBrush(brush);

		for (auto& polygon : polygonList)
		{
			const bool bTrk	= polygon.m_eTrkPen != GPEN::NONE;
			const bool bPad	= polygon.m_ePadPen != GPEN::NONE;
			if ( !bTrk && !bPad ) continue;

			pen.setWidth(bTrk ? trackWidth : padWidth);
			painter.setPen(pen);

			if ( polygon.m_bClosed )
				painter.drawPolygon(polygon);
			else if ( bPad && bTrk )	// Fat tracks with diagonals
			{
				//polygon.Process();	// Not needed since m_bFatEdge already populated
				size_t i(0);
				auto iterA = polygon.begin();
				auto iterB = iterA; iterB++;
				while( iterB != polygon.end() )
				{
					pen.setWidth(polygon.m_bFatEdge[i++] ? padWidth : trackWidth);	painter.setPen(pen);
					painter.drawLine(*iterA, *iterB); ++iterA; ++iterB;
				}
			}
			else if ( polygon.size() == 1 )
				painter.drawPoint( polygon.first() );
			else
			{
				auto iterA = polygon.begin();
				auto iterB = iterA; iterB++;
				while( iterB != polygon.end() )
				{
					painter.drawLine(*iterA, *iterB); ++iterA; ++iterB;
				}
			}
		}
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

	m_scrollArea->CentreView();	// Centre the scrollbars (if needed) for component editor mode

	m_bRepaint = false;
}

void MainWindow::PaintCompDefiner()	// The paint method in "component editor mode"
{
	Board&			board		= m_board;
	CompDefiner&	def			= board.GetCompDefiner();
	const PinGrid&	grid		= def.GetGrid();
	const int&		W			= board.GetGRIDPIXELS();	// Square width in pixels
	const int		C			= W >> 1;					// Half square width in pixels
	const double	dTextScale	= W / 24.0;					// For scaling text when zooming

	// Shift comp to near grid centre
	const int ROWS(def.GetScreenRows()), COLS(def.GetScreenCols());

	const Rect rect(def.GetGridRowMin(), def.GetGridRowMax(), def.GetGridColMin(), def.GetGridColMax());

	QPainter painter;
	const int reqW(W * COLS), reqH(W * ROWS);
	if ( m_mainPixmap.width() != reqW || m_mainPixmap.height() != reqH )
	{
		m_mainPixmap = QPixmap(reqW, reqH);
		m_mainPixmap.setDevicePixelRatio(1.0);
	}
	painter.begin(&m_mainPixmap);	// Paint to main pixmap

	SetQuality(painter);

	m_XCORRECTION = m_YCORRECTION = 0;
	painter.fillRect(m_XGRIDOFFSET, m_YGRIDOFFSET, reqW, reqH, Qt::white);

	m_blackPen.setWidth(0);
	m_whitePen.setWidth(0);
	painter.setPen(m_blackPen);
	painter.setBrush(Qt::NoBrush);

	int X(0), Y(0), L(0), R(0), T(0), B(0);

	// Draw rect around whole board area =========================================================
	int dummy;
	GetLRTB(board, 110, 0, 0, L, dummy, T, dummy);				// 110% size square
	GetLRTB(board, 110, ROWS-1, COLS-1, dummy, R, dummy, B);	// 110% size square
	painter.drawRect(L, T, R-L, B-T);

	// Get footprint bounds ======================================================================
	GetLRTB(board, rect, L, R, T, B);
	L -= C; R += C; T -= C; B += C;
	const int AXIS_X = ( L + R ) / 2;
	const int AXIS_Y = ( T + B ) / 2;

	// Draw shapes ===============================================================================
	painter.save();
	painter.translate(AXIS_X, AXIS_Y);
	for (const auto& mapObj : def.GetShapes() )
	{
		const Shape& s = mapObj.second;

		if ( s.GetDrawFill() )
			m_varBrush.setColor( s.GetFillColor().GetQColor() );
		const bool bCurrentShape = ( mapObj.first == def.GetCurrentShapeId() );
		m_blackPen.setWidth( bCurrentShape ? 3 : 2 );

		painter.setPen( s.GetDrawLine() ? m_blackPen : Qt::NoPen );
		painter.setBrush( s.GetDrawFill() ? m_varBrush : Qt::NoBrush );

		painter.save();
		painter.translate( s.GetCX() * W, s.GetCY() * W );
		painter.rotate( -s.GetA3() );	// A3 > 0 ==> CCW
		const int DX	= static_cast<int>(s.GetDX() * W);
		const int DY	= static_cast<int>(s.GetDY() * W);
		const int X		= static_cast<int>(s.GetDX() * W * 0.5);
		const int Y		= static_cast<int>(s.GetDY() * W * 0.5);
		const int A		= static_cast<int>(s.GetA1() * 16);
		const int Alen	= static_cast<int>(s.GetAlen() * 16);
		switch( s.GetType() )
		{
			case SHAPE::RECT:			painter.drawRect(-X, -Y, DX, DY);	break;
			case SHAPE::ROUNDED_RECT:	painter.drawRoundedRect(-X, -Y, DX, DY, 0.35 * W, 0.35 * W);	break;
			case SHAPE::ELLIPSE:		painter.drawEllipse(-X, -Y, DX, DY);	break;
			case SHAPE::ARC:			painter.drawArc(  -X, -Y, DX, DY, A, Alen);	break;
			case SHAPE::CHORD:			painter.drawChord(-X, -Y, DX, DY, A, Alen);	break;
			default:					assert(SHAPE::LINE == s.GetType() );
										painter.drawLine(-X, -Y, X, Y);		break;
		}
		if ( bCurrentShape )
		{
			// Show the base rect/ellipse for line/arc/chord
			painter.setPen(m_dotPen);
			painter.setBrush(Qt::NoBrush);
			switch( s.GetType() )
			{
				case SHAPE::LINE:	painter.drawRect(-X, -Y, DX, DY);	break;
				case SHAPE::ARC:
				case SHAPE::CHORD:	painter.drawEllipse(-X, -Y, DX, DY);	break;
				default:			break;
			}
		}
		painter.restore();
	}
	painter.restore();
	//============================================================================================

	// Draw grid points ==========================================================================
	if ( board.GetShowGrid() )
		for (int j = 0; j < ROWS; j++)
		for (int i = 0; i < COLS; i++)
		{
			GetXY(board, j, i, X, Y);
			painter.drawPoint(X, Y);
		}

	// Draw dashed rect around footprint boundary and along central axes =========================
	painter.setPen(m_dashPen);
	painter.setBrush(Qt::NoBrush);
	painter.drawRect(L, T, R-L, B-T);
	painter.setPen(m_dotPen);
	painter.drawLine(0, AXIS_Y, reqW, AXIS_Y);
	painter.drawLine(AXIS_X, 0, AXIS_X, reqH);

	// Draw pins =================================================================================
	QFont pinsFont = painter.font();	// Copy of current font
	pinsFont.setPointSize( m_board.GetTextSizePins() );
	painter.setFont(pinsFont);

	m_varBrush.setColor(QColor(192,192,255,128));	// Light blue

	for (int iLoop = 0; iLoop < 2; iLoop++)
	{
		int iPinId(0);
		for (int j = 0, jMax = grid.GetRows(); j < jMax; j++)
		for (int i = 0, iMax = grid.GetCols(); i < iMax; i++, iPinId++)
		{
			auto p = grid.Get(0,j,i);	// Always layer 0 for component editor
			GetXY(board, def.GetGridRowMin() + j, def.GetGridColMin() + i, X, Y );

			painter.save();
			painter.translate(X, Y);

			if ( iLoop == 0 )	// Shade according to GetSurface()
			{
				painter.setPen(Qt::NoPen);
				painter.setBrush(p->GetSurface() == SURFACE_FREE ? Qt::NoBrush :
								 p->GetSurface() == SURFACE_HOLE ? m_darkBrush : m_varBrush);	//TODO Handle SURFACE_GAP, SURFACE PLUG in future
				painter.drawRect(-C,-C,W,W);
			}
			if ( iLoop == 1 )	// Draw pins/labels
			{
				if ( iPinId == def.GetCurrentPinId() )
				{
					painter.setPen(m_redPen);
					painter.drawEllipse(-C,-C,W,W);
				}
				if ( p->GetIsPin() )
				{
					painter.setPen(m_blackPen);
					painter.rotate(270);

					const size_t pinIndex = p->GetPinIndex();
					std::string strPinLabel = ( pinIndex < def.GetNumPins() ) ? def.GetPinLabel(pinIndex) : CompTypes::GetDefaultPinLabel(pinIndex);
					int iFlag = ( pinIndex < def.GetNumPins() ) ? def.GetPinAlign(pinIndex) : Qt::AlignCenter;
					if ( iFlag == Qt::AlignLeft || iFlag == Qt::AlignRight )
					{
						const bool bLeft = ( iFlag == Qt::AlignLeft );
						painter.translate(bLeft ? -C/2 : C/2, 0);
					}
					iFlag |= ( Qt::TextDontClip | Qt::AlignVCenter );

					painter.scale(dTextScale, dTextScale);
					painter.drawText(0,0,0,0, iFlag, strPinLabel.c_str());
				}
			}
			painter.restore();
		}
	}
	painter.end();
}

void MainWindow::PaintBoard()	// The paint method in "circuit layout mode"
{
//	const auto start = std::chrono::steady_clock::now();

	Board&			 board			= m_board;
	CompManager&	 compMgr		= board.GetCompMgr();
	ColorManager&	 colorMgr		= board.GetColorMgr();
	const TRACKMODE& trackMode		= board.GetTrackMode();
	const COMPSMODE& compMode		= board.GetCompMode();
	const bool&		 bVero			= board.GetVeroTracks();
	const bool		 bColor			= trackMode == TRACKMODE::COLOR;
	const bool		 bMono			= trackMode == TRACKMODE::MONO;
	const bool		 bPCB			= trackMode == TRACKMODE::PCB;
	const bool		 bMonoPCB		= bMono || bPCB;
	const bool		 bGroundFill	= !bVero && bMonoPCB && board.GetGroundFill();
	const bool		 bForceXthermal	= board.GetForce_X_Thermals();
#ifdef USE_PIXMAP_CACHE
	const bool		 bPixmapCache	= !bVero && ( bMono || bColor ) && !bGroundFill && !m_bWritePDF;	// true ==> Faster rendering (Mono/Color modes)
#else
	const bool		 bPixmapCache	= false;
#endif
	const bool		 bDirect		= !bVero && !bPixmapCache && !bGroundFill;	// true ==> Draw track "blobs" and pads directly (PDF/Gerber)
	const int&		 layer			= board.GetCurrentLayer();
	const int&		 groundNodeId	= board.GetGroundNodeId(layer);
	const bool		 bWiresAsTracks	= m_bWriteGerber && m_bTwoLayerGerber && board.GetLyrs() == 1;	// true ==> Convert wires to tracks on the top layer
	const int&		 W				= board.GetGRIDPIXELS();		// Square width in pixels
	const int		 C				= W >> 1;						// Half square width in pixels
	const int		 iHalfGap		= std::max(1, W / 12);			// For vero only
	const int		 iGap			= iHalfGap + iHalfGap;			// For vero only
	const int		 iWirePenWidth	= board.GetHalfPixelsFromMIL( board.GetPAD_MIL() ) / 4;	// For wires with no NodeID
	const int		 iWireBoxWidth	= 3 * iWirePenWidth;			// For wires with no NodeID
	const double	 dTextScale		= ( m_bWritePDF ) ? (48.0 / W) : (W / 24.0);	// For scaling text when zooming
	if ( bVero && trackMode != TRACKMODE::OFF ) board.CalcSolder();	// Calculate positions of solder blobs for stripboard builds

	int X(0), Y(0), L(0), R(0), T(0), B(0);

	const int		groundFillColorId	= ( bPCB ) ? ( layer == 0 ? MY_LYR_BOT : MY_LYR_TOP ) : MY_BLACK;
	const QColor	groundFillColor		= colorMgr.GetPixmapColor(groundFillColorId);

	QPen penGry(QColor(200,200,200,255), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	QPen penTop(penGry);	penTop.setColor( colorMgr.GetPixmapColor(MY_LYR_TOP) );
	QPen penBot(penGry);	penBot.setColor( colorMgr.GetPixmapColor(MY_LYR_BOT) );

	colorMgr.SetSaturation( board.GetSaturation() );			// Must do this BEFORE making pixmaps
	colorMgr.SetFillSaturation( board.GetFillSaturation() );	// Must do this BEFORE making pixmaps
#ifdef USE_PIXMAP_CACHE
	if ( bPixmapCache )
		CreatePixmapCache(board, colorMgr);	// Builds pixmaps if the cache is empty
#endif

	board.CalculateColors();	// Work out best way to color things

	// Pre-process component list for rendering
	std::vector<const Component*> sortedComps;
	compMgr.CalculateWireInfo();			// Work out shifts for overlaid wires and flag crossing wires
	compMgr.GetSortedComps(sortedComps);	// Sorted so floating and "plug" components get rendered last

	// Get bounds to minimise looping
	int minRow, minCol, maxRow, maxCol;
	board.GetBounds(minRow, minCol, maxRow, maxCol);

	int gndL, gndR, gndT, gndB;
	board.CalcGroundFillBounds();
	board.GetGroundFillBounds(gndL, gndR, gndT, gndB);
	const int reqW(gndR - gndL), reqH(gndB - gndT);

	m_XCORRECTION = -gndL;
	m_YCORRECTION = -gndT;

	GPainter painter;	// Works like QPainter unless you give it a GStream for Gerber

	QPdfWriter* pdfWriter = nullptr;
	if ( m_bWritePDF )
	{
		pdfWriter = new QPdfWriter(m_pdfFileName);
		pdfWriter->setCreator("VeroRoute");
		pdfWriter->setPageSize(QPageSize(QPageSize::A4));
		pdfWriter->setPageOrientation(QPageLayout::Landscape);
		pdfWriter->setResolution(1200);
		painter.begin(pdfWriter);	// Paint to PDF file
	}
	else if ( m_bWriteGerber )
	{
		assert( bPCB && !board.GetMirrored() );
		auto& os = m_gWriter.GetStream(GFILE::GTO);	// Top silk layer
		os.SetPolarity(GPOLARITY::DARK);
		os.ClearBuffers();
		painter.SetGStream(&os);
	}
	else
	{
		if ( m_mainPixmap.width() != reqW || m_mainPixmap.height() != reqH )
		{
			m_mainPixmap = QPixmap(reqW, reqH);
			m_mainPixmap.setDevicePixelRatio(1.0);
		}
		painter.begin(&m_mainPixmap);	// Paint to main pixmap
	}

	if ( !m_bWriteGerber )
		SetQuality(painter);

	if ( board.GetFlipH() )
	{
		painter.translate(2*m_XGRIDOFFSET + reqW, 0);
		painter.scale(-1, 1);	// Mirror L-R
	}
	if ( board.GetFlipV() )
	{
		painter.translate(0, 2*m_YGRIDOFFSET + reqH);
		painter.scale(1, -1);	// Mirror T-B
	}

	const QColor backgroundColor = ( m_bWritePDF ) ? Qt::white : GetBackgroundColor();
	m_backgroundPen.setColor(backgroundColor);
	m_backgroundBrush.setColor(backgroundColor);
	QPen wirePen(m_blackPen );
	wirePen.setColor(bGroundFill ? backgroundColor : groundFillColor);
	wirePen.setWidth(iWirePenWidth);

	// Draw board background
	QPolygonF edge;		// The board outline in Gerber
	QPolygonF gndPoly;	// The rectangle for ground fill
	if ( m_bWriteGerber )
	{
		// Grow board outline to guarantee separation from tracks and ground
		const double dEdge = board.GetEdgeWidth();
		const double R(reqW), B(reqH);
		const int& X = m_XGRIDOFFSET; const int& Y = m_YGRIDOFFSET;
		gndPoly << QPointF(X,     Y);		edge << QPointF(X	  - dEdge, Y	 - dEdge);
		gndPoly << QPointF(X + R, Y);		edge << QPointF(X + R + dEdge, Y	 - dEdge);
		gndPoly << QPointF(X + R, Y + B);	edge << QPointF(X + R + dEdge, Y + B + dEdge);
		gndPoly << QPointF(X,     Y + B);	edge << QPointF(X	  - dEdge, Y + B + dEdge);
		if ( bGroundFill )
		{
			m_gWriter.GetStream(GFILE::GTL).DrawRegion(gndPoly);	// Top    copper layer
			m_gWriter.GetStream(GFILE::GBL).DrawRegion(gndPoly);	// Bottom copper layer
		}
	}
	else
	{
		painter.fillRect(m_XGRIDOFFSET, m_YGRIDOFFSET, reqW, reqH, bGroundFill ? groundFillColor : backgroundColor);
	}

	// Draw rect around whole board area =========================================================
	if ( m_bWriteGerber )
	{
		m_gWriter.GetStream(GFILE::GKO).DrawLoop(edge, GPEN::GKO);	// Board outline layer
	}
	else
	{
		const int iPenWidth = ( bPCB ) ? static_cast<int>(W * 0.100) : 0;	// Like GPEN::GKO = 10 mil used for Gerber
		m_blackPen.setWidth(iPenWidth);
		m_whitePen.setWidth(iPenWidth);
		painter.setPen(GetBackgroundColor() == Qt::black ? m_whitePen : m_blackPen);
		painter.setBrush(Qt::NoBrush);

		const int dEdge = static_cast<int>( board.GetEdgeWidth() );
		painter.drawRect(m_XGRIDOFFSET - dEdge, m_YGRIDOFFSET - dEdge, reqW + (dEdge<<1), reqH + (dEdge<<1));
	}

	// Draw grid points ==========================================================================
	if ( !bPCB && board.GetShowGrid() )
		for (int j = 0, jMax = board.GetRows(); j < jMax; j++)
		for (int i = 0, iMax = board.GetCols(); i < iMax; i++)
		{
			const Element* pC = board.Get(layer, j, i);
			if ( trackMode == TRACKMODE::OFF || ( !pC->GetHasPin() && pC->GetNodeId() == BAD_NODEID ) )
			{
				GetXY(board, j, i, X, Y);
				painter.drawPoint(X, Y);
			}
		}

	// Draw tracks ===============================================================================
	if ( bPCB || trackMode != TRACKMODE::OFF )	// Force tracks in PCB mode
	{
		painter.save();
		const bool	bGreyPads	= bPCB && !m_bWriteGerber;
		const int	numLoops	= ( bPixmapCache ? 3 : ( bGroundFill ? 2 : 1 ) ) + ( bGreyPads ? 1 : 0);
		// bGroundFill		==> 1st pass draws fat tracks in white, 2nd pass draws tracks
		// bPixmapCache		==> 1st pass draws the pixmaps,			2nd pass fixes up diagonals,  3rd pass draws custom sized pads
		// bGreyPads		==> A final pass will draw the pads in grey
		for (int iLoop = 0; iLoop < numLoops; iLoop++)
		{
			const bool bLastPass = ( iLoop == numLoops - 1 );
			const bool bGap		 = bGroundFill && iLoop == 0;
			const bool bDrawGrey = ( bGreyPads && bLastPass );
			if ( m_bWriteGerber )
			{
				m_gWriter.GetStream(GFILE::GTS).ClearBuffers();	// Top solder mask layer
				m_gWriter.GetStream(GFILE::GTL).ClearBuffers();	// Top copper layer
				m_gWriter.GetStream(GFILE::GTL).SetPolarity(bGap ? GPOLARITY::CLEAR : GPOLARITY::DARK);
				m_gWriter.GetStream(GFILE::GBS).ClearBuffers();	// Bottom solder mask layer
				m_gWriter.GetStream(GFILE::GBL).ClearBuffers();	// Bottom copper layer
				m_gWriter.GetStream(GFILE::GBL).SetPolarity(bGap ? GPOLARITY::CLEAR : GPOLARITY::DARK);
				m_gWriter.GetStream(GFILE::DRL).ClearBuffers();	// Drill file
			}

			for (int j = minRow; j <= maxRow; j++)
			for (int i = minCol; i <= maxCol; i++)
			{
				const Element*	pC				= board.Get(layer, j, i);
				const int&		nodeId			= pC->GetNodeId();
				const bool		bWire			= pC->GetHasWire();
				const bool		bWireAsVia		= bWire && bWiresAsTracks;	// true ==> draw small via pad
				const int		iPerimeterCode	= board.GetPerimeterCode(pC);	// 0 to 255
				const bool		bVia			= pC->GetIsVia()  ||  bWireAsVia;
				const bool		bPad			= !bWireAsVia && pC->GetHasPin();
				assert( !(bVia && bPad) );	// Can't be both a via and a regular pad
				const bool		bIsGnd			= bGroundFill && nodeId == groundNodeId;
				const int		iTagCode		= ( bPad && bIsGnd && nodeId != BAD_NODEID ) ? board.GetTagCode(pC, iPerimeterCode) : 0;

				if ( bForceXthermal && !bPad && bIsGnd ) continue;	// Don't render ground tracks if forcing X shaped thermal reliefs

				bool	bCustomSize(false);
				int		iPadWidthMIL(0), iHoleWidthMIL(0);	// 0 ==> Not a custom size value
				uchar	layerPref(LAYER_X);

				int		padOffsetX(0), padOffsetY(0);
				if ( bPad && !bWire )
				{
					const size_t	 pinIndex	= pC->GetPinIndex();	assert(pinIndex != BAD_PININDEX);
					const int		 compId		= pC->GetCompId();
					const Component& comp		= compMgr.GetComponentById( compId );
					assert( comp.GetType() != COMP::INVALID );

					if ( !bVero )
					{
						comp.GetCompPinOffsets(pinIndex, padOffsetX, padOffsetY);	// Get offsets in mil
						padOffsetX = (padOffsetX * W) / 100;	// Convert from mil to pixels
						padOffsetY = (padOffsetY * W) / 100;	// Convert from mil to pixels
					}

					if ( board.GetLyrs() == 2 )
						layerPref = comp.GetLayerPref(pinIndex);

					bCustomSize = comp.GetCustomPads();
					if ( bCustomSize )
					{
						iPadWidthMIL	= comp.GetPadWidth();
						iHoleWidthMIL	= comp.GetHoleWidth();
					}
				}
				const bool bPadOffset	= ( padOffsetX != 0 || padOffsetY != 0 );
				const bool bBlob		= !bPadOffset || iPerimeterCode != 0;

				QPen& greyPen = ( layerPref == LAYER_X ) ? penGry :
								( layerPref == LAYER_T ) ? penTop : penBot;

				// Skip places with no NodeID assigned unless they are wire ends, or pins in Mono/PCB mode
				if ( nodeId == BAD_NODEID && !bWire && !(bMonoPCB && bPad) ) continue;

				// Use GetPixmapRGB for pixmaps.  It can handle MY_GREY, MY_BLACK as special cases
				const int	colorId				= colorMgr.GetColorId(nodeId);
				const bool	bInvalidColor		= colorId == BAD_COLORID || ( bMonoPCB && nodeId != GetCurrentNodeId() );
				const int	iEffColorId			= ( bInvalidColor )	? ( bPCB ? ( layer == 0 ? MY_LYR_BOT : MY_LYR_TOP ) : MY_BLACK )
												: ( nodeId == GetCurrentNodeId() ) ? MY_GREY : ( colorId % MYNUMCOLORS );

				const bool	 bAllowCustomColor	=	iEffColorId != MY_GREY		&&	iEffColorId != MY_BLACK
												&&	iEffColorId != MY_LYR_BOT	&&	iEffColorId != MY_LYR_TOP;
				const QColor color				= bAllowCustomColor ? colorMgr.GetColorFromNodeId(nodeId)
																	: colorMgr.GetPixmapColor(iEffColorId);

				GetLRTB(board, 100, j, i, L, R, T, B);	// 100% size square
				const int X((L+R)/2), Y((T+B)/2);
				const QPointF pCentre(X,Y);
				const QPointF pCentreOff(X+padOffsetX, Y+padOffsetY);

				// Common special case: Draw blank wire-ends as squares (so we can easily see them)
				if ( nodeId == BAD_NODEID && bWire )
				{
					painter.setPen(wirePen);
					painter.setBrush(Qt::NoBrush);
					painter.drawRect(X-iWireBoxWidth, Y-iWireBoxWidth, iWireBoxWidth*2, iWireBoxWidth*2);
					continue;	// Next grid square
				}

				// Note that bVero, bPixmapCache, bGroundFill, bDirect are mutually exclusive

				if ( bVero ) // Vero shows squares and strips with holes
				{
					if ( bPCB ) continue;	// Vero mode not compatible with PCB mode

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
				}
				if ( bPixmapCache )	// Draw track "blobs" and pads using pre-calculated pixmaps for speed
				{
#ifdef USE_PIXMAP_CACHE
					assert( !bPCB );
					const bool bCustomColor = bAllowCustomColor && colorMgr.GetIsFixed(nodeId);
					if ( iLoop == 0 )
					{
						painter.setPen(Qt::NoPen);
						painter.setBrush(color);
						// Draw blob
						if ( bBlob && !bCustomSize && !bPadOffset )		// Custom/offset stuff is rendered on last loop
						{
							// Draw background square first in relevant color
							painter.drawRect(L+C-m_radPixmapBlob, T+C-m_radPixmapBlob, m_radPixmapBlob << 1, m_radPixmapBlob << 1);

							// Set the area that is not in the "blob" to the background color
							painter.drawPixmap(L+C-m_radPixmapBlob, T+C-m_radPixmapBlob, *(m_ppPixmapBlob[iPerimeterCode]));
						}
						// Draw pad/via
						if ( bCustomColor )
						{
							if ( bVia )
								PaintVia(board, painter, color, pCentre);
							if ( bPad && !bCustomSize && !bPadOffset )	// Custom/offset stuff is rendered on last loop
								PaintPad(board, painter, color, pCentre);
						}
						else	// Non-custom color means we can use a cached pixmap
						{
							if ( bVia )
								painter.drawPixmap(L+C-m_radPixmapVia, T+C-m_radPixmapVia, *(m_ppPixmapVia[iEffColorId]));
							if ( bPad && !bCustomSize && !bPadOffset )	// Custom/offset stuff is rendered on last loop
								painter.drawPixmap(L+C-m_radPixmapPad, T+C-m_radPixmapPad, *(m_ppPixmapPad[iEffColorId]));
						}
					}
					else if ( iLoop == 1 )
					{
						// Read flags for LT and RT so we can fill diagonal gaps produced on previous iLoop
						const bool bUsedLT = ReadCodeBit(NBR_LT, iPerimeterCode);
						const bool bUsedRT = ReadCodeBit(NBR_RT, iPerimeterCode);
						if ( bUsedLT || bUsedRT )
						{
							QPixmap* pLT = bCustomColor ? m_pPixmapDiagLT : m_ppPixmapDiag[iEffColorId];
							QPixmap* pRT = bCustomColor ? m_pPixmapDiagRT : m_ppPixmapDiag[iEffColorId + NUM_PIXMAP_COLORS];
							if ( bCustomColor )
							{
								QPainter painterTmp;
								if ( bUsedLT )
								{
									painterTmp.begin(m_pPixmapDiagLT);
									PaintDiag(board, painterTmp, color, QPointF(m_radPixmapDiag, m_radPixmapDiag), true);
									painterTmp.end();
								}
								if ( bUsedRT )
								{
									painterTmp.begin(m_pPixmapDiagRT);
									PaintDiag(board, painterTmp, color, QPointF(m_radPixmapDiag, m_radPixmapDiag), false);
									painterTmp.end();
								}
							}
							if ( bUsedLT ) painter.drawPixmap(L-m_radPixmapDiag, T-m_radPixmapDiag, *pLT);
							if ( bUsedRT ) painter.drawPixmap(R-m_radPixmapDiag, T-m_radPixmapDiag, *pRT);
						}
					}
					else if (  bCustomSize || bPadOffset )	// Custom/offset stuff ...
					{
						if ( bBlob ) PaintBlob(board, painter, color, pCentre, pCentreOff, iPadWidthMIL, iPerimeterCode, iTagCode, bPad, false);
						if ( bPad )  PaintPad(board, painter, color, pCentreOff, iPadWidthMIL, iHoleWidthMIL);
					}
#endif	// USE_PIXMAP_CACHE
				}
				if ( bGroundFill )	// Draw track "blobs" and pads directly (PDF/Gerber)
				{
					if ( iLoop == 0 )
					{
						if ( !bIsGnd && bBlob )	// Only the non-ground tracks have a "white" surround
							PaintBlob(board, painter, backgroundColor, pCentre, pCentreOff, iPadWidthMIL, iPerimeterCode, iTagCode, bPad, bIsGnd, true);	// Draw fat "white" track blob
						if ( bVia ) PaintVia(board, painter, backgroundColor, pCentre, true);										// Draw fat "white" via
						if ( bPad ) PaintPad(board, painter, backgroundColor, pCentreOff, iPadWidthMIL, iHoleWidthMIL, true);		// Draw fat "white" pad
					}
					else if ( iLoop == 1 )	// Draw track "blobs" and pads directly
					{
						if ( bBlob ) PaintBlob(board, painter, color, pCentre, pCentreOff, iPadWidthMIL, iPerimeterCode, iTagCode, bPad, bIsGnd);	// Draw track blob
						if ( bVia )  PaintVia(board, painter, color, pCentre);									// Draw via same color as track
						if ( bPad )  PaintPad(board, painter, color, pCentreOff, iPadWidthMIL, iHoleWidthMIL);	// Draw pad same color as track
					}
					else if ( bDrawGrey )
					{
						if ( bVia ) PaintViaGrey(board, painter, pCentre);								// Draw grey via
						if ( bPad ) PaintPadGrey(board, painter, greyPen, pCentreOff, iPadWidthMIL);	// Draw "grey" pad
					}
				}
				if ( bDirect )	// Draw track "blobs" and pads directly (PDF/Gerber)
				{
					if ( iLoop == 0 )
					{
						if ( bBlob ) PaintBlob(board, painter, color, pCentre, pCentreOff, iPadWidthMIL, iPerimeterCode, iTagCode, bPad, false);	// Draw track blob
						if ( bVia )  PaintVia(board, painter, color, pCentre);									// Draw via same color as track
						if ( bPad )  PaintPad(board, painter, color, pCentreOff, iPadWidthMIL, iHoleWidthMIL);	// Draw pad same color as track
					}
					else if ( bDrawGrey )
					{
						if ( bVia ) PaintViaGrey(board, painter, pCentre);								// Draw grey via
						if ( bPad ) PaintPadGrey(board, painter, greyPen, pCentreOff, iPadWidthMIL);	// Draw "grey" pad
					}
				}
			}
			if ( bWiresAsTracks )
			{
				for (const auto& pComp : sortedComps)	// Iterate sorted components
				{
					const Component& comp = *pComp;
					if ( comp.GetType() != COMP::WIRE || !comp.GetIsPlaced() ) continue;	// Only want placed wires
					if ( !compMgr.GetWireCanBeTrack(&comp) ) continue;

					QPolygonF polygon;
					GetXY(board, comp.GetRow(), comp.GetCol(), X, Y);
					polygon << QPointF(X, Y);
					GetXY(board, comp.GetRow() + comp.GetCompRows() - 1, comp.GetCol() + comp.GetCompCols() - 1, X, Y);
					polygon << QPointF(X, Y);
					m_gWriter.GetStream(GFILE::GTL).AddTrack(polygon, bGap ? GPEN::TRK_GAP : GPEN::TRK);
				}
			}
			if ( m_bWriteGerber )
			{
				m_gWriter.GetStream(GFILE::GTS).DrawBuffers();	// Top solder mask layer
				m_gWriter.GetStream(GFILE::GTL).DrawBuffers();	// Top copper layer
				m_gWriter.GetStream(GFILE::GBS).DrawBuffers();	// Bottom solder mask layer
				m_gWriter.GetStream(GFILE::GBL).DrawBuffers();	// Bottom copper layer
				m_gWriter.GetStream(GFILE::DRL).DrawBuffers();	// Drill file
			}
		}	// Next iLoop
		painter.restore();
	}

	// Draw target board area ====================================================================
	if ( m_board.GetShowTarget() && !bMonoPCB )
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
	if ( !m_bWriteGerber && trackMode != TRACKMODE::OFF && board.GetRoutingEnabled() )
	{
		painter.save();
		m_redPen.setWidth(W / 8);
		m_backgroundPen.setWidth(0);
		painter.setBrush(Qt::NoBrush);
		for (int j = minRow; j <= maxRow; j++)
		for (int i = minCol; i <= maxCol; i++)
		{
			const Element* pC = board.Get(layer, j, i);
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
		}
		painter.restore();
	}

	// Draw solder ===============================================================================
	if ( ( bColor || bMono ) && bVero )
	{
		const bool bVertical = board.GetVerticalStrips();
		painter.save();
		painter.setPen(Qt::NoPen);
		painter.setBrush(m_darkBrush);
		for (int j = minRow; j <= maxRow; j++)
		for (int i = minCol; i <= maxCol; i++)
		{
			const Element* pC = board.Get(layer, j, i);
			if ( !pC->GetSolderR() ) continue;
			GetLRTB(board, 100, j, i, L, R, T, B);	// 100% size square
			if ( bVertical )
				painter.drawEllipse(R-(W/3), (T+B)/2 - (W/6), (2*W)/3, W/3);
			else
				painter.drawEllipse((L+R)/2 - (W/6), B-(W/3), W/3, (2*W)/3);
		}
		painter.restore();
	}

	// Draw vias =================================================================================
	if ( !m_bWriteGerber && !bVero && ( bPCB || trackMode != TRACKMODE::OFF ) )	// Force in PCB mode
	{
		painter.save();
		m_backgroundPen.setWidth( board.GetPixelsFromMIL( board.GetVIAHOLE_MIL() ) );
		painter.setPen(m_backgroundPen);
		painter.setBrush(Qt::NoBrush);
		for (int j = minRow; j <= maxRow; j++)
		for (int i = minCol; i <= maxCol; i++)
		{
			const Element* pC = board.Get(layer, j, i);
			if ( !pC->GetIsVia() ) continue;
			GetLRTB(board, 100, j, i, L, R, T, B);	// 100% size square
			painter.drawPoint((L+R)/2, (T+B)/2);
		}
		painter.restore();
	}

	// Draw Component outlines and pins ==========================================================
	QPen& penPlaced	= ( bMono ) ? m_lightBluePen : ( bPCB ) ? m_whitePen : m_blackPen;	// For placed (non-floating) components
	QPen  fillBlackPen = m_blackPen;	// Used for lines in the component pixmap
	fillBlackPen.setWidth(2);

	if ( compMode != COMPSMODE::OFF || bMonoPCB )	// Mono/PCB modes still need pin holes drawn for placed parts
	{
		const bool bFill = !bMonoPCB && board.GetFillSaturation() > 0;	// No component fill in Mono/PCB mode
		for (const auto& pComp : sortedComps)	// Iterate sorted components
		{
			const Component& comp			= *pComp;
			const COMP&		 compType		= comp.GetType();
			const char&		 compDirection	= comp.GetDirection();
			const bool		 bMark			= compType == COMP::MARK;
			const bool		 bWire			= compType == COMP::WIRE;
			const bool		 bVeroLabel		= compType == COMP::VERO_NUMBER || compType == COMP::VERO_LETTER;
			const bool		 bPlaced		= comp.GetIsPlaced();
			if ( bPCB && (bMark || bVeroLabel) )	continue;	// Don't show markers and vero-lables in PCB mode
			if ( m_bWriteGerber && !bPlaced ) continue;	// Don't write floating components to Gerber
			if ( bWiresAsTracks && bWire && compMgr.GetWireCanBeTrack(&comp) ) continue;
			const bool		 bFound			= compMgr.GetFound( comp.GetId() );
			const bool		 bPinLabels		= !bMonoPCB && (comp.GetPinFlags() & PIN_LABELS) > 0 && board.GetShowPinLabels();
			const bool		 bRectPins		= !bMonoPCB && (comp.GetPinFlags() & PIN_RECT) > 0;
			const bool		 bHighlightComp	= board.GetGroupMgr().GetIsUserComp( comp.GetId() );
			const bool		 bCustomSize	= comp.GetCustomPads();
			const int		 iPadWidthMIL	= bCustomSize ? comp.GetPadWidth()  : board.GetPAD_MIL();
			const int		 iHoleWidthMIL	= bCustomSize ? comp.GetHoleWidth() : board.GetHOLE_MIL();
			const bool		 blankWire		= bWire && comp.GetNodeId(0) == BAD_NODEID;

			// Begin draw component pins ---------------------------------------------------------
			if ( !m_bWriteGerber && !blankWire && ( compMode != COMPSMODE::OFF || ( bMonoPCB && bPlaced ) ) ) 	// Skip blank wires
			{
				painter.save();

				QFont pinsFont = painter.font();	// Copy of current font
				pinsFont.setPointSize( m_board.GetTextSizePins() );
				painter.setFont(pinsFont);

				if ( bMonoPCB && bPlaced )
				{
					m_backgroundPen.setWidth( board.GetPixelsFromMIL(iHoleWidthMIL) );
					painter.setPen(m_backgroundPen);
				}
				else
				{
					penPlaced.setWidth(0);		// For pin labels
					m_orangePen.setWidth(0);	// For pin labels and pins
					m_redPen.setWidth(0);		// For pin labels and pins
					m_greyPen.setWidth(0);		// For pins
					painter.setPen(bFound ? m_orangePen : bPlaced ? m_greyPen : m_redPen);
				}
				painter.setBrush(Qt::NoBrush);

				if ( bMark )	// Markers are a special case since they don't actually have a pin !!!
				{
					// Only draw markers as holes in ground fill MONO mode and only in places with no track
					if ( bMono && bGroundFill && board.Get(layer, comp.GetRow(), comp.GetCol())->GetNodeId() == BAD_NODEID )
					{
						GetLRTB(board, 100, comp.GetRow(), comp.GetCol(), L, R, T, B);	// 100% size square
						painter.drawPoint((L+R)/2, (T+B)/2);							// A pin is drawn with a circle
					}
				}
				else			// Regular components/pads/wires ...
				{
					for (int jj = 0, jjMax = comp.GetCompRows(), j = comp.GetRow(); jj < jjMax; jj++, j++)
					for (int ii = 0, iiMax = comp.GetCompCols(), i = comp.GetCol(); ii < iiMax; ii++, i++)
					{
						const size_t iPinIndex = comp.GetCompElement(jj,ii)->GetPinIndex();
						if ( iPinIndex == BAD_PININDEX ) continue;

						int padOffsetX(0), padOffsetY(0);
						if ( !bWire && !bVero )
						{
							comp.GetCompPinOffsets(iPinIndex, padOffsetX, padOffsetY);	// Get offsets in mil
							padOffsetX = (padOffsetX * W) / 100;	// Convert from mil to pixels
							padOffsetY = (padOffsetY * W) / 100;	// Convert from mil to pixels
						}

						if ( bColor && !bPlaced )	// Color pins of floating components (if in Color mode)
						{
							const int&	 nodeId			= comp.GetNodeId(iPinIndex);
							const bool	 bCurrentNodeId	= nodeId != BAD_NODEID && nodeId == GetCurrentNodeId();
							const QColor color			= bCurrentNodeId ? colorMgr.GetPixmapColor(MY_GREY)
																		 : colorMgr.GetColorFromNodeId(nodeId);
							m_varPen.setColor(color);
							m_varBrush.setColor(color);
							painter.setBrush( bMonoPCB ? Qt::NoBrush : m_varBrush);	// No pin color fill in Mono/PCB mode
						}

						const int iPinSizeMIL = ( !bColor || bPlaced ) ? iHoleWidthMIL : std::min(3*iHoleWidthMIL/2, iPadWidthMIL);
						GetLRTB(board, iPinSizeMIL, j, i, L, R, T, B);
						L += padOffsetX;	R += padOffsetX;	T += padOffsetY;	B += padOffsetY;

						// Stop pins vanishing if zoomed too far out
						if ( L == R ) { L--; R++; }
						if ( T == B ) { T--; B++; }

						if ( bPinLabels )	// Write pin labels
						{
							painter.save();
							painter.translate((L+R)/2, (T+B)/2);

							// Set text orientation
							switch( compDirection )
							{
								case 'W':
								case 'E':	painter.rotate(270);	break;
							}

							// Handle L/R pin label alignment
							int iFlag = comp.GetPinAlign(iPinIndex);
							if ( iFlag == Qt::AlignLeft || iFlag == Qt::AlignRight )
							{
								const bool bLeft = ( iFlag == Qt::AlignLeft );
								switch( compDirection )
								{
									case 'E':
									case 'S':	iFlag = bLeft ? Qt::AlignRight : Qt::AlignLeft;	// Swap align L/R
												painter.translate(bLeft ? C/2 : -C/2, 0);
												break;
									default:	painter.translate(bLeft ? -C/2 : C/2, 0);
								}
							}
							iFlag |= ( Qt::TextDontClip | Qt::AlignVCenter );

							painter.scale(dTextScale, dTextScale);
							painter.setPen(bFound ? m_orangePen : bPlaced ? penPlaced : m_redPen);
							painter.drawText(0,0,0,0, iFlag, comp.GetPinLabel(iPinIndex).c_str());
							painter.restore();
						}
						else if ( bRectPins )	// Draw switch pins as rectangles
						{
							const int d = board.GetHalfPixelsFromMIL( iPinSizeMIL );
							switch( compDirection )
							{
								case 'W':
								case 'E':	L -= d; R += d;	break;
								case 'N':
								case 'S':	T -= d; B += d;	break;
							}
							painter.drawRect(L, T, R-L, B-T);
						}
						else	// A regular pin is drawn as a circle
						{
							if ( bMonoPCB && bPlaced )
							{
								GetLRTB(board, 100, j, i, L, R, T, B);	// 100% size square
								painter.drawPoint(padOffsetX + (L+R)/2, padOffsetY + (T+B)/2);
							}
							else
								painter.drawEllipse(L, T, R-L, B-T);
						}
					}
				}
				painter.restore();
			}
			// End draw component pins -----------------------------------------------------------

			// Begin draw component fill + outline -----------------------------------------------
			if ( compMode != COMPSMODE::OFF && !comp.GetShapes().empty() )
			{
				// Set pen width.  Selected component shown thicker than normal components

				if ( bFound )
					m_orangePen.setWidth(4);// Make found components stand out
				else if ( bPlaced )
				{
					if ( bPCB )	// Use floating point pen width to better match Gerber output
						penPlaced.setWidthF( bHighlightComp ? ( board.GetSilkWidth() * 1.5 )
															: ( bMark ? ( board.GetSilkWidth() * 0.5 )
																	  :   board.GetSilkWidth() ) );
					else
						penPlaced.setWidth( bHighlightComp ? 3 : bMark ? 1 : 2 );
				}
				else
					m_redPen.setWidth(4);	// Make floating components stand out in red

				QPen& linePen = bFound ? m_orangePen : bPlaced ? penPlaced : m_redPen;
				painter.setPen(linePen);
				painter.setBrush(Qt::NoBrush);

				GetXY(board, comp, X, Y);	// Get footprint centre

				// Implement wire shift
				if ( bWire && bPlaced )
				{
					if ( comp.GetCompRows() == 1 )	// Horizontal
						Y += compMgr.GetWireShift( &comp ) * 0.1 * W;
					else
						X += compMgr.GetWireShift( &comp ) * 0.1 * W;
				}

				double SL,ST,SR,SB;
				comp.GetSafeBounds(SL,SR,ST,SB);
				SL = SL * W - C;	SR = SR * W + C;
				ST = ST * W - C;	SB = SB * W + C;
				const int iReqW	= static_cast<int>(SR - SL);
				const int iReqH	= static_cast<int>(SB - ST);
				const int iSL	= static_cast<int>(SL);
				const int iST	= static_cast<int>(ST);
				GPainter painterTmp;
				QPixmap tmpPixmap(iReqW, iReqH);
				tmpPixmap.setDevicePixelRatio(1.0);
				const QColor maskColor = comp.GetNewColor().GetQColor();// We'll mask out pixels with this color at the end

				if ( bFill )
				{
					painterTmp.begin(&tmpPixmap);
					painterTmp.fillRect(0, 0, iReqW, iReqH, maskColor);	// This will be turned transparent later
					painterTmp.setPen(Qt::NoPen);
				}

				for (int iLoop = ( bFill ) ? 0 : 1; iLoop < 2; iLoop++)
				{
					GPainter* pPainter = ( iLoop == 0 ) ? &painterTmp : &painter;

					pPainter->save();	// Save (original axes)
					if ( iLoop == 0 )
						pPainter->translate(-SL,-ST);	// Shape coordinates are relative to pixmap top left
					else
						pPainter->translate(X, Y);		// Shape coordinates are relative to footprint centre

					if ( bFill && iLoop == 1 )	// Draw the pixmap created on the previous pass
					{
						painter.setOpacity( /*bWire ? 1.0 :*/ board.GetFillSaturation() * 0.01);
						painter.drawPixmap(iSL, iST, tmpPixmap);
						painter.setOpacity(1.0);
					}

					switch( compDirection )	// Rotated axes at footprint centre
					{
						case 'W':	break;
						case 'E':	pPainter->rotate(180);	break;
						case 'N':	pPainter->rotate(90);	break;
						case 'S':	pPainter->rotate(270);	break;
					}

					for (size_t i = 0, numShapes = comp.GetNumShapes(); i < numShapes; i++)
					{
						const Shape& s = comp.GetShape(i);

						if ( iLoop == 0 && !s.GetDrawFill() ) continue;
						if ( iLoop == 1 && (s.GetDrawFill() || !s.GetDrawLine()) ) continue;
						if ( iLoop == 0 )	// Drawing fill
						{
							m_varBrush.setColor( s.GetFillColor().GetQColor() );
							pPainter->setBrush(m_varBrush);
							pPainter->setPen( s.GetDrawLine() ? fillBlackPen : Qt::NoPen );
						}
						else if ( bColor && !bFill && bWire )	// If Fill slider if fully left in Color mode, draw wire in color
						{
							const int nodeId = comp.GetNodeId(0);
							if ( nodeId != BAD_NODEID )
							{
								const QColor color = ( nodeId == GetCurrentNodeId() ) ? colorMgr.GetPixmapColor(MY_GREY)
																					  : colorMgr.GetColorFromNodeId(nodeId);
								m_varBrush.setColor( color );
								pPainter->setBrush(m_varBrush);
								pPainter->setPen( s.GetDrawLine() ? fillBlackPen : Qt::NoPen );
							}
						}

						pPainter->save();
						pPainter->translate( s.GetCX() * W, s.GetCY() * W );	// Translate to shape centre
						pPainter->rotate( -s.GetA3() );	// A3 > 0 ==> CCW		// Rotate about shape centre

						const int DX	= static_cast<int>(s.GetDX() * W);
						const int DY	= static_cast<int>(s.GetDY() * W);
						const int X		= static_cast<int>(s.GetDX() * W * 0.5);
						const int Y		= static_cast<int>(s.GetDY() * W * 0.5);
						const int A		= static_cast<int>(s.GetA1() * 16);
						const int Alen	= static_cast<int>(s.GetAlen() * 16);
						switch( s.GetType() )
						{
							case SHAPE::RECT:			pPainter->drawRect(-X, -Y, DX, DY);	break;
							case SHAPE::ROUNDED_RECT:	pPainter->drawRoundedRect(-X, -Y, DX, DY, 0.35 * W, 0.35 * W);	break;
							case SHAPE::ELLIPSE:		pPainter->drawEllipse(-X, -Y, DX, DY);	break;
							case SHAPE::ARC:			pPainter->drawArc(  -X, -Y, DX, DY, A, Alen);	break;
							case SHAPE::CHORD:			pPainter->drawChord(-X, -Y, DX, DY, A, Alen);	break;
							default:					assert( SHAPE::LINE == s.GetType() );
														pPainter->drawLine(-X, -Y, X, Y);	break;
						}
						pPainter->restore();
					}
					pPainter->restore();	// Restore (original axes)

					if ( iLoop == 0 )	// Finish off making the pixmap
					{
						painterTmp.end();
						// Turn the "mask" pixels transparent
						tmpPixmap.setMask( tmpPixmap.createMaskFromColor(maskColor, Qt::MaskInColor) );
					}
				}
			}
		}
	}

	// Draw air wires ============================================================================
	if ( !m_bWriteGerber && trackMode != TRACKMODE::OFF && GetCurrentNodeId() != BAD_NODEID )
	{
		painter.save();
		m_redPen.setWidth( static_cast<int>(0.175 * W) );
		m_redPen.setCapStyle(Qt::SquareCap);
		m_redPen.setStyle(Qt::DotLine);
		painter.setPen(m_redPen);
		painter.setBrush(Qt::NoBrush);

		int padOffsetX, padOffsetY;	// For handling offset pads

		std::list<SpanningTreeHelper::AIRWIRE_POINT> spanTreePoints;

		for (int jj = minRow; jj <= maxRow; jj++)
		for (int ii = minCol; ii <= maxCol; ii++)
		{
			Element*	pD		= board.Get(0, jj, ii);	// Layer 0 only
			const bool	bPin	= pD->GetHasPin() && !pD->GetHasWire();
			if ( !bPin ) continue;
			if ( pD->GetNodeId() != GetCurrentNodeId() ) continue;

			GetXY(board, jj, ii, X, Y);

			if ( !bVero )
			{
				const Component& comp = compMgr.GetComponentById( pD->GetCompId() );
				comp.GetCompPinOffsets(pD->GetPinIndex(), padOffsetX, padOffsetY);	// Get offsets in mil
				X += (padOffsetX * W) / 100;	// Convert from mil to pixels
				Y += (padOffsetY * W) / 100;	// Convert from mil to pixels
			}

			spanTreePoints.push_back( SpanningTreeHelper::AIRWIRE_POINT(QPointF(X, Y), pD->GetRouteId()) );
		}

		std::list< SpanningTreeHelper::AIRWIRE_LINE > spanTreeLines;
		SpanningTreeHelper::BuildAirWires(spanTreePoints, spanTreeLines);	// Build air-wires

		for (const auto& o : spanTreeLines)
			painter.drawLine(o.first.first, o.second.first);

		m_redPen.setCapStyle(Qt::RoundCap);
		m_redPen.setStyle(Qt::SolidLine);
		painter.restore();
	}

	// Draw flying wires =========================================================================
	if ( !m_bWriteGerber && !bMonoPCB && compMode != COMPSMODE::OFF && board.GetShowFlyWires() )
	{
		painter.save();

		QPen  blackPen = m_blackPen;	// Used for lines in the component pixmap
		blackPen.setWidth(2);
		m_varPen.setWidth(static_cast<int>(0.175 * W));
		m_varPen.setCapStyle(Qt::SquareCap);
		m_varPen.setStyle(Qt::DotLine);
		painter.setBrush(Qt::NoBrush);
		const int		dH = static_cast<int>(0.1*W);	// Param for wire rounded rect
		const double	dW(0.35*W);						// Param for wire rounded rect

		int padOffsetX, padOffsetY;	// For handling offset pads

		std::set<int> visitedNodeIds;

		for (int j = minRow; j <= maxRow; j++)
		for (int i = minCol; i <= maxCol; i++)
		{
			Element*	pC		= board.Get(0, j, i);	// Layer 0 only
			if ( !board.GetAllowFlyWire(pC) ) continue;
			const int&	nodeId	= pC->GetNodeId();
			if ( visitedNodeIds.find(nodeId) != visitedNodeIds.end() ) continue;
			visitedNodeIds.insert(nodeId);

			const bool	 bCurrentNodeId	= nodeId == GetCurrentNodeId();
			const QColor color	= bCurrentNodeId ? colorMgr.GetPixmapColor(MY_GREY)
												 : colorMgr.GetColorFromNodeId(nodeId);
			m_varPen.setColor(color);

			std::list<QPointF> spanTreePoints;

			GetXY(board, j, i, X, Y);

			if ( !bVero )
			{
				const Component& comp = compMgr.GetComponentById( pC->GetCompId() );
				comp.GetCompPinOffsets(pC->GetPinIndex(), padOffsetX, padOffsetY);	// Get offsets in mil
				X += (padOffsetX * W) / 100;	// Convert from mil to pixels
				Y += (padOffsetY * W) / 100;	// Convert from mil to pixels
			}

			spanTreePoints.push_back( QPointF(X, Y) );

			for (int jj = j; jj <= maxRow; jj++)
			for (int ii = (jj == j) ? (i+1) : minCol; ii <= maxCol; ii++)
			{
				Element*	pD	= board.Get(0, jj, ii);	// Layer 0 only
				if ( pD->GetNodeId() != nodeId || !board.GetAllowFlyWire(pD) ) continue;

				GetXY(board, jj, ii, X, Y);

				if ( !bVero )
				{
					const Component& comp = compMgr.GetComponentById( pD->GetCompId() );
					comp.GetCompPinOffsets(pD->GetPinIndex(), padOffsetX, padOffsetY);	// Get offsets in mil
					X += (padOffsetX * W) / 100;	// Convert from mil to pixels
					Y += (padOffsetY * W) / 100;	// Convert from mil to pixels
				}

				spanTreePoints.push_back( QPointF(X, Y) );
			}

			std::list< SpanningTreeHelper::LINE > spanTreeLines;
			SpanningTreeHelper::Build(spanTreePoints, spanTreeLines, true);	// true ==> daisy chain
			for (const auto& o : spanTreeLines)
			{
				const QPointF	vec	= ( o.second - o.first );
				const QPointF	mid	= ( o.second + o.first ) * 0.5;
				const int		dL	= static_cast<int>(PolygonHelper::Length(vec) * 0.5);
				painter.save();
				painter.translate(mid.x(), mid.y());
				painter.rotate(atan2(vec.y(), vec.x()) * 180.0 / M_PI);
				painter.setPen(m_varPen);		painter.drawLine(-dL, 0, dL, 0);
				painter.setPen(fillBlackPen);	painter.drawRoundedRect(-dL, -dH, dL+dL, dH+dH, dW, dW);
				painter.restore();
			}
		}
		m_varPen.setCapStyle(Qt::RoundCap);
		painter.restore();
	}

	// Draw component text =======================================================================
	if ( compMode != COMPSMODE::OFF )
	{
		for (const auto& pComp : sortedComps)	// Iterate sorted components
		{
			const Component& comp			= *pComp;
			const COMP&		 compType		= comp.GetType();
			const char&		 compDirection	= comp.GetDirection();
			const bool		 bMark			= compType == COMP::MARK;
			const bool		 bWire			= compType == COMP::WIRE;
			const bool		 bVeroLabel		= compType == COMP::VERO_NUMBER || compType == COMP::VERO_LETTER;
			if ( bMark || bWire || bVeroLabel ) continue;
			const bool		 bPlaced		= comp.GetIsPlaced();
			if ( m_bWriteGerber && !bPlaced ) continue;		// Don't write floating components to Gerber
			const bool		 bFound			= compMgr.GetFound( comp.GetId() );

			painter.save();
			double dCopyTextScale = dTextScale;
			if ( bPCB )
			{
				dCopyTextScale *= m_board.GetTextSizeComp() * (20.0 / 243 );	// Scale to make the Gerber font size similar to regular component font size
			}
			else
			{
				QFont compFont = painter.font();	// Copy of current font
				compFont.setPointSize( m_board.GetTextSizeComp() );
				painter.setFont(compFont);
			}

			// Use floating point pen width to better match Gerber output.
			// Scale the pen width down to compensate for painter.scale() scaling things up in the loop below.
			const double dPenWidth = ( bPCB ) ? board.GetSilkWidth() / dCopyTextScale : 0;
			m_orangePen.setWidthF(dPenWidth);	// Use colored text for found components
			m_redPen.setWidthF(dPenWidth);		// Use red text for floating components
			penPlaced.setWidthF(dPenWidth);		// Use this for placed components

			GetXY(board, comp, X, Y);	// Get footprint centre

			int offsetRow(0), offsetCol(0);
			comp.GetLabelOffsets(offsetRow, offsetCol);

			X += W * 0.0625 * offsetCol; // Offset for text is 1/16 of a grid square
			Y += W * 0.0625 * offsetRow; // Offset for text is 1/16 of a grid square

			painter.translate(X, Y);
			if ( compDirection == 'N' || compDirection == 'S' )
				painter.rotate(270);

			const std::string& myStr = ( compMode == COMPSMODE::NAME )  ? comp.GetNameStr() :
									   ( compMode == COMPSMODE::VALUE ) ? comp.GetValueStr() : "";

			painter.scale(dCopyTextScale, dCopyTextScale);
			painter.setPen(bFound ? m_orangePen : bPlaced ? penPlaced : m_redPen);
			painter.drawText(0,0,0,0, Qt::AlignCenter | Qt::TextDontClip, myStr.c_str(), bPCB);
			painter.restore();
		}
	}

	// Draw the User-defined "trax" component ====================================================
	if ( !m_bWriteGerber && ( compMode != COMPSMODE::OFF || trackMode != TRACKMODE::OFF ) )
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

			for (int j = 0, jRow = trax.GetRow(), compRows = trax.GetCompRows(); j < compRows; j++, jRow++)
			for (int i = 0, iCol = trax.GetCol(), compCols = trax.GetCompCols(); i < compCols; i++, iCol++)
				if ( trax.GetCompElement(j, i)->ReadFlagBits(RECTSET) )
				{
					GetLRTB(board, 100, jRow, iCol, L, R, T, B);	// 100% size square
					painter.drawRect(L,T,R-L,B-T);
				}
		}
	}

	// Draw vero labels ==========================================================================
	if ( !bPCB )
	{
		painter.save();

		QFont labelsFont = painter.font();	// Copy of current font
		labelsFont.setPointSize( m_board.GetTextSizeComp() );
		painter.setFont(labelsFont);

		painter.setBrush(Qt::NoBrush);
		penPlaced.setWidth(0);
		m_redPen.setWidth(0);

		for (const auto& pComp : sortedComps)	// Iterate sorted components
		{
			const Component& comp			= *pComp;
			const COMP&		 compType		= comp.GetType();
			const bool		 bVeroNumber	= compType == COMP::VERO_NUMBER;
			const bool		 bVeroLetter	= compType == COMP::VERO_LETTER;
			if ( !(bVeroNumber || bVeroLetter) ) continue;
			const char&		 compDirection	= comp.GetDirection();
			const bool		 bPlaced		= comp.GetIsPlaced();	// Only unplaced when a label is created by copying another ("V" key)

			int index(0), length(comp.GetSize());
			for (int jj = 0, jjMax = comp.GetCompRows(), j = comp.GetRow(); jj < jjMax; jj++, j++)
			for (int ii = 0, iiMax = comp.GetCompCols(), i = comp.GetCol(); ii < iiMax; ii++, i++, index++)
			{
				GetLRTB(board, 100, j, i, L, R, T, B);

				painter.save();
				painter.translate((L+R)/2, (T+B)/2);

				// Set text orientation, and forward/reverse number order
				int iNumber = index + 1;	// Increasing numbers by default
				switch( compDirection )
				{
					case 'W':	painter.rotate(270);			break;
					case 'E':	painter.rotate(270);
								iNumber = 1 + length - iNumber;	break;	// Reverse number order
					case 'S':	iNumber = 1 + length - iNumber;	break;	// Reverse number order
				}

				std::string label("");
				if ( bVeroNumber )	// Numbers go: 1,2,3 etc
					label = std::to_string(iNumber);
				else				// Letters go: A,B,...,Y,Z,AA,AB,...,AY,AZ,BA,BB, ... etc
				{
					if ( iNumber > 26 ) label += ( char('A') + static_cast<char>((iNumber-1)/26 - 1) );
					label += ( char('A') + (iNumber-1)%26 );
				}

				// We always want the vero labels to appear non-mirrored
				if ( board.GetFlipH() )	painter.scale(-1, 1);	// Mirror L-R
				if ( board.GetFlipV() )	painter.scale(1, -1);	// Mirror T-B

				painter.scale(dTextScale, dTextScale);
				painter.setPen(bPlaced ? penPlaced : m_redPen);
				painter.drawText(0,0,0,0, Qt::TextDontClip | Qt::AlignVCenter | Qt::AlignHCenter, label.c_str());
				painter.restore();
			}
		}
		painter.restore();
	}

	// Draw User-defined labels ==================================================================
	if ( !bPCB && board.GetShowText() )
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

			const int textW = static_cast<int>((R-L)/dTextScale);
			const int textH = static_cast<int>((B-T)/dTextScale);

			font.setBold( rect.GetStyle() & TEXT_BOLD );
			font.setItalic( rect.GetStyle() & TEXT_ITALIC );
			font.setUnderline( rect.GetStyle() & TEXT_UNDERLINE );
			font.setPointSize( rect.GetSize() );
			painter.setFont(font);

			m_varPen.setColor( rect.GetQColor() );
			painter.setPen(m_varPen);
			painter.setBrush(Qt::NoBrush);
			painter.save();
			painter.translate((bMono && layer == 0) ? R : L, T);							// Mirror all text boxes in Mono mode for bottom layer
			painter.scale((bMono && layer == 0) ? -dTextScale : dTextScale, dTextScale);	// Mirror all text boxes in Mono mode for bottom layer
			painter.drawText(0, 0, textW, textH, Qt::TextWordWrap | rect.GetFlagsH() | rect.GetFlagsV(), QString::fromStdString(rect.GetStr()));
			painter.restore();

			if ( mapObj.first == GetCurrentTextId() )
			{
				painter.setPen(m_dotPen);
				painter.drawRect(L, T, R-L, B-T);
				painter.drawRect(R-C, B-C, C, C);
			}
		}
	}

	// Draw a cross on the pad being used for pad offset =========================================
	if ( !m_bWriteGerber && !bVero && m_padOffsetDlg->isVisible() )
	{
		const Element* pC = m_board.Get(layer, m_gridRow, m_gridCol);
		if ( pC->GetHasPin() && !pC->GetHasWire() )
		{
			GetXY(board, m_gridRow, m_gridCol, X, Y);

			int padOffsetX, padOffsetY;	// For handling offset pads
			const Component& comp = compMgr.GetComponentById( pC->GetCompId() );
			comp.GetCompPinOffsets(pC->GetPinIndex(), padOffsetX, padOffsetY);	// Get offsets in mil

			X += (padOffsetX * W) / 100;	// Convert from mil to pixels
			Y += (padOffsetY * W) / 100;	// Convert from mil to pixels

			painter.save();
			painter.setBrush(Qt::NoBrush);
			m_yellowPen.setWidth(2);
			painter.setPen(m_yellowPen);
			painter.drawLine(X-C, Y, X+C, Y);
			painter.drawLine(X, Y-C, X, Y+C);
			painter.restore();
		}
	}

	// Draw places of min track separation =======================================================
	if ( !m_bWriteGerber && !bVero && trackMode != TRACKMODE::OFF && m_board.GetShowCloseTracks() )
	{
		painter.save();
		painter.setBrush(Qt::NoBrush);
		for (int lyr = 0; lyr < m_board.GetLyrs(); lyr++)
		{
			m_orangePen.setWidth(lyr == layer ? 3 : 1);	// Current layer shown fatter
			painter.setPen(m_orangePen);
			for (const auto& o : m_board.GetWarnPoints(lyr))
			{
				GetXY(board, o.y(), o.x(), X, Y);
				if ( lyr == layer )
					painter.drawRect(X-C/3, Y-C/3, W/3, W/3);
				else
					painter.drawRect(X-C/6, Y-C/6, W/6, W/6);
			}
		}
		painter.restore();
	}

	if ( !m_bWriteGerber && m_bRuler )
	{
		QPointF A,B;
		GetRulerExact(board, m_rulerA, A);
		GetRulerExact(board, m_rulerB, B);

		int XA, YA, XB, YB;
		GetXY(board, A.ry(), A.rx(), XA, YA);
		GetXY(board, B.ry(), B.rx(), XB, YB);

		painter.save();
		m_rulerPen.setWidth(C);
		painter.setPen(m_rulerPen);
		painter.setBrush(Qt::NoBrush);
		painter.drawLine(XA,YA,XB,YB);
		painter.restore();
	}

	if ( m_bWriteGerber )
		m_gWriter.GetStream(GFILE::GTO).DrawBuffers();	// Top silk layer

	painter.end();

	delete pdfWriter;

//	const auto elapsed = std::chrono::steady_clock::now() - start;
//	const auto duration_ms	= std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
//	std::cout << "Time : " << duration_ms << std::endl;
}

void MainWindow::GetFirstRowCol(int& iRow, int& iCol) const
{
	const int& W = m_board.GetGRIDPIXELS();	// Square width in pixels
	iRow = 1 + m_scrollArea->verticalScrollBar()->value()   / W;	// The first fully visible row
	iCol = 1 + m_scrollArea->horizontalScrollBar()->value() / W;	// The first fully visible col
}

void MainWindow::GetXY(const GuiControl& guiCtrl, double row, double col, int& X, int& Y) const
{
	// For rendering.
	// Takes a point in the Board and returns coordinates in the drawn image.
	const int& W = guiCtrl.GetGRIDPIXELS();	// Square width in pixels
	const int  C = W >> 1;					// Half square width in pixels
	X = m_XGRIDOFFSET + m_XCORRECTION + C + static_cast<int>(col * W);
	Y = m_YGRIDOFFSET + m_YCORRECTION + C + static_cast<int>(row * W);
}

void MainWindow::GetLRTB(const GuiControl& guiCtrl, double percent, double row, double col, int& L, int& R, int& T, int& B) const
{
	// For rendering.
	// Takes a point in the Board and returns bounding box coordinates in the drawn image.
	// Setting percent to 100 will make the bounding box the same size as the grid square.
	const int& W = guiCtrl.GetGRIDPIXELS();	// Square width in pixels
	int X(0), Y(0);
	GetXY(guiCtrl, row, col, X, Y);
	const int S = static_cast<int>( round(W * 0.005 * percent) );
	L = X - S;	T = Y - S;
	R = X + S;	B = Y + S;
}

void MainWindow::GetLRTB(const GuiControl& guiCtrl, const Component& comp, int& L, int& R, int& T, int& B) const
{
	// For rendering.
	// Takes a component in the Board and returns bounding box coordinates in the drawn image.
	GetXY(guiCtrl, comp.GetRow(),		comp.GetCol(),		L, T);
	GetXY(guiCtrl, comp.GetLastRow(),	comp.GetLastCol(),	R, B);
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

	if ( !guiCtrl.GetVeroTracks() )	// Offset the footprint if all pin offsets are equal
	{
		const int& W = guiCtrl.GetGRIDPIXELS();	// Square width in pixels
		int padOffsetX, padOffsetY;
		comp.GetCompShapeOffsets(padOffsetX, padOffsetY);	// Get offsets in mil
		X += (padOffsetX * W) / 100;	// Convert from mil to pixels
		Y += (padOffsetY * W) / 100;	// Convert from mil to pixels
	}
}

void MainWindow::GetRulerExact(Board& board, const QPoint& p, QPointF& pOut) const
{
	pOut =  p;
	if ( !board.GetVeroTracks() )
	{
		const Element* pC = board.Get(0, p.y(), p.x());
		if ( pC->GetHasPin() && !pC->GetHasWire() )
		{
			int padOffsetX(0), padOffsetY(0);
			const Component& comp = board.GetCompMgr().GetComponentById( pC->GetCompId() );
			comp.GetCompPinOffsets(pC->GetPinIndex(), padOffsetX, padOffsetY);	// Get offsets in mil
			pOut += QPointF(0.01 * padOffsetX, 0.01 * padOffsetY);
		}
	}
}
