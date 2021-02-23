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

	static inline void Build(const std::list<QPointF>& pointsIn, std::list<LINE>& linesOut, const bool& bDaisyChain = false)
	{
		typedef std::pair<size_t, size_t>	INDICES;
		typedef std::pair<INDICES, qreal>	EDGE;

		linesOut.clear();

		const size_t N = pointsIn.size();
		if ( N < 2 ) return;

		std::vector<size_t>		nConn;	nConn.resize(N,0);	// Number of direct connections to each point
		std::vector<QPointF>	v;		v.resize(N);		// Points stored as a vector (for access via index)
		size_t i(0);
		for (auto& o: pointsIn) v[i++] = o;

		std::list<EDGE> edges;	// Working list of edges
		for (size_t i = 0; i < N; i++)
			for (size_t j = i + 1; j < N; j++)
				edges.push_back( EDGE(INDICES(i,j), PolygonHelper::Length(v[i]-v[j])) );

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

	SpanningTreeHelper() { assert( true || PreventBuildWarnings() ); }
private:
	bool PreventBuildWarnings() const
	{
		std::list<QPointF>	in;
		std::list<LINE>		out;
		Build(in, out);
		return true;
	}
};
