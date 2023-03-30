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

#include "Template.h"

// Manager class to handle component templates

Q_DECL_CONSTEXPR static const int ALIASES_FILE_VERSION_1 = 1;
Q_DECL_CONSTEXPR static const int ALIASES_FILE_VERSION_CURRENT = ALIASES_FILE_VERSION_1;

struct StringPair
{
	StringPair(const std::string& importStr) : m_importStr(importStr) {}
	StringPair(const std::string& importStr, const std::string& notesStr) : m_importStr(importStr), m_notesStr(notesStr) {}
	bool operator<(const StringPair& o) const
	{
		if ( m_importStr != o.m_importStr ) return m_importStr < o.m_importStr;
		return m_notesStr < o.m_notesStr;	// Should not happen in practice
	}
	bool operator==(const StringPair& o) const
	{
		return m_importStr	== o.m_importStr
			&& m_notesStr	== o.m_notesStr;
	}
	std::string m_importStr;
	std::string m_notesStr;
};

class TemplateManager
{
public:
	TemplateManager()	{}
	~TemplateManager()	{ SaveAliasFile(); }
	const std::string& GetPathStr() const { return m_pathStr; }
	void SetPathStr(const std::string& str)	{ m_pathStr = str; LoadAliasFile(); }
	size_t GetSize(bool bGeneric) const
	{
		return bGeneric ? m_listGeneric.size() : m_listUser.size();
	}
	const Component& GetNth(bool bGeneric, size_t N) const
	{
		assert( N < GetSize(bGeneric) );
		auto iter = bGeneric ? m_listGeneric.begin() : m_listUser.begin();
		for (size_t i = 0; i < N; i++) iter++;
		return *iter;
	}

	// Aliases for part types (footprints)
	const std::map<std::string, std::string>& GetMapAliasToImportStr() const { return m_mapAliasToImportStr; }
	const std::string& GetImportStrFromAlias(const std::string& aliasStr) const
	{
		static std::string emptyStr("");
		auto iter = m_mapAliasToImportStr.find(aliasStr);
		return ( iter != m_mapAliasToImportStr.end() ) ? iter->second : emptyStr;
	}
	void RemoveAlias(const std::string& aliasStr)
	{
		const auto iter = m_mapAliasToImportStr.find(aliasStr);
		if ( iter != m_mapAliasToImportStr.end() ) m_mapAliasToImportStr.erase(iter);
	}
	void RemoveAllAliases() { m_mapAliasToImportStr.clear(); }
	void AddAlias(const std::string& aliasStr, const std::string& importStr) { m_mapAliasToImportStr[aliasStr] = importStr; }
	void ClearInvalidAliases() { for (auto& o : m_mapAliasToImportStr) if ( !CheckPartOK(o.second) ) o.second = ""; }

	// Persistance of aliases between sessions
	QString GetAliasFilename() const
	{
		char buffer[256] = {'\0'};
		sprintf(buffer, "%s/aliases/all.dat", GetPathStr().c_str());	// Maybe use separate files for Tango and Orcad in future
		return QString(buffer);
	}
	void LoadAliasFile()
	{
		m_mapAliasToImportStr.clear();
		DataStream inStream(DataStream::READ);
		if ( inStream.Open( GetAliasFilename() ) )
		{
			int iAliasFileVersion(0);
			inStream.Load(iAliasFileVersion);
			unsigned int iSize(0);
			inStream.Load(iSize);
			for(unsigned int i = 0; i < iSize; i++)
			{
				std::string A, B;
				inStream.Load(A);
				inStream.Load(B);
				m_mapAliasToImportStr[A] = B;
			}
			inStream.Close();
		}
	}
	void SaveAliasFile()
	{
		DataStream outStream(DataStream::WRITE);
		if ( outStream.Open( GetAliasFilename() ) )
		{
			outStream.Save(ALIASES_FILE_VERSION_CURRENT);
			outStream.Save(static_cast<unsigned int>( m_mapAliasToImportStr.size() ));
			for (const auto& o : m_mapAliasToImportStr)
			{
				outStream.Save(o.first);
				outStream.Save(o.second);
			}
			outStream.Close();
		}
	}
	void AddDefaults()
	{
		std::string nameStr(""), valueStr("");
		for (const auto& eType : CompTypes::GetListCompTypes())
		{
			if ( eType == COMP::TRACKS ) continue;		// Not a real component
			if ( eType == COMP::VERO_NUMBER ) continue;	// Not a real component
			if ( eType == COMP::VERO_LETTER ) continue;	// Not a real component
			if ( eType == COMP::CUSTOM ) continue;		// Don't show custom components in the left pane
			const int numPins = CompTypes::GetDefaultNumPins(eType);
			std::vector<int> nodeList;
			nodeList.resize(static_cast<size_t>(numPins), BAD_NODEID);
			Component comp(nameStr, valueStr, eType, nodeList);

			comp.SetId(BAD_COMPID);	// This indicates a component template
			Add(true, comp);	// true ==> generic
		}
	}
	bool Add(bool bGeneric, const Component& comp, std::string* pErrorStr = nullptr)
	{
		Template entry;
		if ( !entry.MakeTemplate(comp) )
		{
			if ( pErrorStr ) *pErrorStr = "MakeTemplate() failed";
			return false;
		}

		auto& lst = bGeneric ? m_listGeneric : m_listUser;

		if ( !bGeneric )	// A new entry in the "User-Defined" list need extra checks ...
		{
			for (const auto& o : lst)
			{
				// ... it must have a unique (TypeStr,ValueStr) combination
				if ( entry.GetFullTypeStr() == o.GetFullTypeStr() && entry.GetValueStr() == o.GetValueStr() )
				{
					if ( pErrorStr ) *pErrorStr = "The template (Type = " + o.GetFullTypeStr() + ") "
												+ "(Value = " + o.GetValueStr() + ") already exists";
					return false;
				}
				// ... if its a COMP::CUSTOM part, then it must have a unique import string
				if ( entry.GetType() == COMP::CUSTOM )
				{
					if ( !entry.GetImportStr().empty() && entry.GetImportStr() == o.GetImportStr() )
					{
						if ( pErrorStr ) *pErrorStr = "The template (Type = " + o.GetFullTypeStr() + ") "
													+ "(Value = " + o.GetValueStr() + ") "
													+ "already has the Import string " + o.GetImportStr();
						return false;
					}
				}
			}
		}

		// We've got a valid entry, so insert it at the relevant place in the list

		auto iter = lst.begin();
		bool bOK = iter == lst.end() || entry.IsLessThan(*iter, bGeneric);
		if ( bOK ) lst.insert(iter, entry);

		while( iter != lst.end() && !bOK )
		{
			Template& prev = *iter;
			++iter;
			bOK = prev.IsLessThan(entry, bGeneric)
			   && ( iter == lst.end() || entry.IsLessThan(*iter, bGeneric) );
			if ( bOK ) lst.insert(iter, entry);
		}
		if ( !bOK )
		{
			if ( pErrorStr ) *pErrorStr = "Could not insert template in list";
		}
		return bOK;
	}
	bool Remove(const Component& comp)
	{
		Template entry;
		if ( !entry.MakeTemplate(comp) ) return false;

		const auto iter = std::find(m_listUser.begin(), m_listUser.end(), entry);
		const bool bOK = ( iter != m_listUser.end() );
		if ( bOK ) m_listUser.erase(iter);	// Erase entry if it exists
		return bOK;
	}
	bool GetFromImportStr(const std::string& importStr, Component& out) const
	{
		if ( importStr.empty() ) return false;
		for (const auto& o : m_listUser)
			if ( o.GetType() == COMP::CUSTOM && o.GetImportStr() == importStr ) { out = o; return true; }
		return false;
	}
	// Helper for alias dialog
	void CalcValidImportStrings(std::list<StringPair>& outList)
	{
		outList.clear();
		outList.push_back( StringPair("PADS") );	// Special case.  PADS is an allowed import string (like a SIP but broken into separate objects)
		for (const auto& o : m_listGeneric)
			if ( !o.GetImportStr().empty() )
				outList.push_back( StringPair(o.GetImportStr()) );
		for (const auto& o : m_listUser)
			if ( o.GetType() == COMP::CUSTOM && !o.GetImportStr().empty() )
				outList.push_back( StringPair(o.GetImportStr()) );

		// Also see how suffices are handled in CheckPartOK() below
		for (auto& o : outList)
		{
			const bool bPADS = ( o.m_importStr == "PADS" );
			const COMP eType = bPADS ? COMP::SIP : CompTypes::GetTypeFromImportStr(o.m_importStr);	// Treat PADS like SIP regarding number of pins
			const int minPins = CompTypes::GetMinNumPins(eType);
			const int maxPins = CompTypes::GetMaxNumPins(eType);
			const int modPins = CompTypes::GetModuloNumPins(eType);

			switch(eType)
			{
				case COMP::SIP:
				case COMP::DIP:
				case COMP::SWITCH_ST_DIP:
				case COMP::SWITCH_ST:
				case COMP::SWITCH_DT:
				case COMP::STRIP_100:
				case COMP::BLOCK_100:
				case COMP::BLOCK_200:
					o.m_importStr += "x";
					o.m_notesStr = "x=[" + std::to_string(minPins) + "," + std::to_string(maxPins) + "] is the number of pins."
								 + ( modPins != 1 ? ("  x must divide by " + std::to_string(modPins) + ".") : "" )
								 + ( bPADS ? "  Each pin becomes a separate 'Pad' object." : "" );
					break;
				case COMP::RESISTOR:
				case COMP::INDUCTOR:
				case COMP::DIODE:
					o.m_importStr += "x";
					o.m_notesStr = "x=[" + std::to_string(CompTypes::GetMinLength(eType)-1) + ","
										 + std::to_string(CompTypes::GetMaxLength(eType)-1) + "] is the length in units of 100 mil.  Omit x for 300 mil default.";
					break;
				case COMP::CAP_CERAMIC:
				case COMP::CAP_FILM:
				case COMP::CAP_FILM_WIDE:
					o.m_importStr += "x";
					o.m_notesStr = "x=[" + std::to_string(CompTypes::GetMinLength(eType)-1) + ","
										 + std::to_string(CompTypes::GetMaxLength(eType)-1) + "] is the length in units of 100 mil.  Omit x for 200 mil default.";
					break;
				default:	break;
			}
		}
		outList.sort();
		outList.unique();
		// Make sure each import string is not listed as an alias
		for (auto& o : outList) RemoveAlias(o.m_importStr);
	}
	// Following is a helper for the import code
	bool CheckPartOK(const std::string& importStr) const { return CheckPartOK(CompStrings("", "", importStr)); }
	bool CheckPartOK(const CompStrings& compStrings, std::list<std::string>* pOffBoard = nullptr, std::string* pErrorStr = nullptr, Component* pComp = nullptr) const
	{
		const std::string& nameStr		= compStrings.m_nameStr;
		const std::string& valueStr		= compStrings.m_valueStr;
		const std::string& importStr	= compStrings.m_importStr;

		std::string dummyStr;
		std::string& errorStr = ( pErrorStr ) ? *pErrorStr : dummyStr;

		Component dummyComp;
		Component& comp = ( pComp ) ? *pComp : dummyComp;

		// List of package identifiers for footprints with variable numbers of pins/lengths.
		// "PADS" ==> Create separate on-board PAD objects for an off-board part.
		// "SWITCH_ST_DIP" must be tested before "SWITCH_ST_DIP".
		Q_DECL_CONSTEXPR size_t NUM_VARIABLE_PIN_PARTS = 15;
		const std::string strVar[NUM_VARIABLE_PIN_PARTS] = {"SIP", "DIP", "PADS", "SWITCH_ST_DIP", "SWITCH_ST", "SWITCH_DT", "STRIP_100MIL", "BLOCK_100MIL", "BLOCK_200MIL", "RESISTOR", "INDUCTOR", "DIODE", "CAP_CERAMIC", "CAP_FILM", "CAP_FILM_WIDE"};

		// If footprint is variable length, then get the number of pins/length from importStr.
		std::string	importStrCut( importStr );	// Cut down version of importStr. e.g.  DIP40 ==> DIP
		std::string	pinStr;					// Number of pins
		int numPins(0), nLength(0);	// Invalid by default

		bool bOffBoard(false);
		for (size_t i = 0; i < NUM_VARIABLE_PIN_PARTS; i++)
		{
			const std::string&	strTmp	= strVar[i];	// e.g. "SIP", "DIP, etc
			const auto			L		= strTmp.length();
			if ( importStr.length() >= L && importStr.substr(0, L) == strTmp )
			{
				pinStr			= importStr.substr(L);		// e.g. "DIP40" ==> "40"
				importStrCut	= importStr.substr(0, L);	// e.g. "DIP40" ==> "DIP"
				if ( importStrCut == "PADS" )	// If we have an off-board part ...
				{
					bOffBoard		= true;
					importStrCut	= "SIP";	// ... treat it as a SIP for the moment
				}
				if ( importStrCut == "RESISTOR" || importStrCut == "INDUCTOR" || importStrCut == "DIODE" || importStrCut == "CAP_CERAMIC" || importStrCut == "CAP_FILM" || importStrCut == "CAP_FILM_WIDE" )
				{
					nLength = atoi( pinStr.c_str() );	// Missing or zero ==> Use default length
					if ( nLength > 0 )					// The length is in 100ths of a mil ...
						nLength += 1;					// ... so must add 1 to get part length in grid squares
				}
				else	// SIP, DIP, SWITCH_ST_DIP, SWITCH_ST, SWITCH_DT, STRIP_100MIL, BLOCK_100MIL, BLOCK_200MIL
				{
					numPins = atoi( pinStr.c_str() );
					if ( numPins == 0 )					// Missing or zero ...
						numPins = -1;					// ... use -1 instead.  Don't use 0 as that implies "use default".
				}
				break;
			}
		}

		const std::string strID = "Part: Name = " + nameStr + ", Value = " + valueStr + ", Type = " + importStr;

		bool bCustom(false);	// true ==> We've found a custom template with matching import string

		const COMP eType = CompTypes::GetTypeFromImportStr(importStrCut);
		bool bOK = ( eType != COMP::CUSTOM && eType != COMP::TRACKS && eType != COMP::VERO_NUMBER && eType != COMP::VERO_LETTER && eType != COMP::INVALID );
		if ( !bOK )	// Search template manager
			bOK = bCustom = GetFromImportStr(importStrCut, comp);
		if ( !bOK ) { errorStr = strID + "\nError: Unknown part type"; return bOK; }

		// Check pins per component is within limits
		if ( numPins == 0 ) numPins = ( bCustom ) ? static_cast<int>( comp.GetNumPins() ) : CompTypes::GetDefaultNumPins(eType);
		bOK = ( numPins > 0 );
		if ( !bOK ) { errorStr = strID + "\nError: Part has no pins"; return bOK; }

		const int minPins	= CompTypes::GetMinNumPins(eType);
		const int maxPins	= CompTypes::GetMaxNumPins(eType);
		const int modPins	= CompTypes::GetModuloNumPins(eType);

		bOK = bCustom || ( numPins >= minPins );
		if ( !bOK ) { errorStr = strID + "\nError: Part has fewer pins than VeroRoute supports"; return bOK; }

		bOK = bCustom || ( numPins <= maxPins );
		if ( !bOK ) { errorStr = strID + "\nError: Part has more pins than VeroRoute supports"; return bOK; }

		bOK = bCustom || ( 0 == numPins % modPins );
		if ( !bOK ) { errorStr = strID + "\nError: Part has pins that is not a multiple of " + std::to_string(modPins); return bOK; }

		// Check length is within limits for components with fixed numbers of pins
		if ( nLength > 0 )
		{
			bOK = bCustom || ( nLength >= CompTypes::GetMinLength(eType) );
			if ( !bOK ) { errorStr = strID + "\nError: Part length is smaller than VeroRoute support"; return bOK; }

			bOK = bCustom || ( nLength <= CompTypes::GetMaxLength(eType) );
			if ( !bOK ) { errorStr = strID + "\nError: Part length is larger than VeroRoute supports"; return bOK; }
		}

		if ( bCustom )
		{
			assert( comp.GetType() == COMP::CUSTOM );
			comp.SetNameStr(nameStr);
			comp.SetValueStr(valueStr);
		}
		else
		{
			assert( eType != COMP::INVALID );
			std::vector<int>	nodeList;
			nodeList.resize(static_cast<size_t>(numPins), BAD_NODEID);

			comp = Component(nameStr, valueStr, eType, nodeList);
			if ( nLength > 0 )
			{
				while ( comp.GetCols() < nLength ) comp.Stretch(true);	// grow
				while ( comp.GetCols() > nLength ) comp.Stretch(false);	// shrink
			}
		}

		if ( bOK && bOffBoard && pOffBoard )	// If have a valid offboard part (i.e. PADS with a valid suffix)
			pOffBoard->push_back(nameStr);		// ... add it to the list of off-board parts
		return bOK;
	}
private:
	std::string							m_pathStr;				// Path to the "templates" and "aliases" folders
	std::list<Template>					m_listGeneric;			// List of generic components
	std::list<Template>					m_listUser;				// List of template components
	std::map<std::string, std::string>	m_mapAliasToImportStr;	// Aliases for VeroRoute import strings
};
