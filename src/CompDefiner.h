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

#include "Grid.h"
#include "Shape.h"

class Component;

// Class used to build a description of a custom component

// The following flags are used to form a bitfield that determine how pins may be drawn
Q_DECL_CONSTEXPR static const uchar	PIN_RECT	= 1;	// Draw pin as a rectangle instead of circle (e.g. for switches/relays)
Q_DECL_CONSTEXPR static const uchar	PIN_LABELS	= 2;	// Allow pin labels to be drawn
Q_DECL_CONSTEXPR static const uchar	PIN_CUSTOM	= 4;	// Allow over-ride of pad and hole size

Q_DECL_CONSTEXPR static const int	BAD_ID		= -1;

// Quicker to use struct than a std::pair
struct IntShape
{
	IntShape(int i, const Shape& s) : first(i), second(s) {}
	IntShape(const IntShape& o) { *this = o; }
	IntShape& operator=(const IntShape& o)
	{
		first	= o.first;
		second	= o.second;
		return *this;
	}
	bool operator<(const IntShape& o) const
	{
		if ( second != o.second ) return second < o.second;
		return first < o.first;
	}
	bool operator==(const IntShape& o) const
	{
		return first == o.first && second == o.second;
	}
	bool operator!=(const IntShape& o) const
	{
		return !(*this == o);
	}
	int		first;
	Shape	second;
};

class TemplateManager;

class CompDefiner : public Persist
{
public:
	CompDefiner() { Clear(); }
	virtual ~CompDefiner() {}
	CompDefiner(const CompDefiner& o) { *this = o; }
	void Populate(const Component& o);	// Set up using an existing component
	void Clear()
	{
		m_currentPinId = m_currentShapeId = BAD_ID;
		m_iPinFlags = 0; m_iPadWidth = 70; m_iHoleWidth = 35;
		m_bAllowFlyWire = false;
		m_valueStr = m_prefixStr = m_typeStr = m_importStr = "";
		m_iLabelOffsetRow	= m_iLabelOffsetCol = 0;
		m_grid.Allocate(1,4,4);
		m_grid.Clear( Pin(BAD_PINCHAR, SURFACE_FULL, HOLE_FREE) );
		m_pinLabels.clear();
		m_pinAligns.clear();
		m_mapShapes.clear();
		AddRect();	// Provide a Rect by default
	}
	CompDefiner& operator=(const CompDefiner& o)
	{
		m_currentPinId		= o.m_currentPinId;
		m_currentShapeId	= o.m_currentShapeId;
		m_iPinFlags			= o.m_iPinFlags;
		m_iPadWidth			= o.m_iPadWidth;
		m_iHoleWidth		= o.m_iHoleWidth;
		m_bAllowFlyWire		= o.m_bAllowFlyWire;
		m_valueStr			= o.m_valueStr;
		m_prefixStr			= o.m_prefixStr;
		m_typeStr			= o.m_typeStr;
		m_importStr			= o.m_importStr;
		m_iLabelOffsetRow	= o.m_iLabelOffsetRow;
		m_iLabelOffsetCol	= o.m_iLabelOffsetCol;
		m_grid				= o.m_grid;
		AllocatePins( o.GetNumPins() );
		std::copy(o.m_pinLabels.begin(), o.m_pinLabels.end(), m_pinLabels.begin());
		std::copy(o.m_pinAligns.begin(), o.m_pinAligns.end(), m_pinAligns.begin());
		m_mapShapes.clear();
		for (const auto& mapObj : o.m_mapShapes) m_mapShapes.push_back(mapObj);
		return *this;
	}
	size_t GetNumPins() const
	{
		return m_pinLabels.size();
	}
	void AllocatePins(size_t numPins)
	{
		m_pinLabels.clear();	m_pinLabels.resize(numPins, "");
		m_pinAligns.clear();	m_pinAligns.resize(numPins, Qt::AlignHCenter);
		SetDefaultPinLabels();
	}
	void ReAllocatePins(size_t maxPinNumber)
	{
		// Take copy of old array values
		std::vector<std::string>	labels;	labels.resize(GetNumPins());
		std::vector<int>			aligns;	aligns.resize(GetNumPins());
		std::copy(m_pinLabels.begin(), m_pinLabels.end(), labels.begin());
		std::copy(m_pinAligns.begin(), m_pinAligns.end(), aligns.begin());
		AllocatePins(maxPinNumber);
		// Use old values in new arrays
		std::copy(labels.begin(), labels.end(), m_pinLabels.begin());
		std::copy(aligns.begin(), aligns.end(), m_pinAligns.begin());
	}
	void SetDefaultPinLabels()
	{
		for (size_t i = 0, iSize = GetNumPins(); i < iSize; i++)
			m_pinLabels[i] = CompTypes::GetDefaultPinLabel(i);
	}
	bool operator==(const CompDefiner& o) const	// Compare persisted info
	{
		bool bOK = m_currentPinId		== o.m_currentPinId
				&& m_currentShapeId		== o.m_currentShapeId
				&& m_iPinFlags			== o.m_iPinFlags
				&& m_iPadWidth			== o.m_iPadWidth
				&& m_iHoleWidth			== o.m_iHoleWidth
				&& m_bAllowFlyWire		== o.m_bAllowFlyWire
				&& m_valueStr			== o.m_valueStr
				&& m_prefixStr			== o.m_prefixStr
				&& m_typeStr			== o.m_typeStr
				&& m_importStr			== o.m_importStr
				&& m_iLabelOffsetRow	== o.m_iLabelOffsetRow
				&& m_iLabelOffsetCol	== o.m_iLabelOffsetCol
				&& m_grid				== o.m_grid
				&& m_pinLabels.size()	== o.m_pinLabels.size()
				&& m_pinAligns.size()	== o.m_pinAligns.size()
				&& m_mapShapes.size()	== o.m_mapShapes.size();
		if ( !bOK ) return false;
		for (size_t i = 0, iSize = m_pinLabels.size(); i < iSize; i++)
			if ( m_pinLabels[i] != o.m_pinLabels[i] ) return false;
		for (size_t i = 0, iSize = m_pinAligns.size(); i < iSize; i++)
			if ( m_pinAligns[i] != o.m_pinAligns[i] ) return false;
		for (auto iterA = m_mapShapes.begin(), iterB = o.m_mapShapes.begin(); iterA != m_mapShapes.end() && iterB != o.m_mapShapes.end(); ++iterA, ++iterB)
			if ( (*iterA) != (*iterB) ) return false;
		return true;
	}
	bool operator!=(const CompDefiner& o) const
	{
		return !(*this == o);
	}
	bool SetCurrentPinId(int i)				{ const bool bChanged = ( m_currentPinId	!= i );	m_currentPinId		= i; return bChanged; }
	bool SetCurrentShapeId(int i)			{ const bool bChanged = ( m_currentShapeId	!= i );	m_currentShapeId	= i; return bChanged; }
	bool SetPinFlags(uchar i)				{ const bool bChanged = ( m_iPinFlags		!= i );	m_iPinFlags			= i; return bChanged; }
	bool SetPadWidth(int i)					{ const bool bChanged = ( m_iPadWidth		!= i );	m_iPadWidth			= i;
											  if ( bChanged && GetHoleWidth() > i-8 ) SetHoleWidth( i-8 );	// 8 ==> minimum annular ring = 4 mil
											  return bChanged;
											}
	bool SetHoleWidth(int i)				{ const bool bChanged = ( m_iHoleWidth		!= i );	m_iHoleWidth		= i;
											  if ( bChanged && GetPadWidth() < i+8 ) SetPadWidth( i+8 );	// 8 ==> minimum annular ring = 4 mil
											  return bChanged;
											}
	bool SetAllowFlyWire(bool b)			{ const bool bChanged = ( m_bAllowFlyWire	!= b );	m_bAllowFlyWire		= b; return bChanged; }
	bool SetValueStr(const std::string& s)	{ const bool bChanged = ( m_valueStr		!= s );	m_valueStr			= s; return bChanged; }
	bool SetPrefixStr(const std::string& s)	{ const bool bChanged = ( m_prefixStr		!= s );	m_prefixStr			= s; return bChanged; }
	bool SetTypeStr(const std::string& s)	{ const bool bChanged = ( m_typeStr			!= s );	m_typeStr			= s; return bChanged; }
	bool SetImportStr(const std::string& s)	{ const bool bChanged = ( m_importStr		!= s );	m_importStr			= s; return bChanged; }
	bool SetLabelOffsetRow(int i)			{ const bool bChanged = ( m_iLabelOffsetRow	!= i );	m_iLabelOffsetRow	= i; return bChanged; }
	bool SetLabelOffsetCol(int i)			{ const bool bChanged = ( m_iLabelOffsetCol	!= i );	m_iLabelOffsetCol	= i; return bChanged; }
	bool SetGrid(const PinGrid& o)			{ const bool bChanged = ( m_grid			!= o );	m_grid				= o; return bChanged; }
	void SetPinLabel(size_t iPinIndex, const std::string& s)
	{
		if ( iPinIndex < m_pinLabels.size() ) m_pinLabels[iPinIndex] = s;
	}
	void SetPinAlign(size_t iPinIndex, int i)
	{
		if ( iPinIndex < m_pinAligns.size() ) m_pinAligns[iPinIndex] = i;
	}
	void AddShape(int id, const Shape& o)	{ assert( id != BAD_ID );	m_mapShapes.push_back( IntShape(id, o) ); }
	const int&				GetCurrentPinId() const		{ return m_currentPinId; }
	const int&				GetCurrentShapeId() const	{ return m_currentShapeId; }
	const uchar&			GetPinFlags() const			{ return m_iPinFlags; }
	const int&				GetPadWidth() const			{ return m_iPadWidth; }
	const int&				GetHoleWidth() const		{ return m_iHoleWidth; }
	const bool&				GetAllowFlyWire() const		{ return m_bAllowFlyWire; }
	const std::string&		GetValueStr() const			{ return m_valueStr; }
	const std::string&		GetPrefixStr() const		{ return m_prefixStr; }
	const std::string&		GetTypeStr() const			{ return m_typeStr; }
	const std::string&		GetImportStr() const		{ return m_importStr; }
	const int&				GetLabelOffsetRow() const	{ return m_iLabelOffsetRow; }
	const int&				GetLabelOffsetCol() const	{ return m_iLabelOffsetCol; }
	const PinGrid&			GetGrid() const				{ return m_grid; }
	const std::string&		GetPinLabel(size_t iPinIndex) const
	{
		static const std::string emptyStr("");
		return ( iPinIndex < m_pinLabels.size() ) ? m_pinLabels[iPinIndex] : emptyStr;
	}
	const int&				GetPinAlign(size_t iPinIndex) const
	{
		static int defaultAlign(Qt::AlignHCenter);
		return ( iPinIndex < m_pinAligns.size() ) ? m_pinAligns[iPinIndex] : defaultAlign;
	}
	std::list<IntShape>&	GetShapes()					{ return m_mapShapes; }
	Q_DECL_CONSTEXPR static inline int GetMinMargin()	{ return 12; }	// The margin around the footprint on the screen
	int  GetScreenRows() const	{ return 2 * GetMinMargin() + GetGridRows(); }
	int  GetScreenCols() const	{ return 2 * GetMinMargin() + GetGridCols(); }

	// Footprint size and extents
	int  GetGridRows() const	{ return m_grid.GetRows(); }
	int  GetGridCols() const	{ return m_grid.GetCols(); }
	int  GetGridRowMin() const	{ return GetMinMargin(); }
	int  GetGridColMin() const	{ return GetMinMargin(); }
	int  GetGridRowMax() const	{ return GetGridRowMin() + GetGridRows() - 1; }
	int  GetGridColMax() const	{ return GetGridColMin() + GetGridCols() - 1; }
	void GetGridCentre(double& dCentreRow, double& dCentreCol) const	// Footprint centre w.r.t. screen
	{
		dCentreRow = 0.5 * ( GetGridRowMin() + GetGridRowMax() );
		dCentreCol = 0.5 * ( GetGridColMin() + GetGridColMax() );
	}

	Pin&	GetCurrentPin()		{ assert( m_currentPinId != BAD_ID ); return *m_grid.GetAt(m_currentPinId); }
	Shape&	GetCurrentShape()
	{
		assert( m_currentShapeId != BAD_ID );
		for (auto& s : m_mapShapes)
			if ( s.first == m_currentShapeId ) return s.second;
		assert(0);
		return m_mapShapes.begin()->second;
	}
	void	MoveCurrentShape(double dDown, double dRight);
	size_t	GetNumTruePins() const
	{
		size_t count(0);
		for (int i = 0, iSize = m_grid.GetSize(); i < iSize; i++)
			if ( m_grid.GetAtConst(i)->GetIsPin() ) count++;
		return count;
	}
	size_t	GetMaxPinNumber() const
	{
		size_t maxPinNumber(0);
		for (int i = 0, iSize = m_grid.GetSize(); i < iSize; i++)
		{
			const auto& p = m_grid.GetAtConst(i);
			if ( p->GetIsPin() )
				maxPinNumber = std::max(maxPinNumber, p->GetPinIndex() + 1);
		}
		return maxPinNumber;
	}
	void Build(const TemplateManager& templateMgr, Component& comp) const;
	bool SetPinNumber(int i)
	{
		if ( GetCurrentPinId() == BAD_ID ) return false;
		auto& o =  GetCurrentPin();
		o.SetPinIndex( static_cast<size_t>(i - 1) );
		o.SetSurface(SURFACE_FULL);
		o.SetHoleUse(HOLE_FULL);
		ReAllocatePins( GetMaxPinNumber() );
		return true;
	}
	bool IncPinNumber(bool bInc)
	{
		if ( GetCurrentPinId() == BAD_ID ) return false;
		auto& o = GetCurrentPin();
		const size_t iPinIndex = o.GetPinIndex();
		// If we end up with a valid pinIndex (>= 0 and <= 254) then set HOLE_FULL, else set HOLE_FREE
		if ( bInc )
		{
			if ( iPinIndex == BAD_PININDEX )
			{
				switch( o.GetSurface() )
				{
					case SURFACE_HOLE:	o.SetSurface(SURFACE_FREE);	o.SetHoleUse(HOLE_FREE);	return true;
					case SURFACE_FREE:	o.SetSurface(SURFACE_FULL);	o.SetHoleUse(HOLE_FREE);	return true;
					case SURFACE_FULL:	o.SetPinIndex(0);			o.SetHoleUse(HOLE_FULL);	return true;
					default:			assert(0);					return false;	// Don't yet handle SURFACE_GAP / SURFACE_PLUG
				}
			}
			if ( iPinIndex < 254 )	// We're limited to (0 <= pinIndex <= 254)
			{
				o.SetPinIndex(iPinIndex+1);
				return true;
			}
			return false;
		}
		else
		{
			if ( iPinIndex == BAD_PININDEX )
			{
				switch( o.GetSurface() )
				{
					case SURFACE_HOLE:								return false;
					case SURFACE_FREE:	o.SetSurface(SURFACE_HOLE);	o.SetHoleUse(HOLE_FREE);	return true;
					case SURFACE_FULL:	o.SetSurface(SURFACE_FREE);	o.SetHoleUse(HOLE_FREE);	return true;
					default:			assert(0);					return false;	// Don't yet handle SURFACE_GAP / SURFACE_PLUG
				}
			}
			else if ( iPinIndex > 0 )
			{
				o.SetPinIndex(iPinIndex-1);
				o.SetHoleUse(HOLE_FULL);
			}
			else	// iPinIndex == 0
			{
				o.SetPinIndex(BAD_PININDEX);
				o.SetHoleUse(HOLE_FREE);
			}
			return true;
		}
	}
	bool SetSurface(const std::string& str)
	{
		if ( GetCurrentPinId() == BAD_ID ) return false;
		auto& o = GetCurrentPin();
		for (const auto& mapObj : Pin::GetMapSurfaceStrings())
		{
			if ( mapObj.second == str )
			{
				const bool bChanged = o.GetSurface() != mapObj.first;
				if ( bChanged )
					o.SetSurface(mapObj.first);
				return bChanged;
			}
		}
		return false;
	}
	bool SetType(const std::string& str)
	{
		if ( GetCurrentShapeId() == BAD_ID ) return false;
		auto& o = GetCurrentShape();
		for (const auto& mapObj : Shape::GetMapShapeStrings())
		{
			if ( mapObj.second == str )
			{
				const bool bChanged = o.GetType() != mapObj.first;
				if ( bChanged )
					o.SetType(mapObj.first);
				return bChanged;
			}
		}
		return false;
	}
	bool SetPinType(const std::string& str)
	{
		const bool bRect = ( str == "Rectangle" );
		if ( bRect )
			return SetPinFlags( GetPinFlags() | PIN_RECT );		// Set bit
		else
			return SetPinFlags( GetPinFlags() & ~PIN_RECT );	// Clear bit
	}
	bool SetCX(double d)				{ if ( GetCurrentShapeId() == BAD_ID ) return false; GetCurrentShape().SetCX(d); return true; }
	bool SetCY(double d)				{ if ( GetCurrentShapeId() == BAD_ID ) return false; GetCurrentShape().SetCY(d); return true; }
	bool SetDX(double d)				{ if ( GetCurrentShapeId() == BAD_ID ) return false; GetCurrentShape().SetDX(d); return true; }
	bool SetDY(double d)				{ if ( GetCurrentShapeId() == BAD_ID ) return false; GetCurrentShape().SetDY(d); return true; }
	bool SetA1(double d)				{ if ( GetCurrentShapeId() == BAD_ID ) return false; GetCurrentShape().SetA1(d); return true; }
	bool SetA2(double d)				{ if ( GetCurrentShapeId() == BAD_ID ) return false; GetCurrentShape().SetA2(d); return true; }
	bool SetA3(double d)				{ if ( GetCurrentShapeId() == BAD_ID ) return false; GetCurrentShape().SetA3(d); return true; }
	bool SetLine(bool b)				{ if ( GetCurrentShapeId() == BAD_ID ) return false; GetCurrentShape().SetDrawLine(b); return true; }
	bool SetFill(bool b)				{ if ( GetCurrentShapeId() == BAD_ID ) return false; GetCurrentShape().SetDrawFill(b); return true; }
	bool SetFillColor(const MyRGB& r)	{ if ( GetCurrentShapeId() == BAD_ID ) return false; GetCurrentShape().SetFillColor(r); return true; }
	bool GetCanLower() const
	{
		const int& id = GetCurrentShapeId();
		return ( id != BAD_ID ) && ( id != m_mapShapes.begin()->first );
	}
	bool GetCanRaise() const
	{
		const int& id = GetCurrentShapeId();
		return ( id != BAD_ID ) && ( id != m_mapShapes.rbegin()->first );
	}
	bool Lower()
	{
		assert( GetCanLower() );
		auto iterPrior	= m_mapShapes.begin();
		auto iter		= iterPrior; ++iter;
		for (; iter != m_mapShapes.end(); ++iter, iterPrior++)
			if ( iter->first == GetCurrentShapeId() ) { std::swap(*iter, *iterPrior); return true; }
		return false;
	}
	bool Raise()
	{
		assert( GetCanRaise() );
		auto iterPrior	= m_mapShapes.rbegin();
		auto iter		= iterPrior; ++iter;
		for (; iter != m_mapShapes.rend(); ++iter, iterPrior++)
			if ( iter->first == GetCurrentShapeId() ) { std::swap(*iter, *iterPrior); return true; }
		return false;
	}
	int  AddLine()			{ return AddDefaultShape(SHAPE::LINE); }
	int  AddRect()			{ return AddDefaultShape(SHAPE::RECT); }
	int  AddRoundedRect()	{ return AddDefaultShape(SHAPE::ROUNDED_RECT); }
	int  AddEllipse()		{ return AddDefaultShape(SHAPE::ELLIPSE); }
	int  AddArc()			{ return AddDefaultShape(SHAPE::ARC); }
	int  AddChord()			{ return AddDefaultShape(SHAPE::CHORD); }
	int  AddDefaultShape(SHAPE eType)
	{
		const double dX = 0.5 * GetGridCols();
		const double dY = 0.5 * GetGridRows();
		if ( eType == SHAPE::ARC || eType == SHAPE::CHORD )
			return AddShape( Shape(eType, true, false, -dX, dX, -dY, dY, 0, 90) );
		else
			return AddShape( Shape(eType, true, false, -dX, dX, -dY, dY) );
	}
	// Helpers
	int  CopyShape();
	int  DestroyShape();
	int  GetNewShapeId() const;
	bool SetWidth(int i);
	bool SetHeight(int i);
	int  GetPinId(int row, int col) const;					// Pick the most relevant pin at the location
	int  GetShapeId(double dRowIn, double dColIn) const;	// Pick the most relevant shape at the location
	bool GetIsValid(const TemplateManager& templateMgr) const;
	// Persist functions
	virtual void Load(DataStream& inStream) override
	{
		inStream.Load(m_currentPinId);
		inStream.Load(m_currentShapeId);
		inStream.Load(m_iPinFlags);
		m_iPadWidth  = 70;
		m_iHoleWidth = 35;
		if ( inStream.GetVersion() >= VRT_VERSION_39 )
		{
			inStream.Load(m_iPadWidth);		// Added in VRT_VERSION_39
			inStream.Load(m_iHoleWidth);	// Added in VRT_VERSION_39
		}
		m_bAllowFlyWire = false;
		if ( inStream.GetVersion() >= VRT_VERSION_47 )
			inStream.Load(m_bAllowFlyWire);	// Added in VRT_VERSION_47
		inStream.Load(m_valueStr);
		inStream.Load(m_prefixStr);
		inStream.Load(m_typeStr);
		inStream.Load(m_importStr);
		m_iLabelOffsetRow = m_iLabelOffsetCol = 0;
		if ( inStream.GetVersion() >= VRT_VERSION_54 )
		{
			inStream.Load(m_iLabelOffsetRow);	// Added in VRT_VERSION_54
			inStream.Load(m_iLabelOffsetCol);	// Added in VRT_VERSION_54
		}
		m_grid.Load(inStream);

		unsigned int numPins(0);
		if ( inStream.GetVersion() >= VRT_VERSION_51 )
			inStream.Load(numPins);			// Added in VRT_VERSION_51
		AllocatePins(numPins);
		if ( inStream.GetVersion() >= VRT_VERSION_51 )
		{
			for (unsigned int i = 0; i < numPins; i++)
			{
				inStream.Load(m_pinLabels[i]);	// Added in VRT_VERSION_51
				inStream.Load(m_pinAligns[i]);	// Added in VRT_VERSION_51
			}
		}

		unsigned int numShapes(0);
		inStream.Load(numShapes);
		m_mapShapes.clear();
		for (unsigned int i = 0; i < numShapes; i++)
		{
			int		shapeId(BAD_ID);
			Shape	tmp;
			inStream.Load(shapeId);
			tmp.Load(inStream);
			m_mapShapes.push_back( IntShape(shapeId, tmp) );
		}
	}
	virtual void Save(DataStream& outStream) override
	{
		outStream.Save(m_currentPinId);
		outStream.Save(m_currentShapeId);
		outStream.Save(m_iPinFlags);
		outStream.Save(m_iPadWidth);	// Added in VRT_VERSION_39
		outStream.Save(m_iHoleWidth);	// Added in VRT_VERSION_39
		outStream.Save(m_bAllowFlyWire);// Added in VRT_VERSION_47
		outStream.Save(m_valueStr);
		outStream.Save(m_prefixStr);
		outStream.Save(m_typeStr);
		outStream.Save(m_importStr);
		outStream.Save(m_iLabelOffsetRow);	// Added in VRT_VERSION_54
		outStream.Save(m_iLabelOffsetCol);	// Added in VRT_VERSION_54
		m_grid.Save(outStream);

		const unsigned int numPins = static_cast<unsigned int>( GetNumPins() );
		outStream.Save(numPins);		// Added in VRT_VERSION_51
		for (unsigned int i = 0; i < numPins; i++)
		{
			outStream.Save(m_pinLabels[i]);	// Added in VRT_VERSION_51
			outStream.Save(m_pinAligns[i]);	// Added in VRT_VERSION_51
		}

		const unsigned int numShapes = static_cast<unsigned int>( m_mapShapes.size() );
		outStream.Save(numShapes);
		for (auto& mapObj : m_mapShapes)
		{
			int		shapeId	= mapObj.first;
			Shape&	shape	= mapObj.second;
			outStream.Save(shapeId);
			shape.Save(outStream);
		}
	}
private:
	int AddShape(const Shape& o)
	{
		const int id = GetNewShapeId();
		if ( id != BAD_ID ) AddShape(id, o);
		return id;
	}
private:
	// GUI control
	int							m_currentPinId;		// Current index into m_grid
	int							m_currentShapeId;	// Current selected shape
	// Component description
	uchar						m_iPinFlags;		// 1 ==> PIN_RECT, 2 ==> PIN_LABELS, 4 ==> PIN_CUSTOM
	int							m_iPadWidth;		// Used if the PIN_CUSTOM flag is set
	int							m_iHoleWidth;		// Used if the PIN_CUSTOM flag is set
	bool						m_bAllowFlyWire;	// true ==> Allow flying wire to pins
	std::string					m_valueStr;			// Value label (e.g. "MN3004")
	std::string					m_prefixStr;		// Prefix string (e.g. "IC")
	std::string					m_typeStr;			// Component type (e.g. "BBD")
	std::string					m_importStr;		// For Planet/Tango import
	int							m_iLabelOffsetRow;	// Label offset in units of 1/16 of a grid square
	int							m_iLabelOffsetCol;	// Label offset in units of 1/16 of a grid square
	PinGrid						m_grid;
	std::vector<std::string>	m_pinLabels;		// Pin labels
	std::vector<int>			m_pinAligns;		// Pin label alignments (Qt::AlignLeft,Qt::AlignRight,Qt::AlignHCenter)
	std::list<IntShape>			m_mapShapes;		// "Map" of shapeId to Shape.	Coordinates are RELATIVE to footprint centre.
};
