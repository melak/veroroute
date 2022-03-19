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
			m_bTouchCancelled = false;
			m_dSpread = 0;
			m_maxPoints = 0;

			const auto	now				= std::chrono::steady_clock::now();
			const auto	elapsed			= now - m_lastTouchBegin;
			const auto	duration_ms		= std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
			const bool	bDoubleClicked	= ( duration_ms >= 0 && duration_ms <= 300 );
			m_lastTouchBegin = now;

			if ( pMainWindow )
			{
				QTouchEvent*	ev		= (QTouchEvent*)event;
				const auto&		points	= ev->touchPoints();

				m_maxPoints = points.size();

				if ( bDoubleClicked )
					pMainWindow->MouseDoubleClickEvent(points.begin()->pos().toPoint());
				else
					pMainWindow->MousePressEvent(points.begin()->pos().toPoint());
			}
			event->accept();
			return true;
		}
		case QEvent::TouchUpdate:
		{
			if ( m_bTouchCancelled ) return QScrollArea::viewportEvent(event);

			if ( pMainWindow )
			{
				QTouchEvent*	ev			= (QTouchEvent*)event;
				const auto&		points		= ev->touchPoints();

				const int		numPoints	= points.size();
				m_maxPoints = std::max(m_maxPoints, numPoints);

				switch( numPoints )
				{
					case 1:
					{
						if ( m_maxPoints > 1 ) return QScrollArea::viewportEvent(event);

						pMainWindow->MouseMoveEvent(points.begin()->pos().toPoint());	break;	// Only ever had one point ==> mouse move
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
			if ( m_bTouchCancelled ) return QScrollArea::viewportEvent(event);
			if ( m_maxPoints > 1 ) return QScrollArea::viewportEvent(event);

			if ( pMainWindow )
			{
				QTouchEvent*	ev		= (QTouchEvent*)event;
				const auto&		points	= ev->touchPoints();
				pMainWindow->MouseReleaseEvent(points.begin()->pos().toPoint());
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
	return ( points.size() > 1 ) ? PolygonHelper::Length( points[0].pos() - points[1].pos() ) : 0;
}


