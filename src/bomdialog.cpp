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

#include "bomdialog.h"
#include "ui_bomdialog.h"
#include "mainwindow.h"
#include <QtGlobal>

BomDialog::BomDialog(MainWindow* parent)
: QDialog(parent)
, ui(new Ui_BomDialog)
, m_pMainWindow(parent)
{
	ui->setupUi(this);
#ifdef VEROROUTE_ANDROID
	ui->tableWidget->verticalScrollBar()->setStyleSheet( ANDROID_VSCROLL_WIDTH );
	ui->tableWidget->horizontalScrollBar()->setStyleSheet( ANDROID_HSCROLL_HEIGHT );
#endif
	QObject::connect(ui->pushButton,	SIGNAL(clicked()),	this,			SLOT(WriteToFile()));
	QObject::connect(this,				SIGNAL(rejected()),	m_pMainWindow,	SLOT(UpdateControls()));	// Close using X button
}

BomDialog::~BomDialog()
{
	delete ui;
}

struct IsEarlierInBOM
{
	bool operator()(const Component* pA, const Component* pB) const
	{
		// First order by type ...
		if ( pA->GetType() != pB->GetType() ) return static_cast<int>(pA->GetType()) < static_cast<int>(pB->GetType());
		// ... then by value string comparison
		const int i = pA->GetValueStr().compare( pB->GetValueStr() );
		if ( i != 0 ) return i < 0;
		// ... then by name length
		if ( pA->GetNameStr().length() != pB->GetNameStr().length() ) return pA->GetNameStr().length() < pB->GetNameStr().length();
		// ... then by name string comparison
		return pA->GetNameStr().compare( pB->GetNameStr() ) < 0;
	}
};

void BomDialog::Update()
{
	std::vector<const Component*> pComps;	//  Make list of pointers to all components for the B.O.M.
	for (const auto& mapObj : m_pMainWindow->m_board.GetCompMgr().GetMapIdToComp())
	{
		const Component& comp = mapObj.second;
		switch( comp.GetType() )
		{
			case COMP::VERO_NUMBER:
			case COMP::VERO_LETTER:
			case COMP::MARK:
			case COMP::PAD:
			case COMP::PAD_FLYINGWIRE:
			case COMP::WIRE:	break;	// Not true components for BOM
			default:			pComps.push_back(&comp);
		}
	}
	std::stable_sort(pComps.begin(), pComps.end(), IsEarlierInBOM());	// Sort the list appropriately

	// Set up the table
	ui->tableWidget->clear();
	ui->tableWidget->setColumnCount(4);
	ui->tableWidget->setColumnWidth(0,130);
	ui->tableWidget->setColumnWidth(1,250);
	ui->tableWidget->setColumnWidth(2,100);
	ui->tableWidget->setColumnWidth(3,100);
	m_tableHeader << "Name" << "Type" << "Value" << "Quantity";
	ui->tableWidget->setHorizontalHeaderLabels(m_tableHeader);
	ui->tableWidget->verticalHeader()->setVisible(false);
	ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->tableWidget->setSelectionMode(QAbstractItemView::NoSelection);
	ui->tableWidget->setShowGrid(true);

	// Populate the table with data
	std::string rowNames(""), rowType(""), rowValue("");
	int row(-1), rowQuantity(0);
	for (const auto& p : pComps)
	{
		const bool	bLast		= ( p == pComps.back() );
		auto&		strNewValue	= p->GetValueStr();

		auto		strNewType	= CompTypes::GetFamilyStr( p->GetType() );
		if ( !StringHelper::IsEmptyStr(strNewType) ) strNewType += ": ";
		strNewType += p->GetTypeStr();
		if ( p->GetType() == COMP::DIP || p->GetType() == COMP::SIP )
			strNewType += std::to_string(p->GetNumPins());	// e.g. "DIP16"
		if ( p->GetType() == COMP::STRIP_100 || p->GetType() == COMP::BLOCK_100 || p->GetType() == COMP::BLOCK_200 )
			strNewType += std::string(" (") + std::to_string(p->GetNumPins()) + std::string(" pins)");

		const bool bNewRow = ( row == -1) || ( strNewValue != rowValue ) || ( strNewType != rowType );
		if ( bNewRow )
		{
			if ( row != -1 )	// If have a previous row ...
			{
				// Write previous row to table.	Note: No memory leak since setItem() takes ownership.
				ui->tableWidget->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(rowNames)));
				ui->tableWidget->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(rowType)));
				ui->tableWidget->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(rowValue)));
				ui->tableWidget->setItem(row, 3, new QTableWidgetItem(QString::number(rowQuantity)));
			}
			// Initialise data for new row
			row++;
			rowQuantity	= 1;
			rowNames	= p->GetNameStr();
			rowValue	= strNewValue;
			rowType		= strNewType;
		}
		else
		{
			// Update data for row
			rowQuantity++;
			rowNames = rowNames + ", " + p->GetNameStr();
		}
		if ( bLast )	// Very last component in the B.O.M.
		{
			// Write current row to table.	Note: No memory leak since setItem() takes ownership.
			ui->tableWidget->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(rowNames)));
			ui->tableWidget->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(rowType)));
			ui->tableWidget->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(rowValue)));
			ui->tableWidget->setItem(row, 3, new QTableWidgetItem(QString::number(rowQuantity)));
		}
	}
	const int numRows(row + 1);
	ui->tableWidget->setRowCount(numRows);
	if ( numRows > 8 )
		ui->tableWidget->setColumnWidth(3,80);	// Small reduction when have a vertical scroll bar
	ui->pushButton->setDisabled( pComps.empty() );
}

void BomDialog::WriteToFile()
{
#ifdef VEROROUTE_ANDROID
	const QString	name		= m_pMainWindow->GetFileName().isEmpty() ? QString("Circuit.vrt") : m_pMainWindow->GetFileName();
	QString			defaultName	= StringHelper::ReplaceSuffix(name, QString("txt"));	// Replace vrt with txt
	QFile			file(defaultName);
	if ( file.exists() )
		defaultName.clear();
	else
		defaultName = StringHelper::GetTidyFileName(defaultName);
	QMessageBox::information(this, tr("Information"), tr("You must now select or enter a filename ending in .txt"));
	const QString	fileName	= m_pMainWindow->GetSaveFileName(defaultName, tr("Text (*.txt);;All Files (*)"), QString("txt"));
#else
	const QString	fileName	= m_pMainWindow->GetSaveFileName(tr("Choose a TXT file"), tr("Text (*.txt);;All Files (*)"), QString("txt"));
#endif
	if ( !fileName.isEmpty() )
	{
		QFile file;
		file.setFileName( fileName ) ;

		QTextStream os;
		const bool bOK = file.open(QIODevice::WriteOnly);
		if ( bOK )
		{
			os.setDevice(&file);

#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
			os << "Name" << "\t" << "Type" << "\t" << "Value" << "\t" << "Quantity" << Qt::endl;
#else
			os << "Name" << "\t" << "Type" << "\t" << "Value" << "\t" << "Quantity" << endl;
#endif
			const int numRows = ui->tableWidget->rowCount();
			const int numCols = ui->tableWidget->columnCount();
			for (int i = 0; i < numRows; i++)
			{
				for(int j = 0; j < numCols; j++)
				{
					os << ui->tableWidget->item(i,j)->text();
					if ( j != numCols - 1 ) os << "\t";
				}
#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
				os << Qt::endl;
#else
				os << endl;
#endif
			}
#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
			os << Qt::endl;
#else
			os << endl;
#endif
			file.close();
		}
		else
			QMessageBox::information(this, tr("Unable to save file"), fileName);
	}
}

void BomDialog::keyPressEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	m_pMainWindow->specialKeyPressEvent(event);
#endif
	QDialog::keyPressEvent(event);
	event->accept();
}

void BomDialog::keyReleaseEvent(QKeyEvent* event)
{
#ifndef VEROROUTE_ANDROID
	m_pMainWindow->commonKeyReleaseEvent(event);
#endif
	QDialog::keyReleaseEvent(event);
	event->accept();
}
