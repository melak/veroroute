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

#include "textdialog.h"
#include "ui_textdialog.h"
#include "mainwindow.h"

TextDialog::TextDialog(MainWindow* parent)
: QDialog(parent)
, ui(new Ui::TextDialog)
, m_pMainWindow(parent)
{
	ui->setupUi(this);
#ifndef VEROROUTE_ANDROID
	ui->pushButtonOK->hide();
	ui->plainTextEdit->resize(ui->plainTextEdit->width() + 40, ui->plainTextEdit->height());
#endif
	QObject::connect(ui->spinBox,		SIGNAL(valueChanged(int)),	m_pMainWindow,	SLOT(SizeChanged(int)));
	QObject::connect(ui->pushButtonB,	SIGNAL(clicked()),			m_pMainWindow,	SLOT(ToggleBold()));
	QObject::connect(ui->pushButtonI,	SIGNAL(clicked()),			m_pMainWindow,	SLOT(ToggleItalic()));
	QObject::connect(ui->pushButtonU,	SIGNAL(clicked()),			m_pMainWindow,	SLOT(ToggleUnderline()));
	QObject::connect(ui->pushButtonRGB,	SIGNAL(clicked()),			m_pMainWindow,	SLOT(ChooseTextColor()));
	QObject::connect(ui->pushButtonL,	SIGNAL(clicked()),			m_pMainWindow,	SLOT(AlignL()));
	QObject::connect(ui->pushButtonC,	SIGNAL(clicked()),			m_pMainWindow,	SLOT(AlignC()));
	QObject::connect(ui->pushButtonR,	SIGNAL(clicked()),			m_pMainWindow,	SLOT(AlignR()));
	QObject::connect(ui->pushButtonJ,	SIGNAL(clicked()),			m_pMainWindow,	SLOT(AlignJ()));
	QObject::connect(ui->pushButtonTop,	SIGNAL(clicked()),			m_pMainWindow,	SLOT(AlignTop()));
	QObject::connect(ui->pushButtonMid,	SIGNAL(clicked()),			m_pMainWindow,	SLOT(AlignMid()));
	QObject::connect(ui->pushButtonBot,	SIGNAL(clicked()),			m_pMainWindow,	SLOT(AlignBot()));
	QObject::connect(ui->plainTextEdit,	SIGNAL(textChanged()),		this,			SLOT(TextChanged()));
	QObject::connect(ui->pushButtonOK,	SIGNAL(clicked()),			this,			SLOT(hide()));
}

TextDialog::~TextDialog()
{
	delete ui;
}

void TextDialog::Clear()
{
	ui->plainTextEdit->setPlainText( QString("") );
}

void TextDialog::Update(const TextRect& rect, bool bFull)
{
	ui->spinBox->setValue( rect.GetSize() );
	ui->pushButtonB->setChecked(rect.GetStyle() & TEXT_BOLD);
	ui->pushButtonI->setChecked(rect.GetStyle() & TEXT_ITALIC);
	ui->pushButtonU->setChecked(rect.GetStyle() & TEXT_UNDERLINE);
	ui->pushButtonL->setChecked(rect.GetFlagsH() == Qt::AlignLeft);
	ui->pushButtonC->setChecked(rect.GetFlagsH() == Qt::AlignHCenter);
	ui->pushButtonR->setChecked(rect.GetFlagsH() == Qt::AlignRight);
	ui->pushButtonJ->setChecked(rect.GetFlagsH() == Qt::AlignJustify);
	ui->pushButtonTop->setChecked(rect.GetFlagsV() == Qt::AlignTop);
	ui->pushButtonMid->setChecked(rect.GetFlagsV() == Qt::AlignVCenter);
	ui->pushButtonBot->setChecked(rect.GetFlagsV() == Qt::AlignBottom);
	ui->pushButtonRGB->setStyleSheet("border:2px solid " + rect.GetQColor().name());
	if ( bFull )
		ui->plainTextEdit->setPlainText( QString::fromStdString(rect.GetStr()) );
}

void TextDialog::TextChanged()
{
	m_pMainWindow->SetText( ui->plainTextEdit->toPlainText() );
}

void TextDialog::keyPressEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	m_pMainWindow->specialKeyPressEvent(event);
#endif
	QDialog::keyPressEvent(event);
}

void TextDialog::keyReleaseEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	m_pMainWindow->commonKeyReleaseEvent(event);
#endif
	QDialog::keyReleaseEvent(event);
}
