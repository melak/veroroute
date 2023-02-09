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
	typedef std::pair<QPointF, QPointF>	LINE;

	static inline void Build(const std::list<QPointF>& pointsIn, std::list<LINE>& linesOut, bool bDaisyChain = false)
	{
		typedef std::pair<size_t, size_t>	INDICES;
		typedef std::pair<INDICES, qreal>	EDGE;

		linesOut.clear();

		const size_t N = pointsIn.size();
		if ( N < 2 ) return;

		std::vector<size_t>		nConn;	nConn.resize(N,0);	// Number of direct connections to each point (for daisy chain algorithm)
		std::vector<QPointF>	v;		v.resize(N);		// Points stored as a vector (for access via index)
		size_t i(0);
		for (const auto& o: pointsIn) v[i++] = o;

		std::list<EDGE> edges;	// Working list of edges
		for (size_t i = 0; i < N; i++)
			for (size_t j = i + 1; j < N; j++)
				edges.push_back( EDGE(INDICES(i,j), PolygonHelper::Length(v[i] - v[j])) );

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

	typedef std::pair<QPointF, unsigned int>		AIRWIRE_POINT;	// unsigned int holds the Route ID for the point
	typedef std::pair<AIRWIRE_POINT, AIRWIRE_POINT>	AIRWIRE_LINE;

	static inline void BuildAirWires(const std::list<AIRWIRE_POINT>& pointsIn, std::list<AIRWIRE_LINE>& linesOut)
	{
		typedef std::pair<AIRWIRE_LINE, qreal>	AIRWIRE_EDGE;	// qreal holds the length of the AIRWIRE_LINE

		linesOut.clear();

		const size_t N = pointsIn.size();
		if ( N < 2 ) return;

		std::vector<AIRWIRE_POINT>	v;	v.resize(N);	// Points stored as a vector (for access via index)
		size_t i(0);
		for (const auto& o: pointsIn) v[i++] = o;

		std::unordered_map<size_t, size_t> mapRIDtoIndex;	// Map Route IDs to consecutive indexes 0,1,2,... so we can use a ConnectionMatrix
		size_t index(0);	// For populating mapRIDtoIndex

		std::list<AIRWIRE_EDGE> edges;	// Working list of edges.  An edge is an AIRWIRE_LINE plus its calculated length.
		for (size_t i = 0; i < N; i++)
			for (size_t j = i + 1; j < N; j++)
			{
				const unsigned int& RID_i = v[i].second;
				const unsigned int& RID_j = v[j].second;
				if ( RID_i != RID_j ) // Endpoints of edge must have different route IDs
				{
					edges.push_back( AIRWIRE_EDGE( AIRWIRE_LINE(v[i], v[j]), PolygonHelper::Length(v[i].first - v[j].first) ) );

					// Update map of RID to indexes 0,1,2, ...
					if ( mapRIDtoIndex.find(RID_i) == mapRIDtoIndex.end() ) mapRIDtoIndex[RID_i] = index++;
					if ( mapRIDtoIndex.find(RID_j) == mapRIDtoIndex.end() ) mapRIDtoIndex[RID_j] = index++;
				}
			}

		const size_t numRIDs = mapRIDtoIndex.size();	// Number of unique route IDs
		if ( numRIDs < 2 ) return;

		ConnectionMatrix matrix;	// Helper for tracking connectivity between points
		matrix.Allocate(numRIDs);

		while ( linesOut.size() < numRIDs-1 )
		{
			qreal Dmin(DBL_MAX);
			auto iterBest = edges.begin();	// The shortest edge that does not make an unnecessary connection
			for (auto iter = iterBest, iterEnd = edges.end(); iter != iterEnd; ++iter)
			{
				const AIRWIRE_LINE&	line_ij	= iter->first;
				const unsigned int&	RID_i	= line_ij.first.second;
				const unsigned int&	RID_j	= line_ij.second.second;
				const qreal&		D		= iter->second;
				if ( D > Dmin || matrix.GetAreConnected(mapRIDtoIndex[RID_i], mapRIDtoIndex[RID_j]) ) continue;
				iterBest	= iter;
				Dmin		= D;
			}
			const AIRWIRE_LINE&	line_ij	= iterBest->first;
			const unsigned int&	RID_i	= line_ij.first.second;
			const unsigned int&	RID_j	= line_ij.second.second;
			linesOut.push_back( line_ij );	// Add best line to linesOut
			matrix.Connect(mapRIDtoIndex[RID_i], mapRIDtoIndex[RID_j]);	// Update connection matrix
			edges.erase(iterBest);			// Remove best from the working list
		}
	}

	SpanningTreeHelper() { assert( true || PreventBuildWarnings() ); }
private:
	bool PreventBuildWarnings() const
	{
		std::list<QPointF>			inA;
		std::list<LINE>				outA;
		std::list<AIRWIRE_POINT>	inB;
		std::list<AIRWIRE_LINE>		outB;
		Build(inA, outA);
		BuildAirWires(inB, outB);
		return true;
	}
};
