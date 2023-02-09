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

#include "Common.h"

struct Simplex
{
public:
	static int GetLetterIndex(char c)
	{
		const int index = ( c - ' ' );
		return ( index >= 0 && index < 95 ) ? index : -1;	// -1 ==> unsupported character
	}
	static int GetLetterData(int i, int j)
	{
		return ( i < 95 && j < 112 ) ? sm_data[static_cast<size_t>(i)][static_cast<size_t>(j)] : -1;
	}
	static void CalcXlimits(int i, std::pair<int,int>& o)
	{
		if ( i == 0 ) { o.first = 0; o.second = 5; return; }	// ' ' character
		o.first = INT_MAX;	o.second = INT_MIN;
		const int jEnd = 2 + 2 * GetLetterData(i, 0);
		for (int j = 2; j < jEnd; j += 2)	// Loop x,y pairs
		{
			const int ix = GetLetterData(i, j);		// Read x
			const int iy = GetLetterData(i, j+1);	// Read y
			if ( ix == -1 && iy == -1 ) continue;
			o.first	 = std::min(o.first, ix);
			o.second = std::max(o.second, ix);
		}
	}
	static const std::pair<int,int>& GetXlimits(int i)
	{
		if ( sm_xLimits.empty() )	// If cache is empty ...
		{
			sm_xLimits.resize(95);	// ... build it
			for (int ii = 0; ii < 95; ii++) CalcXlimits(ii, sm_xLimits[static_cast<size_t>(ii)]);
		}
		return sm_xLimits[static_cast<size_t>(i)];	// Read cache
	}
private:
	static std::vector< std::vector<int> >		sm_data;	// Data for ASCII characters 32 to 126 inclusive
	static std::vector< std::pair<int,int> >	sm_xLimits;	// A cache of the min/max X values for each letter
};
