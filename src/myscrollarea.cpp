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

			QTouchEvent* ev = (QTouchEvent*)event;
			auto& points = ev->touchPoints();
			if ( pMainWindow )
			{
				const auto elapsed			= std::chrono::steady_clock::now() - m_lastTouchEnd;
				const auto duration_ms		= std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
				const bool bDoubleClicked	= ( duration_ms >= 0 && duration_ms <= 200 );	//TODO

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
			if ( m_bTouchCancelled ) break;

			QTouchEvent* ev = (QTouchEvent*)event;
			auto& points = ev->touchPoints();
			if ( points.size() == 1 )	// Single point ==> mouse move
			{
				if ( pMainWindow )
					pMainWindow->MouseMoveEvent(points.begin()->pos().toPoint());
			}
			else
			{
				m_bTouchCancelled = true;//TODO Temporary hack
				//TODO Process points to handle zoom / scroll etc
			}
			event->accept();
			return true;
		}
		case QEvent::TouchEnd:
		{
			if ( m_bTouchCancelled ) break;

			m_lastTouchEnd = std::chrono::steady_clock::now();
			QTouchEvent* ev = (QTouchEvent*)event;
			auto& points = ev->touchPoints();
			if ( pMainWindow )
				pMainWindow->MouseReleaseEvent(points.begin()->pos().toPoint());
			event->accept();
			return true;
		}
		default:
			break;
	}
	return QScrollArea::viewportEvent(event);
}
