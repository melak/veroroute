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

#include "Version.h"
#include "mainwindow.h"
#include <QApplication>

void versionmsg()
{
	std::cout << "VeroRoute Version " << szVEROROUTE_VERSION << std::endl;
}

void helpmsg()
{
	std::cout << "Usage: veroroute [OPTION] [FILE]"<< std::endl << std::endl;
	std::cout << "Starts the application and opens the specified FILE (a VeroRoute VRT file)." << std::endl << std::endl;
	std::cout << "[OPTION] is one of the following" << std::endl;
	std::cout << "   -p PATH         specify the PATH to the 'tutorials' directory" << std::endl;
	std::cout << "   -h, --help      display this help and exit" << std::endl;
	std::cout << "   -v, --version   display version information and exit" << std::endl;
}

int main(int argc, char *argv[])
{
#ifdef VEROROUTE_ANDROID
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

	QString	pathStr(".");	// By default, take the home directory to be the current one

	for (int i = 1; i < argc; i++)
	{
		const QString str( argv[i] );
		if ( str == "-h" || str == "--help" )		{ helpmsg();	return 0; }
		if ( str == "-v" || str == "--version" )	{ versionmsg();	return 0; }
	}

	// See if any command line arguments contain the "-p" instruction
	int iArgP(0);	// The index of the argument containing the "-p" instruction
	for (int i = 1; i < argc-1; i++)	// The "-p" can't come last
	{
		const QString str( argv[i] );
		if ( str == "-p" )
		{
			iArgP	= i;
			pathStr	= argv[i+1];
			break;
		}
	}

	QCoreApplication::addLibraryPath(pathStr);	// Search "platforms" sub-directory for plugin DLLs
	QApplication a(argc, argv);

	QString appDataPathStr = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	// appDataPathStr on Linux   = "~/.local/share/veroroute"
	// appDataPathStr on Windows = "C:/Users/<USER>/AppData/Roaming/veroroute"

	// If the folder doesn't exist then create it
	QDir dir(appDataPathStr);
	if ( !dir.exists() ) dir.mkpath(".");

	// If the "history" folder doesn't exist then create it
	QDir historyDir(appDataPathStr + QString("/history"));
	if ( !historyDir.exists() ) historyDir.mkpath(".");

	// If the "templates" folder doesn't exist then create it
	QDir templatesDir(appDataPathStr + QString("/templates"));
	if ( !templatesDir.exists() ) templatesDir.mkpath(".");

#ifdef VEROROUTE_ANDROID
	QString tutorialsPathStr = "assets:/";
#else
	// Fallback "tutorials" path should be in same folder as the exe (until distribution method for Windows changes)
	QString tutorialsPathStr = pathStr;

	// Search for relative "tutorials" path assuming the binary is installed in usr/bin
	QString relativeTutorialsPathStr = ("../share/veroroute");
	QDir tutorialsDir(relativeTutorialsPathStr + QString("/tutorials"));
	if ( tutorialsDir.exists() )
		tutorialsPathStr = relativeTutorialsPathStr;
	else	// Search for system wide "tutorials" path
	{
		const auto& locationsConst = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
		for (const auto& dataLocationPath : locationsConst)
		{
			QDir tutorialsDir(dataLocationPath + QString("/tutorials"));
			// Take first hit
			if ( tutorialsDir.exists() )
			{
				tutorialsPathStr = dataLocationPath;
				break;
			}
		}
	}
#endif

	// Spawn main window
	MainWindow w( appDataPathStr, tutorialsPathStr );

	for (int i = 1; i < argc; i++)
	{
		if ( iArgP > 0 && ( i == iArgP || i == iArgP+1 ) ) continue;	// If "-p" was specified, ignore it and the subsequent argv

		QString fileName( argv[i] );
		w.OpenVrt(fileName);
		break;
	}

	w.show();
	return a.exec();
}
