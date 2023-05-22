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

#include "MyRGB.h"
#include "TrackElement.h"	// For BAD_NODEID
#include "Element.h"		// For BAD_COMPID, TRAX_COMPID

class  QPointF;
class  Component;
struct MyPolygonF;

enum class DIAGSMODE { OFF = 0, MIN, MAX };
enum class TRACKMODE { OFF = 0, MONO, COLOR, PCB };
enum class COMPSMODE { OFF = 0, OUTLINE, NAME, VALUE };
enum class HOLETYPE  { NPTH = 0, PTH };

// A class to hold the variables set via the GUI

Q_DECL_CONSTEXPR static const int BAD_TEXTID = -1;

class GuiControl : public Persist, public Merge
{
public:
	GuiControl() {}
	virtual ~GuiControl() {}
	GuiControl(const GuiControl& o) { *this = o; }
	GuiControl& operator=(const GuiControl& o)
	{
		m_backgroundColor	= o.m_backgroundColor;
		m_currentLayer		= o.m_currentLayer;
		m_currentCompId		= o.m_currentCompId;
		m_currentNodeId		= o.m_currentNodeId;
		m_groundNodeId0		= o.m_groundNodeId0;
		m_groundNodeId1		= o.m_groundNodeId1;
		m_currentTextId		= o.m_currentTextId;
		m_diagsMode			= o.m_diagsMode;
		m_iTrackMode		= o.m_iTrackMode;
		m_iCompMode			= o.m_iCompMode;
		m_iHoleType			= o.m_iHoleType;
		m_GRIDPIXELS		= o.m_GRIDPIXELS;
		m_PAD_MIL			= o.m_PAD_MIL;
		m_TRACK_MIL			= o.m_TRACK_MIL;
		m_TAG_MIL			= o.m_TAG_MIL;
		m_HOLE_MIL			= o.m_HOLE_MIL;
		m_GAP_MIL			= o.m_GAP_MIL;
		m_MASK_MIL			= o.m_MASK_MIL;
		m_SILK_MIL			= o.m_SILK_MIL;
		m_EDGE_MIL			= o.m_EDGE_MIL;
		m_VIAPAD_MIL		= o.m_VIAPAD_MIL;
		m_VIAHOLE_MIL		= o.m_VIAHOLE_MIL;
		m_iRenderQuality	= o.m_iRenderQuality;
		m_iSaturation		= o.m_iSaturation;
		m_iFillSaturation	= o.m_iFillSaturation;
		m_iCropMargin		= o.m_iCropMargin;
		m_iTargetRows		= o.m_iTargetRows;
		m_iTargetCols		= o.m_iTargetCols;
		m_iTextSizeComp		= o.m_iTextSizeComp;
		m_iTextSizePins		= o.m_iTextSizePins;
		m_iRoutingMethod	= o.m_iRoutingMethod;
		m_bShowTarget		= o.m_bShowTarget;
		m_bShowCloseTracks	= o.m_bShowCloseTracks;
		m_bWireShare		= o.m_bWireShare;
		m_bWireCross		= o.m_bWireCross;
		m_bVeroTracks		= o.m_bVeroTracks;
		m_bCurvedTracks		= o.m_bCurvedTracks;
		m_bFatTracks		= o.m_bFatTracks;
		m_bRoutingEnabled	= o.m_bRoutingEnabled;
		m_bViasEnabled		= o.m_bViasEnabled;
		m_bShowGrid			= o.m_bShowGrid;
		m_bShowText			= o.m_bShowText;
		m_bFlipH			= o.m_bFlipH;
		m_bFlipV			= o.m_bFlipV;
		m_bPinLabels		= o.m_bPinLabels;
		m_bFlyWires			= o.m_bFlyWires;
		m_bGroundFill		= o.m_bGroundFill;
		m_bVerticalStrips	= o.m_bVerticalStrips;
		m_bCompEdit			= o.m_bCompEdit;
		m_bXthermals		= o.m_bXthermals;
		m_bInverseMono		= o.m_bInverseMono;
		return *this;
	}
	bool operator==(const GuiControl& o) const	// Compare persisted info
	{
		return	m_backgroundColor	== o.m_backgroundColor
			&&	m_currentLayer		== o.m_currentLayer
			&&	m_currentCompId		== o.m_currentCompId
			&&	m_currentNodeId		== o.m_currentNodeId
			&&	m_groundNodeId0		== o.m_groundNodeId0
			&&	m_groundNodeId1		== o.m_groundNodeId1
			&&	m_currentTextId		== o.m_currentTextId
			&&	m_diagsMode			== o.m_diagsMode
			&&	m_iTrackMode		== o.m_iTrackMode
			&&	m_iCompMode			== o.m_iCompMode
			&&	m_iHoleType			== o.m_iHoleType
			&&	m_GRIDPIXELS		== o.m_GRIDPIXELS
			&&	m_PAD_MIL			== o.m_PAD_MIL
			&&	m_TRACK_MIL			== o.m_TRACK_MIL
			&&	m_TAG_MIL			== o.m_TAG_MIL
			&&	m_HOLE_MIL			== o.m_HOLE_MIL
			&&	m_GAP_MIL			== o.m_GAP_MIL
			&&	m_MASK_MIL			== o.m_MASK_MIL
			&&	m_SILK_MIL			== o.m_SILK_MIL
			&&	m_EDGE_MIL			== o.m_EDGE_MIL
			&&	m_VIAPAD_MIL		== o.m_VIAPAD_MIL
			&&	m_VIAHOLE_MIL		== o.m_VIAHOLE_MIL
			&&	m_iRenderQuality	== o.m_iRenderQuality
			&&	m_iSaturation		== o.m_iSaturation
			&&	m_iFillSaturation	== o.m_iFillSaturation
			&&	m_iCropMargin		== o.m_iCropMargin
			&&	m_iTargetRows		== o.m_iTargetRows
			&&	m_iTargetCols		== o.m_iTargetCols
			&&	m_iTextSizeComp		== o.m_iTextSizeComp
			&&	m_iTextSizePins		== o.m_iTextSizePins
			&&	m_iRoutingMethod	== o.m_iRoutingMethod
			&&	m_bShowTarget		== o.m_bShowTarget
			&&	m_bShowCloseTracks	== o.m_bShowCloseTracks
			&&	m_bWireShare		== o.m_bWireShare
			&&	m_bWireCross		== o.m_bWireCross
			&&	m_bVeroTracks		== o.m_bVeroTracks
			&&	m_bCurvedTracks		== o.m_bCurvedTracks
			&&	m_bFatTracks		== o.m_bFatTracks
			&&	m_bRoutingEnabled	== o.m_bRoutingEnabled
			&&	m_bViasEnabled		== o.m_bViasEnabled
			&&	m_bShowGrid			== o.m_bShowGrid
			&&	m_bShowText			== o.m_bShowText
			&&	m_bFlipH			== o.m_bFlipH
			&&	m_bFlipV			== o.m_bFlipV
			&&	m_bPinLabels		== o.m_bPinLabels
			&&	m_bFlyWires			== o.m_bFlyWires
			&&	m_bGroundFill		== o.m_bGroundFill
			&&	m_bVerticalStrips	== o.m_bVerticalStrips
			&&	m_bCompEdit			== o.m_bCompEdit
			&&	m_bXthermals		== o.m_bXthermals
			&&	m_bInverseMono		== o.m_bInverseMono;
	}
	bool operator!=(const GuiControl& o) const
	{
		return !(*this == o);
	}
	void Clear()
	{
		SetCurrentCompId(BAD_COMPID);
		SetCurrentNodeId(BAD_NODEID);
		SetGroundNodeId0(BAD_NODEID);
		SetGroundNodeId1(BAD_NODEID);
		SetCurrentTextId(BAD_TEXTID);
		SetTrackMode(TRACKMODE::COLOR);
		SetCompMode(COMPSMODE::NAME);
		SetRoutingEnabled(false);
		SetFlipH(false);
		SetFlipV(false);
		SetGroundFill(false);
		SetCompEdit(false);
	}
	// Merge interface functions
	virtual void UpdateMergeOffsets(MergeOffsets& o) override
	{
		if ( m_currentCompId != BAD_COMPID &&
			 m_currentCompId != TRAX_COMPID )	o.deltaCompId = std::max(o.deltaCompId, m_currentCompId + 1);
		if ( m_currentNodeId != BAD_NODEID )	o.deltaNodeId = std::max(o.deltaNodeId, m_currentNodeId + 1);
		if ( m_groundNodeId0 != BAD_NODEID )	o.deltaNodeId = std::max(o.deltaNodeId, m_groundNodeId0 + 1);
		if ( m_groundNodeId1 != BAD_NODEID )	o.deltaNodeId = std::max(o.deltaNodeId, m_groundNodeId1 + 1);
	}
	virtual void ApplyMergeOffsets(const MergeOffsets& o) override
	{
		if ( m_currentCompId != BAD_COMPID &&
			 m_currentCompId != TRAX_COMPID)	m_currentCompId += o.deltaCompId;
		if ( m_currentNodeId != BAD_NODEID )	m_currentNodeId += o.deltaNodeId;
		if ( m_groundNodeId0 != BAD_NODEID )	m_groundNodeId0 += o.deltaNodeId;
		if ( m_groundNodeId1 != BAD_NODEID )	m_groundNodeId1 += o.deltaNodeId;
	}
	void Merge(const GuiControl& o)
	{
		m_currentCompId	= o.m_currentCompId;
		m_currentNodeId	= o.m_currentNodeId;
		m_groundNodeId0	= o.m_groundNodeId0;
		m_groundNodeId1	= o.m_groundNodeId1;
	}
	// Persist functions
	virtual void Load(DataStream& inStream) override
	{
		m_backgroundColor = MyRGB(0xFFFFFF);
		if ( inStream.GetVersion() >= VRT_VERSION_50 )
			m_backgroundColor.Load(inStream);	// Added in VRT_VERSION_50
		m_currentLayer = 0;
		if ( inStream.GetVersion() >= VRT_VERSION_34 )
			inStream.Load(m_currentLayer);		// Added in VRT_VERSION_34
		inStream.Load(m_currentCompId);
		inStream.Load(m_currentNodeId);
		m_groundNodeId0 = BAD_NODEID;
		if ( inStream.GetVersion() >= VRT_VERSION_3 )
			inStream.Load(m_groundNodeId0);		// Added in VRT_VERSION_3
		m_groundNodeId1 = BAD_NODEID;
		if ( inStream.GetVersion() >= VRT_VERSION_34 )
			inStream.Load(m_groundNodeId1);		// Added in VRT_VERSION_34
		m_currentTextId = BAD_TEXTID;
		if ( inStream.GetVersion() >= VRT_VERSION_14 )
			inStream.Load(m_currentTextId);		// Added in VRT_VERSION_14
		int diagMode(0), trackMode(0), compMode(0), holeType(0);
		inStream.Load(diagMode);
		inStream.Load(trackMode);
		inStream.Load(compMode);
		if ( inStream.GetVersion() >= VRT_VERSION_33 )
			inStream.Load(holeType);			// Added in VRT_VERSION_33
		m_diagsMode		= static_cast<DIAGSMODE>	(diagMode);
		m_iTrackMode	= static_cast<TRACKMODE>	(trackMode);
		m_iCompMode		= static_cast<COMPSMODE>	(compMode);
		m_iHoleType		= static_cast<HOLETYPE>		(holeType);
		inStream.Load(m_GRIDPIXELS);
		inStream.Load(m_PAD_MIL);
		inStream.Load(m_TRACK_MIL);
		m_TAG_MIL = m_TRACK_MIL;
		if ( inStream.GetVersion() >= VRT_VERSION_52 )
			inStream.Load(m_TAG_MIL);			// Added in VRT_VERSION_52
		inStream.Load(m_HOLE_MIL);
		m_GAP_MIL = 10;
		if ( inStream.GetVersion() >= VRT_VERSION_3 )
			inStream.Load(m_GAP_MIL);			// Added in VRT_VERSION_3
		m_MASK_MIL = 4;
		m_SILK_MIL = 7;
		if ( inStream.GetVersion() >= VRT_VERSION_32 )
		{
			inStream.Load(m_MASK_MIL);			// Added in VRT_VERSION_32
			inStream.Load(m_SILK_MIL);			// Added in VRT_VERSION_32
		}
		m_EDGE_MIL = 20;
		if ( inStream.GetVersion() >= VRT_VERSION_33 )
			inStream.Load(m_EDGE_MIL);			// Added in VRT_VERSION_33
		m_VIAPAD_MIL	= 50;
		m_VIAHOLE_MIL	= 25;
		if ( inStream.GetVersion() >= VRT_VERSION_35 )
		{
			inStream.Load(m_VIAPAD_MIL);		// Added in VRT_VERSION_35
			inStream.Load(m_VIAHOLE_MIL);		// Added in VRT_VERSION_35
		}
		inStream.Load(m_iRenderQuality);
		m_iSaturation = 100;
		if ( inStream.GetVersion() >= VRT_VERSION_6 )
			inStream.Load(m_iSaturation);		// Added in VRT_VERSION_6
		m_iFillSaturation = 0;
		if ( inStream.GetVersion() >= VRT_VERSION_29 )
			inStream.Load(m_iFillSaturation);	// Added in VRT_VERSION_29
		m_iCropMargin = 0;
		if ( inStream.GetVersion() >= VRT_VERSION_13 )
			inStream.Load(m_iCropMargin);		// Added in VRT_VERSION_13
		m_iTargetRows = 10;
		if ( inStream.GetVersion() >= VRT_VERSION_21 )
			inStream.Load(m_iTargetRows);		// Added in VRT_VERSION_21
		m_iTargetCols = 10;
		if ( inStream.GetVersion() >= VRT_VERSION_21 )
			inStream.Load(m_iTargetCols);		// Added in VRT_VERSION_21
		m_iTextSizeComp = 9;
		if ( inStream.GetVersion() >= VRT_VERSION_17 )
			inStream.Load(m_iTextSizeComp);		// Added in VRT_VERSION_17
		m_iTextSizePins = 9;
		if ( inStream.GetVersion() >= VRT_VERSION_17 )
			inStream.Load(m_iTextSizePins);		// Added in VRT_VERSION_17
		m_iRoutingMethod = 0;
		if ( inStream.GetVersion() >= VRT_VERSION_24 )
			inStream.Load(m_iRoutingMethod);	// Added in VRT_VERSION_24
		m_bShowTarget = false;
		if ( inStream.GetVersion() >= VRT_VERSION_21 )
			inStream.Load(m_bShowTarget);		// Added in VRT_VERSION_21
		m_bShowCloseTracks = false;
		if ( inStream.GetVersion() >= VRT_VERSION_44 )
			inStream.Load(m_bShowCloseTracks);	// Added in VRT_VERSION_44
		m_bWireShare = true;
		if ( inStream.GetVersion() >= VRT_VERSION_28 )
			inStream.Load(m_bWireShare);		// Added in VRT_VERSION_28
		m_bWireCross = false;
		if ( inStream.GetVersion() >= VRT_VERSION_28 )
			inStream.Load(m_bWireCross);		// Added in VRT_VERSION_28
		inStream.Load(m_bVeroTracks);
		inStream.Load(m_bCurvedTracks);
		m_bFatTracks = true;
		if ( inStream.GetVersion() >= VRT_VERSION_36 )
			inStream.Load(m_bFatTracks);		// Added in VRT_VERSION_36
		inStream.Load(m_bRoutingEnabled);
		m_bViasEnabled = true;
		if ( inStream.GetVersion() >= VRT_VERSION_37 )
			inStream.Load(m_bViasEnabled);		// Added in VRT_VERSION_37
		m_bShowGrid = m_iTrackMode != TRACKMODE::OFF;
		if ( inStream.GetVersion() >= VRT_VERSION_5 )
			inStream.Load(m_bShowGrid);			// Added in VRT_VERSION_5
		m_bShowText = true;
		if ( inStream.GetVersion() >= VRT_VERSION_14 )
			inStream.Load(m_bShowText);			// Added in VRT_VERSION_14
		inStream.Load(m_bFlipH);
		m_bFlipV = false;
		if ( inStream.GetVersion() >= VRT_VERSION_14 )
			inStream.Load(m_bFlipV);			// Added in VRT_VERSION_14
		m_bPinLabels = false;
		if ( inStream.GetVersion() >= VRT_VERSION_2 )
			inStream.Load(m_bPinLabels);		// Added in VRT_VERSION_2
		m_bFlyWires = true;
		if ( inStream.GetVersion() >= VRT_VERSION_48 )
			inStream.Load(m_bFlyWires);			// Added in VRT_VERSION_48
		m_bGroundFill = false;
		if ( inStream.GetVersion() >= VRT_VERSION_3 )
			inStream.Load(m_bGroundFill);		// Added in VRT_VERSION_3
		m_bVerticalStrips = true;
		if ( inStream.GetVersion() >= VRT_VERSION_12 )
			inStream.Load(m_bVerticalStrips);	// Added in VRT_VERSION_12
		m_bCompEdit = false;
		if ( inStream.GetVersion() >= VRT_VERSION_19 )
			inStream.Load(m_bCompEdit);			// Added in VRT_VERSION_19
		m_bXthermals = false;
		if ( inStream.GetVersion() >= VRT_VERSION_53 )
			inStream.Load(m_bXthermals);		// Added in VRT_VERSION_53
		m_bInverseMono = false;
		if ( inStream.GetVersion() >= VRT_VERSION_57 )
			inStream.Load(m_bInverseMono);		// Added in VRT_VERSION_57
				
	}
	virtual void Save(DataStream& outStream) override
	{
		m_backgroundColor.Save(outStream);	// Added in VRT_VERSION_50
		outStream.Save(m_currentLayer);		// Added in VRT_VERSION_34
		outStream.Save(m_currentCompId);
		outStream.Save(m_currentNodeId);
		outStream.Save(m_groundNodeId0);	// Added in VRT_VERSION_3
		outStream.Save(m_groundNodeId1);	// Added in VRT_VERSION_34
		outStream.Save(m_currentTextId);	// Added in VRT_VERSION_14
		outStream.Save(static_cast<int>(m_diagsMode));
		outStream.Save(static_cast<int>(m_iTrackMode));
		outStream.Save(static_cast<int>(m_iCompMode));
		outStream.Save(static_cast<int>(m_iHoleType));	// Added in VRT_VERSION_33
		outStream.Save(m_GRIDPIXELS);
		outStream.Save(m_PAD_MIL);
		outStream.Save(m_TRACK_MIL);
		outStream.Save(m_TAG_MIL);			// Added in VRT_VERSION_52
		outStream.Save(m_HOLE_MIL);
		outStream.Save(m_GAP_MIL);			// Added in VRT_VERSION_3
		outStream.Save(m_MASK_MIL);			// Added in VRT_VERSION_32
		outStream.Save(m_SILK_MIL);			// Added in VRT_VERSION_32
		outStream.Save(m_EDGE_MIL);			// Added in VRT_VERSION_33
		outStream.Save(m_VIAPAD_MIL);		// Added in VRT_VERSION_35
		outStream.Save(m_VIAHOLE_MIL);		// Added in VRT_VERSION_35
		outStream.Save(m_iRenderQuality);
		outStream.Save(m_iSaturation);		// Added in VRT_VERSION_6
		outStream.Save(m_iFillSaturation);	// Added in VRT_VERSION_29
		outStream.Save(m_iCropMargin);		// Added in VRT_VERSION_13
		outStream.Save(m_iTargetRows);		// Added in VRT_VERSION_21
		outStream.Save(m_iTargetCols);		// Added in VRT_VERSION_21
		outStream.Save(m_iTextSizeComp);	// Added in VRT_VERSION_17
		outStream.Save(m_iTextSizePins);	// Added in VRT_VERSION_17
		outStream.Save(m_iRoutingMethod);	// Added in VRT_VERSION_24
		outStream.Save(m_bShowTarget);		// Added in VRT_VERSION_21
		outStream.Save(m_bShowCloseTracks);	// Added in VRT_VERSION_44
		outStream.Save(m_bWireShare);		// Added in VRT_VERSION_28
		outStream.Save(m_bWireCross);		// Added in VRT_VERSION_28
		outStream.Save(m_bVeroTracks);
		outStream.Save(m_bCurvedTracks);
		outStream.Save(m_bFatTracks);		// Added in VRT_VERSION_36
		outStream.Save(m_bRoutingEnabled);
		outStream.Save(m_bViasEnabled);		// Added in VRT_VERSION_37
		outStream.Save(m_bShowGrid);		// Added in VRT_VERSION_5
		outStream.Save(m_bShowText);		// Added in VRT_VERSION_14
		outStream.Save(m_bFlipH);
		outStream.Save(m_bFlipV);			// Added in VRT_VERSION_14
		outStream.Save(m_bPinLabels);		// Added in VRT_VERSION_2
		outStream.Save(m_bFlyWires);		// Added in VRT_VERSION_48
		outStream.Save(m_bGroundFill);		// Added in VRT_VERSION_3
		outStream.Save(m_bVerticalStrips);	// Added in VRT_VERSION_12
		outStream.Save(m_bCompEdit);		// Added in VRT_VERSION_19
		outStream.Save(m_bXthermals);		// Added in VRT_VERSION_53
		outStream.Save(m_bInverseMono);		// Added in VRT_VERSION_57
	}
	bool SetBackgroundColor(const MyRGB& o)	{ const bool bChanged = m_backgroundColor	!= o; m_backgroundColor	= o; return bChanged;}
	bool SetCurrentLayer(int i)				{ const bool bChanged = m_currentLayer		!= i; m_currentLayer	= i; return bChanged; }
	bool SetCurrentNodeId(int i)			{ const bool bChanged = m_currentNodeId		!= i; m_currentNodeId	= i; return bChanged; }
	bool SetCurrentCompId(int i)			{ const bool bChanged = m_currentCompId		!= i; m_currentCompId	= i; return bChanged; }
	bool SetGroundNodeId0(int i)			{ const bool bChanged = m_groundNodeId0		!= i; m_groundNodeId0	= i; return bChanged; }
	bool SetGroundNodeId1(int i)			{ const bool bChanged = m_groundNodeId1		!= i; m_groundNodeId1	= i; return bChanged; }
	bool SetCurrentTextId(int i)			{ const bool bChanged = m_currentTextId		!= i; m_currentTextId	= i; return bChanged; }
	bool SetDiagsMode(DIAGSMODE e)			{ const bool bChanged = m_diagsMode			!= e; m_diagsMode		= e; return bChanged; }
	bool SetTrackMode(TRACKMODE e)			{ const bool bChanged = m_iTrackMode		!= e; m_iTrackMode		= e; return bChanged; }
	bool SetCompMode(COMPSMODE e)			{ const bool bChanged = m_iCompMode			!= e; m_iCompMode		= e; return bChanged; }
	bool SetHoleType(HOLETYPE e)			{ const bool bChanged = m_iHoleType			!= e; m_iHoleType		= e; return bChanged; }
	bool SetGRIDPIXELS(int i)				{ const bool bChanged = m_GRIDPIXELS		!= i; m_GRIDPIXELS		= i; return bChanged; }
	bool SetPAD_MIL(int i)					{ const bool bChanged = m_PAD_MIL			!= i; m_PAD_MIL			= i;
											  if ( bChanged && GetHOLE_MIL() > i-8 ) SetHOLE_MIL( i-8 );		// 8 ==> minimum annular ring = 4 mil
											  return bChanged; }
	bool SetTRACK_MIL(int i)				{ const bool bChanged = m_TRACK_MIL			!= i; m_TRACK_MIL		= i; return bChanged; }
	bool SetTAG_MIL(int i)					{ const bool bChanged = m_TAG_MIL			!= i; m_TAG_MIL			= i; return bChanged; }
	bool SetHOLE_MIL(int i)					{ const bool bChanged = m_HOLE_MIL			!= i; m_HOLE_MIL		= i;
											  if ( bChanged && GetPAD_MIL() < i+8 ) SetPAD_MIL( i+8 );			// 8 ==> minimum annular ring = 4 mil
											  return bChanged; }
	bool SetGAP_MIL(int i)					{ const bool bChanged = m_GAP_MIL			!= i; m_GAP_MIL			= i; return bChanged; }
	bool SetMASK_MIL(int i)					{ const bool bChanged = m_MASK_MIL			!= i; m_MASK_MIL		= i; return bChanged; }
	bool SetSILK_MIL(int i)					{ const bool bChanged = m_SILK_MIL			!= i; m_SILK_MIL		= i; return bChanged; }
	bool SetEDGE_MIL(int i)					{ const bool bChanged = m_EDGE_MIL			!= i; m_EDGE_MIL		= i; return bChanged; }
	bool SetVIAPAD_MIL(int i)				{ const bool bChanged = m_VIAPAD_MIL		!= i; m_VIAPAD_MIL		= i;
											  if ( bChanged && GetVIAHOLE_MIL() > i-8 ) SetVIAHOLE_MIL( i-8 );	// 8 ==> minimum annular ring = 4 mil
											  return bChanged;
											}
	bool SetVIAHOLE_MIL(int i)				{ const bool bChanged = m_VIAHOLE_MIL		!= i; m_VIAHOLE_MIL		= i;
											  if ( bChanged && GetVIAPAD_MIL() < i+8 ) SetVIAPAD_MIL( i+8 );	// 8 ==> minimum annular ring = 4 mil
											  return bChanged; }
	bool SetRenderQuality(int i)			{ const bool bChanged = m_iRenderQuality	!= i; m_iRenderQuality	= i; return bChanged; }
	bool SetSaturation(int i)				{ const bool bChanged = m_iSaturation		!= i; m_iSaturation		= i; return bChanged; }
	bool SetFillSaturation(int i)			{ const bool bChanged = m_iFillSaturation	!= i; m_iFillSaturation	= i; return bChanged; }
	bool SetCropMargin(int i)				{ const bool bChanged = m_iCropMargin		!= i; m_iCropMargin		= i; return bChanged; }
	bool SetTargetRows(int i)				{ const bool bChanged = m_iTargetRows		!= i; m_iTargetRows		= i; return bChanged; }
	bool SetTargetCols(int i)				{ const bool bChanged = m_iTargetCols		!= i; m_iTargetCols		= i; return bChanged; }
	bool SetTextSizeComp(int i)				{ const bool bChanged = m_iTextSizeComp		!= i; m_iTextSizeComp	= i; return bChanged; }
	bool SetTextSizePins(int i)				{ const bool bChanged = m_iTextSizePins		!= i; m_iTextSizePins	= i; return bChanged; }
	bool SetRoutingMethod(int i)			{ const bool bChanged = m_iRoutingMethod	!= i; m_iRoutingMethod	= i; return bChanged; }
	bool SetShowTarget(bool b)				{ const bool bChanged = m_bShowTarget		!= b; m_bShowTarget		= b; return bChanged; }
	bool SetShowCloseTracks(bool b)			{ const bool bChanged = m_bShowCloseTracks	!= b; m_bShowCloseTracks= b; return bChanged; }
	bool SetWireShare(bool b)				{ const bool bChanged = m_bWireShare		!= b; m_bWireShare		= b; return bChanged; }
	bool SetWireCross(bool b)				{ const bool bChanged = m_bWireCross		!= b; m_bWireCross		= b; return bChanged; }
	bool SetVeroTracks(bool b)				{ const bool bChanged = m_bVeroTracks		!= b; m_bVeroTracks		= b; return bChanged; }
	bool SetCurvedTracks(bool b)			{ const bool bChanged = m_bCurvedTracks		!= b; m_bCurvedTracks	= b; return bChanged; }
	bool SetFatTracks(bool b)				{ const bool bChanged = m_bFatTracks		!= b; m_bFatTracks		= b; return bChanged; }
	bool SetRoutingEnabled(bool b)			{ const bool bChanged = m_bRoutingEnabled	!= b; m_bRoutingEnabled	= b; return bChanged; }
	bool SetViasEnabled(bool b)				{ const bool bChanged = m_bViasEnabled		!= b; m_bViasEnabled	= b; return bChanged; }
	bool SetShowGrid(bool b)				{ const bool bChanged = m_bShowGrid			!= b; m_bShowGrid		= b; return bChanged; }
	bool SetShowText(bool b)				{ const bool bChanged = m_bShowText			!= b; m_bShowText		= b; return bChanged; }
	bool SetFlipH(bool b)					{ const bool bChanged = m_bFlipH			!= b; m_bFlipH			= b; return bChanged; }
	bool SetFlipV(bool b)					{ const bool bChanged = m_bFlipV			!= b; m_bFlipV			= b; return bChanged; }
	bool SetShowPinLabels(bool b)			{ const bool bChanged = m_bPinLabels		!= b; m_bPinLabels		= b; return bChanged; }
	bool SetShowFlyWires(bool b)			{ const bool bChanged = m_bFlyWires			!= b; m_bFlyWires		= b; return bChanged; }
	bool SetGroundFill(bool b)				{ const bool bChanged = m_bGroundFill		!= b; m_bGroundFill		= b; return bChanged; }
	bool SetVerticalStrips(bool b)			{ const bool bChanged = m_bVerticalStrips	!= b; m_bVerticalStrips	= b; return bChanged; }
	bool SetCompEdit(bool b)				{ const bool bChanged = m_bCompEdit			!= b; m_bCompEdit		= b; return bChanged; }
	bool SetXthermals(bool b)				{ const bool bChanged = m_bXthermals		!= b; m_bXthermals		= b; return bChanged; }
	bool SetInverseMono(bool b)				{ const bool bChanged = m_bInverseMono		!= b; m_bInverseMono	= b; return bChanged; }
	const MyRGB&		GetBackgroundColor() const	{ return m_backgroundColor; }
	const int&			GetCurrentLayer() const		{ return m_currentLayer; }
	const int&			GetCurrentNodeId() const	{ return m_currentNodeId; }
	const int&			GetCurrentCompId() const	{ return m_currentCompId; }
	const int&			GetGroundNodeId0() const	{ return m_groundNodeId0; }
	const int&			GetGroundNodeId1() const	{ return m_groundNodeId1; }
	const int&			GetCurrentTextId() const	{ return m_currentTextId; }
	const DIAGSMODE&	GetDiagsMode() const		{ return m_diagsMode; }
	const TRACKMODE&	GetTrackMode() const		{ return m_iTrackMode; }
	const COMPSMODE&	GetCompMode() const			{ return m_iCompMode; }
	const HOLETYPE&		GetHoleType() const			{ return m_iHoleType; }
	const int&			GetGRIDPIXELS() const		{ return m_GRIDPIXELS; }
	const int&			GetPAD_MIL() const			{ return m_PAD_MIL; }
	const int&			GetTRACK_MIL() const		{ return m_TRACK_MIL; }
	const int&			GetTAG_MIL() const			{ return m_TAG_MIL; }
	const int&			GetHOLE_MIL() const			{ return m_HOLE_MIL; }
	const int&			GetGAP_MIL() const			{ return m_GAP_MIL; }
	const int&			GetMASK_MIL() const			{ return m_MASK_MIL; }
	const int&			GetSILK_MIL() const			{ return m_SILK_MIL; }
	const int&			GetEDGE_MIL() const			{ return m_EDGE_MIL; }
	const int&			GetVIAPAD_MIL() const		{ return m_VIAPAD_MIL; }
	const int&			GetVIAHOLE_MIL() const		{ return m_VIAHOLE_MIL; }
	Q_DECL_CONSTEXPR static inline int GetPAD_IC_MIL()		{ return 24; }
	Q_DECL_CONSTEXPR static inline int GetTRACK_IC_MIL()	{ return 18; }
	Q_DECL_CONSTEXPR static inline int GetMIN_IC_MIL()		{ return 14; }
	const int&			GetRenderQuality() const	{ return m_iRenderQuality; }
	const int&			GetSaturation() const		{ return m_iSaturation; }
	const int&			GetFillSaturation() const	{ return m_iFillSaturation; }
	const int&			GetCropMargin() const		{ return m_iCropMargin; }
	const int&			GetTargetRows() const		{ return m_iTargetRows; }
	const int&			GetTargetCols() const		{ return m_iTargetCols; }
	const int&			GetTextSizeComp() const		{ return m_iTextSizeComp; }
	const int&			GetTextSizePins() const		{ return m_iTextSizePins; }
	const int&			GetRoutingMethod() const	{ return m_iRoutingMethod; }
	const bool&			GetShowTarget() const		{ return m_bShowTarget; }
	const bool&			GetShowCloseTracks() const	{ return m_bShowCloseTracks; }
	const bool&			GetWireShare() const		{ return m_bWireShare; }
	const bool&			GetWireCross() const		{ return m_bWireCross; }
	const bool&			GetVeroTracks() const		{ return m_bVeroTracks; }
	const bool&			GetCurvedTracks() const		{ return m_bCurvedTracks; }
	const bool&			GetFatTracks() const		{ return m_bFatTracks; }
	const bool&			GetRoutingEnabled() const	{ return m_bRoutingEnabled; }
	const bool&			GetViasEnabled() const		{ return m_bViasEnabled; }
	const bool&			GetShowGrid() const			{ return m_bShowGrid; }
	const bool&			GetShowText() const			{ return m_bShowText; }
	const bool&			GetFlipH() const			{ return m_bFlipH; }
	const bool&			GetFlipV() const			{ return m_bFlipV; }
	const bool&			GetShowPinLabels() const	{ return m_bPinLabels; }
	const bool&			GetShowFlyWires() const		{ return m_bFlyWires; }
	const bool&			GetGroundFill() const		{ return m_bGroundFill; }
	const bool&			GetVerticalStrips() const	{ return m_bVerticalStrips; }
	const bool&			GetCompEdit() const			{ return m_bCompEdit; }
	const bool&			GetXthermals() const		{ return m_bXthermals; }
	const bool&			GetInverseMono() const		{ return m_bInverseMono; }
	// Helpers
	bool		SetGroundNodeId()					{ return ( GetCurrentLayer() == 0 ) ? SetGroundNodeId0( GetCurrentNodeId() ) : SetGroundNodeId1( GetCurrentNodeId() ); }
	const int&	GetGroundNodeId(int lyr) const		{ return ( lyr == 0 ) ? GetGroundNodeId0() : GetGroundNodeId1(); }
	bool		GetUsePCBshapes()					{ return GetTrackMode() == TRACKMODE::PCB; }
	bool		GetMirrored() const					{ return GetFlipH() || GetFlipV(); }
	bool		SetTrackSliderValue(int i)			{ const bool bChanged = ( GetTrackSliderValue() != i ); SetTrackMode( static_cast<TRACKMODE>(i) ); return bChanged; }
	bool		SetCompSliderValue(int i)			{ const bool bChanged = ( GetCompSliderValue()  != i ); SetCompMode(  static_cast<COMPSMODE>(i) ); return bChanged; }
	int			GetTrackSliderValue() const			{ return static_cast<int>(GetTrackMode()); }
	int			GetCompSliderValue() const			{ return static_cast<int>(GetCompMode());  }
	double		GetSilkWidth() const				{ return std::max(1.0, GetGRIDPIXELS() * GetSILK_MIL() * 0.010 );  }	// Silk-screen pen width in pixels
	double		GetEdgeWidth() const				{ return std::max(1.0, GetGRIDPIXELS() * GetEDGE_MIL() * 0.010 );  }	// Board edge margin in pixels
	int			GetHalfPixelsFromMIL(int iMIL) const
	{
		return std::max(1, static_cast<int> (GetGRIDPIXELS() * iMIL	* 0.005 ));
	}
	int		GetPixelsFromMIL(int iMIL) const
	{
		return std::max(1, static_cast<int> (GetGRIDPIXELS() * iMIL	* 0.010 ));
	}
	void	CalcBlob(qreal W, const QPointF& pC, const QPointF& pCoffset,
					 int iPadWidthMIL, int iPerimeterCode, int iTagCode,
					 std::list<MyPolygonF>& out,
					 bool bHavePad, bool bHaveSoic, bool bIsGnd, bool bGap = false) const;

	void	CalcSOIC(qreal W, const QPointF& pC, size_t pinIndex, const Component* pComp, std::list<MyPolygonF>& out, bool bSolderMask, bool bIsGnd, bool bGap = false) const;

	void Reset()	// For use with File->New()
	{
		m_currentLayer		= 0;
		m_currentCompId		= BAD_COMPID;
		m_currentNodeId		= BAD_NODEID;
		m_groundNodeId0		= BAD_NODEID;
		m_groundNodeId1		= BAD_NODEID;
		m_currentTextId		= BAD_TEXTID;
		m_diagsMode			= DIAGSMODE::MIN;
		m_iTrackMode		= TRACKMODE::COLOR;
		m_iCompMode			= COMPSMODE::NAME;
		m_iFillSaturation	= 0;
		m_bShowTarget		= false;
		m_bShowCloseTracks	= false;
		m_bVeroTracks		= false;
		m_bCurvedTracks		= false;
		m_bFatTracks		= true;
		m_bRoutingEnabled	= false;
		m_bViasEnabled		= true;
		m_bShowGrid			= true;
		m_bShowText			= true;
		m_bFlipH			= false;
		m_bFlipV			= false;
		m_bPinLabels		= true;
		m_bFlyWires			= true;
		m_bGroundFill		= false;
		m_bCompEdit			= false;
		m_bXthermals		= false;
		m_bInverseMono		= false;
	}
private:
	MyRGB		m_backgroundColor	= MyRGB(0xFFFFFF);
	int			m_currentLayer		= 0;				// Currently selected layer for display
	int			m_currentCompId		= BAD_COMPID;		// Currently selected component ID
	int			m_currentNodeId		= BAD_NODEID;		// Currently selected node ID
	int			m_groundNodeId0		= BAD_NODEID;		// The node ID used for ground-fill on layer 0
	int			m_groundNodeId1		= BAD_NODEID;		// The node ID used for ground-fill on layer 1
	int			m_currentTextId		= BAD_TEXTID;		// Currently selected text box
	DIAGSMODE	m_diagsMode			= DIAGSMODE::MIN;	// OFF, MIN, MAX
	TRACKMODE	m_iTrackMode		= TRACKMODE::COLOR;	// OFF, MONO, COLOR, PCB
	COMPSMODE	m_iCompMode			= COMPSMODE::NAME;	// OFF, OUTLINE, NAME, VALUE
	HOLETYPE	m_iHoleType			= HOLETYPE::NPTH;	// NPTH, PTH (Non-Plated Through Hole, Plated Through Hole)
	int			m_GRIDPIXELS		= 24;				// Default 24 pixels per grid square (i.e. per 100 mil)
	int			m_PAD_MIL			= 60;				// Range 50 to 98
	int			m_TRACK_MIL			= 24;				// Range 12 to 50
	int			m_TAG_MIL			= 12;				// Range 12 to 50 (Same range as m_TRACK_MIL for backward compatibility)
	int			m_HOLE_MIL			= 34;				// Range 20 to 40
	int			m_GAP_MIL			= 10;				// Range  5 to 30
	int			m_MASK_MIL			= 4;				// Range  0 to 10
	int			m_SILK_MIL			= 7;				// Range  1 to 10
	int			m_EDGE_MIL			= 20;				// Range  0 to 50
	int			m_VIAPAD_MIL		= 50;				// Range 50 to 80
	int			m_VIAHOLE_MIL		= 25;				// Range 20 to 40
	int			m_iRenderQuality	= 1;				// 0 (Low) to 1 (High)
	int			m_iSaturation		= 60;				// Track color saturation (20 to 100 percent)
	int			m_iFillSaturation	= 0;				// Component fill saturation (0 to 100 percent)
	int			m_iCropMargin		= 0;				// Number of border squares after auto-crop
	int			m_iTargetRows		= 10;				// Desired board size
	int			m_iTargetCols		= 10;				// Desired board size
	int			m_iTextSizeComp		= 9;				// Point size for component text
	int			m_iTextSizePins		= 9;				// Point size for component pins
	int			m_iRoutingMethod	= 0;				// Routing method. 0 ==> fast, 1 ==> allow rip-up
	bool		m_bShowTarget		= false;			// true ==> show target board area
	bool		m_bShowCloseTracks	= false;			// true ==> show circles near places with smallest track separation
	bool		m_bWireShare		= true;				// true ==> allow 2 wires per hole
	bool		m_bWireCross		= false;			// true ==> allow wires to cross/overlay
	bool		m_bVeroTracks		= false;
	bool		m_bCurvedTracks		= false;
	bool		m_bFatTracks		= true;
	bool		m_bRoutingEnabled	= false;
	bool		m_bViasEnabled		= true;				// true ==> allow vias for 2-layer boards
	bool		m_bShowGrid			= true;				// true ==> show grid dots
	bool		m_bShowText			= true;				// true ==> show text boxes
	bool		m_bFlipH			= false;			// true ==> flip L and R (with no manual manipulation)
	bool		m_bFlipV			= false;			// true ==> flip T and B (with no manual manipulation)
	bool		m_bPinLabels		= true;				// Show SIP/DIP pins as labels in non-mono mode
	bool		m_bFlyWires			= true;				// Show flying wires
	bool		m_bGroundFill		= false;
	bool		m_bVerticalStrips	= true;
	bool		m_bCompEdit			= false;			// true ==> component editor mode
	bool		m_bXthermals		= false;			// true ==> force X thermal reliefs and hide tracks in ground-fill
	bool		m_bInverseMono		= false;			// true ==> white tracks on black background
};
