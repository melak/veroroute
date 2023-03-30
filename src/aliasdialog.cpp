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

#include "aliasdialog.h"
#include "ui_aliasdialog.h"
#include "mainwindow.h"
#include <QtGlobal>

AliasDialog::AliasDialog(MainWindow* parent)
: QDialog(parent)
, ui(new Ui_AliasDialog)
, m_pMainWindow(parent)
, m_iRow(-1)
{
	ui->setupUi(this);
#ifdef VEROROUTE_ANDROID
	ui->tableWidget->verticalScrollBar()->setStyleSheet( ANDROID_VSCROLL_WIDTH );
	ui->tableWidget->horizontalScrollBar()->setStyleSheet( ANDROID_HSCROLL_HEIGHT );
	ui->tableWidget_2->verticalScrollBar()->setStyleSheet( ANDROID_VSCROLL_WIDTH );
	ui->tableWidget_2->horizontalScrollBar()->setStyleSheet( ANDROID_HSCROLL_HEIGHT );
#endif
	QObject::connect(ui->pushButton,	SIGNAL(clicked()),				m_pMainWindow,	SLOT(ReImport()));
	QObject::connect(ui->pushButton_2,	SIGNAL(clicked()),				this,			SLOT(DeleteRow()));
	QObject::connect(ui->pushButton_3,	SIGNAL(clicked()),				this,			SLOT(DeleteAllRows()));
	QObject::connect(ui->tableWidget_2,	SIGNAL(cellChanged(int,int)),	this,			SLOT(CellChanged(int,int)));
	QObject::connect(ui->tableWidget_2,	SIGNAL(cellPressed(int, int)),	this,			SLOT(CellPressed(int,int)));
	QObject::connect(this,				SIGNAL(rejected()),				m_pMainWindow,	SLOT(UpdateControls()));	// Close using X button
}

AliasDialog::~AliasDialog()
{
	delete ui;
}

void AliasDialog::Configure(const std::string& filename, bool bTango)
{
	m_filename	= filename;
	m_bTango	= bTango;
}

struct RowData
{
	RowData(const std::string& aliasStr, const std::string& importStr) : m_aliasStr(aliasStr), m_importStr(importStr) {}
	std::string m_aliasStr;		// Alias string
	std::string m_importStr;	// Valid import string
};

struct IsEarlierRow
{
	bool operator()(const RowData* pA, const RowData* pB) const
	{
		// Want rows with missing import strings on the end of the list
		if ( pA->m_importStr.empty() && !pB->m_importStr.empty() ) return false;
		if ( pB->m_importStr.empty() && !pA->m_importStr.empty() ) return true;
		if ( pA->m_importStr != pB->m_importStr ) return pA->m_importStr < pB->m_importStr;
		return pA->m_aliasStr < pB->m_aliasStr;
	}
};

void AliasDialog::Update()
{
	m_bUpdating = true;

	std::list<StringPair> strList;	// List of valid import strings
	m_pMainWindow->m_templateMgr.CalcValidImportStrings(strList);	// Calculate list, and make sure each one is not listed as an alias

	ui->pushButton->setEnabled( !m_filename.empty() );	// Disable Re-Import button if no filename

	// Set up the table of (valid) import strings
	ui->tableWidget->clear();
	ui->tableWidget->setColumnCount(2);
	ui->tableWidget->setColumnWidth(0,180);
	ui->tableWidget->setColumnWidth(1,510);
	m_tableHeader << "Valid Import Strings" << "Notes";
	ui->tableWidget->setHorizontalHeaderLabels(m_tableHeader);
	ui->tableWidget->verticalHeader()->setVisible(false);
	ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	ui->tableWidget->setShowGrid(true);

	// Populate the table with data
	int nImport(0);
	for (auto& strPair : strList)
	{
		// Write row to table.	Note: No memory leak since setItem() takes ownership.
		auto pItemA = new QTableWidgetItem(QString::fromStdString(strPair.m_importStr));
		auto pItemB = new QTableWidgetItem(QString::fromStdString(strPair.m_notesStr));
		pItemA->setFlags(Qt::NoItemFlags);
		pItemB->setFlags(Qt::NoItemFlags);
		ui->tableWidget->setItem(nImport, 0, pItemA);
		ui->tableWidget->setItem(nImport, 1, pItemB);
		nImport++;		
	}
	ui->tableWidget->setRowCount(nImport);

	//=============================================================

	// Set up the table of aliases for (valid) import strings
	ui->tableWidget_2->clear();
	ui->tableWidget_2->setColumnCount(2);
	ui->tableWidget_2->setColumnWidth(0,180);
	ui->tableWidget_2->setColumnWidth(1,510);
	m_tableHeader_2 << "Import String" << "Alias (Footprint/Package)" ;
	ui->tableWidget_2->setHorizontalHeaderLabels(m_tableHeader_2);
	ui->tableWidget_2->verticalHeader()->setVisible(false);
	ui->tableWidget_2->setEditTriggers(QAbstractItemView::AllEditTriggers);
	ui->tableWidget_2->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->tableWidget_2->setSelectionMode(QAbstractItemView::NoSelection);
	ui->tableWidget_2->setShowGrid(true);

	// Populate the table with data
	auto& mapAliasToImportStr = m_pMainWindow->m_templateMgr.GetMapAliasToImportStr();

	// Copy the info in the map to a std::vector and sort it for display
	std::vector<RowData*> rowDataVec; rowDataVec.resize(mapAliasToImportStr.size(), nullptr);
	size_t iCounter(0);
	for (auto& mapObj : mapAliasToImportStr) rowDataVec[iCounter++] = new RowData(mapObj.first, mapObj.second);
	std::stable_sort(rowDataVec.begin(), rowDataVec.end(), IsEarlierRow());	// Sort the list appropriately

	int nAlias(0);
	for (auto& pRowData : rowDataVec)
	{
		// Write row to table.	Note: No memory leak since setItem() takes ownership.
		auto pItemA	= new QTableWidgetItem(QString::fromStdString(pRowData->m_importStr));
		auto pItemB = new QTableWidgetItem(QString::fromStdString(pRowData->m_aliasStr));

		pItemA->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled);
		pItemB->setFlags(Qt::NoItemFlags);

		ui->tableWidget_2->setItem(nAlias, 0, pItemA);
		ui->tableWidget_2->setItem(nAlias, 1, pItemB);

		nAlias++;
	}
	ui->tableWidget_2->setRowCount(nAlias);
	ui->tableWidget_2->verticalScrollBar()->setSliderPosition( ui->tableWidget_2->verticalScrollBar()->maximum() );
	CellPressed(-1,-1);	// Deselect row in alias table so we disable the Delete button

	for (auto& pRowData : rowDataVec) delete pRowData;	// Deallocate objects in vector

	m_bUpdating = false;
}

bool AliasDialog::Import(bool& bPartTypeOK)
{
	m_errorStr.clear();
	return m_pMainWindow->m_board.Import(m_pMainWindow->m_templateMgr, m_filename, m_errorStr, m_bTango, bPartTypeOK);
}

void AliasDialog::keyPressEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	m_pMainWindow->specialKeyPressEvent(event);
#endif
	QDialog::keyPressEvent(event);
	event->accept();
}

void AliasDialog::keyReleaseEvent(QKeyEvent* event)
{
#ifdef VEROROUTE_ANDROID
	if ( event->key() == Qt::Key_Back )
	{
		QTimer::singleShot(0, m_pMainWindow, SLOT(HideAliasDialog()));
		return event->accept();
	}
#else
	m_pMainWindow->commonKeyReleaseEvent(event);
#endif
	QDialog::keyReleaseEvent(event);
	event->accept();
}

void AliasDialog::DeleteRow()
{
	if ( m_iRow >= 0 && m_iRow < ui->tableWidget_2->rowCount() )
	{
		const std::string aliasStr = ui->tableWidget_2->item(m_iRow, 1)->text().toStdString();
		const std::string messageStr = "The alias\n" + aliasStr + "\nis about to be deleted.  There is no undo for this operation.  Continue?";
		if ( QMessageBox::question(this, tr("Confirm delete alias"),
										 tr(messageStr.c_str()),
										 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No ) return;
		m_pMainWindow->m_templateMgr.RemoveAlias(aliasStr);
		ui->tableWidget_2->removeRow(m_iRow);
		m_iRow = -1;
		Update();
	}
}

void AliasDialog::DeleteAllRows()
{
	const std::string messageStr = "All aliases are about to be deleted.  There is no undo for this operation.  Continue?";
	if ( QMessageBox::question(this, tr("Confirm delete all aliases"),
									 tr(messageStr.c_str()),
									 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No ) return;
	m_pMainWindow->m_templateMgr.RemoveAllAliases();
	ui->tableWidget_2->setRowCount(0);
	m_iRow = -1;
	Update();
}

void AliasDialog::CellPressed(int row, int /*col*/)	// For alias
{
	m_iRow = row;
	ui->pushButton_2->setEnabled(m_iRow != -1);
	ui->pushButton_3->setEnabled(m_iRow != -1);
}

void AliasDialog::CellChanged(int row, int col)		// For alias table
{
	if ( m_bUpdating ) return;
	if ( col == 1 ) return;

	const std::string importStr	= ui->tableWidget_2->item(row, 0)->text().toStdString();
	const std::string aliasStr	= ui->tableWidget_2->item(row, 1)->text().toStdString();

	// Allow invalid aliases to be entered at ths stage.  Let ClearInvalidAliases() take care of them before (re)import.
	m_pMainWindow->m_templateMgr.AddAlias(aliasStr, importStr);
}
