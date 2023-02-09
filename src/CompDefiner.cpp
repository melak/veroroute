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

#include "CompDefiner.h"
#include "Component.h"

void CompDefiner::Populate(const Component& o)
{
	Clear();

	SetPinFlags( o.GetPinFlags() );
	SetPadWidth( o.GetPadWidth() );
	SetHoleWidth( o.GetHoleWidth() );
	SetAllowFlyWire( o.GetAllowFlyWire() );

	// Copy strings
	SetValueStr( o.GetValueStr() );
	SetPrefixStr( o.GetPrefixStr() );
	SetTypeStr( o.GetFullTypeStr() );
	SetImportStr( o.GetFullImportStr() );

	// Copy footprint to PinInfo map
	m_grid.Allocate(o.GetLyrs(), o.GetRows(), o.GetCols());
	for (int i = 0, iSize = o.GetSize(); i < iSize; i++)
	{
		*m_grid.GetAt(i) = *o.GetAtConst(i);

		// The editor does not yet support SURFACE_GAP/SURFACE_PLUG, so map these to SURFACE_FULL
		switch( o.GetAtConst(i)->GetSurface() )
		{
			case SURFACE_GAP:
			case SURFACE_PLUG:	m_grid.GetAt(i)->SetSurface(SURFACE_FULL);	break;
			default:			break;
		}

		// The editor does not yet support HOLE_WIRE, so map this to HOLE_FULL
		switch( o.GetAtConst(i)->GetHoleUse() )
		{
			case HOLE_WIRE:		m_grid.GetAt(i)->SetHoleUse(HOLE_FULL);	break;
			default:			break;
		}
	}

	// Copy pin labels
	AllocatePins( o.GetNumPins() );
	for (size_t i = 0, iSize = GetNumPins(); i < iSize; i++)
	{
		SetPinLabel(i, o.GetPinLabel(i));
		SetPinAlign(i, o.GetPinAlign(i));
	}

	// Copy shapes
	m_mapShapes.clear();
	int iShapeId(0);
	for (const auto& shape : o.GetShapes())
	{
		AddShape(iShapeId, shape);
		iShapeId++;
	}
}

void CompDefiner::Build(Component& comp) const
{
	assert( GetIsValid() );

	// Build component from definition
	comp.SetPinFlags( GetPinFlags() );
	comp.SetPadWidth( GetPadWidth() );
	comp.SetHoleWidth( GetHoleWidth() );
	comp.SetAllowFlyWire( GetAllowFlyWire() );
	comp.SetValueStr( GetValueStr() );
	comp.SetPrefixStr( GetPrefixStr() );
	comp.SetTypeStr( GetTypeStr() );
	comp.SetImportStr( GetImportStr() );

	comp.SetType(COMP::CUSTOM);
	assert( m_grid.GetLyrs() == 1 );
	comp.Allocate(m_grid.GetLyrs(), m_grid.GetRows(), m_grid.GetCols());
	for (int i = 0, iSize = m_grid.GetSize(); i < iSize; i++)
		comp.GetAt(i)->Pin::operator=( *m_grid.GetAtConst(i) );

	// Copy pin labels
	comp.AllocatePins( GetNumTruePins() );
	for (size_t i = 0, iSize = GetNumPins(); i < iSize; i++)
	{
		comp.SetPinLabel(i, GetPinLabel(i));
		comp.SetPinAlign(i, GetPinAlign(i));
	}

	// Copy shapes
	assert( comp.GetNumShapes() == 0 );
	for (const auto& mapObj : m_mapShapes)
		comp.AddOne(mapObj.second);
}

void CompDefiner::MoveCurrentShape(double dDown, double dRight)
{
	Shape& s = GetCurrentShape();
	double dCentreCol(0), dCentreRow(0);
	GetGridCentre(dCentreRow, dCentreCol);	// Footprint centre w.r.t. screen

	const double dNewCX = dCentreCol + s.GetCX() + dRight;	// New shape centre w.r.t. screen grid
	const double dNewCY = dCentreRow + s.GetCY() + dDown;	// New shape centre w.r.t. screen grid
	if ( ( dNewCX > -0.5 && dNewCX < GetScreenCols() -0.5 ) &&
		 ( dNewCY > -0.5 && dNewCY < GetScreenRows() -0.5 ) )
		s.Move(dDown, dRight);
}

// Helpers
int CompDefiner::CopyShape()
{
	assert( GetCurrentShapeId() != BAD_ID );
	const Shape& s = GetCurrentShape();
	const int iNewShapeId = AddShape( s );	assert( iNewShapeId != BAD_ID );
	SetCurrentShapeId( iNewShapeId );
	MoveCurrentShape(1.0, 1.0);	// Apply offset so not overlaying old shape
	return GetCurrentShapeId();
}
int CompDefiner::DestroyShape()
{
	assert( GetCurrentShapeId() != BAD_ID );
	for (auto iter = m_mapShapes.begin(); iter != m_mapShapes.end(); ++iter)
		if ( iter->first == GetCurrentShapeId() ) { m_mapShapes.erase(iter); break; }
	SetCurrentShapeId( BAD_ID );
	return GetCurrentShapeId();
}
int CompDefiner::GetNewShapeId() const
{
	int shapeId(0);
	while ( shapeId != INT_MAX )
	{
		bool bExists(false);
		for (auto iter = m_mapShapes.begin(); iter != m_mapShapes.end() && !bExists; ++iter)
			bExists = ( iter->first == shapeId );
		if ( !bExists ) break;
		shapeId++;
	}
	return ( shapeId == INT_MAX ) ? BAD_ID : shapeId;
}
bool CompDefiner::SetWidth(int i)
{
	assert( i > 0 );
	const bool bChanged = m_grid.GetCols() != i;
	if ( bChanged )
	{
		SetCurrentPinId(BAD_ID);
		SetCurrentShapeId(BAD_ID);
		m_grid.Allocate(1, m_grid.GetRows(), i);
		m_grid.Clear( Pin(BAD_PINCHAR, SURFACE_FULL, HOLE_FREE) );
		m_mapShapes.clear();
		SetAllowFlyWire(false);
		AddRect();	// Provide a Rect by default
	}
	return bChanged;
}
bool CompDefiner::SetHeight(int i)
{
	assert( i > 0 );
	const bool bChanged = m_grid.GetRows() != i;
	if ( bChanged )
	{
		SetCurrentPinId(BAD_ID);
		SetCurrentShapeId(BAD_ID);
		m_grid.Allocate(1, i, m_grid.GetCols());
		m_grid.Clear( Pin(BAD_PINCHAR, SURFACE_FULL, HOLE_FREE) );
		m_mapShapes.clear();
		SetAllowFlyWire(false);
		AddRect();	// Provide a Rect by default
	}
	return bChanged;
}

int CompDefiner::GetPinId(int row, int col) const	// Pick the most relevant pin at the location
{
	const int iRow = row - GetGridRowMin();
	const int iCol = col - GetGridColMin();
	const bool bOK = iRow >= 0 && iRow < m_grid.GetRows()
				  && iCol >= 0 && iCol < m_grid.GetCols();
	return ( bOK ) ? iCol + iRow * m_grid.GetCols() : BAD_ID;
}

int CompDefiner::GetShapeId(double dRowIn, double dColIn) const	// Pick the most relevant shape at the location
{
	double dCentreRow(0), dCentreCol(0);
	GetGridCentre(dCentreRow, dCentreCol);	// Footprint centre w.r.t. screen

	const double dRow(dRowIn - dCentreRow);	// w.r.t. footprint centre
	const double dCol(dColIn - dCentreCol);	// w.r.t. footprint centre
	const double epsilon(0.1);

	int		iBestId(BAD_ID);
	double	dMinArea(INT_MAX);
	for (const auto& mapObj : m_mapShapes)
	{
		const Shape& s		= mapObj.second;
		const double DX		= s.GetDX();
		const double DY		= s.GetDY();
		const double CX		= s.GetCX();
		const double CY		= s.GetCY();
		const double dA3	= s.GetA3() * M_PI / 180.0;	// Convert to radians
		const double dCos	= cos(dA3);
		const double dSin	= sin(dA3);
		const double dX		= dCol - CX;
		const double dY		= dRow - CY;
		const double rx		= dCos * dX - dSin * dY;	// w.r.t. rotated axes at the shape centre
		const double ry		= dSin * dX + dCos * dY;	// w.r.t. rotated axes at the shape centre

		double dArea(INT_MAX);
		bool bOK(false);
		switch( s.GetType() )
		{
			case SHAPE::RECT:
			case SHAPE::ROUNDED_RECT:
				dArea = fabs(DX*DY);			// Area of the rectangle
				bOK   = fabs(2.0*ry) <= ( ( DY <= 0.1 ) ? epsilon : DY );
				bOK  &= fabs(2.0*rx) <= ( ( DX <= 0.1 ) ? epsilon : DX );
				break;
			case SHAPE::ELLIPSE:
			case SHAPE::ARC:
			case SHAPE::CHORD:
			{
				dArea	= M_PI * 0.25*DX*DY;	// Area of the ellipse
				double epsilon(dArea < 0.1 ? 0.1 : 0);
				bOK		= rx*DY*rx*DY + ry*DX*ry*DX <= 0.25*DX*DX*DY*DY + epsilon;
				break;
			}
			default:	// SHAPE::LINE
			{
				assert(s.GetType() == SHAPE::LINE);
				// Check for distance within a narrow ellipse with foci at the endpoints
				// Get true X1,X2,Y1,Y2 locations w.r.t. footprint centre
				const double x1  = s.GetX1() - CX;
				const double x2  = s.GetX2() - CX;
				const double y1  = s.GetY1() - CY;
				const double y2  = s.GetY2() - CY;
				const double X1  = CX + dCos * x1 + dSin * y1;	// w.r.t. footprint centre
				const double Y1  = CY - dSin * x1 + dCos * y1;	// w.r.t. footprint centre
				const double X2  = CX + dCos * x2 + dSin * y2;	// w.r.t. footprint centre
				const double Y2  = CY - dSin * x2 + dCos * y2;	// w.r.t. footprint centre
				const double DX  = fabs(X2 - X1);
				const double DY  = fabs(Y2 - Y1);
				const double dx1 = dCol - X1;
				const double dy1 = dRow - Y1;
				const double dx2 = dCol - X2;
				const double dy2 = dRow - Y2;
				dArea	= sqrt(DX*DX + DY*DY);	// "Area" for line is actually length
				bOK		= sqrt(dx1*dx1 + dy1*dy1) + sqrt(dx2*dx2 + dy2*dy2) < dArea + epsilon;
				break;
			}
		}
		if ( !bOK ) continue;
		if ( iBestId == BAD_ID || dArea <= dMinArea )
		{
			iBestId		= mapObj.first;
			dMinArea	= dArea;
		}
	}
	return iBestId;
}

bool CompDefiner::GetIsValid() const
{
	if ( StringHelper::IsEmptyStr(m_typeStr) ) return false;
	if ( StringHelper::IsEmptyStr(m_valueStr) ) return false;
	if ( StringHelper::HasSpaces(m_importStr) ) return false;	// Import string must not have spaces
	if ( CompTypes::GetTypeFromImportStr(m_importStr) != COMP::INVALID ) return false;	// Reserved string
	// Following is copied from Board::Import() method.
	// List of package identifiers for footprints with variable numbers of pins/lengths.
	const int NUM_VARIABLE_PIN_PARTS = 13;
	const std::string strVar[NUM_VARIABLE_PIN_PARTS] = {"SIP", "DIP", "PADS", "SWITCH_ST_DIP", "SWITCH_ST", "SWITCH_DT", "STRIP_100MIL", "BLOCK_100MIL", "BLOCK_200MIL", "RESISTOR", "DIODE", "CAP_CERAMIC", "CAP_FILM"};
	for (int i = 0; i < NUM_VARIABLE_PIN_PARTS; i++)
	{
		const std::string&	strTmp	= strVar[i];	// e.g. "SIP", "DIP, etc
		const auto			L		= strTmp.length();
		if ( m_importStr.length() >= L && m_importStr.substr(0, L) == strTmp ) return false;
	}
	if ( m_grid.GetSize() == 0 ) return false;	// Should not be possible
	bool bOK(false);
	for (const auto& mapObj : m_mapShapes)
		if ( mapObj.second.GetDrawLine() && !mapObj.second.GetDrawFill() ) { bOK = true; break; }
	if ( !bOK ) return false;	// Must have a shape with line and no fill (so  it can't be faded out)
	// Pin indexes must be consecutive at start at 0
	std::list<size_t> pinIndexes;
	for (int i = 0, iSize = m_grid.GetSize(); i < iSize; i++)
		if ( m_grid.GetAtConst(i)->GetIsPin() )
			pinIndexes.push_back( m_grid.GetAtConst(i)->GetPinIndex() );

	pinIndexes.sort();
	size_t iTest(0);
	for (const auto& pinIndex : pinIndexes)
	{
		if ( pinIndex != iTest ) return false;
		iTest++;
	}
	return true;
}
