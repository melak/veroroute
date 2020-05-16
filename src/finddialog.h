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
class Ui_FindDialog;

class FindDialog : public QDialog
{
	Q_OBJECT

public:
	explicit FindDialog(MainWindow* parent = 0);
	~FindDialog();

	void UpdateControls();
public slots:
	void ToggleName(bool b);
	void ToggleExact(bool b);
	void TextChanged(const QString& str);
protected:
	void closeEvent(QCloseEvent* event);
	void keyPressEvent(QKeyEvent* event);
	void keyReleaseEvent(QKeyEvent* event);
private:
	Ui_FindDialog*	ui;
	MainWindow*		m_pMainWindow;
	bool			m_bName = true;		// true/false ==> name/value
	bool			m_bExact = false;	// true/false ==> exact match/substring match
	QString			m_str;
};
