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

#include "Common.h"
#include "ConnectionMatrix.h"
#include "PolygonHelper.h"

// Builds a minimal spanning tree or daisy chain between a set of points

struct SpanningTreeHelper
{
	typedef std::pair<QPointF, unsigned int>	POINT;	// unsigned int is an optional attribute for the point
	typedef std::pair<POINT, POINT>				LINE;

	static inline void Build(const std::list<POINT>& pointsIn, std::list<LINE>& linesOut, const bool& bDaisyChain = false)
	{
		typedef std::pair<size_t, size_t>	INDICES;
		typedef std::pair<INDICES, qreal>	EDGE;

		linesOut.clear();

		const size_t N = pointsIn.size();
		if ( N < 2 ) return;

		std::vector<size_t>		nConn;	nConn.resize(N,0);	// Number of direct connections to each point (for daisy chain algorithm)
		std::vector<POINT>		v;		v.resize(N);		// Points stored as a vector (for access via index)
		size_t i(0);
		for (const auto& o: pointsIn) v[i++] = o;

		std::list<EDGE> edges;	// Working list of edges
		for (size_t i = 0; i < N; i++)
			for (size_t j = i + 1; j < N; j++)
				edges.push_back( EDGE(INDICES(i,j), PolygonHelper::Length(v[i].first - v[j].first)) );

		ConnectionMatrix matrix;	// Helper for tracking connectivity between points
		matrix.Allocate(N);

		while ( linesOut.size() < N-1 )
		{
			qreal Dmin(DBL_MAX);
			auto iterBest = edges.begin();	// The shortest edge that does not make an unnecessary connection
			for (auto iter = iterBest, iterEnd = edges.end(); iter != iterEnd; ++iter)
			{
				const INDICES&	ij	= iter->first;
				const qreal&	D	= iter->second;
				if ( D > Dmin || matrix.GetAreConnected(ij.first, ij.second) ) continue;
				if ( bDaisyChain && (nConn[ij.first] > 1 || nConn[ij.second] > 1) ) continue;
				iterBest	= iter;
				Dmin		= D;
			}
			const INDICES& ij = iterBest->first;
			linesOut.push_back( LINE(v[ij.first], v[ij.second]) );	// Add best to output list
			matrix.Connect(ij.first, ij.second);					// Update connection matrix
			nConn[ij.first]++;	nConn[ij.second]++;					// Update number of direct connections
			edges.erase(iterBest);									// Remove best from the working list
		}
	}

	static inline void BuildAirWires(const std::list<POINT>& pointsIn, std::list<LINE>& linesOut)
	{
		std::list<LINE> lines;	// Working list of lines

		Build(pointsIn, lines);	// Build minimal spanning tree

		// Erase all LINEs that are between points with the same route ID
		for (auto iter = lines.begin(); iter != lines.end();)
			if ( iter->first.second == iter->second.second ) iter = lines.erase(iter); else ++iter;

		// Then map the Route IDs to consecutive indexes 0,1,2,... so we can use a ConnectionMatrix
		std::unordered_map<size_t, size_t> mapRIDtoIndex;
		size_t index(0);
		for (const auto& o : lines)
		{
			if ( mapRIDtoIndex.find(o.first.second)  == mapRIDtoIndex.end() ) mapRIDtoIndex[o.first.second]  = index++;
			if ( mapRIDtoIndex.find(o.second.second) == mapRIDtoIndex.end() ) mapRIDtoIndex[o.second.second] = index++;
		}

		const size_t N = mapRIDtoIndex.size();
		if ( N < 2 ) return;

		std::list<qreal> lengths;	// Working list of lengths
		for (const auto& o : lines)
			lengths.push_back( PolygonHelper::Length(o.first.first - o.second.first) );

		ConnectionMatrix matrix;	// Helper for tracking connectivity between indices
		matrix.Allocate(N);

		while ( linesOut.size() < N-1 )
		{
			qreal Dmin(DBL_MAX);
			// Now have 2 iterators that go in step through the lists of lines and lengths
			auto iterLineBest	= lines.begin();	// The shortest edge that does not make an unnecessary connection
			auto iterLine		= iterLineBest;
			auto iterLineEnd	= lines.end();
			auto iterLengthBest	= lengths.begin();
			auto iterLength		= iterLengthBest;
			auto iterLengthEnd	= lengths.end();
			for (; iterLine != iterLineEnd && iterLength != iterLengthEnd; ++iterLine, ++iterLength )
			{
				const qreal& D	= *iterLength;
				if ( D > Dmin || matrix.GetAreConnected(mapRIDtoIndex[iterLine->first.second], mapRIDtoIndex[iterLine->second.second]) ) continue;
				iterLineBest	= iterLine;
				iterLengthBest	= iterLength;
				Dmin			= D;
			}
			linesOut.push_back( *iterLineBest );	// Add best to output list
			matrix.Connect(mapRIDtoIndex[iterLineBest->first.second], mapRIDtoIndex[iterLineBest->second.second]);	// Update connection matrix
			lines.erase(iterLineBest);				// Remove best from the working list of lines
			lengths.erase(iterLengthBest);			// ... and from the working list of lengths
		}
	}

	SpanningTreeHelper() { assert( true || PreventBuildWarnings() ); }
private:
	bool PreventBuildWarnings() const
	{
		std::list<POINT>	in;
		std::list<LINE>		out;
		Build(in, out);
		return true;
	}
};
