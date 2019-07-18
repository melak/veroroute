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

#include "Rect.h"

// Bitfield for text style
const int TEXT_NORMAL		= 0;
const int TEXT_BOLD			= 1;
const int TEXT_ITALIC		= 2;
const int TEXT_UNDERLINE	= 4;

class TextRect : public Rect
{
public:
	TextRect() : Rect()	{ SetDefaults(); }
	TextRect(int rowMin, int rowMax, int colMin, int colMax) : Rect(rowMin, rowMax, colMin, colMax) { SetDefaults(); }
	~TextRect()	{}
	TextRect(const TextRect& o)	: Rect(o) { *this = o; }
	TextRect& operator=(const TextRect& o)
	{
		Rect::operator=(o);
		m_str	= o.m_str;
		m_size	= o.m_size;
		m_style	= o.m_style;
		m_flags	= o.m_flags;
		m_R		= o.m_R;
		m_G		= o.m_G;
		m_B		= o.m_B;
		return *this;
	}
	bool operator==(const TextRect& o) const
	{
		return Rect::operator==(o)
			&& m_str	== o.m_str
			&& m_size	== o.m_size
			&& m_style	== o.m_style
			&& m_flags	== o.m_flags
			&& m_R		== o.m_R
			&& m_G		== o.m_G
			&& m_B		== o.m_B;
	}
	bool operator!=(const TextRect& o) const
	{
		return !(*this == o);
	}
	void SetDefaults()
	{
		m_str.clear();
		m_size	= 9;
		m_style	= TEXT_NORMAL;
		m_flags	= Qt::AlignJustify;
		m_R = m_G = m_B = 0;
	}
	bool SetStr(const std::string& s)	{ const bool bChanged = (m_str   != s);	m_str   = s; return bChanged; }
	bool SetSize(const int& i)			{ const bool bChanged = (m_size  != i);	m_size  = i; return bChanged; }
	bool SetStyle(const int& i)			{ const bool bChanged = (m_style != i);	m_style = i; return bChanged; }
	bool SetFlags(const int& i)			{ const bool bChanged = (m_flags != i);	m_flags = i; return bChanged; }
	bool SetRGB(const int& R, const int& G, const int& B)
	{
		const bool bChanged = (m_R != R || m_G != G || m_B != B);
		m_R = R; m_G = G; m_B = B;
		return bChanged;
	}
	const std::string&	GetStr() const		{ return m_str; }
	const int&			GetSize() const		{ return m_size; }
	const int&			GetStyle() const	{ return m_style; }
	const int&			GetFlags() const	{ return m_flags; }
	const int&			GetR() const		{ return m_R; }
	const int&			GetG() const		{ return m_G; }
	const int&			GetB() const		{ return m_B; }
	// Persist interface functions
	virtual void Load(DataStream& inStream) override
	{
		Rect::Load(inStream);
		inStream.Load(m_str);
		inStream.Load(m_size);
		inStream.Load(m_style);
		inStream.Load(m_flags);
		if ( inStream.GetVersion() >= VRT_VERSION_15 )
		{
			inStream.Load(m_R);	// Added in VRT_VERSION_15
			inStream.Load(m_G);	// Added in VRT_VERSION_15
			inStream.Load(m_B);	// Added in VRT_VERSION_15
		}
	}
	virtual void Save(DataStream& outStream) override
	{
		Rect::Save(outStream);
		outStream.Save(m_str);
		outStream.Save(m_size);
		outStream.Save(m_style);
		outStream.Save(m_flags);
		outStream.Save(m_R);	// Added in VRT_VERSION_15
		outStream.Save(m_G);	// Added in VRT_VERSION_15
		outStream.Save(m_B);	// Added in VRT_VERSION_15
	}
private:
	std::string	m_str;
	int			m_size;			// Point size
	int			m_style;		// Bitfield using TEXT_NORMAL, TEXT_BOLD, TEXT_ITALIC, TEXT_UNDERLINE
	int			m_flags;		// Qt::AlignLeft,Qt::AlignRight,Qt::AlignHCenter,Qt::AlignJustify
	int			m_R, m_G, m_B;	// Color
};
