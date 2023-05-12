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

#include "Component.h"

class Template : public Component
{
public:
	Template()	{}
	Template(const Template& o) : Component() { *this = o; }
	Template& operator=(const Template& o)
	{
		Component::operator=(o);	// Call operator= in base class
		return *this;
	}
	virtual ~Template() override {}
	bool MakeTemplate(const Component& o)	// Clears data not relevant to the template definition
	{
		Component::operator=(o);	// Call operator= in base class
		SetImportStr( GetFullImportStr() );
		if ( GetIsTemplate() ) return true;	// Nothing more to do
		switch( GetType() )
		{
			case COMP::TO92:
			case COMP::TO18:
			case COMP::TO39:
			case COMP::TO220:
			case COMP::SOIC8:
			case COMP::SOIC14:
			case COMP::SOIC16:
			case COMP::SOIC14W:
			case COMP::SOIC16W:
			case COMP::SOIC20W:
			case COMP::SOIC24W:
			case COMP::SOIC28W:
			case COMP::DIP:
			case COMP::DIP_RECTIFIER:
			case COMP::SIP:
			case COMP::CUSTOM:
				SetId(BAD_COMPID);	// This bad ID indicates the component is actually a template
				// Wipe irrelevant data
				SetNameStr("");
				for (size_t i = 0; i < GetNumPins(); i++)
				{
					SetNodeId(i, BAD_NODEID);
					for (int lyr = 0; lyr < 2; lyr++) SetOrigId(lyr, i, BAD_NODEID);
				}
				SetRow(0); SetCol(0);
				SetDirection('W');
				SetIsPlaced(false);
				return true;
			default: return false;
		}
	}
	bool IsLessThan(const Template& o, bool bGeneric) const
	{
		if ( bGeneric ) return *this < o;

		std::string a = GetFullTypeStr();
		std::string b = o.GetFullTypeStr();
		int i = a.compare( b );								// Compare Type strings
		if ( i != 0 ) return i < 0;
		return GetValueStr().compare( o.GetValueStr() );	// Compare Value strings
	}
	bool operator<(const Template& o) const
	{
		// First order by type
		if ( CompTypes::GetListOrder(GetType()) != CompTypes::GetListOrder(o.GetType()) ) return CompTypes::GetListOrder(GetType()) < CompTypes::GetListOrder(o.GetType());
		if ( GetType() != o.GetType() ) return static_cast<int>(GetType()) < static_cast<int>(o.GetType());
		// .. then by least pins
		if ( GetNumPins() != o.GetNumPins() ) return GetNumPins() < o.GetNumPins();
		// ... then by type string comparison
		const int iCompareType = GetTypeStr().compare( o.GetTypeStr() );
		if ( iCompareType != 0 ) return iCompareType < 0;
		// ... then by value string comparison
		const int iCompareVal = GetValueStr().compare( o.GetValueStr() );
		return iCompareVal < 0;
	}
	bool operator==(const Template& o) const
	{
		return GetType()		== o.GetType()
			&& GetNumPins()		== o.GetNumPins()
			&& GetTypeStr()		== o.GetTypeStr()
			&& GetValueStr()	== o.GetValueStr();
	}
};
