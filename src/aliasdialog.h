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

#pragma once

#include <QDialog>

class MainWindow;
class Ui_AliasDialog;
class QTableWidgetItem;

#include <string>

class AliasDialog : public QDialog
{
	Q_OBJECT

public:
	explicit AliasDialog(MainWindow* parent = nullptr);
	~AliasDialog();

	void Configure(const std::string& filename, bool bTango);
	void Update();
	bool Import(bool& bPartTypeOK);
	const std::string& GetFilename() const { return m_filename; }
	const std::string& GetErrorStr() const { return m_errorStr; }
protected:
	void keyPressEvent(QKeyEvent* event);
	void keyReleaseEvent(QKeyEvent* event);
public slots:
	void DeleteRow();						// For alias table
	void DeleteAllRows();					// For alias table
	void CellPressed(int row, int col);		// For alias table
	void CellChanged(int row, int col);		// For alias table
	void CellChangedTop(int row, int col);	// For valid import strings table
private:
	Ui_AliasDialog*	ui;
	MainWindow*			m_pMainWindow;
	QStringList			m_tableHeader;
	QStringList			m_tableHeader_2;
	std::string			m_filename;
	std::string			m_errorStr;
	bool				m_bTango = false;
	int					m_iRow;
	bool				m_bUpdating = false;
};
