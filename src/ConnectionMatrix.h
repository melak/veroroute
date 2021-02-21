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

// Keeps track of connectivity between a set of target pins in the routing algorithm.

class ConnectionMatrix
{
public:
	ConnectionMatrix() {}
	~ConnectionMatrix() { DeAllocate(); }
	void Allocate(const size_t& numTargetPins)
	{
		m_N				= numTargetPins;
		const size_t N2	= m_N * m_N;
		m_pConn			= new bool[N2];
		m_ppConn		= new bool*[m_N];
		memset(m_pConn, 0, N2 * sizeof(bool));
		for (size_t i = 0; i < m_N; i++) m_ppConn[i] = m_pConn + i * m_N;
		for (size_t i = 0; i < m_N; i++) m_ppConn[i][i] = true;	// Each pin is connected to itself
		m_cost = (unsigned int)(N2 - m_N);	// Cost = number of false values in the connection matrix
	}
	void DeAllocate()
	{
		if ( m_ppConn ) delete[] m_ppConn;	m_ppConn	= nullptr;
		if ( m_pConn  ) delete[] m_pConn;	m_pConn		= nullptr;
	}
	void Connect(const size_t& j, const size_t& k)
	{
		// Make j-k connection and enforce transitivity

		typedef std::pair<unsigned int, unsigned int> CONNECTION;
		std::list<CONNECTION> list;		// Helper for updating the connection matrix
		list.push_back( CONNECTION(j,k) );
		while ( !list.empty() )
		{
			auto iter = list.begin();	// Read info from first list entry ...
			const auto a = iter->first;
			const auto b = iter->second;
			list.erase( iter );			// ... then remove the list entry

			if ( !m_ppConn[a][b] )	// If no a-b connection ...
			{
				m_ppConn[a][b] = m_ppConn[b][a] = true;	// Make a-b connection ...
				m_cost -= 2;							// Update cost
				for (unsigned int c = 0; c < m_N; c++)	// Update 1st-order transitive relations
				{
					if ( m_ppConn[a][c] )
					{
						if ( !m_ppConn[b][c] ) list.push_back( CONNECTION(b,c) );	// a-c connection ==> b-c connection
					}
					else
					{
						if (  m_ppConn[b][c] ) list.push_back( CONNECTION(a,c) );	// b-c connection ==> a-c connection
					}
				}
			}
		}
	}
	const bool& GetAreConnected(const size_t& j, const size_t& k) const { return m_ppConn[j][k]; }
	const unsigned int&	GetCost() const { return m_cost; }
private:
	size_t			m_N			= 0;		// Number of target pins
	bool*			m_pConn		= nullptr;	//
	bool**			m_ppConn	= nullptr;	// m_ppConn[j][k] is true if pins j and k are connected
	unsigned int	m_cost		= UINT_MAX; // Zero ==> all target pins are connected
};
