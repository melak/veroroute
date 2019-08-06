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

#include "Component.h"

void Component::SetDefaultPinFlags()
{
	switch ( GetType() )
	{
		case COMP::TO92					:
		case COMP::TO18					:
		case COMP::TO39					:
		case COMP::TO220				:
		case COMP::SIP					:
		case COMP::DIP					:
		case COMP::DIP_RECTIFIER		:
		case COMP::STRIP_100			:
		case COMP::BLOCK_100			:
		case COMP::BLOCK_200			: return SetPinFlags(PIN_LABELS);
		case COMP::SWITCH_ST			:
		case COMP::SWITCH_DT			:
		case COMP::SWITCH_ST_DIP		:
		case COMP::SWITCH_BUTTON_4PIN	:
		case COMP::RELAY_HK19F			:
		case COMP::RELAY_HJR_4102		:
		case COMP::RELAY_FTR_B3C		:
		case COMP::RELAY_G2R_2			:
		case COMP::RELAY_G2R_2A			:
		case COMP::RELAY_G2RK_2			:
		case COMP::RELAY_G2RK_2A		:
		case COMP::RELAY_G3MB_202P		:
		case COMP::RELAY_JQC_3F_APPROX	:
		case COMP::RELAY_S1A050000		:
		case COMP::RELAY_TRCD			:
		case COMP::FUSE_HOLDER			: return SetPinFlags(PIN_RECT);
		case COMP::RELAY_DIP_4PIN		:
		case COMP::RELAY_DIP_8PIN		: return SetPinFlags(PIN_RECT | PIN_LABELS);
		case COMP::VIA					:
		case COMP::PAD					:
		case COMP::WIRE					:
		case COMP::RESISTOR				:
		case COMP::INDUCTOR				:
		case COMP::DIODE				:
		case COMP::LED					:
		case COMP::CAP_CERAMIC			:
		case COMP::CAP_FILM				:
		case COMP::CAP_FILM_WIDE		:
		case COMP::CAP_ELECTRO_200_NP	:
		case COMP::CAP_ELECTRO_200		:
		case COMP::CAP_ELECTRO_250_NP	:
		case COMP::CAP_ELECTRO_250		:
		case COMP::CAP_ELECTRO_300_NP	:
		case COMP::CAP_ELECTRO_300		:
		case COMP::CAP_ELECTRO_400_NP	:
		case COMP::CAP_ELECTRO_400		:
		case COMP::CAP_ELECTRO_500_NP	:
		case COMP::CAP_ELECTRO_500		:
		case COMP::CAP_ELECTRO_600_NP	:
		case COMP::CAP_ELECTRO_600		:
		case COMP::TRIM_VERT			:
		case COMP::TRIM_VERT_OFFSET		:
		case COMP::TRIM_VERT_OFFSET_WIDE:
		case COMP::TRIM_FLAT			:
		case COMP::TRIM_FLAT_WIDE		:
		case COMP::TRIM_3006P			:
		case COMP::TRIM_3006W			:
		case COMP::TRIM_3006Y			:
		case COMP::TRIM_3329H			:
		case COMP::TRIM_3329P_DK9_RC	:
		case COMP::CRYSTAL				: assert( GetPinFlags() == 0 ); return SetPinFlags(0);
		case COMP::CUSTOM				:
		case COMP::TRACKS				: return;
		default:	assert(0);			  return;	// Unhandled eType
	}
}

void Component::AddDefaultShapes()
{
	switch( GetType() )
	{
		case COMP::CUSTOM:
		case COMP::TRACKS:	return;
		default:			m_shapes.clear();
	}
	switch( GetType() )
	{
		case COMP::VIA:						Add( Shape(SHAPE::ELLIPSE,	-0.30,  0.30, -0.30,  0.30) );
											Add( Shape(SHAPE::LINE,		-0.21,  0.21, -0.21,  0.21) );
											Add( Shape(SHAPE::LINE,		-0.21,  0.21,  0.21, -0.21) );	break;
		case COMP::PAD:						Add( Shape(SHAPE::ELLIPSE,	-0.45,  0.45, -0.45,  0.45) );	break;
		case COMP::LED:						Add( Shape(SHAPE::CHORD,	-0.75,  0.75, -0.75,  0.75, 30, -30) );	break;
		case COMP::CAP_ELECTRO_200:			Add( Shape(SHAPE::LINE,		 0.77,  0.77, -0.64,  0.64) );
		case COMP::CAP_ELECTRO_200_NP:		Add( Shape(SHAPE::ELLIPSE,	-1.00,  1.00, -1.00,  1.00) );	break;
		case COMP::CAP_ELECTRO_250:			Add( Shape(SHAPE::LINE,		 0.96,  0.96, -0.80,  0.80) );
		case COMP::CAP_ELECTRO_250_NP:		Add( Shape(SHAPE::ELLIPSE,	-1.25,  1.25, -1.25,  1.25) );	break;
		case COMP::CAP_ELECTRO_300:			Add( Shape(SHAPE::LINE,		 1.15,  1.15, -0.96,  0.96) );
		case COMP::CAP_ELECTRO_300_NP:		Add( Shape(SHAPE::ELLIPSE,	-1.50,  1.50, -1.50,  1.50) );	break;
		case COMP::CAP_ELECTRO_400:			Add( Shape(SHAPE::LINE,		 1.53,  1.53, -1.29,  1.29) );
		case COMP::CAP_ELECTRO_400_NP:		Add( Shape(SHAPE::ELLIPSE,	-2.00,  2.00, -2.00,  2.00) );	break;
		case COMP::CAP_ELECTRO_500:			Add( Shape(SHAPE::LINE,		 1.92,  1.92, -1.61,  1.61) );
		case COMP::CAP_ELECTRO_500_NP:		Add( Shape(SHAPE::ELLIPSE,	-2.50,  2.50, -2.50,  2.50) );	break;
		case COMP::CAP_ELECTRO_600:			Add( Shape(SHAPE::LINE,		 2.30,  2.30, -1.93,  1.93) );
		case COMP::CAP_ELECTRO_600_NP:		Add( Shape(SHAPE::ELLIPSE,	-3.00,  3.00, -3.00,  3.00) );	break;
		case COMP::TRIM_VERT:				Add( Shape(SHAPE::RECT,		-1.50,  1.50, -0.50,  0.50) );	break;
		case COMP::TRIM_VERT_OFFSET:		Add( Shape(SHAPE::RECT,		-1.50,  1.50, -0.75,  0.75) );	break;
		case COMP::TRIM_VERT_OFFSET_WIDE:	Add( Shape(SHAPE::RECT,		-1.50,  1.50, -1.00,  1.00) );	break;
		case COMP::TRIM_FLAT:				Add( Shape(SHAPE::RECT,		-1.50,  1.50, -1.50,  1.50) );	break;
		case COMP::TRIM_FLAT_WIDE:			Add( Shape(SHAPE::RECT,		-1.50,  1.50, -1.50,  1.50) );	break;
		case COMP::TRIM_3006P:				Add( Shape(SHAPE::RECT,		-3.75,  3.75, -0.50,  0.50) );	break;
		case COMP::TRIM_3006W:				Add( Shape(SHAPE::RECT,		-3.75,  3.75, -1.45,  1.45) );	break;
		case COMP::TRIM_3006Y:				Add( Shape(SHAPE::RECT,		-3.75,  3.75, -0.50,  0.50) );	break;
		case COMP::TRIM_3329H:				Add( Shape(SHAPE::ELLIPSE,	-1.25,  1.25, -1.25,  1.25) );	break;
		case COMP::TRIM_3329P_DK9_RC:		Add( Shape(SHAPE::ELLIPSE,	-1.25,  1.25, -1.25,  1.25) );	break;
		case COMP::CRYSTAL:					Add( Shape(SHAPE::ELLIPSE,	-1.00,  1.00, -1.00,  1.00) );	break;
		case COMP::TO92:					Add( Shape(SHAPE::CHORD,	-1.40,  1.40, -0.65,  1.15, -20, 200) );break;
		case COMP::TO18:					Add( Shape(SHAPE::ARC,		-0.95,  0.95, -0.95,  0.95, 101, 80) );
											Add( Shape(SHAPE::LINE,		-0.17, -0.17, -0.95, -1.13) );
											Add( Shape(SHAPE::LINE,		-0.17,  0.17, -1.13, -1.13) );
											Add( Shape(SHAPE::LINE,		 0.17,  0.17, -1.13, -0.95) );	break;
		case COMP::TO39:					Add( Shape(SHAPE::ARC,		-1.80,  1.80, -1.80,  1.80, 145, 125) );
											Add( Shape(SHAPE::LINE,		-1.47, -1.71, -1.03, -1.27) );
											Add( Shape(SHAPE::LINE,		-1.71, -1.27, -1.27, -1.71) );
											Add( Shape(SHAPE::LINE,		-1.27, -1.03, -1.71, -1.47) );	break;
		case COMP::TO220:					Add( Shape(SHAPE::RECT,		-1.56,  1.56, -0.56,  0.56) );
											Add( Shape(SHAPE::LINE,		-1.56,  1.56, -0.31, -0.31) );	break;
		case COMP::SWITCH_BUTTON_4PIN:		Add( Shape(SHAPE::RECT,		-1.35,  1.35, -1.35,  1.35) );	break;
		case COMP::RELAY_HK19F:				Add( Shape(SHAPE::RECT,		-3.98,  3.98, -1.97,  1.97) );	break;
		case COMP::RELAY_HJR_4102:			Add( Shape(SHAPE::RECT,		-3.09,  3.09, -2.05,  2.05) );	break;
		case COMP::RELAY_FTR_B3C:			Add( Shape(SHAPE::RECT,		-2.35,  2.35, -1.42,  1.42) );	break;
		case COMP::RELAY_G2R_2:				Add( Shape(SHAPE::RECT,		-5.71,  5.71, -2.56,  2.56) );	break;
		case COMP::RELAY_G2R_2A:			Add( Shape(SHAPE::RECT,		-5.71,  5.71, -2.56,  2.56) );	break;
		case COMP::RELAY_G2RK_2:			Add( Shape(SHAPE::RECT,		-5.71,  5.71, -2.56,  2.56) );	break;
		case COMP::RELAY_G2RK_2A:			Add( Shape(SHAPE::RECT,		-5.71,  5.71, -2.56,  2.56) );	break;
		case COMP::RELAY_G3MB_202P:			Add( Shape(SHAPE::RECT,		-4.82,  4.82, -1.08,  1.08) );	break;
		case COMP::RELAY_JQC_3F_APPROX:		Add( Shape(SHAPE::RECT,		-3.74,  3.74, -3.05,  3.05) );	break;
		case COMP::RELAY_S1A050000:			Add( Shape(SHAPE::RECT,		-3.74,  3.74, -1.00,  1.00) );	break;
		case COMP::RELAY_TRCD:				Add( Shape(SHAPE::RECT,		-4.53,  4.53, -3.17,  3.17) );	break;
		case COMP::FUSE_HOLDER:				Add( Shape(SHAPE::RECT,		-4.85,  4.85, -1.35,  1.35) );	break;
		// Following handle variable length components
		case COMP::DIP:
		case COMP::SIP:
		case COMP::DIP_RECTIFIER:
		case COMP::RELAY_DIP_4PIN:
		case COMP::RELAY_DIP_8PIN:
		{
			double w(0.35 + 0.5*(GetCols() - 1)), h(0.35 + 0.5*(GetRows()-1));
			Add( Shape(SHAPE::LINE,		-w, -w,  0.25,  h) );
			Add( Shape(SHAPE::LINE,		-w,  w,  h,  h) );
			Add( Shape(SHAPE::LINE,		 w,  w,  h, -h) );
			Add( Shape(SHAPE::LINE,		 w, -w, -h, -h) );
			Add( Shape(SHAPE::LINE,		-w, -w, -h, -0.25) );
			Add( Shape(SHAPE::ARC, -w-0.25, -w+0.25, -0.25,  0.25, -90, 90) );
			break;
		}
		case COMP::CAP_CERAMIC:
		{
			double w(0.4 + 0.5*(GetCols() - 1)), h(0.4 + 0.5*(GetRows()-1));
			Add( Shape(SHAPE::ELLIPSE, -w,  w, -h,  h) );
			break;
		}
		case COMP::CAP_FILM:
		case COMP::CAP_FILM_WIDE:
		{
			double w(0.45 + 0.5*(GetCols() - 1)), h(0.45 + 0.5*(GetRows()-1));
			Add( Shape(SHAPE::ROUNDED_RECT, -w,  w, -h,  h) );
			break;
		}
		case COMP::SWITCH_ST:
		case COMP::SWITCH_DT:
		{
			double w(0.7 + 0.5*(GetCols() - 1)), h(0.7 + 0.5*(GetRows()-1));
			Add( Shape(SHAPE::ROUNDED_RECT, -w,  w, -h,  h) );
			break;
		}
		case COMP::SWITCH_ST_DIP:
		{
			double w(0.35 + 0.5*(GetCols() - 1)), h(0.35 + 0.5*(GetRows()-1));
			Add( Shape(SHAPE::RECT, -w,  w, -h,  h) );
			break;
		}
		case COMP::RESISTOR:
		case COMP::INDUCTOR:
		{
			double w(-0.32 + 0.5*(GetCols() - 1)), h(0.32 + 0.5*(GetRows()-1));
			Add( Shape(SHAPE::ARC, -w - 0.64, -w + 0.16, -h -0.08, -h + 0.72,  53, -53) );
			Add( Shape(SHAPE::ARC,  w - 0.16,  w + 0.64, -h -0.08, -h + 0.72, 233, 127) );
			Add( Shape(SHAPE::LINE,	-w, w, -h, -h) );
			Add( Shape(SHAPE::LINE,	-w, w,  h,  h) );
			break;
		}
		case COMP::WIRE:
		{
			double w(0.5*(GetCols() - 1)), h(0.1 + 0.5*(GetRows()-1));
			Add( Shape(SHAPE::ROUNDED_RECT, -w,  w, -h,  h) );
			break;
		}
		case COMP::DIODE:
		{
			double w(0.35 + 0.5*(GetCols() - 1)), h(0.35 + 0.5*(GetRows()-1));
			Add( Shape(SHAPE::RECT, -w,  w, -h,  h) );
			Add( Shape(SHAPE::LINE,	w - 0.1666, w - 0.1666, -h, h) );
			break;
		}
		case COMP::STRIP_100:
		case COMP::BLOCK_100:
		case COMP::BLOCK_200:
		{
			const int		jj = GetRows() / 2;	// Middle row
			const double	dx = ( GetType() == COMP::BLOCK_200 ) ? 0.5 : 0;
			for (int ii = 0; ii < GetCols(); ii++)
			{
				if ( !Get(jj,ii)->GetIsPin() ) continue;
				double w(0.5*GetCols()- ii), h( 0.5 * GetRows() );
				Add( Shape(SHAPE::ROUNDED_RECT, -w-dx,  -w+dx+1, -h,  h) );
			}
			break;
		}
		default:	assert(0);	// Unhandled eType
	}
}
