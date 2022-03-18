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

InfoDialog::InfoDialog(QWidget* parent)
: QWidget(parent)
, ui(new Ui_InfoDialog)
, m_pMainWindow(nullptr)
{
	ui->setupUi( reinterpret_cast<QDialog*>(this) );
#ifdef VEROROUTE_ANDROID
	ui->textEdit->verticalScrollBar()->setStyleSheet( ANDROID_VSCROLL_WIDTH );
	ui->textEdit->horizontalScrollBar()->setStyleSheet( ANDROID_HSCROLL_HEIGHT );
#endif
	ShowButtons(false);
}

InfoDialog::~InfoDialog()
{
	delete ui;
}

void InfoDialog::SetMainWindow(MainWindow* p)
{
	m_pMainWindow = p;
	QObject::connect(ui->textEdit,	SIGNAL(textChanged()),	this,			SLOT(TextChanged()));
	QObject::connect(ui->prev,		SIGNAL(clicked()),		m_pMainWindow,	SLOT(LoadPrevTutorial()));
	QObject::connect(ui->reload,	SIGNAL(clicked()),		m_pMainWindow,	SLOT(LoadTutorial()));
	QObject::connect(ui->next,		SIGNAL(clicked()),		m_pMainWindow,	SLOT(LoadNextTutorial()));
}

void InfoDialog::SetReadOnly(bool b)
{
	ui->textEdit->setReadOnly(b);
}

void InfoDialog::TextChanged()
{
	m_pMainWindow->SetInfoStr( ui->textEdit->toPlainText() );
}

void InfoDialog::ShowButtons(bool b)
{
	ui->prev->setVisible(b);
	ui->reload->setVisible(b);
	ui->next->setVisible(b);
	// Set bigger text edit box if we hide the buttons
	QRect rect = ui->textEdit->geometry();
	rect.setHeight(b ? 591 : 630);
	ui->textEdit->setGeometry(rect);
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
	ui->textEdit->clear();
	QTextCursor			textCursor		= ui->textEdit->textCursor();
	QTextBlockFormat	textBlockFormat	= textCursor.blockFormat();
	//textBlockFormat.setAlignment(Qt::AlignJustify);	//TODO See if tutorials can be written so this is useable
	textCursor.mergeBlockFormat(textBlockFormat);
	ui->textEdit->setTextCursor(textCursor);
	ui->textEdit->append( QString::fromStdString(m_initialStr) );
	ui->textEdit->verticalScrollBar()->setSliderPosition(0);
}

bool InfoDialog::GetIsModified()
{
	return m_initialStr != m_pMainWindow->m_board.GetInfoStr();;
}

void InfoDialog::keyPressEvent(QKeyEvent* event)
{
	// In tutorial mode, forward event to main window (apart from special cases)
	bool bForwardToMainWindow = ui->textEdit->isReadOnly();
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
#ifndef VEROROUTE_ANDROID
	m_pMainWindow->specialKeyPressEvent(event);
#endif
	QWidget::keyPressEvent(event);
	event->accept();
}

void InfoDialog::keyReleaseEvent(QKeyEvent* event)
{
	// In tutorial mode, forward event to main window
	if ( ui->textEdit->isReadOnly() )
		return m_pMainWindow->keyReleaseEvent(event);
#ifdef VEROROUTE_ANDROID
	if ( event->key() == Qt::Key_Back )
		return m_pMainWindow->keyReleaseEvent(event);	// Try Undo operation
#else
	m_pMainWindow->commonKeyReleaseEvent(event);
#endif
	QWidget::keyReleaseEvent(event);
	event->accept();
}
