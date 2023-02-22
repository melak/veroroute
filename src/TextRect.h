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
#include "MyRGB.h"

// Bitfield for text style
Q_DECL_CONSTEXPR static const int TEXT_NORMAL	 = 0;
Q_DECL_CONSTEXPR static const int TEXT_BOLD		 = 1;
Q_DECL_CONSTEXPR static const int TEXT_ITALIC	 = 2;
Q_DECL_CONSTEXPR static const int TEXT_UNDERLINE = 4;

class TextRect : public Rect, public MyRGB
{
public:
	TextRect() : Rect(), MyRGB() {}
	TextRect(int rowMin, int rowMax, int colMin, int colMax) : Rect(rowMin, rowMax, colMin, colMax), MyRGB() {}
	virtual ~TextRect() override {}
	TextRect(const TextRect& o)	: Rect(o), MyRGB(o) { *this = o; }
	TextRect& operator=(const TextRect& o)
	{
		Rect::operator=(o);
		MyRGB::operator=(o);
		m_str		= o.m_str;
		m_size		= o.m_size;
		m_style		= o.m_style;
		m_flagsH	= o.m_flagsH;
		m_flagsV	= o.m_flagsV;
		return *this;
	}
	bool operator==(const TextRect& o) const
	{
		return Rect::operator==(o)
			&& MyRGB::operator==(o)
			&& m_str	== o.m_str
			&& m_size	== o.m_size
			&& m_style	== o.m_style
			&& m_flagsH	== o.m_flagsH
			&& m_flagsV	== o.m_flagsV;
	}
	bool operator!=(const TextRect& o) const
	{
		return !(*this == o);
	}
	bool SetStr(const std::string& s)	{ const bool bChanged = (m_str    != s);	m_str    = s; return bChanged; }
	bool SetSize(int i)					{ const bool bChanged = (m_size   != i);	m_size   = i; return bChanged; }
	bool SetStyle(int i)				{ const bool bChanged = (m_style  != i);	m_style  = i; return bChanged; }
	bool SetFlagsH(int i)				{ const bool bChanged = (m_flagsH != i);	m_flagsH = i; return bChanged; }
	bool SetFlagsV(int i)				{ const bool bChanged = (m_flagsV != i);	m_flagsV = i; return bChanged; }
	const std::string&	GetStr() const		{ return m_str; }
	const int&			GetSize() const		{ return m_size; }
	const int&			GetStyle() const	{ return m_style; }
	const int&			GetFlagsH() const	{ return m_flagsH; }
	const int&			GetFlagsV() const	{ return m_flagsV; }
	// Persist interface functions
	virtual void Load(DataStream& inStream) override
	{
		Rect::Load(inStream);
		inStream.Load(m_str);
		inStream.Load(m_size);
		inStream.Load(m_style);
		inStream.Load(m_flagsH);
		if ( inStream.GetVersion() >= VRT_VERSION_49 )
			inStream.Load(m_flagsV);	// Added in VRT_VERSION_49
		if ( inStream.GetVersion() >= VRT_VERSION_15 )
			MyRGB::Load(inStream);		// Added in VRT_VERSION_15
	}
	virtual void Save(DataStream& outStream) override
	{
		Rect::Save(outStream);
		outStream.Save(m_str);
		outStream.Save(m_size);
		outStream.Save(m_style);
		outStream.Save(m_flagsH);
		outStream.Save(m_flagsV);	// Added in VRT_VERSION_49
		MyRGB::Save(outStream);		// Added in VRT_VERSION_15
	}
private:
	std::string	m_str;
	int			m_size	= 9;					// Point size
	int			m_style		= TEXT_NORMAL;		// Bitfield using TEXT_NORMAL, TEXT_BOLD, TEXT_ITALIC, TEXT_UNDERLINE
	int			m_flagsH	= Qt::AlignJustify;	// Qt::AlignLeft,Qt::AlignRight,Qt::AlignHCenter,Qt::AlignJustify
	int			m_flagsV	= Qt::AlignTop;		// Qt::AlignTop,Qt::AlignVCenter,Qt::AlignBottom
};
