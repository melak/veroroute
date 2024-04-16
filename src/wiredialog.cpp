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

#include "wiredialog.h"
#include "ui_wiredialog.h"
#include "mainwindow.h"

WireDialog::WireDialog(MainWindow* parent)
: QDialog(parent)
, ui(new Ui_WireDialog)
, m_pMainWindow(parent)
{
	ui->setupUi(this);

#ifdef VEROROUTE_FONT_SIZE
	VEROROUTE_FONT_SIZE(ui->groupBox)
	VEROROUTE_FONT_SIZE(ui->checkBox_share)
	VEROROUTE_FONT_SIZE(ui->checkBox_cross)
#endif

	QObject::connect(ui->checkBox_share,	SIGNAL(toggled(bool)),	m_pMainWindow,	SLOT(SetWireShare(bool)));
	QObject::connect(ui->checkBox_cross,	SIGNAL(toggled(bool)),	m_pMainWindow,	SLOT(SetWireCross(bool)));
	QObject::connect(this,					SIGNAL(rejected()),		m_pMainWindow,	SLOT(UpdateControls()));	// Close using X button
}

WireDialog::~WireDialog()
{
	delete ui;
}

void WireDialog::UpdateControls()
{
	Board& board = m_pMainWindow->m_board;
	ui->checkBox_share->setChecked( board.GetWireShare() );
	ui->checkBox_cross->setChecked( board.GetWireCross() );
}

void WireDialog::keyPressEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	m_pMainWindow->specialKeyPressEvent(event);
#endif
	QDialog::keyPressEvent(event);
	event->accept();
}

void WireDialog::keyReleaseEvent(QKeyEvent* event)
{
#ifdef VEROROUTE_ANDROID
	if ( event->key() == Qt::Key_Back )
	{
		QTimer::singleShot(0, m_pMainWindow, SLOT(HideWireDialog()));
		return event->accept();
	}
#else
	m_pMainWindow->commonKeyReleaseEvent(event);
#endif
	QDialog::keyReleaseEvent(event);
	event->accept();
}
