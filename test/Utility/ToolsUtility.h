#pragma once

#include <string>
#include <vector>
#include "Common/Simd/Simd.h"
#include "IMGUI/DebugGUIDefines.h"

#if DEBUG_GUI_ENABLED
namespace Terrain 
{ 
	struct TileKey;

	namespace Generation { struct RoomKey; }
}

namespace ToolsUtility
{
	extern bool make_paths_relative;

	std::wstring FindTextEditor();
	void OpenInTextEditor( const std::wstring& filename );
	void OpenWith( const std::wstring& filename, const std::wstring& program );
	void OpenContainingFolder( const std::wstring& filename );

	void FileMenu( const std::wstring& filename );
	void SubversionMenu( const std::wstring& filename );
	void FileContextMenu( const std::wstring& filename, const char* str_id = nullptr );

	std::vector< simd::color > GetRandomColours();

	void MakeRelativePath( std::wstring& path ); //make relative to Art or Metadata

	inline void MakeRelativePaths( std::vector< std::wstring >& paths ) { for( auto& path : paths ) MakeRelativePath( path ); }

	//these functions will set the special .mobile/.china loading flags if necessary
	std::wstring GetOpenFilename( const wchar_t* filter, std::wstring& last_open_dir, const bool add_to_recently_used = true );
	std::vector< std::wstring > GetOpenFilenames( const wchar_t* filter, std::wstring& last_open_dir, const bool add_to_recently_used = true );

	inline std::wstring GetOpenFilename( const wchar_t* filter ) { std::wstring ignore_last_open_dir; return GetOpenFilename( filter, ignore_last_open_dir ); }
	inline std::vector< std::wstring > GetOpenFilenames( const wchar_t* filter ) { std::wstring ignore_last_open_dir; return GetOpenFilenames( filter, ignore_last_open_dir ); }

	std::wstring GetSaveFilename( const wchar_t* filter, const wchar_t* initial_dir = nullptr, const bool warn_on_overwrite = true );
	bool GetSaveFilename( const wchar_t* filter, std::wstring& filename_in_out, const wchar_t* initial_dir = nullptr, const bool warn_on_overwrite = true );

	std::vector< std::wstring > GetSaveFilenames( const wchar_t* filter, const wchar_t* initial_dir = nullptr, const bool warn_on_overwrite = true );

	void RenderTileKeyVisualisation( const Terrain::TileKey& tile_key, const std::string& feature_name, const int storey, const bool display_storey, const int camera_rotation = 0 );
	void RenderRoomKeyVisualisation( const Terrain::Generation::RoomKey& room_key, const std::string& feature_name );
};

#endif
