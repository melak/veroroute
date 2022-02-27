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

#include "pindialog.h"
#include "ui_pindialog.h"
#include "mainwindow.h"

PinDialog::PinDialog(MainWindow* parent)
: QDialog(parent)
, ui(new Ui_PinDialog)
, m_pMainWindow(parent)
{
	ui->setupUi(this);
#ifndef Q_OS_ANDROID
	ui->pushButtonOK->hide();
	ui->tableWidget->resize(ui->tableWidget->width(), ui->tableWidget->height() + 30);
#endif
	QObject::connect(ui->tableWidget,	SIGNAL(cellChanged(int,int)),	this,	SLOT(CellChanged(int,int)));
	QObject::connect(ui->pushButtonOK,	SIGNAL(clicked()),				this,	SLOT(hide()));
}

PinDialog::~PinDialog()
{
	delete ui;
}

Component* PinDialog::GetUserComp() const
{
	if ( m_pMainWindow->m_board.GetGroupMgr().GetNumUserComps() != 1 ) return nullptr;	// Need single component selected
	Component& comp = m_pMainWindow->m_board.GetUserComponent();
	return ( (comp.GetPinFlags() & PIN_LABELS) > 0 ) ? &comp : nullptr;	// Component must allow pin labels to be drawn
}

void PinDialog::CellChanged(int row, int col)
{
	if ( col == 0 ) return;

	const bool& bCompEdit = m_pMainWindow->m_board.GetCompEdit();

	CompDefiner*	pDef	= bCompEdit ? &m_pMainWindow->m_board.GetCompDefiner() : nullptr;
	Component*		pComp	= bCompEdit ? nullptr : GetUserComp();
	assert(pDef != nullptr || pComp != nullptr);

	QTableWidgetItem*	pItemLabel	= ui->tableWidget->item(row, col);
	const size_t		iPinIndex	= static_cast<size_t>(row);
	const std::string	strLabel	= pItemLabel->text().toStdString();

	if ( col == 1 )
	{
		const std::string& s = pDef ? pDef->GetPinLabel(iPinIndex) : pComp->GetPinLabel(iPinIndex);
		if ( s != strLabel ) // If changed
		{
			if ( pDef  ) pDef->SetPinLabel(iPinIndex, strLabel);
			if ( pComp ) pComp->SetPinLabel(iPinIndex, strLabel);
			m_pMainWindow->RepaintSkipRouting();
		}
	}
	if ( col == 2)
	{
		const int iAlign = ( strLabel == "L" || strLabel == "l" ) ? Qt::AlignLeft  :
						   ( strLabel == "R" || strLabel == "r" ) ? Qt::AlignRight : Qt::AlignHCenter;
		const int& i = pDef ? pDef->GetPinAlign(iPinIndex) : pComp->GetPinAlign(iPinIndex);
		if ( i != iAlign ) // If changed
		{
			if ( pDef )  pDef->SetPinAlign(iPinIndex, iAlign);
			if ( pComp ) pComp->SetPinAlign(iPinIndex, iAlign);
			m_pMainWindow->RepaintSkipRouting();
		}
		if ( strLabel != "L" && strLabel != "R" && strLabel != "C" )
			Update();	// Enforce L,R,C in GUI
	}
}

void PinDialog::Update()
{
	const bool& bCompEdit = m_pMainWindow->m_board.GetCompEdit();

	CompDefiner*	pDef	= bCompEdit ? &m_pMainWindow->m_board.GetCompDefiner() : nullptr;
	Component*		pComp	= bCompEdit ? nullptr : GetUserComp();
	const size_t	numPins	= pDef ? pDef->GetNumPins() : pComp ? pComp->GetNumPins() : 0;
	assert(pDef != nullptr || pComp != nullptr || numPins == 0);

	// Set up the table
	ui->tableWidget->clear();
	ui->tableWidget->setRowCount(static_cast<int>(numPins));
	ui->tableWidget->setColumnCount(3);
	ui->tableWidget->setColumnWidth(0,40);
	ui->tableWidget->setColumnWidth(1,105);
	ui->tableWidget->setColumnWidth(2,50);
	m_tableHeader << "Pin" << "Label" << "Align";
	ui->tableWidget->setHorizontalHeaderLabels(m_tableHeader);
	ui->tableWidget->verticalHeader()->setVisible(false);
	ui->tableWidget->setEditTriggers(QAbstractItemView::AllEditTriggers);
	ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->tableWidget->setSelectionMode(QAbstractItemView::NoSelection);
	ui->tableWidget->setShowGrid(true);

	// Populate the table with data
	for (size_t iPinIndex = 0; iPinIndex < numPins; iPinIndex++)
	{
		const int& iAlign = pDef ? pDef->GetPinAlign(iPinIndex) : pComp->GetPinAlign(iPinIndex);
		for (int iCol = 0; iCol < 3; iCol++)
		{
			std::string str;
			switch( iCol )
			{
				case 0:	str = CompTypes::GetDefaultPinLabel(iPinIndex);	break;
				case 1:	str = pDef ? pDef->GetPinLabel(iPinIndex) : pComp->GetPinLabel(iPinIndex);	break;
				case 2:	str = ( iAlign == Qt::AlignLeft  ) ? "L" :
							  ( iAlign == Qt::AlignRight ) ? "R" :"C";	break;
			}
			auto pItem = new QTableWidgetItem(QString::fromStdString(str));
			if ( iCol != 1 )
				pItem->setData(Qt::TextAlignmentRole, Qt::AlignCenter);

			if ( iCol == 0 )
				pItem->setFlags(Qt::NoItemFlags);
			else
				pItem->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled);

			ui->tableWidget->setItem(static_cast<int>(iPinIndex), iCol, pItem);
		}
	}
}

void PinDialog::keyPressEvent(QKeyEvent* event)
{
	m_pMainWindow->specialKeyPressEvent(event);
	QDialog::keyPressEvent(event);
}

void PinDialog::keyReleaseEvent(QKeyEvent* event)
{
	m_pMainWindow->commonKeyReleaseEvent(event);
	QDialog::keyReleaseEvent(event);
}
