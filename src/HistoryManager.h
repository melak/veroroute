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

// Class to manage history files for undo/redo functionality
// An average vrt file could be around 75k.
// An upper limit of 1000 files has been set, so the max undo level is 999.
// This is to prevent the files using too much disk space.
// On Android, the upper limit has been reduced to 100 files.
//
// Each VeroRoute instance on a machine uses a different instance ID
// when writing to the shared history folder.
// The instance ID determines the filenames that the instance will use.
// An instance with an ID of "N" (where N >= 1) will write the following binary files to the history folder:
//
//	"circuit_N.log"		This stores the path to the user's vrt file (the user's last save)
//	"entries_N.log"		This stores a copy of the entries in the Undo/Redo list, and the current place within that list.
//	"history_N_0.vrt"
//	"history_N_1.vrt"
//  etc
//
//  There is a history vrt file for each entry in the Undo/Redo list.

Q_DECL_CONSTEXPR static const int HISTORY_VERSION_1 = 1;
Q_DECL_CONSTEXPR static const int HISTORY_VERSION_CURRENT = HISTORY_VERSION_1;

#include "Board.h"
#include "VeroRouteAndroid.h"
#include <QFile>
#include <QDateTime>

#ifdef VEROROUTE_ANDROID
	Q_DECL_CONSTEXPR static const unsigned int MAX_HISTORY_FILES = 100;
#else
	Q_DECL_CONSTEXPR static const unsigned int MAX_HISTORY_FILES = 1000;
#endif

typedef std::tuple<unsigned int, int, std::string>	HistoryItem;	// <index, object id, description>
typedef std::list<HistoryItem>::const_iterator		HistoryItemIter;

class HistoryManager
{
public:
	HistoryManager() {}
	~HistoryManager() { Clear(); }
	void SetPathStr(const std::string& str)		{ m_pathStr = str; }
	void SetInstanceID(unsigned int ID)			{ m_ID = ID; }
	bool GetIsLocked() const					{ return m_bLocked; }
	bool GetCanUndo() const						{ return !m_list.empty() && m_currentIter != m_list.begin(); }
	bool GetCanRedo() const						{ return !m_list.empty() && GetNextIter() != m_list.end();   }
	const std::string& GetUndoText() const		{ return std::get<2>(*m_currentIter); }
	const std::string& GetRedoText() const		{ return std::get<2>(*GetNextIter()); }
#ifdef VEROROUTE_ANDROID
	QString GetLastHistoryFile()	// Called at program startup (for crash recovery)
	{
		// Android should not run two instances of VeroRoute (so m_ID should always be 1).
		// The presence of files in the history folder at startup means VeroRoute did not shut down properly.

		// The history files form a circular buffer, and we want to find the youngest.
		QString		strLastHistoryFile;
		QDateTime	dateTimeLast;
		QFileInfo	fileInfo;
		for (unsigned int n = 0; n < MAX_HISTORY_FILES; n++)
		{
			QFile file( GetHistoryFilename(n) );
			if ( !file.exists() ) break;

			fileInfo.setFile( file );
			QDateTime dateTime = fileInfo.lastModified();

			if ( strLastHistoryFile.isEmpty() || ( dateTime.msecsTo(dateTimeLast) < 0) )
			{
				strLastHistoryFile	= file.fileName();
				dateTimeLast		= dateTime;
			}
		}
		return strLastHistoryFile;
	}
#endif
	bool Reset(const std::string& str, Board& board, const QString& lastFileName, int iTutorialNumber)
	{
		if ( m_bLocked ) return false;

		if ( m_ID != 0 ) Clear();	// If m_ID is valid, clear m_list and all files for that ID (history, circuit, entries)

		// Set a new (unique) m_ID
		for (m_ID = 1; m_ID < INT_MAX; m_ID++ )	// Keep increasing this until we get a unique ID
		{
			std::ifstream fTest( GetHistoryFilename(0) );	// See if (zeroth) filename with this ID exists
			if ( !fTest.good() ) break;						// ID is not already in use so break
		}

		AddEntry(0, BAD_COMPID, str);					// Updates the "entries.log" file for m_ID
		SaveCircuitFile(lastFileName, iTutorialNumber);	// Updates the "circuit.log" file for m_ID
		return Save(board);								// Updates one "history.vrt" file for m_ID
	}
	void LoadEntriesFile()	// Import info from "entries.log" for m_ID
	{
		m_list.clear();
		m_currentIter = m_list.begin();

		DataStream inStream(DataStream::READ);
		if ( inStream.Open( GetEntriesFileName() ) )
		{
			int iHistoryVersion(0);
			inStream.Load(iHistoryVersion);

			unsigned int listSize(0);
			inStream.Load(listSize);

			for (unsigned int index = 0; index < listSize; ++index)
			{
				unsigned int	tmp0;	inStream.Load(tmp0);
				int				tmp1;	inStream.Load(tmp1);
				std::string		tmp2;	inStream.Load(tmp2);
				m_list.push_back( HistoryItem(tmp0, tmp1, tmp2) );
			}

			unsigned int currentIterIndex(0);
			inStream.Load(currentIterIndex);

			unsigned int iterIndex(0);
			for (auto iter = m_list.begin(), iterEnd = m_list.end(); iter != iterEnd; ++iter, ++iterIndex)
			{
				if ( iterIndex == currentIterIndex )
				{
					m_currentIter = iter;
					break;
				}
			}
			inStream.Close();
		}
	}
	void SaveEntriesFile()	// Export info to "entries.log" file for m_ID
	{
		DataStream outStream(DataStream::WRITE);
		if ( outStream.Open( GetEntriesFileName() ) )
		{
			outStream.Save(HISTORY_VERSION_CURRENT);

			const unsigned int listSize = (unsigned int) m_list.size();
			outStream.Save(listSize);

			unsigned int currentIterIndex(0), iterIndex(0);
			for (auto iter = m_list.begin(), iterEnd = m_list.end(); iter != iterEnd; ++iter, ++iterIndex)
			{
				if ( iter == m_currentIter ) currentIterIndex = iterIndex;
				outStream.Save(std::get<0>(*iter));	// unsigned int
				outStream.Save(std::get<1>(*iter));	// int
				outStream.Save(std::get<2>(*iter));	// std::string
			}
			outStream.Save(currentIterIndex);
			outStream.Close();
		}
	}
	bool Update(const std::string& str, int objId, Board& board)
	{
		if ( m_bLocked ) return false;

		// For the current m_ID, we only need to update a "history.vrt" file and the "entries.log" file.
		// Calling 	Save()     will create/overwrite a "history.vrt" file.
		// Calling  AddEntry() will create/overwrite the "entries.log" file.

		assert(BAD_COMPID == -1 && BAD_ID == -1);
		if ( objId != -1 )	// objId is typically a "component ID" in layout mode, or a "shape ID" or "pin ID" in component editor mode.
		{					// Lots of other things force an objId == 0 (think of that as "dialog ID").
			if ( (std::get<1>(*m_currentIter) == objId ) &&	// If we're working on the same object as the current entry ...
				 (std::get<2>(*m_currentIter) == str ) )	// ... and have the same text field as the current entry ...
				return Save(board);							// ... then overwrite the current entry instead of adding a new one
		}

		// Update m_list
		auto iterNext = GetNextIter();
		if ( iterNext != m_list.end() )	// We're modifying within the list ...
		{
			// Delete later history files
			for (auto iter = iterNext; iter != m_list.end(); ++iter) remove( GetHistoryFilename(std::get<0>(*iter)) );
			m_list.erase(iterNext, m_list.end());	// Erase later history list items
		}
		else if ( (unsigned int)m_list.size() == MAX_HISTORY_FILES )	// We're at the end of the list and the list is full ...
		{
			// We'll overwrite the first history file, so no need to delete it
			m_list.erase(m_list.begin());	// Erase first history list item
		}
		AddEntry(std::get<0>(*m_currentIter) + 1, objId, str);

		return Save(board);
	}
	void Lock()		{ m_bLocked = true;  }
	void UnLock()	{ m_bLocked = false; }
	bool Undo(Board& board)
	{
		assert( m_bLocked );	// Should only Undo while the list is locked
		if ( !GetCanUndo() ) return false;
		--m_currentIter;
		return Load(board);
	}
	bool Redo(Board& board)
	{
		assert( m_bLocked );	// Should only Redo while the list is locked
		if ( !GetCanRedo() ) return false;
		++m_currentIter;
		return Load(board);
	}
	void Clear()
	{
		assert( !m_bLocked );
		for (unsigned int i = 0; i < MAX_HISTORY_FILES; i++) remove( GetHistoryFilename(i) );	// Delete all history files for the instance
		remove( GetEntriesFileName() );	// Delete the entries file for the instance
		remove( GetCircuitFileName() );	// Delete the circuit file for the instance
		m_list.clear();					// Clear the list
	}
	const char* GetCurrentHistoryFilename() const
	{
		return ( m_list.empty() ) ? "\0" : GetHistoryFilename(std::get<0>(*m_currentIter));
	}
	void LoadCircuitFile(QString& lastFileName, int& iTutorialNumber)
	{
		lastFileName.clear();
		iTutorialNumber = -1;
		DataStream inStream(DataStream::READ);
		if ( inStream.Open( GetCircuitFileName() ) )
		{
			int iHistoryVersion(0);
			inStream.Load(iHistoryVersion);
			inStream.Load(lastFileName);
			inStream.Load(iTutorialNumber);
			inStream.Close();
		}
	}
	void SaveCircuitFile(const QString& lastFileName, int iTutorialNumber)
	{
		DataStream outStream(DataStream::WRITE);
		if ( outStream.Open( GetCircuitFileName() ) )
		{
			outStream.Save(HISTORY_VERSION_CURRENT);
			outStream.Save(lastFileName);
			outStream.Save(iTutorialNumber);
			outStream.Close();
		}
	}
private:
	void AddEntry(unsigned int index, int objId, const std::string& str)
	{
		m_list.push_back( HistoryItem(index % MAX_HISTORY_FILES, objId, str) );
		m_currentIter = m_list.end();
		m_currentIter--;
		SaveEntriesFile();
	}
	bool Load(Board& board)
	{
		DataStream inStream(DataStream::READ);
		if ( !inStream.Open( GetCurrentHistoryFilename() ) ) return false;
		board.Load(inStream);
		inStream.Close();
		return inStream.GetOK();
	}
	bool Save(Board& board)
	{
		DataStream outStream(DataStream::WRITE);
		if ( !outStream.Open( GetCurrentHistoryFilename() ) ) return false;
		board.Save(outStream);
		outStream.Close();
		return true;
	}
	HistoryItemIter GetNextIter() const { auto iter = m_currentIter; ++iter; return iter; }
	const char* GetHistoryFilename(unsigned int index) const
	{
		assert( index < MAX_HISTORY_FILES );	// Sanity check
		memset(m_buffer, 0, 256 * sizeof(char));
		sprintf(m_buffer, "%s/history/history_%u_%u.vrt", m_pathStr.c_str(), m_ID, index);
		return m_buffer;
	}
	const char* GetCircuitFileName() const	// A single file containing the circuit name per VeroRoute instance
	{
		memset(m_buffer, 0, 256 * sizeof(char));
		sprintf(m_buffer, "%s/history/circuit_%u.log", m_pathStr.c_str(), m_ID);
		return m_buffer;
	}
	const char* GetEntriesFileName() const	// A single file containing the undo/redo entries per VeroRoute instance
	{
		memset(m_buffer, 0, 256 * sizeof(char));
		sprintf(m_buffer, "%s/history/entries_%u.log", m_pathStr.c_str(), m_ID);
		return m_buffer;
	}
private:
	std::string				m_pathStr;			// Path to the "history" folder
	unsigned int			m_ID = 0;			// Each VeroRoute instance has a unique m_ID > 0
	bool					m_bLocked = false;	// Should lock the list while doing Undo() or Redo()
	std::list<HistoryItem>	m_list;				// Each history item is an <Index, Description> pair
	HistoryItemIter			m_currentIter;		// Points to the last written history item
	mutable char			m_buffer[256];		// For constructing history filenames
};
