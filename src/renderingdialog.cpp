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

#include "renderingdialog.h"
#include "ui_renderingdialog.h"
#include "mainwindow.h"

RenderingDialog::RenderingDialog(MainWindow* parent)
: QDialog(parent)
, ui(new Ui_RenderingDialog)
, m_pMainWindow(parent)
{
	ui->setupUi(this);

#ifndef VEROROUTE_ANDROID
	ui->pushButtonOK->hide();
#endif

	ui->antiAliasOn->setChecked(true);
	QObject::connect(ui->antiAliasOff,		SIGNAL(toggled(bool)),		m_pMainWindow,	SLOT(SetAntialiasOff(bool)));
	QObject::connect(ui->antiAliasOn,		SIGNAL(toggled(bool)),		m_pMainWindow,	SLOT(SetAntialiasOn(bool)));
	QObject::connect(ui->spinBox_bright,	SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetBrightness(int)));
	QObject::connect(ui->shade,				SIGNAL(toggled(bool)),		m_pMainWindow,	SLOT(SetShowTarget(bool)));
	QObject::connect(ui->comptext,			SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetTextSizeComp(int)));
	QObject::connect(ui->comppins,			SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetTextSizePins(int)));
	QObject::connect(ui->spinBox_height,	SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetTargetRows(int)));
	QObject::connect(ui->spinBox_width,		SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetTargetCols(int)));
	QObject::connect(ui->padWidth,			SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetPadWidth(int)));
	QObject::connect(ui->trackWidth,		SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetTrackWidth(int)));
	QObject::connect(ui->tagWidth,			SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetTagWidth(int)));
	QObject::connect(ui->holeWidth,			SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetHoleWidth(int)));
	QObject::connect(ui->gapWidth,			SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetGapWidth(int)));
	QObject::connect(ui->maskWidth,			SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetMaskWidth(int)));
	QObject::connect(ui->silkWidth,			SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetSilkWidth(int)));
	QObject::connect(ui->edgeWidth,			SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetEdgeWidth(int)));
	QObject::connect(ui->viapadWidth,		SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetViaPadWidth(int)));
	QObject::connect(ui->viaholeWidth,		SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetViaHoleWidth(int)));
	QObject::connect(ui->closeTracks,		SIGNAL(toggled(bool)),		m_pMainWindow,	SLOT(SetShowCloseTracks(bool)));
	QObject::connect(ui->pushButtonOK,		SIGNAL(clicked()),			this,			SLOT(hide()));
}

RenderingDialog::~RenderingDialog()
{
	delete ui;
}

void RenderingDialog::UpdateControls()
{
	Board& board = m_pMainWindow->m_board;

	double minTrk(0), minGnd(0);
	board.GetSeparations(minTrk, minGnd);	// Put be called first since it may clear the warning points

	const bool bCompEdit		= board.GetCompEdit();
	const bool bPCB				= board.GetTrackMode() == TRACKMODE::PCB;
	const bool bMonoPCB			= board.GetTrackMode() == TRACKMODE::MONO || bPCB;
	const bool bNoTrackOptions	= board.GetTrackMode() == TRACKMODE::OFF;
	const bool bGndFill			= board.GetGroundFill();
	const bool bVero			= board.GetVeroTracks();
	const bool bVias			= board.GetViasEnabled();
	const bool bCloseTrackInfo	= board.GetHaveWarnPoints();

	ui->spinBox_bright->setValue( board.GetBackgroundColor().GetR() );
	ui->comptext->setValue( board.GetTextSizeComp() );
	ui->comppins->setValue( board.GetTextSizePins() );
	ui->spinBox_height->setValue( board.GetTargetRows() );
	ui->spinBox_width->setValue( board.GetTargetCols() );
	ui->shade->setChecked( board.GetShowTarget() );
	if ( board.GetRenderQuality() == 0 ) ui->antiAliasOff->setChecked(true); else ui->antiAliasOn->setChecked(true);
	ui->groupBox_bright->setDisabled(bCompEdit || bPCB );
	ui->groupBox_target->setDisabled(	bCompEdit );
	ui->comptext->setDisabled(			bCompEdit );
	ui->comppins->setDisabled(			!bCompEdit && bMonoPCB );
	ui->padWidth->setDisabled(			bCompEdit || bNoTrackOptions || bVero );
	ui->trackWidth->setDisabled(		bCompEdit || bNoTrackOptions || bVero );
	ui->tagWidth->setDisabled(			bCompEdit || bNoTrackOptions || bVero || !bMonoPCB || !bGndFill );
	ui->holeWidth->setDisabled(			bCompEdit || bNoTrackOptions || bVero );
	ui->viapadWidth->setDisabled(		bCompEdit || bNoTrackOptions || bVero || !bVias );
	ui->viaholeWidth->setDisabled(		bCompEdit || bNoTrackOptions || bVero || !bVias );
	ui->gapWidth->setDisabled(			bCompEdit || bVero || !bMonoPCB || !bGndFill );
	ui->groupBox_pcb->setDisabled(		bCompEdit || bVero || !bPCB );

	// ... and corresponding labels
	ui->label_comptext->setDisabled(	bCompEdit );
	ui->label_comppins->setDisabled(	!bCompEdit && bMonoPCB );
	ui->label_pad->setDisabled(			bCompEdit || bNoTrackOptions || bVero );
	ui->label_track->setDisabled(		bCompEdit || bNoTrackOptions || bVero );
	ui->label_tag->setDisabled(			bCompEdit || bNoTrackOptions || bVero || !bMonoPCB || !bGndFill );
	ui->label_hole->setDisabled(		bCompEdit || bNoTrackOptions || bVero );
	ui->label_viapad->setDisabled(		bCompEdit || bNoTrackOptions || bVero || !bVias );
	ui->label_viahole->setDisabled(		bCompEdit || bNoTrackOptions || bVero || !bVias );
	ui->label_gap->setDisabled(			bCompEdit || bVero || !bMonoPCB || !bGndFill );
	ui->label_info->setDisabled(		bCompEdit || bVero );
	ui->label_info_2->setDisabled(		bCompEdit || bVero || !bMonoPCB || !bGndFill );
	ui->closeTracks->setDisabled(		bCompEdit || bVero || !bCloseTrackInfo );
	if ( bCompEdit || bVero || !bCloseTrackInfo )
		ui->closeTracks->hide();
	else
		ui->closeTracks->show();

	ui->padWidth->setValue(		board.GetPAD_MIL()		);
	ui->trackWidth->setValue(	board.GetTRACK_MIL()	);
	ui->tagWidth->setValue(		board.GetTAG_MIL()		);
	ui->holeWidth->setValue(	board.GetHOLE_MIL()		);
	ui->gapWidth->setValue(		board.GetGAP_MIL()		);
	ui->maskWidth->setValue(	board.GetMASK_MIL()		);
	ui->silkWidth->setValue(	board.GetSILK_MIL()		);
	ui->edgeWidth->setValue(	board.GetEDGE_MIL()		);
	ui->viapadWidth->setValue(	board.GetVIAPAD_MIL()	);
	ui->viaholeWidth->setValue(	board.GetVIAHOLE_MIL()	);

	const int minTrkMil = static_cast<int>(minTrk);
	const int minTrkRem = static_cast<int>(10.0 * (minTrk - minTrkMil));	// 0.1 mil resolution
	const int minGndMil = static_cast<int>(minGnd);
	const int minGndRem = static_cast<int>(10.0 * (minGnd - minGndMil));	// 0.1 mil resolution

	std::string str = "Current min track separation = ";
	if ( bCompEdit || bVero )
		str += "n/a";
	else
		str += std::to_string(minTrkMil) + "." + std::to_string(minTrkRem) + " mil";
	ui->label_info->setText( QString::fromStdString(str) );

	std::string str2 = "Current min ground-fill width = ";
	if ( bCompEdit || bVero || !bMonoPCB || !bGndFill )
		str2 += "n/a";
	else
		str2 += std::to_string(minGndMil) + "." + std::to_string(minGndRem) + " mil";
	ui->label_info_2->setText( QString::fromStdString(str2) );

	ui->closeTracks->setChecked( board.GetShowCloseTracks() );
}

void RenderingDialog::keyPressEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	m_pMainWindow->specialKeyPressEvent(event);
#endif
	QDialog::keyPressEvent(event);
}

void RenderingDialog::keyReleaseEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	m_pMainWindow->commonKeyReleaseEvent(event);
#endif
	QDialog::keyReleaseEvent(event);
}
