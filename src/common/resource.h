#pragma once

#include "common.h"
#include <fstream>
#include <sstream>

class Resource
{
public:
	/// Check if the extension of file_path is equal to given ext
	static bool CheckExtension( const std::string &file_path, const std::string &ext );


	/// Open and read a file into a buffer. 
	/// buffer should be NULL when given, it is allocated in the function
	/// user of function should free the buffer when done with it
	static bool ReadFile( std::string &buf, const std::string &file_path );
};


/// Handler for a JSON File used by cJSON. Used to facilitate access
struct Json
{
	Json() : root( NULL ), opened( false ) {}
	~Json() { Close(); }

	bool Open( const std::string &file_path );
	void Close();

	static int ReadInt( cJSON *parent, const std::string &name, int default_val );
	static f32 ReadFloat( cJSON *parent, const std::string &name, f32 default_val );
	static std::string ReadString( cJSON *parent, const std::string &name,
		const std::string &default_val );
	static vec3f ReadVec3( cJSON *parent, const std::string &name, const vec3f &default_val );

	cJSON *root;
	std::string	contents;
	bool opened;
};

struct CSV
{
	enum OpenType
	{
		OpenRead,
		OpenWrite
	};

	enum
	{
		EndOfCells
	};

	std::fstream file;
	std::string fileName;
	u32 size;
	OpenType type;

	CSV() : writePrecision( 1 ), cellPos( 0 ), cellCount( 0 ) {}
	bool Open( const std::string &file, OpenType type );
	void Close();

	void ReadCells();

	void SetPrecision( int n );
	int GetSize();
	u32 GetCellCount() const { return (u32) cells.size(); }

	template<typename T>
	void WriteCell( T t );

	void WriteNewLine() { file << "\n"; }

	template<typename T>
	T ReadNextCell();

	std::vector<std::string> cells;
	std::vector<int> lineCells;		//!< Number of cells for each line
private:
	int writePrecision;
	u32 cellPos;
	u32 cellCount;
};

template<typename T>
inline void CSV::WriteCell( T t )
{
	file << t << ",";
}

template<typename T>
inline T CSV::ReadNextCell()
{
	if ( cellPos < cellCount )
	{
		return cells[cellPos++];
	}
	else
	{
		return EndOfCells;
	}
}
