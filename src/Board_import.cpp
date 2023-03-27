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

bool Board::BuildAndPlacePart(const TemplateManager& templateMgr, 
							  const std::string& nameStr, const std::string& valueStr, const std::string& typeStr, std::string& typeStrCut, std::string& pinStr,
							  std::list<std::string>& offBoard, std::string& errorStr, bool bTango)
{
	// List of package identifiers for footprints with variable numbers of pins/lengths.
	// "PADS" ==> Create separate on-board PAD objects for an off-board part.
	// "SWITCH_ST_DIP" must be tested before "SWITCH_ST_DIP".
	Q_DECL_CONSTEXPR size_t NUM_VARIABLE_PIN_PARTS = 13;
	const std::string strVar[NUM_VARIABLE_PIN_PARTS] = {"SIP", "DIP", "PADS", "SWITCH_ST_DIP", "SWITCH_ST", "SWITCH_DT", "STRIP_100MIL", "BLOCK_100MIL", "BLOCK_200MIL", "RESISTOR", "DIODE", "CAP_CERAMIC", "CAP_FILM"};

	// If footprint is variable length, then get the number of pins/length from typeStr.
	int numPins(0), nLength(0);	// Invalid by default
	for (size_t i = 0; i < NUM_VARIABLE_PIN_PARTS; i++)
	{
		const std::string&	strTmp	= strVar[i];	// e.g. "SIP", "DIP, etc
		const auto			L		= strTmp.length();
		if ( typeStr.length() >= L && typeStr.substr(0, L) == strTmp )
		{
			pinStr		= typeStr.substr(L);	// e.g. "DIP40" ==> "40"
			typeStrCut	= typeStr.substr(0, L);	// e.g. "DIP40" ==> "DIP"
			if ( typeStrCut == "PADS" )				// If we have an off-board part ...
			{
				typeStrCut = "SIP";					// ... treat it as a SIP for the moment
				offBoard.push_back(nameStr);		// ... and add it to the list of off-board parts
			}
			if ( typeStrCut == "RESISTOR" || typeStrCut == "DIODE" || typeStrCut == "CAP_CERAMIC" || typeStrCut == "CAP_FILM" )
			{
				nLength = atoi( pinStr.c_str() );	// Missing or zero ==> Use default length
				if ( nLength > 0 )					// The length is in 100ths of a mil ...
					nLength += 1;					// ... so must add 1 to get part length in grid squares
			}
			else	// DIP/SIP/SWITCH/STRIP/BLOCK
			{
				numPins = atoi( pinStr.c_str() );
				if ( numPins == 0 )					// Missing or zero ...
					numPins = -1;					// ... use -1 instead.  Don't use 0 as that implies "use default".
			}
			break;
		}
	}

	const std::string partErrorStr = ( bTango ? "Part section: " :  "Part: ") + nameStr;

	bool bCustom(false);	// true ==> We've found a custom template with matching import string
	Component custom;		// The matching custom template

	const COMP eType = CompTypes::GetTypeFromImportStr(typeStrCut);
	bool bOK = ( eType != COMP::CUSTOM && eType != COMP::TRACKS && eType != COMP::VERO_NUMBER && eType != COMP::VERO_LETTER && eType != COMP::INVALID );
	if ( !bOK )	// Search template manager
		bOK = bCustom = templateMgr.GetFromImportStr(typeStrCut, custom);
	if ( !bOK ) { errorStr = partErrorStr + "\nVeroRoute does not support the part type: " + typeStr; return bOK; }

	// Check pins per component is within limits
	if ( numPins == 0 ) numPins = ( bCustom ) ? static_cast<int>( custom.GetNumPins() ) : CompTypes::GetDefaultNumPins(eType);
	bOK = ( numPins > 0 );
	if ( !bOK ) { errorStr = partErrorStr + "\nInternal error: Part type has no pins " + typeStr; return bOK; }

	// Check length is within limits for components with fixed numbers of pins
	if ( nLength > 0 )
	{
		bOK = bCustom || ( nLength >= CompTypes::GetMinLength(eType) );
		if ( !bOK ) { errorStr = partErrorStr + "\nInternal error: Part length is too small " + typeStr; return bOK; }

		bOK = bCustom || ( nLength <= CompTypes::GetMaxLength(eType) );
		if ( !bOK ) { errorStr = partErrorStr + "\nInternal error: Part length is too large " + typeStr; return bOK; }
	}

	bOK = bCustom || ( numPins >= CompTypes::GetMinNumPins(eType) );
	if ( !bOK ) { errorStr = partErrorStr + "\nPart type has fewer pins than VeroRoute supports: " + typeStr; return bOK; }
	
	bOK = bCustom || ( numPins <= CompTypes::GetMaxNumPins(eType) );
	if ( !bOK ) { errorStr = partErrorStr + "\nPart type has more pins than VeroRoute supports: " + typeStr; return bOK; }

	if ( bCustom )
	{
		assert( custom.GetType() == COMP::CUSTOM );
		custom.SetNameStr(nameStr);
		custom.SetValueStr(valueStr);
		bOK = ( AddComponent(-1, -1, custom) != BAD_COMPID );	// Create part and place it
	}
	else
	{
		assert( eType != COMP::INVALID );
		std::vector<int>	nodeList;
		nodeList.resize(static_cast<size_t>(numPins), BAD_NODEID);
		Component tmp(nameStr, valueStr, eType, nodeList);
		if ( nLength > 0 )
		{
			while ( tmp.GetCols() < nLength ) tmp.Stretch(true);	// grow
			while ( tmp.GetCols() > nLength ) tmp.Stretch(false);	// shrink
		}
		bOK = ( AddComponent(-1, -1, tmp) != BAD_COMPID );	// Create part and place it
	}
	if ( !bOK ) errorStr = partErrorStr + "\nInternal error creating and placing the part";
	return bOK;
}

// Import Protel V1 / Tango netlist (exported from TinyCAD / gEDA)
bool Board::ImportTango(const TemplateManager& templateMgr, const std::string& filename, std::string& errorStr)
{
	Clear();

	std::string	nameStr;	// "R23","U1"			==> TinyCAD "Ref"     / gEDA "refdes"
	std::string	valueStr;	// "4k7", "TL072"		==> TinyCAD "Name"    / gEDA "device"
	std::string	typeStr;	// "RESISTOR","DIP8"	==> TinyCAD "Package" / gEDA "footprint"
	std::string	typeStrCut;	// Cut down version of typeStr. e.g.  DIP40 ==> DIP
	std::string	pinStr;		// Number of pins, or pin number
	std::string	netStr;		// Net name

	std::ifstream inStream;
	inStream.open(filename.c_str(), std::ios::in | std::ios::binary);
	bool bOK = inStream.is_open();
	bool bPart(false), bNet(false);	// Flags indicating "part" and "netlist" sections
	int iRow(0);					// Row counter within "part" and "netlist" sections
	int iNodeId(BAD_NODEID);		// Increase this with each imported net

	std::list<std::string> offBoard;	// List of off-board part names
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
				nameStr = str;	// TinyCAD "Ref" / gEDA "refdes"	==> VeroRoute "Name"
				bOK  = ( str.find("-") == std::string::npos );	// Name should not have a "-"
				if ( !bOK ) errorStr = "Part section: " + nameStr + "\nPart names must not contain a minus sign";
				if ( !bOK ) break;

				bOK = ( m_compMgr.GetComponentIdFromName(nameStr) == BAD_COMPID );	// Name must be unique
				if ( !bOK ) errorStr = "Part section: " + nameStr + "\nPart names must be unique";
				if ( !bOK ) break;
			}
			if ( iRow == 1 )
			{
				typeStrCut = typeStr = str;	// TinyCAD "Package" / gEDA "footprint"	==> VeroRoute "Type"
			}
			if ( iRow == 2 )
			{
				valueStr = str;	// TinyCAD "Name" / gEDA "device"	==> VeroRoute "Value"

				// Build the part and place it ...
				bOK = BuildAndPlacePart(templateMgr, nameStr, valueStr, typeStr, typeStrCut, pinStr, offBoard, errorStr, true);	// true ==> Tango
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

				pinStr = str.substr(pos+1);
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
	if ( bOK )
		BreakSIPSintoPADS(offBoard);

	return bOK;
}


// Import OrcadPCB2 netlist (exported from KiCAD)
bool Board::ImportOrcad(const TemplateManager& templateMgr, const std::string& filename, std::string& errorStr)
{
	Clear();

	std::string	nameStr;	// "R23"		==> KiCAD "Reference"
	std::string	valueStr;	// "4k7"		==> KiCAD "Value"
	std::string	typeStr;	// "RESISTOR"	==> KiCAD "Footprint"
	std::string	typeStrCut;	// Cut down version of typeStr. e.g.  DIP40 ==> DIP
	std::string	pinStr;		// Number of pins, or pin number
	std::string	netStr;		// Net name

	std::ifstream inStream;
	inStream.open(filename.c_str(), std::ios::in | std::ios::binary);
	bool bOK = inStream.is_open();
	int maxNodeId(BAD_NODEID);		// Increase this with each new node we encounter

	std::unordered_map<std::string, int> mapNetToNodeId;

	bool bPartStart(false);
	std::list<std::string> offBoard;	// List of off-board part names

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

			typeStrCut	= typeStr = strList[2];
			nameStr		= strList[3];
			valueStr	= ( numSubStrings == 5 ) ? strList[4] : "";

			bOK = ( m_compMgr.GetComponentIdFromName(nameStr) == BAD_COMPID );	// Name must be unique
			if ( !bOK ) errorStr = "\nPart name " + nameStr + " is not unique";
			if ( !bOK ) break;

			// Build the part and place it ...
			bOK = BuildAndPlacePart(templateMgr, nameStr, valueStr, typeStr, typeStrCut, pinStr, offBoard, errorStr, false);	// false ==> Orcad
			if ( !bOK ) break;

			bPartStart = true;
		}
		else
		{
			// we're in the pin section...
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
			if ( !bOK ) errorStr = "Expecting format:  ( PINNUMBER NETNAME )  , but got:" + str;
			if ( !bOK ) break;

			pinStr = strList[1];
			netStr = strList[2];

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
			if ( !bOK ) errorStr = "Internal error: VeroRoute has run out of net IDs";
			if ( !bOK ) break;

			// Paint the component pin using SetNodeIdByUser
			const size_t	iPinIndex	= static_cast<size_t>( atoi(pinStr.c_str()) - 1 );
			const int		compId		= m_compMgr.GetComponentIdFromName( nameStr );

			bOK = compId != BAD_COMPID;
			if ( !bOK ) errorStr = "Internal error: VeroRoute lost the component ID";
			if ( !bOK ) break;

			const Component& comp = m_compMgr.GetComponentById( compId );
			assert( comp.GetType() != COMP::INVALID );

			bOK = iPinIndex < comp.GetNumPins();
			if ( !bOK ) errorStr = "Part: " + nameStr + "\nLine has invalid pin number: " + str;
			if ( !bOK ) break;
			
			int row, col;
			bOK = GetPinRowCol(compId, iPinIndex, row, col);
			if ( !bOK ) errorStr = "Part: " + nameStr + "\nLine: " + str + "\nInternal error mapping the pin to a board location";
			if ( !bOK ) break;

			SetNodeIdByUser(0, row, col, nodeId, true);	// true ==> paint pins
		}
	}
	if ( inStream.is_open() ) inStream.close();

	// Break the SIPs representing off-board parts into PADs
	if ( bOK )
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
