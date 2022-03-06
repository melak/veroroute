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

#include "finddialog.h"
#include "ui_finddialog.h"
#include "mainwindow.h"

FindDialog::FindDialog(MainWindow* parent)
: QDialog(parent)
, ui(new Ui_FindDialog)
, m_pMainWindow(parent)
{
	ui->setupUi(this);
	ui->radioName->setChecked(true);
	QObject::connect(ui->radioName,		SIGNAL(toggled(bool)),			this,	SLOT(ToggleName(bool)));
	QObject::connect(ui->checkExact,	SIGNAL(toggled(bool)),			this,	SLOT(ToggleExact(bool)));
	QObject::connect(ui->nameEdit,		SIGNAL(textChanged(QString)),	this,	SLOT(TextChanged(QString)));
}

FindDialog::~FindDialog()
{
	delete ui;
}

void FindDialog::UpdateControls()
{
	if ( isHidden() && !m_str.isEmpty() )
		ui->nameEdit->setText(QString(""));
}

void FindDialog::ToggleName(bool b)
{
	m_bName = b;
	m_pMainWindow->Find(m_bName, m_bExact, m_str);
}

void FindDialog::ToggleExact(bool b)
{
	m_bExact = b;
	m_pMainWindow->Find(m_bName, m_bExact, m_str);
}

void FindDialog::TextChanged(const QString& str)
{
	m_str = str;
	m_pMainWindow->Find(m_bName, m_bExact, m_str);
}

void FindDialog::closeEvent(QCloseEvent* event)
{
	ui->nameEdit->setText(QString(""));
	QDialog::closeEvent(event);
}

void FindDialog::keyPressEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	m_pMainWindow->specialKeyPressEvent(event);
#endif
	QDialog::keyPressEvent(event);
}

void FindDialog::keyReleaseEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	m_pMainWindow->commonKeyReleaseEvent(event);
#endif
	QDialog::keyReleaseEvent(event);
}
