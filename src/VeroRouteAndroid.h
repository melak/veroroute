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

#include <QtGlobal>

#ifdef Q_OS_ANDROID
	#define VEROROUTE_ANDROID
#endif

// Remove comment from next line to test Android UI on a Desktop build
//#define VEROROUTE_ANDROID

// Remove the following comment so we can dump some debug info into the status bar
//#define VEROROUTE_DEBUG

// Following are experimental attempts with font size/scaling on builds for Android phones
//#define VEROROUTE_NO_DPI_SCALING
//#define VEROROUTE_FONT_SIZE(x) { QFont font = x->font(); font.setPixelSize(12); x->setFont(font); }
