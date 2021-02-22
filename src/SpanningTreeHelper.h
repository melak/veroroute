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

// Builds a minimal spanning tree between a set of points

struct SpanningTreeHelper
{
	typedef std::pair<QPointF, QPointF>	EDGE;
	typedef std::pair<size_t, size_t>	EDGE_INDICES;

	static void Build(const std::list<QPointF>& pointsIn, std::list<EDGE>& edgesOut)
	{
		edgesOut.clear();

		const size_t N = pointsIn.size();
		if ( N < 2 ) return;

		std::vector<QPointF> v;	v.resize(N);	// Points stored as a vector (for access via index)
		size_t i(0);
		for (auto& o: pointsIn) v[i++] = o;

		std::list<EDGE_INDICES> edges;	// Working list of edges
		for (size_t i = 0; i < N; i++)
			for (size_t j = i + 1; j < N; j++)
				edges.push_back( EDGE_INDICES(i,j) );

		ConnectionMatrix matrix;	// Helper for tracking connectivity between points
		matrix.Allocate(N);

		while ( edgesOut.size() < N-1 )
		{
			double Dmin(DBL_MAX);
			EDGE_INDICES best;	// The shortest edge that does not make an unnecessary connection
			for (auto& edge : edges)
			{
				const double D = PolygonHelper::Length( v[edge.first] - v[edge.second] );
				if ( D < Dmin && !matrix.GetAreConnected(edge.first, edge.second) )
				{
					best	= edge;
					Dmin	= D;
				}
			}
			edgesOut.push_back( EDGE(v[best.first], v[best.second]) );	// Add best to output list
			matrix.Connect(best.first, best.second);					// Update connection matrix
			auto iter = std::find(edges.begin(), edges.end(), best);	// Remove best edge from the working list of edges
			if ( iter != edges.end() ) edges.erase(iter);
		}
	}

	SpanningTreeHelper() { assert( true || PreventBuildWarnings() ); }
private:
	bool PreventBuildWarnings() const
	{
		std::list<QPointF>	in;
		std::list<EDGE>		out;
		Build(in, out);
		return true;
	}
};
