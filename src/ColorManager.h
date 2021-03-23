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

#include "AdjInfoManager.h"
#include "MyRGB.h"
#include "Grid.h"

#define MYNUMCOLORS 		12
#define MY_GREY				(MYNUMCOLORS)
#define MY_BLACK			(MYNUMCOLORS+1)
#define NUM_PIXMAP_COLORS	(MYNUMCOLORS+2)
#define MY_LYR_BOT			(MYNUMCOLORS+3)
#define MY_LYR_TOP			(MYNUMCOLORS+4)

// Manager class to handle assignment of colors to nodeIds

const int BAD_COLORID = -1;

class ColorManager : public Persist, public Merge
{
public:
	ColorManager() : m_iSaturation(100), m_iFillSaturation(0), m_bReAssign(true) {}
	virtual ~ColorManager() {}
	ColorManager(const ColorManager& o) { *this = o; }	// Never called
	ColorManager& operator=(const ColorManager& o)
	{
		m_mapNodeIdToColorId.clear();
		m_mapNodeIdToColorId.insert(o.m_mapNodeIdToColorId.begin(), o.m_mapNodeIdToColorId.end());
		m_mapNodeIdToCustomColor.clear();
		m_mapNodeIdToCustomColor.insert(o.m_mapNodeIdToCustomColor.begin(), o.m_mapNodeIdToCustomColor.end());
		m_iSaturation		= o.m_iSaturation;
		m_iFillSaturation	= o.m_iFillSaturation;
		m_bReAssign			= o.m_bReAssign;
		return *this;
	}
	bool operator==(const ColorManager& o) const
	{
		if ( m_mapNodeIdToCustomColor.size() != o.m_mapNodeIdToCustomColor.size() ) return false;
		for (const auto& mapObj : m_mapNodeIdToCustomColor)
		{
			const auto iter = o.m_mapNodeIdToCustomColor.find( mapObj.first );
			if ( iter == o.m_mapNodeIdToCustomColor.end() ) return false;
			if ( mapObj.second != iter->second ) return false;
		}
		return true;
	}
	bool operator!=(const ColorManager& o) const
	{
		return !(*this == o);
	}
	void Clear()					{ m_mapNodeIdToColorId.clear(); m_mapNodeIdToCustomColor.clear(); ReAssignColors(); }
	void ReAssignColors()			{ m_bReAssign = true; }
	void SetSaturation(int i)		{ m_iSaturation = i; }
	void SetFillSaturation(int i)	{ m_iFillSaturation = i; }
	void SetNodeColor(const int& nodeId, const QColor& color)
	{
		if ( nodeId != BAD_NODEID ) m_mapNodeIdToCustomColor[nodeId] = MyRGB(color);
	}
	bool GetIsFixed(const int& nodeId) const
	{
		return m_mapNodeIdToCustomColor.find(nodeId) != m_mapNodeIdToCustomColor.end();
	}
	void Fix(const int& nodeId)
	{
		if ( nodeId == BAD_NODEID || GetIsFixed(nodeId) ) return;	// Already fixed
		const int colorId = m_mapNodeIdToColorId[nodeId];
		const bool bOK = colorId != BAD_COLORID;	assert(bOK);
		if ( bOK )
			m_mapNodeIdToCustomColor[nodeId] = GetPixmapRGB(colorId, false);
	}
	void Unfix(const int& nodeId)
	{
		auto iter = m_mapNodeIdToCustomColor.find(nodeId);
		if ( iter != m_mapNodeIdToCustomColor.end() )
			m_mapNodeIdToCustomColor.erase(iter);
	}
	void CalculateColors(const AdjInfoManager& adjManager, ElementGrid* pBoard)	// The coloring algorithm
	{
		std::vector<int> nodeIds;
		adjManager.GetBoardNodeIds(nodeIds);	// The set of valid nodeIDs on the board

		if ( m_bReAssign )
		{
			if ( pBoard->GetNumNodeIds() > MYNUMCOLORS )
				m_mapNodeIdToColorId.clear();	// Only wipe if we require more colors
			m_bReAssign = false;
		}
		std::list<int> cnList[MYNUMCOLORS];	// Lists of nodeIds used by colours
		int iStartColorId(0);
		for (size_t i = 0, iSize = nodeIds.size(); i < iSize; i++)	// Loop nodeIds on board
		{
			const int&	nodeIdI		= nodeIds[i];
			const int	colorIdI	= GetColorId(nodeIdI);
			if ( colorIdI != BAD_COLORID )
			{
				cnList[colorIdI].push_back(nodeIdI);	// Update list
				continue;	// Don't recolor
			}

			const AdjInfo* pI = adjManager.GetAdjInfo( nodeIdI );

			int bestColorId(0), minCost(INT_MAX);
			for (int iLoopColor = 0; iLoopColor < MYNUMCOLORS; iLoopColor++)
			{
				const int iColorId = ( iStartColorId + iLoopColor ) % MYNUMCOLORS;
				m_mapNodeIdToColorId[nodeIdI] = iColorId;
				int cost(0);
				for (size_t j = 0; j < iSize; j++)	// Loop nodeIds on board
				{
					if ( j == i ) continue;	// Skip self
					const int&	nodeIdJ	= nodeIds[j];

					if ( !pI->GetHasAdj(nodeIdJ) ) continue;	// Skip non-adjacent nodes
					if ( iColorId == GetColorId(nodeIdJ) ) cost++;
				}
				if ( cost < minCost ) { minCost = cost;	bestColorId = iColorId; }	// Update bestColorId
			}
			m_mapNodeIdToColorId[nodeIdI] = bestColorId;
			cnList[bestColorId].push_back(nodeIdI);	// Update list
			iStartColorId = bestColorId;
		}
		// Handle case when we have unused colors
		while ( true )
		{
			// Find first unused colorId, and the most used colorId (used more than once)
			int iMostUsedColor(BAD_COLORID), iUnusedColor(BAD_COLORID);
			size_t nMaxCount(1);
			for (int i = 0; i < MYNUMCOLORS; i++)
			{
				if ( cnList[i].size() > nMaxCount ) { iMostUsedColor = i; nMaxCount = cnList[i].size(); }
				if ( cnList[i].size() == 0 && iUnusedColor == BAD_COLORID) iUnusedColor = i;
			}
			if ( iUnusedColor == BAD_COLORID ) break;		// All colors used
			if ( iMostUsedColor == BAD_COLORID ) break;		// No color used more than once
			// Do the color swap
			auto iter = cnList[iMostUsedColor].begin(); ++iter;
			const int iNodeId = *iter;						// Pick second lowest nodeId
			m_mapNodeIdToColorId[iNodeId] = iUnusedColor;	// Give it the unused colorId
			cnList[iMostUsedColor].erase(iter);				// Remove nodeId from list of iMostUsedColor
			cnList[iUnusedColor].push_back(iNodeId);		// Add nodeId to list of iUnusedColor
		}
	}
	int GetColorId(const int& nodeId) const
	{
		if ( nodeId == BAD_NODEID ) return BAD_COLORID;
		const auto iter = m_mapNodeIdToColorId.find(nodeId);
		return ( iter != m_mapNodeIdToColorId.end() ) ? iter->second : BAD_COLORID;
	}
	QColor GetColorFromNodeId(const int& nodeId, bool bUseSaturation = true) const
	{
		// First check if the nodeId is in the custom list
		const auto iter = m_mapNodeIdToCustomColor.find(nodeId);
		if ( iter != m_mapNodeIdToCustomColor.end() )
		{
			int R(0), G(0), B(0);
			const MyRGB& rgb = iter->second;
			rgb.GetRGB(R, G, B);
			if ( bUseSaturation ) HandleSaturation(R, G, B);
			return QColor(R, G, B);
		}
		// Use auto-calculated colors
		return GetPixmapColor(GetColorId(nodeId), bUseSaturation);
	}
	MyRGB GetPixmapRGB(const int& colorId, bool bUseSaturation = true) const
	{
		int R(0), G(0), B(0);
		if		( colorId == BAD_COLORID )	{ R = G = B = 255; }
		else if ( colorId == MY_GREY )		{ R = G = B = 96; }
		else if ( colorId == MY_LYR_BOT )	{ G = 128; }
		else if ( colorId == MY_LYR_TOP )	{ G = 96; B = 192; }
		else if	( colorId >= 0 && colorId < MYNUMCOLORS )
		{
			const MyRGB& rgb = sm_color[colorId % MYNUMCOLORS];
			rgb.GetRGB(R, G, B);
			if ( bUseSaturation ) HandleSaturation(R, G, B);
		}
		else
		{
			assert(colorId == MY_BLACK);
		}
		MyRGB rgb;
		rgb.SetRGB(R, G, B);
		return rgb;
	}
	QColor GetPixmapColor(const int& colorId, bool bUseSaturation = true) const
	{
		return GetPixmapRGB(colorId, bUseSaturation).GetQColor();
	}
	void HandleSaturation(int& R, int&G, int& B) const
	{
		const int iA = (100 - m_iSaturation) * 255;
		if ( m_iFillSaturation == 0 )
		{
			R = ( iA + m_iSaturation * R ) / 100;
			G = ( iA + m_iSaturation * G ) / 100;
			B = ( iA + m_iSaturation * B ) / 100;
		}
		else
		{
			R = G = B = iA / 100;
		}
	}
	// Merge interface functions
	virtual void UpdateMergeOffsets(MergeOffsets& o) override
	{
		for (const auto& mapObj : m_mapNodeIdToCustomColor)
			if ( mapObj.first != BAD_NODEID ) o.deltaNodeId = std::max(o.deltaNodeId, mapObj.first  + 1);
	}
	virtual void ApplyMergeOffsets(const MergeOffsets& o) override
	{
		std::unordered_map<int,MyRGB> tmp;
		for (const auto& mapObj : m_mapNodeIdToCustomColor)
			tmp[mapObj.first + o.deltaNodeId] = mapObj.second;
		m_mapNodeIdToCustomColor.clear();
		m_mapNodeIdToCustomColor.insert(tmp.begin(), tmp.end());
	}
	void Merge(const ColorManager& src)
	{
		m_mapNodeIdToCustomColor.insert(src.m_mapNodeIdToCustomColor.begin(), src.m_mapNodeIdToCustomColor.end());
	}
	// Persist interface functions
	virtual void Load(DataStream& inStream) override
	{
		m_mapNodeIdToCustomColor.clear();
		unsigned int numNodeIds(0);
		inStream.Load(numNodeIds);
		for (unsigned int i = 0; i < numNodeIds; i++)
		{
			int iNodeId;
			MyRGB rgb;
			inStream.Load(iNodeId);
			rgb.Load(inStream);
			m_mapNodeIdToCustomColor[iNodeId] = rgb;
		}
	}
	virtual void Save(DataStream& outStream) override
	{
		const unsigned int numNodeIds = static_cast<unsigned int>( m_mapNodeIdToCustomColor.size() );
		outStream.Save(numNodeIds);
		for (auto& mapObj : m_mapNodeIdToCustomColor)
		{
			outStream.Save(mapObj.first);
			mapObj.second.Save(outStream);
		}
	}
private:
	static MyRGB					sm_color[MYNUMCOLORS];	// Hard-coded colors
	std::unordered_map<int,int>		m_mapNodeIdToColorId;
	std::unordered_map<int,MyRGB>	m_mapNodeIdToCustomColor;
	int								m_iSaturation;		// 0 to 100. At 0 the colors would all fade to white.
	int								m_iFillSaturation;	// 0 to 100. If non-zero then turn colors grey.
	bool							m_bReAssign;
};
