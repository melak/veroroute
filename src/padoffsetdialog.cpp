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

#include "padoffsetdialog.h"
#include "ui_padoffsetdialog.h"
#include "mainwindow.h"

PadOffsetDialog::PadOffsetDialog(MainWindow* parent)
: QDialog(parent)
, ui(new Ui_PadOffsetDialog)
, m_pMainWindow(parent)
{
	ui->setupUi(this);
#ifndef VEROROUTE_ANDROID
	ui->pushButtonOK->hide();
#endif

	QFont font = ui->textL->font();
	font.setFamily(QString("Arial Unicode MS"));
	font.setPointSize(12);

	ui->textL->setFont(font);
	ui->textR->setFont(font);
	ui->textT->setFont(font);
	ui->textB->setFont(font);

	// Unicode triangles ...
	ui->textL->setText(QChar(0x25c0));
	ui->textR->setText(QChar(0x25b6));
	ui->textT->setText(QChar(0x25b2));
	ui->textB->setText(QChar(0x25bc));

	QObject::connect(ui->textC,			SIGNAL(clicked()),	m_pMainWindow,	SLOT(PadCentre()));
	QObject::connect(ui->textL,			SIGNAL(clicked()),	m_pMainWindow,	SLOT(PadMoveL()));
	QObject::connect(ui->textR,			SIGNAL(clicked()),	m_pMainWindow,	SLOT(PadMoveR()));
	QObject::connect(ui->textT,			SIGNAL(clicked()),	m_pMainWindow,	SLOT(PadMoveT()));
	QObject::connect(ui->textB,			SIGNAL(clicked()),	m_pMainWindow,	SLOT(PadMoveB()));
	QObject::connect(ui->pushButtonOK,	SIGNAL(clicked()),	m_pMainWindow,	SLOT(HidePadOffsetDialog()));
}

PadOffsetDialog::~PadOffsetDialog()
{
	delete ui;
}

void PadOffsetDialog::keyPressEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	m_pMainWindow->specialKeyPressEvent(event);
#endif
	QDialog::keyPressEvent(event);
}

void PadOffsetDialog::keyReleaseEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	m_pMainWindow->commonKeyReleaseEvent(event);
#endif
	QDialog::keyReleaseEvent(event);
}
