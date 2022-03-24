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

#include <QtGui>
#include <QScrollArea>
#include <QScrollBar>
#include <chrono>

// Wrapper to send mouse wheel and key press events to the mainwindow instead of the scroll bars

class MyScrollArea : public QScrollArea
{
	Q_OBJECT

public:
	MyScrollArea(QWidget* parent = nullptr);
	~MyScrollArea() {}

	void SetRequestCentreView(bool b) { m_bRequestCentreView = b; }
	void CentreView()
	{
		if ( !m_bRequestCentreView ) return;
		auto* pH = horizontalScrollBar();
		auto* pV = verticalScrollBar();
		const int iHcentre	= ( pH->minimum() + pH->maximum() ) / 2;
		const int iVcentre	= ( pV->minimum() + pV->maximum() ) / 2;
		const bool bChanged	= ( pH->value() != iHcentre ) || ( pV->value() != iVcentre );
		if ( bChanged )
		{
			pH->setValue(iHcentre);
			pV->setValue(iVcentre);
			m_bRequestCentreView = false;
		}
	}
protected:
	void resizeEvent(QResizeEvent* event)
	{
		QScrollArea::resizeEvent(event);
		CentreView();
	}
	void wheelEvent(QWheelEvent* event)
	{
		if ( m_parent ) QCoreApplication::sendEvent(m_parent, event);
	}
	void keyPressEvent(QKeyEvent* event)
	{
		if ( m_parent ) QCoreApplication::sendEvent(m_parent, event);
	}
	void keyReleaseEvent(QKeyEvent* event )
	{
		if ( m_parent ) QCoreApplication::sendEvent(m_parent, event);
	}
	bool viewportEvent(QEvent* event);
private:
	qreal GetSpread(const QList<QTouchEvent::TouchPoint>& points) const;
private:
	QWidget* m_parent;
	std::chrono::steady_clock::time_point m_lastTouchBegin;
	bool		m_bRequestCentreView	= false;
	bool		m_bTouchCancelled		= false;
	bool		m_bDoubleClickCancelled	= false;
	long long	m_releaseDuration_ms	= 0;
	qreal		m_dSpread				= 0;
	int			m_maxPoints				= 0;
};
