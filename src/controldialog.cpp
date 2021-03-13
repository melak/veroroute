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

#include "controldialog.h"
#include "ui_controldialog.h"
#include "mainwindow.h"

ControlDialog::ControlDialog(QWidget* parent)
: QWidget(parent)
, ui(new Ui_ControlDialog)
, m_pMainWindow(nullptr)
{
	ui->setupUi((QDialog*)(this));

	QFont font = ui->rotateCCW->font();
	font.setFamily(QString("Arial Unicode MS"));
	font.setPointSize(12);
	ui->rotateCCW->setFont(font);
	ui->rotateCW->setFont(font);
	ui->textL->setFont(font);
	ui->textR->setFont(font);
	ui->textT->setFont(font);
	ui->textB->setFont(font);
	
	// Unicode arrowed circles...
	ui->rotateCCW->setText(QChar(0x21ba));
	ui->rotateCW->setText(QChar(0x21bb));

	// Unicode triangles ...
	ui->textL->setText(QChar(0x25c0));
	ui->textR->setText(QChar(0x25b6));
	ui->textT->setText(QChar(0x25b2));
	ui->textB->setText(QChar(0x25bc));
}

void ControlDialog::SetMainWindow(MainWindow* p)
{
	m_pMainWindow = p;

	QObject::connect(ui->trackSlider,		SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(TrackSliderChanged(int)));
	QObject::connect(ui->saturationSlider,	SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SaturationSliderChanged(int)));
	QObject::connect(ui->compSlider,		SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(CompSliderChanged(int)));
	QObject::connect(ui->fillSlider,		SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(FillSliderChanged(int)));
	QObject::connect(ui->crop,				SIGNAL(clicked()),			m_pMainWindow,	SLOT(Crop()));
	QObject::connect(ui->margin,			SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(MarginChanged(int)));

	QObject::connect(ui->nameEdit,			SIGNAL(textChanged(const QString&)),		m_pMainWindow, SLOT(SetCompName(const QString&)));
	QObject::connect(ui->valueEdit,			SIGNAL(textChanged(const QString&)),		m_pMainWindow, SLOT(SetCompValue(const QString&)));
	QObject::connect(ui->typeComboBox,		SIGNAL(currentTextChanged(const QString&)),	m_pMainWindow, SLOT(SetCompType(const QString&)));
	QObject::connect(ui->rotateCCW,			SIGNAL(clicked()),			m_pMainWindow,	SLOT(CompRotateCCW()));
	QObject::connect(ui->rotateCW,			SIGNAL(clicked()),			m_pMainWindow,	SLOT(CompRotateCW()));
	QObject::connect(ui->grow,				SIGNAL(clicked()),			m_pMainWindow,	SLOT(CompGrow()));
	QObject::connect(ui->shrink,			SIGNAL(clicked()),			m_pMainWindow,	SLOT(CompShrink()));
	QObject::connect(ui->grow_2,			SIGNAL(clicked()),			m_pMainWindow,	SLOT(CompGrow2()));
	QObject::connect(ui->shrink_2,			SIGNAL(clicked()),			m_pMainWindow,	SLOT(CompShrink2()));
	QObject::connect(ui->textC,				SIGNAL(clicked()),			m_pMainWindow,	SLOT(CompTextCentre()));
	QObject::connect(ui->textL,				SIGNAL(clicked()),			m_pMainWindow,	SLOT(CompTextL()));
	QObject::connect(ui->textR,				SIGNAL(clicked()),			m_pMainWindow,	SLOT(CompTextR()));
	QObject::connect(ui->textT,				SIGNAL(clicked()),			m_pMainWindow,	SLOT(CompTextT()));
	QObject::connect(ui->textB,				SIGNAL(clicked()),			m_pMainWindow,	SLOT(CompTextB()));

	QObject::connect(ui->custom,			SIGNAL(toggled(bool)),		m_pMainWindow,	SLOT(SetCompCustomFlag(bool)));
	QObject::connect(ui->padWidth,			SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetCompPadWidth(int)));
	QObject::connect(ui->holeWidth,			SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SetCompHoleWidth(int)));
	QObject::connect(ui->brokenList,		SIGNAL(itemClicked(QListWidgetItem*)),	m_pMainWindow, SLOT(SetNodeId(QListWidgetItem*)));

	QObject::connect(ui->autoRoute,			SIGNAL(toggled(bool)),		m_pMainWindow,	SLOT(EnableRouting(bool)));
	QObject::connect(ui->autoRoute,			SIGNAL(toggled(bool)),		ui->paste,		SLOT(setEnabled(bool)));
	QObject::connect(ui->fast,				SIGNAL(toggled(bool)),		m_pMainWindow,	SLOT(EnableFastRouting(bool)));
	QObject::connect(ui->paste,				SIGNAL(clicked()),			m_pMainWindow,	SLOT(Paste()));
	QObject::connect(ui->tidy,				SIGNAL(clicked()),			m_pMainWindow,	SLOT(Tidy()));
	QObject::connect(ui->wipe,				SIGNAL(clicked()),			m_pMainWindow,	SLOT(WipeTracks()));

	QObject::connect(ui->autoColor,			SIGNAL(toggled(bool)),		m_pMainWindow,	SLOT(AutoColor(bool)));
	QObject::connect(ui->setColor,			SIGNAL(clicked()),			m_pMainWindow,	SLOT(SelectNodeColor()));
}

ControlDialog::~ControlDialog()
{
	delete ui;
}

void ControlDialog::ClearList()
{
	auto* pList = ui->brokenList;
	for (int i = 0; i < pList->count(); i++)
	{
		QListWidgetItem* p = pList->takeItem(i);
		if ( p ) delete p;
	}
	pList->clear();
}

void ControlDialog::SetListItem(const int nodeId)
{
	auto* pList = ui->brokenList;
	bool bFound(false);
	for (int i = 0; i < pList->count() && !bFound; i++)
	{
		const QString& str = pList->item(i)->text();
		bFound = ( str.left(str.size() - 11).toInt() == nodeId );	// 11 because of " (Floating)" suffix below
		if ( bFound ) pList->setCurrentRow(i);
	}
	if ( !bFound && pList->count() > 0 ) pList->setCurrentRow(0, QItemSelectionModel::Clear);

	ui->tidy->setEnabled( !ui->autoRoute->isChecked() && pList->count() == 0 );
}

void ControlDialog::AddListItem(const int nodeId, bool bFloating)
{
	const std::string str = std::to_string(nodeId) + ( bFloating ? " (Floating)" : "           " );
	ui->brokenList->addItem( str.c_str() );
}

void ControlDialog::UpdateCompControls()	// Component controls
{
	Board& board = m_pMainWindow->m_board;

	std::string nameStr(""), valueStr(""), typeStr("");
	bool bCustom(false);
	int iPadWidth(0), iHoleWidth(0);

	if ( board.GetGroupMgr().GetNumUserComps() == 1 )
	{
		const Component& comp = board.GetUserComponent();
		nameStr			= comp.GetNameStr();
		valueStr		= comp.GetValueStr();
		typeStr			= comp.GetTypeStr();
		bCustom			= comp.GetCustomPads();
		iPadWidth		= comp.GetPadWidth();
		iHoleWidth		= comp.GetHoleWidth();

		int currentIndex = ui->typeComboBox->findText( QString::fromStdString( typeStr ));
		if ( currentIndex != -1 )
			ui->typeComboBox->setCurrentIndex(currentIndex);
		else
		{
			currentIndex = 0;

			// Wipe the Type combobox and repopulate it
			ui->typeComboBox->blockSignals(true);	// Block signals while populating box
			ui->typeComboBox->clear();
			if ( board.GetDisableChangeType() )
				ui->typeComboBox->addItem(QString::fromStdString(typeStr));	// Single item
			else
			{
				int index(0);
				for (const auto& compType : GetListCompTypes())
				{
					if ( AllowTypeChange(comp.GetType(), compType) )	// Only put allowed types in combo
					{
						ui->typeComboBox->addItem(QString::fromStdString( GetDefaultTypeStr(compType) ));
						if ( compType == comp.GetType() ) currentIndex = index;
						index++;
					}
				}
			}
			ui->typeComboBox->blockSignals(false);	// We're done populating, so unblock signals
			ui->typeComboBox->setCurrentIndex(currentIndex);
		}
	}
	else
		ui->typeComboBox->clear();

	ui->nameEdit->setText(nameStr.c_str());
	ui->valueEdit->setText(valueStr.c_str());
	ui->nameEdit->show();
	ui->valueEdit->show();

	if ( ui->custom->isChecked() != bCustom ) ui->custom->setChecked(bCustom);
	if ( bCustom )
	{
		if ( ui->padWidth->value()  != iPadWidth  ) ui->padWidth->setValue(iPadWidth);
		if ( ui->holeWidth->value() != iHoleWidth ) ui->holeWidth->setValue(iHoleWidth);
		ui->padWidth->show();
		ui->holeWidth->show();
		ui->label_pad->show();
		ui->label_hole->show();
	}
	else
	{
		ui->padWidth->hide();
		ui->holeWidth->hide();
		ui->label_pad->hide();
		ui->label_hole->hide();
	}

	const bool bCompEdit		= board.GetCompEdit();
	const bool bNoTrackOptions	= board.GetTrackMode() == TRACKMODE::OFF;
	const bool bVero			= board.GetVeroTracks();
	const bool bNoText			= bCompEdit || board.GetDisableCompText();
	const bool bNoRotate		= bCompEdit || board.GetDisableRotate();

	// Disable controls	...
	ui->nameEdit->setDisabled(		bNoText );
	ui->valueEdit->setDisabled(		bNoText );
	ui->textC->setDisabled(			bNoText );
	ui->textL->setDisabled(			bNoText );
	ui->textR->setDisabled(			bNoText );
	ui->textT->setDisabled(			bNoText );
	ui->textB->setDisabled(			bNoText );
	ui->rotateCCW->setDisabled(		bNoRotate );
	ui->rotateCW->setDisabled(		bNoRotate );
	ui->shrink->setDisabled(		bCompEdit || board.GetDisableStretch(false) );
	ui->grow->setDisabled(			bCompEdit || board.GetDisableStretch(true) );
	ui->shrink_2->setDisabled(		bCompEdit || board.GetDisableStretchWidth(false) );
	ui->grow_2->setDisabled(		bCompEdit || board.GetDisableStretchWidth(true) );
	ui->typeComboBox->setDisabled(	bCompEdit || board.GetDisableChangeType() );
	ui->custom->setDisabled(		bCompEdit || bNoTrackOptions || bVero || board.GetDisableChangeCustom() );
	ui->padWidth->setDisabled(		bCompEdit || bNoTrackOptions || bVero || board.GetDisableChangeCustom() || !ui->custom->isChecked() );
	ui->holeWidth->setDisabled(		bCompEdit || bNoTrackOptions || bVero || board.GetDisableChangeCustom() || !ui->custom->isChecked() );

	// ... and the corresponding labels
	ui->label_name->setDisabled(	bNoText );
	ui->label_value->setDisabled(	bNoText );
	ui->label_label->setDisabled(	bNoText );
	ui->label_rotate->setDisabled(	bNoRotate );
	ui->label_length->setDisabled(	bCompEdit || (board.GetDisableStretch(false)      && board.GetDisableStretch(true)) );
	ui->label_width->setDisabled(	bCompEdit || (board.GetDisableStretchWidth(false) && board.GetDisableStretchWidth(true)) );
	ui->label_type->setDisabled(	bCompEdit || board.GetDisableChangeType() );
	ui->label_pad->setDisabled(		bCompEdit || bNoTrackOptions || bVero || board.GetDisableChangeCustom() );
	ui->label_hole->setDisabled(	bCompEdit || bNoTrackOptions || bVero || board.GetDisableChangeCustom() );
}

void ControlDialog::UpdateControls()	// Non-component controls
{
	Board& board = m_pMainWindow->m_board;

	const bool bCompEdit	= board.GetCompEdit();
	const bool bNoTracks	= board.GetTrackMode() == TRACKMODE::OFF;
	const bool bColor		= board.GetTrackMode() == TRACKMODE::COLOR;
	const bool bComps		= board.GetCompMode()  != COMPSMODE::OFF;

	ui->crop->setDisabled( bCompEdit );
	ui->margin->setDisabled( bCompEdit );
	ui->margin->setValue( board.GetCropMargin() );

	ui->label_saturation->setEnabled( !bCompEdit && bColor );
	ui->label_fill->setEnabled( !bCompEdit && bComps && ( bColor || bNoTracks) );

	ui->autoRoute->setChecked( board.GetRoutingEnabled() );
	ui->autoRoute->setDisabled( bCompEdit );
	ui->fast->setChecked( board.GetRoutingMethod() == 0 );
	ui->fast->setDisabled( bCompEdit );
	ui->wipe->setDisabled( bCompEdit || board.GetDisableWipe() );

	// Do sliders last (they can trigger a redraw)
	ui->trackSlider->setEnabled( !bCompEdit );
	ui->saturationSlider->setEnabled( !bCompEdit && bColor );
	ui->compSlider->setEnabled( !bCompEdit );
	ui->fillSlider->setEnabled( !bCompEdit && bComps && ( bColor || bNoTracks) );
	ui->trackSlider->setValue( board.GetTrackSliderValue() );
	ui->saturationSlider->setValue( board.GetSaturation() );
	ui->compSlider->setValue( board.GetCompSliderValue() );
	ui->fillSlider->setValue( board.GetFillSaturation() );

	const bool bNodeIdOK = board.GetCurrentNodeId() != BAD_NODEID;
	ui->autoColor->setEnabled( bColor && bNodeIdOK );
	ui->setColor->setEnabled( bColor && bNodeIdOK );
	ui->autoColor->setChecked( bNodeIdOK && !board.GetColorMgr().GetIsFixed( board.GetCurrentNodeId() ) );

	QColor color = bColor ? board.GetColorMgr().GetColorFromNodeId( board.GetCurrentNodeId(), false ) : Qt::black;
	ui->setColor->setStyleSheet("border:2px solid " + color.name());
}

void ControlDialog::wheelEvent(QWheelEvent* event)
{
	QWidget::wheelEvent(event);
	event->accept();
}

void ControlDialog::mousePressEvent(QMouseEvent* event)
{
	QWidget::mousePressEvent(event);
	event->accept();
}

void ControlDialog::mouseDoubleClickEvent(QMouseEvent* event)
{
	QWidget::mouseDoubleClickEvent(event);
	event->accept();
}

void ControlDialog::mouseMoveEvent(QMouseEvent* event)
{
	QWidget::mouseMoveEvent(event);
	event->accept();
}

void ControlDialog::mouseReleaseEvent(QMouseEvent* event)
{
	QWidget::mouseReleaseEvent(event);
	event->accept();
}

void ControlDialog::keyPressEvent(QKeyEvent* event)
{
	m_pMainWindow->specialKeyPressEvent(event);
	QWidget::keyPressEvent(event);
	event->accept();
}

void ControlDialog::keyReleaseEvent(QKeyEvent* event)
{
	m_pMainWindow->commonKeyReleaseEvent(event);
	QWidget::keyReleaseEvent(event);
	event->accept();
}
