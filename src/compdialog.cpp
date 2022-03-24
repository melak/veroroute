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

#include "compdialog.h"
#include "ui_compdialog.h"
#include "mainwindow.h"

CompDialog::CompDialog(QWidget* parent)
: QWidget(parent)
, ui(new Ui_CompDialog)
, m_pMainWindow(nullptr)
{
	ui->setupUi( reinterpret_cast<QDialog*>(this) );

	QFont font = ui->pushButtonU->font();
	font.setFamily(QString("Arial Unicode MS"));
	font.setPointSize(12);
	ui->pushButtonU->setFont(font);
	ui->pushButtonD->setFont(font);

	// Unicode triangles ...
	ui->pushButtonU->setText(QChar(0x25b3));
	ui->pushButtonD->setText(QChar(0x25bd));

	ui->spinBox_Width->installEventFilter( this );	// Prevent accidental wheel behaviour from wiping the footprint
	ui->spinBox_Height->installEventFilter( this );	// Prevent accidental wheel behaviour from wiping the footprint

	ui->comboBox_Shape->blockSignals(true);		// Block signals while populating box
	ui->comboBox_Shape->clear();
	for (const auto& str : Shape::GetListShapeStrings())
		ui->comboBox_Shape->addItem(QString::fromStdString( str ));
	ui->comboBox_Shape->blockSignals(false);	// We're done populating, so unblock signals

	ui->comboBox_Surface->blockSignals(true);	// Block signals while populating box
	for (const auto& str : Pin::GetListSurfaceStrings())
		ui->comboBox_Surface->addItem(QString::fromStdString( str ));
	ui->comboBox_Surface->blockSignals(false);	// We're done populating, so unblock signals

	ui->comboBox_PinShape->blockSignals(true);	// Block signals while populating box
	ui->comboBox_PinShape->clear();
	ui->comboBox_PinShape->addItem(QString("Circle"));
	ui->comboBox_PinShape->addItem(QString("Rectangle"));
	ui->comboBox_PinShape->blockSignals(false);	// We're done populating, so unblock signals

#ifdef Q_OS_ANDROID
	QFont tmp = ui->comboBox_PinShape->font();
	tmp.setPointSize(12);
	ui->comboBox_PinShape->setFont(tmp);
	ui->comboBox_Surface->setFont(tmp);
	ui->comboBox_Shape->setFont(tmp);
#endif
}

void CompDialog::SetMainWindow(MainWindow* p)
{
	m_pMainWindow = p;

	QObject::connect(ui->lineEdit_Value,	SIGNAL(textChanged(QString)),			m_pMainWindow,	SLOT(DefinerSetValueStr(QString)));
	QObject::connect(ui->lineEdit_Prefix,	SIGNAL(textChanged(QString)),			m_pMainWindow,	SLOT(DefinerSetPrefixStr(QString)));
	QObject::connect(ui->lineEdit_Type,		SIGNAL(textChanged(QString)),			m_pMainWindow,	SLOT(DefinerSetTypeStr(QString)));
	QObject::connect(ui->lineEdit_Import,	SIGNAL(textChanged(QString)),			m_pMainWindow,	SLOT(DefinerSetImportStr(QString)));
	QObject::connect(ui->spinBox_Width,		SIGNAL(valueChanged(int)),				m_pMainWindow,	SLOT(DefinerWidthChanged(int)));
	QObject::connect(ui->spinBox_Height,	SIGNAL(valueChanged(int)),				m_pMainWindow,	SLOT(DefinerHeightChanged(int)));
	QObject::connect(ui->comboBox_PinShape,	SIGNAL(currentTextChanged(QString)),	m_pMainWindow,	SLOT(DefinerSetPinShapeType(QString)));
	QObject::connect(ui->checkBox_PinLabels,SIGNAL(toggled(bool)),					m_pMainWindow,	SLOT(DefinerToggledPinLabels(bool)));
	QObject::connect(ui->custom,			SIGNAL(toggled(bool)),					m_pMainWindow,	SLOT(DefinerToggledCustomFlag(bool)));
	QObject::connect(ui->padWidth,			SIGNAL(valueChanged(int)),				m_pMainWindow,	SLOT(DefinerPadWidthChanged(int)));
	QObject::connect(ui->holeWidth,			SIGNAL(valueChanged(int)),				m_pMainWindow,	SLOT(DefinerHoleWidthChanged(int)));
	QObject::connect(ui->spinBox_PinNumber,	SIGNAL(valueChanged(int)),				m_pMainWindow,	SLOT(DefinerSetPinNumber(int)));
	QObject::connect(ui->comboBox_Surface,	SIGNAL(currentTextChanged(QString)),	m_pMainWindow,	SLOT(DefinerSetSurface(QString)));
	QObject::connect(ui->pushButtonRGB,		SIGNAL(clicked()),						m_pMainWindow,	SLOT(DefinerChooseColor()));
	QObject::connect(ui->pushButtonU,		SIGNAL(clicked()),						m_pMainWindow,	SLOT(DefinerRaise()));
	QObject::connect(ui->pushButtonD,		SIGNAL(clicked()),						m_pMainWindow,	SLOT(DefinerLower()));
	QObject::connect(ui->comboBox_Shape,	SIGNAL(currentTextChanged(QString)),	m_pMainWindow,	SLOT(DefinerSetShapeType(QString)));
	QObject::connect(ui->checkBox_Line,		SIGNAL(toggled(bool)),					m_pMainWindow,	SLOT(DefinerToggleShapeLine(bool)));
	QObject::connect(ui->checkBox_Fill,		SIGNAL(toggled(bool)),					m_pMainWindow,	SLOT(DefinerToggleShapeFill(bool)));
	QObject::connect(ui->doubleSpinBox_CX,	SIGNAL(valueChanged(double)),			m_pMainWindow,	SLOT(DefinerSetCX(double)));
	QObject::connect(ui->doubleSpinBox_CY,	SIGNAL(valueChanged(double)),			m_pMainWindow,	SLOT(DefinerSetCY(double)));
	QObject::connect(ui->doubleSpinBox_DX,	SIGNAL(valueChanged(double)),			m_pMainWindow,	SLOT(DefinerSetDX(double)));
	QObject::connect(ui->doubleSpinBox_DY,	SIGNAL(valueChanged(double)),			m_pMainWindow,	SLOT(DefinerSetDY(double)));
	QObject::connect(ui->doubleSpinBox_A1,	SIGNAL(valueChanged(double)),			m_pMainWindow,	SLOT(DefinerSetA1(double)));
	QObject::connect(ui->doubleSpinBox_A2,	SIGNAL(valueChanged(double)),			m_pMainWindow,	SLOT(DefinerSetA2(double)));
	QObject::connect(ui->doubleSpinBox_A3,	SIGNAL(valueChanged(double)),			m_pMainWindow,	SLOT(DefinerSetA3(double)));
	QObject::connect(ui->pushButton_Build,	SIGNAL(clicked()),						m_pMainWindow,	SLOT(DefinerBuild()));
}

bool CompDialog::eventFilter(QObject* object, QEvent* event)
{
	if ( event->type() == QEvent::Wheel && qobject_cast<QAbstractSpinBox*>( object ) )
	{
		event->ignore();
		return true;
	}
	return QWidget::eventFilter(object, event);
}

CompDialog::~CompDialog()
{
	delete ui;
}

void CompDialog::Update()
{
	m_bUpdatingControls = true;

	CompDefiner&	def				= m_pMainWindow->GetCompDefiner();
	const bool		bValidPinId		= BAD_ID != def.GetCurrentPinId();
	const bool		bValidShapeId	= BAD_ID != def.GetCurrentShapeId();

	ui->lineEdit_Value->setText( def.GetValueStr().c_str() );
	ui->lineEdit_Prefix->setText( def.GetPrefixStr().c_str() );
	ui->lineEdit_Type->setText( def.GetTypeStr().c_str() );
	ui->lineEdit_Import->setText( def.GetImportStr().c_str() );

	ui->lineEdit_Value->show();
	ui->lineEdit_Prefix->show();
	ui->lineEdit_Type->show();
	ui->lineEdit_Import->show();

	ui->spinBox_Width->setValue( def.GetGridCols() );
	ui->spinBox_Height->setValue( def.GetGridRows() );

	ui->comboBox_PinShape->setCurrentIndex( ( (def.GetPinFlags() & PIN_RECT) > 0 ) ? 1 : 0 );
	ui->checkBox_PinLabels->setChecked( def.GetPinFlags() & PIN_LABELS );

	ui->custom->setChecked( def.GetPinFlags() & PIN_CUSTOM );
	ui->padWidth->setValue( def.GetPadWidth() );
	ui->holeWidth->setValue( def.GetHoleWidth() );

	const Pin& pin = def.GetCurrentPin();
	if ( bValidPinId )
	{
		ui->spinBox_PinNumber->setValue( static_cast<int>(pin.GetPinIndex() + 1) );
		ui->comboBox_Surface->setCurrentIndex( pin.GetSurface() == SURFACE_FULL ? 0 :
											   pin.GetSurface() == SURFACE_FREE ? 1 : 2 );	// Set to match order in which combo was populated
	}

	MyRGB rgb;
	if ( bValidShapeId )
	{
		const Shape& s = def.GetCurrentShape();
		ui->comboBox_Shape->setCurrentIndex( static_cast<int>( s.GetType() ) );	// Works because SHAPE enum (0,1,2,...) matches how combo was populated
		ui->checkBox_Line->setChecked( s.GetDrawLine() );
		ui->checkBox_Fill->setChecked( s.GetDrawFill() );
		ui->doubleSpinBox_CX->setValue(  s.GetCX() );
		ui->doubleSpinBox_CY->setValue( -s.GetCY() );	// Control assumes CY goes up
		ui->doubleSpinBox_DX->setValue(  s.GetDX() );
		ui->doubleSpinBox_DY->setValue(  s.GetDY() );
		ui->doubleSpinBox_A1->setValue(  s.GetA1() );
		ui->doubleSpinBox_A2->setValue(  s.GetA2() );
		ui->doubleSpinBox_A3->setValue(  s.GetA3() );
		rgb = s.GetFillColor();
	}
	ui->pushButtonRGB->setStyleSheet("border:2px solid " + rgb.GetQColor().name());
	EnableControls();

	m_bUpdatingControls = false;
}

void CompDialog::EnableControls()	// Enable/disable controls
{
	CompDefiner&	def					= m_pMainWindow->GetCompDefiner();
	const bool		bValidPinId			= BAD_ID != def.GetCurrentPinId();
	const bool		bValidShapeId		= BAD_ID != def.GetCurrentShapeId();
	const bool		bValidDefinition	= def.GetIsValid();
	const bool		bCustom				= def.GetPinFlags() & PIN_CUSTOM;
	if ( bCustom )
	{
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

	bool bAngle(false);	// true ==> show angle controls
	bool bFill(false);	// true ==> allow fill option on shapr
	if ( bValidShapeId )
	{
		const SHAPE& eType = def.GetCurrentShape().GetType();
		bAngle = eType == SHAPE::ARC  || eType == SHAPE::CHORD;
		bFill  = eType != SHAPE::LINE && eType != SHAPE::ARC;
	}

	ui->label_Type->setText( StringHelper::IsEmptyStr( def.GetTypeStr() ) ? "Type *" : "Type");
	ui->label_Value->setText( StringHelper::IsEmptyStr( def.GetValueStr() ) ? "Value *" : "Value");

	ui->label_PinNumber->setEnabled( bValidPinId );
	ui->spinBox_PinNumber->setEnabled( bValidPinId );
	ui->comboBox_Surface->setEnabled( bValidPinId && !def.GetCurrentPin().GetIsPin() );
	ui->label_CX->setEnabled( bValidShapeId );
	ui->label_CY->setEnabled( bValidShapeId );
	ui->label_DX->setEnabled( bValidShapeId );
	ui->label_DY->setEnabled( bValidShapeId );
	ui->label_A1->setEnabled( bAngle );
	ui->label_A2->setEnabled( bAngle );
	ui->label_A3->setEnabled( bValidShapeId );
	ui->label_Shape->setEnabled( bValidShapeId );
	ui->comboBox_Shape->setEnabled( bValidShapeId );
	ui->checkBox_Line->setEnabled( bFill && ui->checkBox_Fill->isChecked() );
	ui->checkBox_Fill->setEnabled( bFill && ui->checkBox_Line->isChecked() );
	ui->pushButtonRGB->setEnabled( bFill && ui->checkBox_Fill->isChecked() );
	ui->pushButtonU->setEnabled( def.GetCanRaise() );
	ui->pushButtonD->setEnabled( def.GetCanLower() );
	ui->doubleSpinBox_CX->setEnabled( bValidShapeId );
	ui->doubleSpinBox_CY->setEnabled( bValidShapeId );
	ui->doubleSpinBox_DX->setEnabled( bValidShapeId );
	ui->doubleSpinBox_DY->setEnabled( bValidShapeId );
	ui->doubleSpinBox_A1->setEnabled( bAngle );
	ui->doubleSpinBox_A2->setEnabled( bAngle );
	ui->doubleSpinBox_A3->setEnabled( bValidShapeId );
	ui->pushButton_Build->setEnabled( bValidDefinition );
}

void CompDialog::wheelEvent(QWheelEvent* event)
{
	QWidget::wheelEvent(event);
	event->accept();
}

void CompDialog::mousePressEvent(QMouseEvent* event)
{
	QWidget::mousePressEvent(event);
	event->accept();
}

void CompDialog::mouseDoubleClickEvent(QMouseEvent* event)
{
	QWidget::mouseDoubleClickEvent(event);
	event->accept();
}

void CompDialog::mouseMoveEvent(QMouseEvent* event)
{
	QWidget::mouseMoveEvent(event);
	event->accept();
}

void CompDialog::mouseReleaseEvent(QMouseEvent* event)
{
	QWidget::mouseReleaseEvent(event);
	event->accept();
}

void CompDialog::keyPressEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	m_pMainWindow->specialKeyPressEvent(event);
#endif
	QWidget::keyPressEvent(event);
	event->accept();
}

void CompDialog::keyReleaseEvent(QKeyEvent* event)
{
#ifdef VEROROUTE_ANDROID
	if ( event->key() == Qt::Key_Back )
		return m_pMainWindow->keyReleaseEvent(event);	// Try Undo operation
#else
	m_pMainWindow->commonKeyReleaseEvent(event);
#endif
	QWidget::keyReleaseEvent(event);
	event->accept();
}
