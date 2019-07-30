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

#include "CompElement.h"
#include "Element.h"
#include "CompTypes.h"	// For component length limits

// Grid is a templatized 2-dimensional array, with data that can be indexed by row and column

template<class T> 
class Grid : public Persist, public Merge
{
public:
	Grid(int rows = 0, int cols = 0) : m_rows(rows), m_cols(cols), m_pData(nullptr), m_ppData(nullptr) { Allocate(rows, cols); }
	Grid(const Grid& o) : m_rows(0), m_cols(0), m_pData(nullptr), m_ppData(nullptr) { *this = o; }
	virtual ~Grid() { DeAllocate(); }
	Grid& operator=(const Grid& o)
	{
		Allocate(o.m_rows, o.m_cols);
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++) m_pData[i] = o.m_pData[i];
		return *this;
	}
	bool operator==(const Grid& o) const	// Compare persisted info
	{
		bool bOK = m_rows == o.m_rows
				&& m_cols == o.m_cols;
		const int iSize = GetSize();
		for (int i = 0; i < iSize && bOK; i++) bOK = ( m_pData[i] == o.m_pData[i] );
		return bOK;
	}
	bool operator!=(const Grid& o) const
	{
		return !(*this == o);
	}
	void Clear(const T& val)
	{
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++) m_pData[i] = val;
	}
	void Allocate(int rows, int cols)
	{
		DeAllocate();
		m_rows		= rows;
		m_cols		= cols;
		m_pData		= new T[m_rows * m_cols];
		m_ppData	= new T*[m_rows];
		for (int i = 0; i < m_rows; i++) m_ppData[i] = m_pData + i * m_cols;
	}
	void DeAllocate()
	{
		if ( m_ppData )	{ delete[] m_ppData;	m_ppData = nullptr; }
		if ( m_pData ) 	{ delete[] m_pData;		m_pData  = nullptr; }
		m_rows = m_cols = 0;
	}
	const int&  GetCols() const					{ return m_cols; }
	const int&  GetRows() const					{ return m_rows; }
	int	 GetSize() const						{ return m_rows * m_cols; }
	void GetRowCol(T* p, int& row, int& col) const
	{
		const size_t i = ( p - m_pData );
		assert( i < static_cast<size_t> ( GetSize() ) );
		row = static_cast<int> (i) / m_cols;
		col = static_cast<int> (i) % m_cols;
	}
	T*	 GetAt(int i)							{ return m_pData + i; }
	T*	 GetAtConst(int i) const				{ return m_pData + i; }
	T*	 Get(int row, int col) const			{ return m_ppData[row] + col; }
	void SetAt(int i, const T& val)				{ m_pData[i] = val; }
	void Set(int row, int col, const T& val)	{ m_ppData[row][col] = val; }
	void Grow(int incRows, int incCols)	// Grow/shrink the current array
	{
		if ( incRows == 0 && incCols == 0 ) return;
		Grid<T> tmp(*this);	// Make a temporary copy of this grid
		Allocate(m_rows + incRows, m_cols + incCols);	// Re-allocate this grid to make it larger/smaller

		// Now copy the tmp data into the new grid
		for (int iRow = 0; iRow < std::min(GetRows(), tmp.GetRows()); iRow++)
		for (int iCol = 0; iCol < std::min(GetCols(), tmp.GetCols()); iCol++)
			m_ppData[iRow][iCol] = tmp.m_ppData[iRow][iCol];
	}
	// Merge interface functions
	virtual void UpdateMergeOffsets(MergeOffsets& o) override
	{
		o.deltaRow = std::max(o.deltaRow, GetRows() + 1);
		//o.deltaCol = std::max(o.deltaCol, GetCols() + 1);
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++) m_pData[i].UpdateMergeOffsets(o);
	}
	virtual void ApplyMergeOffsets(const MergeOffsets& o) override
	{
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++) m_pData[i].ApplyMergeOffsets(o);
	}
	void Merge(const Grid& src, const MergeOffsets& o)
	{
		const int rows = src.GetRows();
		const int cols = src.GetCols();
		for (int j = 0; j < rows; j++) for (int i = 0; i < cols; i++)
			Get(j + o.deltaRow, i + o.deltaCol)->Merge(*src.Get(j, i));
	}
	// Persist interface functions
	virtual void Load(DataStream& inStream) override
	{
		inStream.Load(m_rows);
		inStream.Load(m_cols);
		Allocate(m_rows, m_cols);
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++) m_pData[i].Load(inStream);
	}
	virtual void Save(DataStream& outStream) override
	{
		outStream.Save(m_rows);
		outStream.Save(m_cols);
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++) m_pData[i].Save(outStream);
	}
private:
	int	m_rows;
	int	m_cols;
	T*	m_pData;
	T**	m_ppData;	// So we can access data using [][]
};

// The PinGrid class is used by the component editor class (CompDefiner)
typedef Grid<Pin> PinGrid;

typedef Grid<TrackElement> TrackElementGrid;

// The CompElementGrid class is used for component footprints and "track footprints".
// It can handle simple transformations like stretching and rotating.
// The internal "direction" of a CompElementGrid is always West ('W')
// meaning that Pin 1 is to the West (left) of the centre.
// For example, a resistor footprint is always stored as a CompElementGrid with a
// single horizontal row, with the first pin on the far left and the second pin on the far right.
// If we rotate a component so it points in another direction on screen, then any
// methods that access the CompElementGrid information in a spatial way should specify
// the component direction so the internal data can be read correctly.

class CompElementGrid : public Grid<CompElement>
{
public:
	CompElementGrid(int rows = 0, int cols = 0) : Grid<CompElement>(rows, cols) {}
	CompElementGrid(const CompElementGrid& o) : Grid<CompElement>(o) { *this = o; }
	virtual ~CompElementGrid() {}
	CompElementGrid& operator=(const CompElementGrid& o) { Grid<CompElement>::operator=(o); return *this; }
	bool operator==(const CompElementGrid& o) const { return Grid<CompElement>::operator==(o); }
	bool operator!=(const CompElementGrid& o) const	{ return Grid<CompElement>::operator!=(o); }
	const int& GetCols(char direction = 'W') const
	{
		switch ( direction )	// Component direction: 'W','E','N','S'
		{
			case 'N':
			case 'S':	return Grid<CompElement>::GetRows();
			default:	return Grid<CompElement>::GetCols();
		}
	}
	const int& GetRows(char direction = 'W') const
	{
		switch ( direction )	// Component direction: 'W','E','N','S'
		{
			case 'N':
			case 'S':	return Grid<CompElement>::GetCols();
			default:	return Grid<CompElement>::GetRows();
		}
	}
	CompElement* Get(int row, int col, char direction = 'W') const
	{
		Transform(row, col, direction);	// Handle direction transformation
		return Grid<CompElement>::Get(row, col);
	}
	void SetupWire()
	{
		assert( GetRows() == 1 && GetCols() > 1 );
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++) GetAt(i)->SetWireOccupancies();
	}
	void StretchSimple(bool bGrow, const CompElement& initVal)	// For simple 2-pin components like resistors, wires, diodes, caps
	{
		// Pins are assumed to be first and last element on the row
		if ( GetRows() == 1 && ( bGrow || GetCols() > 2 ) )
		{
			CompElement a(*GetAt(0)), b(*GetAt(GetCols()-1));	// Read ends
			Allocate(GetRows(), bGrow ? GetCols() + 1 : GetCols() - 1);	// Resize
			Clear(initVal);
			*GetAt(0) = a; *GetAt(GetCols()-1) = b;	// Set ends
		}
	}
	void StretchComplex(const COMP& eType, bool bGrow)	// For ICs and switches
	{
		if ( !bGrow && GetCols() == GetMinLength(eType) ) return; 	// Don't shrink to less than min allowed length
		if (  bGrow && GetCols() == GetMaxLength(eType) ) return; 	// Don't expand to more than max allowed length
		const int iDelta = GetStretchIncrement(eType);
		return Allocate(GetRows(), bGrow ? GetCols() + iDelta : GetCols() - iDelta);	// Resize
	}
	void StretchWidthIC(bool bGrow)	// For DIPs only
	{
		if ( !bGrow && GetRows() == 2 ) return;	// Don't shrink DIP width to less than 2 rows
		return Allocate(bGrow ? GetRows() + 1 : GetRows() - 1, GetCols());	// Resize
	}
	virtual void Load(DataStream& inStream) override
	{
		if ( inStream.GetVersion() >= VRT_VERSION_11 )
			return Grid<CompElement>::Load(inStream);

		// Handle legacy VRTs where components used a simple PinGrid instead of an CompElementGrid
		int rows(0), cols(0);
		inStream.Load(rows);
		inStream.Load(cols);
		Allocate(rows, cols);
		CompElement tmp;
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++)
		{
			tmp.Pin::Load(inStream);	// Just load the Pin info
			SetAt(i, tmp);
		}
	}
private:
	void Transform(int& row, int& col, char direction) const
	{
		// On input:  (row,col) are the "footprint" co-ordinates (as seen on screen).
		// On output: (row,col) have been set to the corresponding internal values.
		const int rTmp(row), cTmp(col);
		switch( direction ) // Component direction: 'W','E','N','S'
		{
			case 'W':	return;
			case 'E':	row = Grid<CompElement>::GetRows()-1 - rTmp;
						col = Grid<CompElement>::GetCols()-1 - cTmp;	return;
			case 'N':	row = Grid<CompElement>::GetRows()-1 - cTmp;
						col = rTmp;								return;
			case 'S':	row = cTmp;
						col = Grid<CompElement>::GetCols()-1 - rTmp;	return;
		}
	}
};


// The ElementGrid is used for the board and has a hidden toroidal behaviour.
// So things that move too far right can appear on the left.
// Similarly for top and bottom.
// It's a lazy way of avoiding bounds checking during development.
// More importantly, it makes the neighbouring element concept simpler
// since every element will always have 8 non-null neighbour pointers.

class ElementGrid : public Grid<Element>
{
public:
	ElementGrid(int rows = 0, int cols = 0) : Grid<Element>(rows, cols) {}
	ElementGrid(const ElementGrid& o) : Grid<Element>(o) { *this = o; }
	virtual ~ElementGrid() {}
	ElementGrid& operator=(const ElementGrid& o) { Grid<Element>::operator=(o); return *this; }
	bool operator==(const ElementGrid& o) const { return Grid<Element>::operator==(o); }
	bool operator!=(const ElementGrid& o) const	{ return Grid<Element>::operator!=(o); }
	void MakeToroid(int& row, int& col) const
	{
		// Make co-ordinates wrap around at grid edges
		while ( row <  0 )			{ row += GetRows(); }
		while ( row >= GetRows() )	{ row -= GetRows(); }
		while ( col <  0 )			{ col += GetCols(); }
		while ( col >= GetCols() )	{ col -= GetCols(); }
	}
	Element* Get(int row, int col) const
	{
		MakeToroid(row, col);	// Make co-ordinates wrap around at grid edges
		return Grid<Element>::Get(row, col);
	}
	void Pan(int iDown, int iRight)	// Input arguments can be +ve or -ve.
	{
		if ( iDown == 0 && iRight == 0 ) return;

		ElementGrid tmp(*this);	// Make a temporary copy of this grid

		for (int iRow = 0; iRow < GetRows(); iRow++)
		for (int iCol = 0; iCol < GetCols(); iCol++)
			Set(iRow, iCol, *(tmp.Get(iRow - iDown, iCol - iRight)));	// ElementGrid::Get() accounts for toroidal behaviour
	}
	bool CopyFrom(const TrackElementGrid& o)
	{
		if ( o.GetRows() != GetRows() || o.GetCols() != GetCols() ) return false;
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++)
			GetAt(i)->TrackElement::operator=(*o.GetAtConst(i));
		return true;
	}
	void CopyTo(TrackElementGrid& o) const
	{
		if ( o.GetRows() != GetRows() || o.GetCols() != GetCols() )
			o.Allocate(GetRows(), GetCols());
		const int iSize = GetSize();
		for (int i = 0; i < iSize; i++)
			o.GetAt(i)->operator=(*GetAtConst(i));
	}
};
