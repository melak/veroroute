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

#include "FootPrint.h"
#include "CompManager.h"
#include "RectManager.h"

void FootPrint::BuildDefault(COMP type)
{
	assert(type != COMP::INVALID);

	// This method is for building a regular component (not a "tracks" component, or "custom" component)
	SetType(type);

	int numRows(0), numCols(0);
	const std::string pinStr = CompTypes::GetMakeInstructions(m_type, numRows, numCols);

	const bool bOK = ( pinStr.size() == static_cast<size_t>(numRows * numCols) );	assert( bOK );	// Check string length OK
	if ( !bOK ) return;

	Allocate(1, numRows, numCols);

	const bool	bPlug	= CompTypes::IsPlug(m_type);	// A "plug" component can plug the gap between rows of IC pins.
	const char*	szpins	= pinStr.c_str();

	const int iSize = GetSize();
	for (int i = 0; i < iSize; i++)
	{
		Pin* p = GetAt(i);
		const char pinChar = szpins[i];
		switch( pinChar )
		{
			case '.':	p->SetSurface(SURFACE_FREE);	break;
			case '-':	p->SetSurface(SURFACE_GAP);		break;
			case '*':	p->SetSurface(SURFACE_FULL);	p->SetSoicChar(SOIC_TRACKS_TOP);	break;
			default:	p->SetSurface(bPlug ? SURFACE_PLUG : SURFACE_FULL);
		}
		p->SetPinIndex( GetPinIndexFromLegacyPinChar(static_cast<uchar>(pinChar)) );
	}
	bool bIsSOIC(false);	//TODO If SOIC parts are introduced as a COMP type, then we need to set this as necessary
	if ( bIsSOIC )
		SetupOccupanciesSOIC();	// Setup hole use, and SOIC info
	else
		SetupOccupanciesTH();	// Setup hole use (and for wires setup surface use too), and SOIC info
}

void FootPrint::BuildTrax(CompManager* pCompMgr, const RectManager& rectMgr, const ElementGrid& o, int nLyr, int nRowMin, int nRowMax, int nColMin, int nColMax)
{
	// This method is for building a "tracks" component
	SetType(COMP::TRACKS);

	const int numRows = 1 + nRowMax - nRowMin;
	const int numCols = 1 + nColMax - nColMin;

	Allocate(1, numRows, numCols);

	size_t	pinIndex;
	int		compId;

	int jRow(nRowMin);
	for (int j = 0; j < numRows; j++, jRow++)
	{
		int iCol(nColMin);
		for (int i = 0; i < numCols; i++, iCol++)
		{
			if ( !rectMgr.ContainsPoint(jRow,iCol) ) continue;

			CompElement*	pTarget = Grid<CompElement>::Get(0, j, i);
			Element*		pSource = o.Get(nLyr, jRow, iCol);	assert( pSource );

			int iNodeId = pSource->GetNodeId();

			if ( pSource->GetHasPin() )	// For pins/wires, let the track contain the origId before the part was placed
			{
				for (int iSlot = 0; iSlot < 2; iSlot++)
				{
					pSource->GetSlotInfo(iSlot, pinIndex, compId);
					if ( pinIndex == BAD_PININDEX ) continue;
					const Component& comp = pCompMgr->GetComponentById( compId );
					assert( comp.GetType() != COMP::INVALID );
					iNodeId = comp.GetOrigId(nLyr, pinIndex);
					break;
				}
			}
			else if ( !pSource->ReadFlagBits(USERSET) )	// Don't copy auto-routed tracks
				iNodeId = BAD_NODEID;
			pTarget->SetNodeId(iNodeId);
			pTarget->SetCode(pSource->GetCode());	// Note: This is wrong at boundaries
			pTarget->MarkFlagBits(RECTSET);
		}
	}
}
