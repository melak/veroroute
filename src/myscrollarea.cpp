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
#include "PolygonHelper.h"

MyScrollArea::MyScrollArea(QWidget* parent) : QScrollArea(parent), m_parent(parent)
{
	m_lastTouchBegin = std::chrono::steady_clock::now();
}

bool MyScrollArea::viewportEvent(QEvent* event)
{
	MainWindow* pMainWindow = (MainWindow*)m_parent;

	switch( event->type() )
	{
		case QEvent::MouseButtonPress:
		{
			QMouseEvent* ev = (QMouseEvent*)event;
			if ( pMainWindow ) pMainWindow->MousePressEvent(ev->pos(), ev->button() & Qt::LeftButton, ev->button() & Qt::RightButton);
			event->accept();
			return true;
		}
		case QEvent::MouseButtonDblClick:
		{
			QMouseEvent* ev = (QMouseEvent*)event;
			if ( pMainWindow ) pMainWindow->MouseDoubleClickEvent(ev->pos());
			event->accept();
			return true;
		}
		case QEvent::MouseMove:
		{
			QMouseEvent* ev = (QMouseEvent*)event;
			if ( pMainWindow ) pMainWindow->MouseMoveEvent(ev->pos());
			event->accept();
			return true;
		}
		case QEvent::MouseButtonRelease:
		{
			QMouseEvent* ev = (QMouseEvent*)event;
			if ( pMainWindow ) pMainWindow->MouseReleaseEvent(ev->pos());
			event->accept();
			return true;
		}
		case QEvent::TouchCancel:
		{
			m_bTouchCancelled = true;
			event->accept();
			return true;
		}
		case QEvent::TouchBegin:
		{
			bool bDoubleClicked(false);
			if ( !m_bTouchCancelled && !m_bDoubleClickCancelled )
			{
				const auto	elapsed			= std::chrono::steady_clock::now() - m_lastTouchBegin;
				const auto	duration_ms		= std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
				const auto	effective_ms	= ( duration_ms > m_releaseDuration_ms ) ? ( duration_ms - m_releaseDuration_ms ) : 0;
#ifdef VEROROUTE_DEBUG
				if ( pMainWindow ) pMainWindow->m_labelDebug->setText(QString::number(effective_ms));
				if ( pMainWindow ) pMainWindow->m_labelDebug->show();
#endif
				bDoubleClicked = ( effective_ms >= 0 && effective_ms <= 500 );
			}
			m_bDoubleClickCancelled = bDoubleClicked;	// Prevent daisy chaining of double-clicks

			if ( pMainWindow )
			{
				QTouchEvent*	ev		= (QTouchEvent*)event;
				const auto&		points	= ev->points();

				m_maxPoints			= points.size();	// Reset m_maxPoints
				m_bTouchCancelled	= false;			// Reset m_bTouchCancelled
				m_dSpread			= 0;				// Reset m_dSpread

				if ( bDoubleClicked )
					pMainWindow->MouseDoubleClickEvent(points.begin()->position().toPoint());
				else
					pMainWindow->MousePressEvent(points.begin()->position().toPoint());
			}
			m_lastTouchBegin = std::chrono::steady_clock::now();	// Processing the events could have taken some time, so measure from here
			event->accept();
			return true;
		}
		case QEvent::TouchUpdate:
		{
			if ( m_bTouchCancelled ) return QScrollArea::viewportEvent(event);

			if ( pMainWindow )
			{
				QTouchEvent*	ev			= (QTouchEvent*)event;
				const auto&		points		= ev->points();
				const int		numPoints	= points.size();

				m_maxPoints = std::max(m_maxPoints, numPoints);

				if ( m_maxPoints > 1 ) m_bDoubleClickCancelled = true;

				switch( numPoints )
				{
					case 1:
					{
						if ( m_maxPoints > 1 ) return QScrollArea::viewportEvent(event);

						pMainWindow->MouseMoveEvent(points.begin()->position().toPoint());	break;	// Only ever had one point ==> mouse move
					}
					case 2:	// 2 points ==> try handle zoom
					{
						if ( m_maxPoints > 2 ) return QScrollArea::viewportEvent(event);

						const qreal dNewSpread = GetSpread(points);	assert(dNewSpread > 0);
						if ( m_dSpread == 0 )
						{
							m_dSpread = dNewSpread;	// Initial spread measure
						}
						else if ( m_dSpread > 0 && dNewSpread > (m_dSpread * 1.20) && pMainWindow->CanZoomIn() )	// If we've increased the spread by 20%
						{
							m_dSpread = dNewSpread;	// Update the spread measure ...
							pMainWindow->ZoomIn();	// ... and zoom in
						}
						else if ( m_dSpread > 0 && dNewSpread < (m_dSpread * 0.833) && pMainWindow->CanZoomOut() )	// If we've decreased the spread by 20%
						{
							m_dSpread = dNewSpread;	// Update the spread measure ...
							pMainWindow->ZoomOut();	// ... and zoom out
						}
						else
							return QScrollArea::viewportEvent(event);
						break;
					}
					default:	return QScrollArea::viewportEvent(event);
				}
			}
			event->accept();
			return true;
		}
		case QEvent::TouchEnd:
		{
			if ( m_bTouchCancelled || ( m_maxPoints > 1 ) ) return QScrollArea::viewportEvent(event);

			if ( pMainWindow )
			{
				QTouchEvent*	ev		= (QTouchEvent*)event;
				const auto&		points	= ev->points();

				// The MouseReleaseEvent() can take some time due to updating the broken nets list.  Measure how long it takes.
				const auto	begin		= std::chrono::steady_clock::now();
				pMainWindow->MouseReleaseEvent(points.begin()->position().toPoint());
				const auto	elapsed		= std::chrono::steady_clock::now() - begin;
				m_releaseDuration_ms	= std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
			}
			event->accept();
			return true;
		}
		default:
			return QScrollArea::viewportEvent(event);
	}
}

qreal MyScrollArea::GetSpread(const QList<QTouchEvent::TouchPoint>& points) const
{
	// Just use first 2 points
	return ( points.size() > 1 ) ? PolygonHelper::Length( points[0].position() - points[1].position() ) : 0;
}
