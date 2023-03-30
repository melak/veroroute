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

#include "Board.h"

bool Board::BuildAndPlacePart(TemplateManager& templateMgr, const CompStrings& compStrings, std::list<std::string>& offBoard, std::string& errorStr, bool& bPartTypeOK)
{
	Component comp;	// Only configured if the part type is OK

	// First see if m_importStr is an alias for a valid import string
	const std::string& validImportStr = templateMgr.GetImportStrFromAlias(compStrings.m_importStr);

	CompStrings test(compStrings);	// A working copy of compStrings (with possibly modified m_importStr)
	if ( !validImportStr.empty() ) test.m_importStr = validImportStr;

	bool bOK = bPartTypeOK = templateMgr.CheckPartOK(test, &offBoard, &errorStr, &comp);
	if ( !bOK )
		templateMgr.AddAlias(compStrings.m_importStr, "");	// Don't have a valid import string yet
	else
	{
		bOK = ( AddComponent(-1, -1, comp) != BAD_COMPID );	// Create the part and place it
		if ( !bOK ) 
		{
			const std::string strID = "Part: Name = " + compStrings.m_nameStr + ", Value = " + compStrings.m_valueStr + ", Type = " + compStrings.m_importStr;
			errorStr = strID + "\nInternal error creating and placing part";
		}
	}
	return bOK;
}

// Helper to get all part info from a Protel V1 / Tango file
bool Board::GetPartsTango(const std::string& filename, std::list<CompStrings>& listOut) const
{
	CompStrings compStrings;

	// References to keep code tidy
	std::string& nameStr	= compStrings.m_nameStr;	// TinyCAD "Ref"	 / gEDA "refdes"	==> VeroRoute "Name"	(e.g. "U4")
	std::string& valueStr	= compStrings.m_valueStr;	// TinyCAD "Name"	 / gEDA "device"	==> VeroRoute "Value"	(e.g. "TL072")
	std::string& importStr	= compStrings.m_importStr;	// TinyCAD "Package" / gEDA "footprint"	==> VeroRoute "Type"	(e.g. "DIP8")

	std::ifstream inStream;
	inStream.open(filename.c_str(), std::ios::in | std::ios::binary);
	bool bOK = inStream.is_open();
	bool bPart(false), bNet(false);	// Flags indicating "part" and "netlist" sections
	int iRow(0);					// Row counter within "part" and "netlist" sections

	while( bOK )	// Loop through file
	{
		if ( inStream.eof() ) break;

		std::string str;							// For reading from file.  Ensure clear before reading
		StringHelper::getline_safe(inStream, str);	// Read the whole line and handle line-ending nicely

		if ( str == "[" ) { bOK = !bPart && !bNet;	bPart = true;	iRow = 0;	compStrings.Clear();	continue; }
		if ( str == "]" ) { bOK =  bPart && !bNet;	bPart = false;				continue; }
		if ( str == "(" ) { bOK = !bPart && !bNet;	bNet  = true;	iRow = 0; 	continue; }
		if ( str == ")" ) { bOK = !bPart &&  bNet;	bNet  = false;				continue; }

		if ( bPart )	// We're in a "Part" description section
		{
			switch(iRow)
			{
				case 0:	nameStr		= str;	break;
				case 1:	importStr	= str;	break;
				case 2:	valueStr	= str;	listOut.push_back(compStrings);	break;
			}
		}
		iRow++;
	}
	if ( inStream.is_open() ) inStream.close();

	return bOK;
}

// Helper to get all part info from an OrcadPCB2 file
bool Board::GetPartsOrcad(const std::string& filename, std::list<CompStrings>& listOut) const
{
	CompStrings compStrings;

	// References to keep code tidy
	std::string& nameStr	= compStrings.m_nameStr;	// KiCAD "Reference" ==> VeroRoute "Name"	(e.g. "U4")
	std::string& valueStr	= compStrings.m_valueStr;	// KiCAD "Value"	 ==> VeroRoute "Value"	(e.g. "TL072")
	std::string& importStr	= compStrings.m_importStr;	// KiCAD "Footprint" ==> VeroRoute "Type"	(e.g. "DIP8")

	std::ifstream inStream;
	inStream.open(filename.c_str(), std::ios::in | std::ios::binary);
	bool bOK = inStream.is_open();

	bool bPartStart(false);
	while( bOK )	// Loop through file
	{
		if ( inStream.eof() ) break;

		std::string str;							// For reading from file.  Ensure clear before reading
		StringHelper::getline_safe(inStream, str);	// Read the whole line and handle line-ending nicely
		if ( str.empty() ) continue;	// Skip blank lines

		// First non-blank char on every line should be '(' or ')' or '*'
		const bool bCurvedOpen	= str.find("(")	!= std::string::npos;
		const bool bCurvedClose	= str.find(")")	!= std::string::npos;

		if ( bCurvedOpen && str.find("{") != std::string::npos && str.find("}") != std::string::npos )
			continue;	// Line is a comment so skip it

		if ( !bCurvedOpen && !bCurvedClose )
		{
			if ( str.find("*") != std::string::npos )
				break;	// reached the '*'

			bOK = false;
			//errorStr = "Expecting all lines to start with '('' or ')'' or '*'";
			break;
		}

		if ( bCurvedOpen && !bPartStart )
		{
			// We're expecting the line to say something like "( /5D5ADFE2 DIP16 IC1 SAD1024"

			std::vector<std::string> strList;

			StringHelper::GetSubStrings(str, strList);	// Break str into space separated list
			const size_t numSubStrings = strList.size();

			bOK = ( numSubStrings == 4 || numSubStrings == 5 );
			//if ( !bOK ) errorStr = "Expecting format:  ( /5D5ADFE2 FOOTPRINT NAME VALUE  , but got:" + str;
			if ( !bOK ) break;

			importStr	= strList[2];
			nameStr		= strList[3];
			valueStr	= ( numSubStrings == 5 ) ? strList[4] : "";

			listOut.push_back(compStrings);

			bPartStart = true;
		}
		else
		{
			// We're in the pin section...
			assert( bCurvedClose );	// Should have ')' on every line
			if ( !bCurvedOpen )
			{
				// If we have just a ')' on the line then we're done with the pins ...
				bPartStart = false;	// ... and we're done with the part, move onto the next one
				continue;
			}
		}
	}
	if ( inStream.is_open() ) inStream.close();

	return bOK;
}

// Import Protel V1 / Tango netlist (exported from TinyCAD / gEDA)
bool Board::ImportTango(TemplateManager& templateMgr, const std::string& filename, std::string& errorStr, bool& bPartTypeOK)
{
	Clear();

	CompStrings compStrings;

	// References to keep code tidy
	std::string& nameStr	= compStrings.m_nameStr;	// TinyCAD "Ref"	 / gEDA "refdes"	==> VeroRoute "Name"	(e.g. "U4")
	std::string& valueStr	= compStrings.m_valueStr;	// TinyCAD "Name"	 / gEDA "device"	==> VeroRoute "Value"	(e.g. "TL072")
	std::string& importStr	= compStrings.m_importStr;	// TinyCAD "Package" / gEDA "footprint"	==> VeroRoute "Type"	(e.g. "DIP8")
	std::string netStr;	// Net name

	std::ifstream inStream;
	inStream.open(filename.c_str(), std::ios::in | std::ios::binary);
	bool bOK = inStream.is_open();
	bool bPart(false), bNet(false);	// Flags indicating "part" and "netlist" sections
	int iRow(0);					// Row counter within "part" and "netlist" sections
	int iNodeId(BAD_NODEID);		// Increase this with each imported net

	std::list<std::string> offBoard;	// List of off-board part names (for breaking into pads)

	while( bOK )	// Loop through file
	{
		if ( inStream.eof() ) break;

		std::string str;							// For reading from file.  Ensure clear before reading
		StringHelper::getline_safe(inStream, str);	// Read the whole line and handle line-ending nicely

		if ( str == "[" ) { bOK = !bPart && !bNet;	bPart = true;	iRow = 0;	continue; }
		if ( str == "]" ) { bOK =  bPart && !bNet;	bPart = false;				continue; }
		if ( str == "(" ) { bOK = !bPart && !bNet;	bNet  = true;	iRow = 0; 	continue; }
		if ( str == ")" ) { bOK = !bPart &&  bNet;	bNet  = false;				continue; }

		if ( bPart )	// We're in a "Part" description section
		{
			if ( iRow == 0 )
			{
				nameStr = str;
				bOK  = ( nameStr.find("-") == std::string::npos );	// Name should not have a "-"
				if ( !bOK ) errorStr = "Part section: " + nameStr + "\nPart names must not contain a minus sign";
				if ( !bOK ) break;

				bOK = ( m_compMgr.GetComponentIdFromName(nameStr) == BAD_COMPID );	// Name must be unique
				if ( !bOK ) errorStr = "Part section: " + nameStr + "\nPart names must be unique";
				if ( !bOK ) break;
			}
			if ( iRow == 1 )
			{
				importStr = str;
			}
			if ( iRow == 2 )
			{
				valueStr = str;

				// Build the part and place it ...
				bOK = BuildAndPlacePart(templateMgr, compStrings, offBoard, errorStr, bPartTypeOK);
				if ( !bOK ) break;
			}
		}
		if ( bNet )	// We're in a "Netlist" description section
		{
			if ( iRow == 0 )
			{
				netStr = str;
				iNodeId++;	// str has the net name, so make a new NodeID for it
			}
			else
			{
				// The line should have a part identifier and pin number separated by a '-'.
				const auto pos = str.find("-");
				bOK  = ( pos != std::string::npos );	// Should have a "-" on the line
				if ( !bOK ) errorStr = "Net section: " + netStr + "\nLine has no minus sign: " + str;
				if ( !bOK ) break;

				const std::string pinStr = str.substr(pos+1);;		// Pin number
				bOK = ( pinStr.find("-") == std::string::npos );	// Should only have one "-" on the line
				if ( !bOK ) errorStr = "Net section: " + netStr + "\nLine has more than one minus sign: " + str;
				if ( !bOK ) break;

				const std::string lineNameStr = str.substr(0, pos);
				bOK = !StringHelper::IsEmptyStr(lineNameStr);	// Should have part name
				if ( !bOK ) errorStr = "Net section: " + netStr + "\nLine has no part identifier: " + str;
				if ( !bOK ) break;

				// Paint the component pin using SetNodeIdByUser
				const size_t	iPinIndex	= static_cast<size_t>( atoi(pinStr.c_str()) - 1 );
				const int		compId		= m_compMgr.GetComponentIdFromName(lineNameStr);

				bOK = compId != BAD_COMPID;
				if ( !bOK ) errorStr = "Net section: " + netStr + "\nLine has unknown part identifier: " + str;
				if ( !bOK ) break;

				const Component& comp = m_compMgr.GetComponentById( compId );
				assert( comp.GetType() != COMP::INVALID );

				bOK = iPinIndex < comp.GetNumPins();
				if ( !bOK ) errorStr = "Net section: " + netStr + "\nLine has invalid pin number: " + str;
				if ( !bOK ) break;

				int row, col;
				bOK = GetPinRowCol(compId, iPinIndex, row, col);
				if ( !bOK ) errorStr = "Net section: " + netStr + "\nLine: " + str + "\nInternal error mapping the pin to a board location";
				if ( !bOK ) break;

				SetNodeIdByUser(0, row, col, iNodeId, true);	// true ==> paint pins
			}
		}
		iRow++;
	}
	if ( inStream.is_open() ) inStream.close();

	// Break the SIPs representing off-board parts into PADs
	BreakSIPSintoPADS(offBoard);

	return bOK;
}

// Import OrcadPCB2 netlist (exported from KiCAD)
bool Board::ImportOrcad(TemplateManager& templateMgr, const std::string& filename, std::string& errorStr, bool& bPartTypeOK)
{
	Clear();

	CompStrings compStrings;

	// References to keep code tidy
	std::string& nameStr	= compStrings.m_nameStr;	// KiCAD "Reference" ==> VeroRoute "Name"	(e.g. "U4")	
	std::string& valueStr	= compStrings.m_valueStr;	// KiCAD "Value"	 ==> VeroRoute "Value"	(e.g. "TL072")
	std::string& importStr	= compStrings.m_importStr;	// KiCAD "Footprint" ==> VeroRoute "Type"	(e.g. "DIP8")

	std::ifstream inStream;
	inStream.open(filename.c_str(), std::ios::in | std::ios::binary);
	bool bOK = inStream.is_open();
	int maxNodeId(BAD_NODEID);		// Increase this with each new node we encounter

	std::unordered_map<std::string, int> mapNetToNodeId;

	std::list<std::string> offBoard;	// List of off-board part names

	bool bPartStart(false);

	while( bOK )	// Loop through file
	{
		if ( inStream.eof() ) break;

		std::string str;							// For reading from file.  Ensure clear before reading
		StringHelper::getline_safe(inStream, str);	// Read the whole line and handle line-ending nicely
		if ( str.empty() ) continue;	// Skip blank lines

		// First non-blank char on every line should be '(' or ')' or '*'
		const bool bCurvedOpen	= str.find("(")	!= std::string::npos;
		const bool bCurvedClose	= str.find(")")	!= std::string::npos;

		if ( bCurvedOpen && str.find("{") != std::string::npos && str.find("}") != std::string::npos )
			continue;	// Line is a comment so skip it

		if ( !bCurvedOpen && !bCurvedClose )
		{
			if ( str.find("*") != std::string::npos )
				break;	// reached the '*'

			bOK = false;
			errorStr = "Expecting all lines to start with '('' or ')'' or '*'";
			break;
		}

		if ( bCurvedOpen && !bPartStart )
		{
			// We're expecting the line to say something like "( /5D5ADFE2 DIP16 IC1 SAD1024"

			std::vector<std::string> strList;

			StringHelper::GetSubStrings(str, strList);	// Break str into space separated list
			const size_t numSubStrings = strList.size();

			bOK = ( numSubStrings == 4 || numSubStrings == 5 );
			if ( !bOK ) errorStr = "Expecting format:  ( /5D5ADFE2 FOOTPRINT NAME VALUE  , but got:" + str;
			if ( !bOK ) break;

			importStr	= strList[2];
			nameStr		= strList[3];
			valueStr	= ( numSubStrings == 5 ) ? strList[4] : "";

			bOK = ( m_compMgr.GetComponentIdFromName(nameStr) == BAD_COMPID );	// Name must be unique
			if ( !bOK ) errorStr = "\nPart name " + nameStr + " is not unique";
			if ( !bOK ) break;

			// Build the part and place it ...
			bOK = BuildAndPlacePart(templateMgr, compStrings, offBoard, errorStr, bPartTypeOK);
			if ( !bOK ) break;

			bPartStart = true;
		}
		else
		{
			const std::string strID = "Part: Type = " + importStr + ", Name = " + nameStr + ", Value = " + valueStr;
	
			// We're in the pin section...
			assert( bCurvedClose );	// Should have ')' on every line
			if ( !bCurvedOpen )
			{
				// If we have just a ')' on the line then we're done with the pins ...
				bPartStart = false;	// ... and we're done with the part, move onto the next one
				continue;
			}

			std::vector<std::string> strList;
			StringHelper::GetSubStrings(str, strList);	// Break str into space separated list
			bOK = ( strList.size() == 4 );
			if ( !bOK ) errorStr = strID + "\nLine: " + str + "\nError: Expecting line with format: ( PINNUMBER NETNAME )";
			if ( !bOK ) break;

			const std::string pinStr = strList[1];;	// Pin number
			const std::string netStr = strList[2];;	// Net name

			int nodeId(BAD_NODEID);
			auto iter = mapNetToNodeId.find(netStr);
			if ( iter != mapNetToNodeId.end() )
				nodeId = iter->second;
			else
			{
				maxNodeId++;
				mapNetToNodeId[netStr] = nodeId = maxNodeId;
			}
			bOK = ( nodeId != BAD_NODEID );
			if ( !bOK ) errorStr = strID + "\nInternal error: VeroRoute has run out of net IDs";
			if ( !bOK ) break;

			// Paint the component pin using SetNodeIdByUser
			const size_t	iPinIndex	= static_cast<size_t>( atoi(pinStr.c_str()) - 1 );
			const int		compId		= m_compMgr.GetComponentIdFromName( nameStr );

			bOK = compId != BAD_COMPID;
			if ( !bOK ) errorStr = strID + "\nInternal error: VeroRoute lost the component ID";
			if ( !bOK ) break;

			const Component& comp = m_compMgr.GetComponentById( compId );
			assert( comp.GetType() != COMP::INVALID );

			bOK = iPinIndex < comp.GetNumPins();
			if ( !bOK ) errorStr = strID + "\nLine: " + str + "\nError: Line has invalid pin number";
			if ( !bOK ) break;
			
			int row, col;
			bOK = GetPinRowCol(compId, iPinIndex, row, col);
			if ( !bOK ) errorStr = strID + "\nLine: " + str + "\nInternal error mapping the pin to a board location";
			if ( !bOK ) break;

			SetNodeIdByUser(0, row, col, nodeId, true);	// true ==> paint pins
		}
	}
	if ( inStream.is_open() ) inStream.close();

	// Break the SIPs representing off-board parts into PADs
	BreakSIPSintoPADS(offBoard);

	return bOK;
}

void Board::BreakSIPSintoPADS(const std::list<std::string>& offBoard)
{
	// Break the SIPS representing off-board parts into PADs
	for (const auto& nameStr : offBoard)
	{
		const int	compId	= m_compMgr.GetComponentIdFromName( nameStr );
		Component&	comp	= m_compMgr.GetComponentById( compId );
		assert( comp.GetType() != COMP::INVALID );
		BreakComponentIntoPads(comp);
	}
}

void Board::BreakComponentIntoPads(Component& comp)
{
	const COMP& eType = comp.GetType();
	if ( eType == COMP::MARK || eType == COMP::PAD || eType == COMP::PAD_FLYINGWIRE || eType == COMP::WIRE || eType == COMP::VERO_NUMBER || eType == COMP::VERO_LETTER ) return;	// Not real components
	if ( !comp.GetIsPlaced() ) return;	// Can't break a floating component

	std::vector<int> nodeList = { BAD_NODEID };	// Re-used for each new pad

	const size_t numPins = comp.GetNumPins();
	for (size_t iPinIndex = 0; iPinIndex < numPins; iPinIndex++)	// Loop component pins
	{
		// Create a new PAD component for the pin, with suitable name, value, nodeId
		nodeList[0] = comp.GetNodeId(iPinIndex);
		Component tmp(comp.GetNameStr() + std::string("_") + std::to_string(iPinIndex+1), comp.GetValueStr(), COMP::PAD, nodeList);

		// Find board location of existing pin, and put the new PAD there
		int row, col;
		if ( GetPinRowCol(comp.GetId(), iPinIndex, row, col) )
		{
			tmp.SetRow(row);
			tmp.SetCol(col);
			AddComponent(-1, -1, tmp, false);	// Add PAD floating over the existing pin
		}
	}
	DestroyComponent(comp);	// All pins have been copied, so destroy the old component
	PlaceFloaters();		// Unfloat the new PADs
}

bool Board::GetPinRowCol(int compId, size_t iPinIndex, int& row, int& col) const
{
	if ( compId == BAD_COMPID || iPinIndex == BAD_PININDEX ) return false;

	for (int i = 0, iSize = GetSize(); i < iSize; i++)
	{
		const Element* p = GetAtConst(i);
		if ( !p->GetHasWire() && p->GetCompId() == compId && p->GetPinIndex() == iPinIndex )
		{
			GetRowCol(p, row, col);
			return true;
		}
	}
	return false;
}
