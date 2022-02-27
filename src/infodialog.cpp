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

#include "infodialog.h"
#include "ui_infodialog.h"
#include "mainwindow.h"

InfoDialog::InfoDialog(MainWindow* parent)
: QDialog(parent)
, ui(new Ui::InfoDialog)
, m_pMainWindow(parent)
{
	ui->setupUi(this);
#ifndef Q_OS_ANDROID
	ui->pushButtonOK->hide();
	ui->prev->move(ui->prev->x() + 30, ui->prev->y());
	ui->reload->move(ui->reload->x() + 30, ui->reload->y());
	ui->next->move(ui->next->x() + 30, ui->next->y());
#endif
	ShowButtons(false);
	QObject::connect(ui->plainTextEdit,	SIGNAL(textChanged()),	this,			SLOT(TextChanged()));
	QObject::connect(ui->prev,			SIGNAL(clicked()),		m_pMainWindow,	SLOT(LoadPrevTutorial()));
	QObject::connect(ui->reload,		SIGNAL(clicked()),		m_pMainWindow,	SLOT(LoadTutorial()));
	QObject::connect(ui->next,			SIGNAL(clicked()),		m_pMainWindow,	SLOT(LoadNextTutorial()));
	QObject::connect(ui->pushButtonOK,	SIGNAL(clicked()),		this,			SLOT(hide()));
}

InfoDialog::~InfoDialog()
{
	delete ui;
}

void InfoDialog::SetReadOnly(bool b)
{
	ui->plainTextEdit->setReadOnly(b);
}

void InfoDialog::TextChanged()
{
	m_pMainWindow->SetInfoStr( ui->plainTextEdit->toPlainText() );
}

void InfoDialog::ShowButtons(bool b)
{
	ui->prev->setVisible(b);
	ui->reload->setVisible(b);
	ui->next->setVisible(b);
}

void InfoDialog::EnablePrev(bool b)
{
	ui->prev->setEnabled(b);
}

void InfoDialog::EnableNext(bool b)
{
	ui->next->setEnabled(b);
}

void InfoDialog::Update()
{
	m_initialStr = m_pMainWindow->m_board.GetInfoStr();
	ui->plainTextEdit->setPlainText( QString::fromStdString(m_initialStr) );
}

bool InfoDialog::GetIsModified()
{
	return m_initialStr != m_pMainWindow->m_board.GetInfoStr();;
}

void InfoDialog::keyPressEvent(QKeyEvent* event)
{
	// In tutorial mode, forward event to main window (apart from special cases)
	bool bForwardToMainWindow = ui->plainTextEdit->isReadOnly();
	switch( event->key() )
	{
		case Qt::Key_Tab:
		case Qt::Key_N:
		case Qt::Key_O:
		case Qt::Key_M:
		case Qt::Key_S:
		case Qt::Key_Q:		bForwardToMainWindow = false;
	}
	if ( bForwardToMainWindow )
		return m_pMainWindow->keyPressEvent(event);
	m_pMainWindow->specialKeyPressEvent(event);
	QDialog::keyPressEvent(event);
}

void InfoDialog::keyReleaseEvent(QKeyEvent* event)
{
	// In tutorial mode, forward event to main window
	if ( ui->plainTextEdit->isReadOnly() )
		return m_pMainWindow->keyReleaseEvent(event);
	m_pMainWindow->commonKeyReleaseEvent(event);
	QDialog::keyReleaseEvent(event);
}
