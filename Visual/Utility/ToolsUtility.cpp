#include "ToolsUtility.h"

#if DEBUG_GUI_ENABLED

#if defined(WIN32) && !defined(CONSOLE)
#	include <filesystem>
#endif

#include "Common/File/FileSystem.h"
#include "Common/File/InputFileStream.h"
#include "Common/File/PathHelper.h"
#include "Common/Utility/Clipboard.h"
#include "Common/Utility/ContainerOperations.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/Format.h"
#include "Common/Utility/CFlags.h"

#include "ClientInstanceCommon/Terrain/TileDescription.h"
#include "ClientInstanceCommon/Terrain/TerrainEdgeType.h"
#include "ClientInstanceCommon/Terrain/Generation/Room.h"

#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/imgui/imgui_ex.h"
#include "Visual/Utility/CommonDialogs.h"

//#define NO_OPTIMISE
#ifdef NO_OPTIMISE
#pragma optimize( "", off )
#endif

namespace ToolsUtility
{
	bool make_paths_relative = true;

#if defined(WIN32) && !defined(CONSOLE)
	std::wstring FindTextEditor()
	{
		// Prefer Sublime Text/Notepad++
		for( const auto& program : { "Sublime Text/sublime_text.exe", "Sublime Text 3/sublime_text.exe", "Notepad++/notepad++.exe" } )
		{
			// Prefer x64
			for( const auto& path : { "Program Files", "Program Files (x86)" } )
			{
				// Drive should be C, but should test all drive letters
				for( const auto& drive : "CABDEFGHIJKLMNOPQRSTUVWXYZ" )
				{
					std::filesystem::path test_path = Utility::safe_format( "{}:/{}/{}", drive, path, program );
					std::error_code e;
					if( std::filesystem::exists( test_path, e ) )
						return test_path.wstring();
				}
			}
		}
	
		return L"notepad.exe";
	}

	std::wstring FindTortoiseProc()
	{
		for( const auto& path : { "Program Files", "Program Files (x86)" } )
		{
			for( const auto& drive : "CABDEFGHIJKLMNOPQRSTUVWXYZ" )
			{
				std::filesystem::path test_path = Utility::safe_format( "{}:/{}/TortoiseSVN/bin/TortoiseProc.exe", drive, path );
				std::error_code e;
				if( std::filesystem::exists( test_path, e ) )
					return test_path.wstring();
			}
		}

		return L"TortoiseProc.exe";
	}

	std::wstring GetQuotedPath( const std::wstring& filename )
	{
		if( PathHelper::PathIsRelative( filename ) )
			return L'"' + PathHelper::NormalisePath(FileSystem::GetCurrentDirectory() + L'/' + filename, L'\\') + L'"';
		else
			return L'"' + PathHelper::NormalisePath(filename, L'\\') + L'"';
	}
#endif

	void OpenInTextEditor( const std::wstring& filename )
	{
#if defined(WIN32) && !defined(CONSOLE)
		static const std::wstring text_editor_path = FindTextEditor();
	#ifdef CHECK_LOCAL_FILE_ALIASES
		if( File::check_local_mobile_files )
		{
			const std::wstring mobile_filename = File::InjectName( filename, L".mobile" );
			if( FileSystem::FileExists( mobile_filename ) )
			{
				FileSystem::RunProgram( text_editor_path, GetQuotedPath( mobile_filename ) );
				return;
			}
		}
	#endif
		FileSystem::RunProgram( text_editor_path, GetQuotedPath( filename ) );
#endif
	}

	void RunTortoiseProc( const std::wstring& filename, const std::wstring& command )
	{
#if defined(WIN32) && !defined(CONSOLE)
		static const std::wstring tortoise_proc_path = FindTortoiseProc();
	#ifdef CHECK_LOCAL_FILE_ALIASES
		if( File::check_local_mobile_files )
		{
			const std::wstring mobile_filename = File::InjectName( filename, L".mobile" );
			if( FileSystem::FileExists( mobile_filename ) )
			{
				FileSystem::RunProgram( tortoise_proc_path, L"/command:" + command + L" /path:" + GetQuotedPath( mobile_filename ) );
				return;
			}
		}
	#endif
		FileSystem::RunProgram( tortoise_proc_path, L"/command:" + command + L" /path:" + GetQuotedPath( filename ) );
#endif
	}

	void OpenWith( const std::wstring& filename, const std::wstring& program )
	{
#if defined(WIN32) && !defined(CONSOLE)
	#ifdef CHECK_LOCAL_FILE_ALIASES
		if( File::check_local_mobile_files )
		{
			const std::wstring mobile_filename = File::InjectName( filename, L".mobile" );
			if( FileSystem::FileExists( mobile_filename ) )
			{
				FileSystem::RunProgram( program, GetQuotedPath( mobile_filename ) );
				return;
			}
		}
	#endif
		FileSystem::RunProgram( program, GetQuotedPath( filename ) );
#endif
	}

	void OpenContainingFolder( const std::wstring& filename )
	{
#if defined(WIN32) && !defined(CONSOLE)
		std::wstring path = GetQuotedPath( filename );
	#ifdef CHECK_LOCAL_FILE_ALIASES
		if ( File::check_local_mobile_files )
		{
			const std::wstring mobile_filename = File::InjectName( filename, L".mobile" );
			if ( FileSystem::FileExists( mobile_filename ) )
				path = GetQuotedPath( mobile_filename );
		}
	#endif
		const std::wstring params = L"/select, " + path;
		ShellExecute( nullptr, nullptr, L"explorer.exe", params.c_str(), nullptr, SW_SHOW );
#endif
	}

	void FileMenu( const std::wstring& filename )
	{
		enum OpenWithTools {
			AssetViewer = 0,
			RoomEditor,
			GraphEditor,
			TileTestbed,
			NumOpenWithTools
		};

		static const std::wstring executable_names[ NumOpenWithTools ] = {
			L"AssetViewer_x64.exe",
			L"RoomEditor_x64.exe",
			L"GraphEditor_x64.exe",
			L"TileTestbed_x64.exe",
		};

		static const std::string display_names[ NumOpenWithTools ] = {
			"Asset Viewer",
			"Room Editor",
			"Graph Editor",
			"Tile Testbed",
		};

		static const std::unordered_map< std::wstring, std::vector< OpenWithTools > > open_with = {
			{ L".arm", { RoomEditor } },
			{ L".red", { RoomEditor } },
			{ L".tgr", { GraphEditor, TileTestbed } },
			{ L".dgr", { GraphEditor, TileTestbed } },
			{ L".tdt", { AssetViewer } },
			{ L".tgt", { AssetViewer } },
			{ L".ao",  { AssetViewer } },
		};

		const std::wstring ext = PathHelper::GetExtension( filename );

		const auto it = open_with.find( ext );
		if( it != open_with.end() )
			for( const auto& program : it->second )
				if( ImGui::MenuItem( ( "Open with " + display_names[ program ] ).c_str() ) )
					OpenWith( filename, executable_names[ program ] );

		if( ext == L".ao" )
		{
			if( ImGui::MenuItem( "Edit .ao" ) )
				OpenInTextEditor( filename );

			if( ImGui::MenuItem( "Edit .aoc" ) )
				OpenInTextEditor( filename + L"c" );
		}
		else if( ext == L".ot" )
		{
			if( ImGui::MenuItem( "Edit .ot" ) )
				OpenInTextEditor( filename );

			if( ImGui::MenuItem( "Edit .otc" ) )
				OpenInTextEditor( filename + L"c" );

			if( ImGui::MenuItem( "Edit .ots" ) )
				OpenInTextEditor( filename + L"s" );
		}
		else if( ImGui::MenuItem( "Edit File" ) )
			OpenInTextEditor( filename );

		if( ImGui::MenuItem( "Copy File Path" ) )
			CopyTextToClipboard( filename );

		if( ImGui::MenuItem( "Open Containing Folder" ) )
			OpenContainingFolder( filename );

		SubversionMenu( filename );
	}

	void SubversionMenu( const std::wstring& filename )
	{
		if( ImGui::BeginMenu( "SVN" ) )
		{
			if( ImGui::MenuItem( "Diff" ) )
				RunTortoiseProc( filename, L"diff" );
			if( ImGui::MenuItem( "Log" ) )
				RunTortoiseProc( filename, L"log" );
			if( ImGui::MenuItem( "Revert" ) )
				RunTortoiseProc( filename, L"revert" );
			if( ImGui::MenuItem( "Blame" ) )
				RunTortoiseProc( filename, L"blame" );
			if( ImGui::MenuItem( "Update" ) )
				RunTortoiseProc( filename, L"update" );
			if( ImGui::MenuItem( "Commit" ) )
				RunTortoiseProc( filename, L"commit" );

			ImGui::EndMenu();
		}
	}

	void FileContextMenu( const std::wstring& filename, const char* str_id )
	{
		if( ImGui::BeginPopupContextItem( str_id ) )
		{
			FileMenu( filename );
			ImGui::EndPopup();
		}
	}

	std::vector< simd::color > GetRandomColours()
	{
		std::vector< simd::color > colours;
		try
		{
			File::InputFileStream stream( L"Tools/Common/random_colours.txt" );
			std::wstring str;
			while( stream >> str )
			{
				if( str.length() != 9 )
					continue;

				const auto colour = wcstoul( str.c_str() + 1, nullptr, 16 );
				if( colour == 0 )
					continue;

				const auto r = colour >> 24;
				const auto g = ( colour >> 16 ) & 0xFF;
				const auto b = ( colour >> 8 ) & 0xFF;
				const auto a = colour & 0xFF;
				colours.emplace_back( r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f );
			}
		}
		catch( ... ) {}

		if( colours.empty() )
		{
			colours.emplace_back( 1.0f, 0.0f, 0.0f, 1.0f );
			colours.emplace_back( 1.0f, 0.5f, 0.0f, 1.0f );
			colours.emplace_back( 1.0f, 1.0f, 0.0f, 1.0f );
			colours.emplace_back( 1.0f, 1.0f, 0.0f, 1.0f );
			colours.emplace_back( 0.5f, 1.0f, 0.0f, 1.0f );
			colours.emplace_back( 0.0f, 1.0f, 0.5f, 1.0f );
			colours.emplace_back( 0.0f, 1.0f, 1.0f, 1.0f );
			colours.emplace_back( 0.0f, 0.5f, 1.0f, 1.0f );
			colours.emplace_back( 0.0f, 0.0f, 1.0f, 1.0f );
			colours.emplace_back( 0.5f, 0.0f, 1.0f, 1.0f );
			colours.emplace_back( 1.0f, 0.0f, 1.0f, 1.0f );
		}

		return colours;
	}

	std::wstring GetOpenFilenameInternal( const wchar_t* filter, std::wstring& last_open_dir, const bool add_to_recently_used )
	{
		return Utility::GetFilenameBox( filter, last_open_dir, add_to_recently_used );
	}

	std::vector< std::wstring > GetOpenFilenamesInternal( const wchar_t* filter, std::wstring& last_open_dir, const bool add_to_recently_used )
	{
		return Utility::GetFilenamesBox( filter, last_open_dir, add_to_recently_used );
	}

	bool CheckFileOverride( std::wstring& filename, const std::wstring_view file_override_type, const bool remove_override = true )
	{
		const auto found = CaseInsensitiveFind( filename, file_override_type );
		if( found == -1 || found >= filename.length() || found + file_override_type.length() >= filename.length() )
			return false;

		if( remove_override )
		{
			assert( BeginsWith( file_override_type, L"." ) && EndsWith( file_override_type, L"." ) );
			const auto type_begin = filename.begin() + found;
			const auto type_end = type_begin + file_override_type.length();
			std::copy( type_end, filename.end(), type_begin + 1 );
			filename.resize( filename.size() - file_override_type.length() + 1 );
		}
		return true;
	}

	bool CheckFileOverride( std::vector< std::wstring >& filenames, const std::wstring_view file_override_type, const bool remove_override = true )
	{
		bool changed = false;
		for( std::wstring& filename : filenames )
		{
			changed |= CheckFileOverride( filename, file_override_type, remove_override );
		}
		return changed;
	}

	void MakeRelativePath( std::wstring& path ) //make relative to Art or Metadata
	{
		const auto art = path.find( L"/Art/" );
		if( art != path.npos )
		{
			path.erase( 0, art + 1 );
			return;
		}
		const auto metadata = path.find( L"/Metadata/" );
		if( metadata != path.npos )
		{
			path.erase( 0, metadata + 1 );
			return;
		}
	}

	std::wstring GetOpenFilename( const wchar_t* filter, std::wstring& last_open_dir, const bool add_to_recently_used )
	{
		std::wstring filename = GetOpenFilenameInternal( filter, last_open_dir, add_to_recently_used );
		if( make_paths_relative )
			MakeRelativePath( filename );
#ifdef CHECK_LOCAL_FILE_ALIASES
		if( CheckFileOverride( filename, L".mobile." ) && !File::check_local_mobile_files )
			File::check_local_mobile_files = true;
		if( CheckFileOverride( filename, L".china." ) && !File::check_local_china_files )
			File::check_local_china_files = true;
#endif
		return filename;
	}

	std::vector< std::wstring > GetOpenFilenames( const wchar_t* filter, std::wstring& last_open_dir, const bool add_to_recently_used )
	{
		std::vector< std::wstring > filenames = GetOpenFilenamesInternal( filter, last_open_dir, add_to_recently_used );
		if( make_paths_relative )
			MakeRelativePaths( filenames );
#ifdef CHECK_LOCAL_FILE_ALIASES
		if( CheckFileOverride( filenames, L".mobile." ) && !File::check_local_mobile_files )
			File::check_local_mobile_files = true;
		if( CheckFileOverride( filenames, L".china." ) && !File::check_local_china_files )
			File::check_local_china_files = true;
#endif
		return filenames;
	}

	bool GetSaveFilenameInternal( const wchar_t* filter, std::wstring& filename_in_out, const wchar_t* initial_dir, const bool warn_on_overwrite )
	{
		return Utility::SaveFilenameBox( filter, filename_in_out, initial_dir, warn_on_overwrite );
	}

	std::wstring GetSaveFilename( const wchar_t* filter, const wchar_t* initial_dir, const bool warn_on_overwrite )
	{
		std::wstring filename;
		if( GetSaveFilename( filter, filename, initial_dir, warn_on_overwrite ) )
			return filename;
		else
			return {};
	}

	bool GetSaveFilename( const wchar_t* filter, std::wstring& filename_in_out, const wchar_t* initial_dir, const bool warn_on_overwrite )
	{
		if( !GetSaveFilenameInternal( filter, filename_in_out, initial_dir, warn_on_overwrite ) )
			return false;
		if( make_paths_relative )
			MakeRelativePath( filename_in_out );
#ifdef CHECK_LOCAL_FILE_ALIASES
		if( !File::check_local_mobile_files && CheckFileOverride( filename_in_out, L".mobile.", false ) )
			File::check_local_mobile_files = true;
		if( !File::check_local_china_files && CheckFileOverride( filename_in_out, L".china.", false ) )
			File::check_local_china_files = true;
#endif
		return true;
	}

	std::vector< std::wstring > GetSaveFilenamesInternal( const wchar_t* filter, const wchar_t* initial_dir, const bool warn_on_overwrite )
	{
		return Utility::SaveFilenamesBox( filter, initial_dir, warn_on_overwrite );
	}

	std::vector< std::wstring > GetSaveFilenames( const wchar_t* filter, const wchar_t* initial_dir, const bool warn_on_overwrite )
	{
		std::vector< std::wstring > filenames = GetSaveFilenamesInternal( filter, initial_dir, warn_on_overwrite );
		if( make_paths_relative )
			MakeRelativePaths( filenames );
#ifdef CHECK_LOCAL_FILE_ALIASES
		if( !File::check_local_mobile_files && CheckFileOverride( filenames, L".mobile.", false ) )
			File::check_local_mobile_files = true;
		if( !File::check_local_china_files && CheckFileOverride( filenames, L".china.", false ) )
			File::check_local_china_files = true;
#endif
		return filenames;
	}

	std::string GetEdgeLabel( const Terrain::TerrainEdgeTypeHandle& edge_type, const unsigned char join_location, const unsigned char secondary_join_location )
	{
		if( edge_type.IsNull() )
			return "";
		else if( edge_type->IsVirtual() )
			return WstringToString( Utility::format( L"{} ({}/{})", edge_type->GetName(), join_location, secondary_join_location ) );
		else
			return WstringToString( Utility::format( L"{} ({})", edge_type->GetName(), join_location ) );
	}

	std::string GetEdgeHover( const Terrain::TerrainEdgeTypeHandle& edge_type, const unsigned char join_location, const unsigned char secondary_join_location )
	{
		if( edge_type.IsNull() )
			return "Edge Type: (null)";
		else if( edge_type->IsVirtual() )
			return WstringToString( Utility::format( L"Edge Type: {}\nConnection Points: {}/{}", edge_type.GetFilename(), join_location, secondary_join_location ) );
		else
			return WstringToString( Utility::format( L"Edge Type: {}\nConnection Point: {}", edge_type.GetFilename(), join_location ) );
	}

	void RenderTileKeyVisualisation( const Terrain::TileKey& tile_key, const std::string& feature_name, const int storey, const bool display_storey, const int camera_rotation )
	{
		const auto RenderGroundType = [&]( const unsigned corner )
		{
			ImGui::TableNextColumn();

			const auto& type = tile_key.ground_type[ corner ];
			const auto& storey = tile_key.storey_number[ corner ];

			ImGui::PushID( &type );
			if( type.IsNull() )
			{
				ImGuiEx::PushDisable();
				ImGui::Button( Utility::format( "({:d})", storey ).c_str(), ImVec2( -FLT_MIN, 0.0f ) );
				ImGuiEx::PopDisable();
				if( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
					ImGui::SetTooltip( "Ground Type: (wildcard)\nStorey: %hhi", storey );

			}
			else
			{
				const std::string label = Utility::format( "{} ({:d})", WstringToString( type->GetName() ), storey );

				if( ImGui::Button( label.c_str(), ImVec2( -FLT_MIN, 0.0f ) ) )
					ToolsUtility::OpenInTextEditor( type.GetFilename().c_str() );

				if( ImGui::IsItemHovered() )
					ImGui::SetTooltip( "Ground Type: %s\nStorey: %hhi", WstringToString( type.GetFilename() ).c_str(), storey );
			}
			ImGui::PopID();
	};

		const auto RenderEdgeType = [&]( const unsigned side )
		{
			ImGui::TableNextColumn();

			const auto& type = tile_key.edge_type[ side ];
			const auto& join_location1 = tile_key.edge_join_location[ side ];
			const auto& join_location2 = tile_key.secondary_join_location[ side ];

			ImGui::PushID( &type );

			const std::string label = GetEdgeLabel( type, join_location1, join_location2 );
			if( ImGui::Button( label.c_str(), ImVec2( -FLT_MIN, 0.0f ) ) && !type.IsNull() )
				ToolsUtility::OpenInTextEditor( type.GetFilename().c_str() );

			if( ImGui::IsItemHovered() )
				ImGui::SetTooltip( "%s", GetEdgeHover( type, join_location1, join_location2 ).c_str() );

			ImGui::PopID();
		};

		const auto RenderSizeAndFeatureName = [&]()
		{
			ImGui::TableNextColumn();

			if( display_storey )
			{
				if( feature_name.empty() )
					ImGui::Button( Utility::format( "{:d}x{:d} @ {:d}", (int)tile_key.x_size, (int)tile_key.y_size, storey ).c_str(), ImVec2( -FLT_MIN, 0.0f ) );
				else
					ImGui::Button( Utility::format( "{:d}x{:d} @ {:d}: \"{}\"", (int)tile_key.x_size, (int)tile_key.y_size, storey, feature_name ).c_str(), ImVec2( -FLT_MIN, 0.0f ) );
			}
			else
			{
				if( feature_name.empty() )
					ImGui::Button( Utility::format( "{:d}x{:d}", (int)tile_key.x_size, (int)tile_key.y_size ).c_str(), ImVec2( -FLT_MIN, 0.0f ) );
				else
					ImGui::Button( Utility::format( "{:d}x{:d}: \"{}\"", (int)tile_key.x_size, (int)tile_key.y_size, feature_name ).c_str(), ImVec2( -FLT_MIN, 0.0f ) );
			}

			if( ImGui::IsItemHovered() )
				ImGui::SetTooltip( "Tile Size: %dx%d\nFeature Name: \"%s\"\nTile Storey: %d", (int)tile_key.x_size, (int)tile_key.y_size, feature_name.c_str(), storey );
		};

		ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2( 2, 2 ) );
		if( ImGui::BeginTable( "tile_key", 3, ImGuiTableFlags_SizingStretchSame, ImVec2( -FLT_MIN, 0.0f ) ) )
		{
			// tile key visualisation
			RenderGroundType( ( 3 + camera_rotation ) % 4 );  RenderEdgeType( ( 2 + camera_rotation ) % 4 );  RenderGroundType( ( 2 + camera_rotation ) % 4 );
			RenderEdgeType(   ( 3 + camera_rotation ) % 4 );  RenderSizeAndFeatureName();					  RenderEdgeType(   ( 1 + camera_rotation ) % 4 );
			RenderGroundType( ( 0 + camera_rotation ) % 4 );  RenderEdgeType( ( 0 + camera_rotation ) % 4 );  RenderGroundType( ( 1 + camera_rotation ) % 4 );

			ImGui::EndTable();
		}

		ImGui::PopStyleVar();
	}

	void RenderRoomKeyVisualisation( const Terrain::Generation::RoomKey& room_key, const std::string& feature_name )
	{
		if( room_key.exit_data.size() < 4 )
			return;

		const auto RenderCorner = []()
		{
			ImGui::TableNextColumn();
		};

		const auto RenderNullButton = []()
		{
			ImGui::TableNextColumn();
			ImGui::Button( " ", ImVec2( -FLT_MIN, 0.0f ) );
		};

		const auto RenderCentre = [&]()
		{
			ImGui::TableNextColumn();
			if( feature_name.empty() )
			{
				const std::string label = Utility::format( "{:d}x{:d}", room_key.width, room_key.height );
				ImGui::Button( label.c_str(), ImVec2( -FLT_MIN, 0.0f ) );
			}
			else
			{
				const std::string label = Utility::format( "{:d}x{:d}: \"{}\"", room_key.width, room_key.height, feature_name.c_str() );
				ImGui::Button( label.c_str(), ImVec2( -FLT_MIN, 0.0f ) );
			}
			if( ImGui::IsItemHovered() )
				ImGui::SetTooltip( "Size: %dx%d\nFeature Name: \"%s\"", room_key.width, room_key.height, feature_name.c_str() );
		};

		const auto RenderDoor = [&]( const Terrain::Generation::RoomKey::ExitDatum& door )
		{
			ImGui::TableNextColumn();

			const Terrain::TerrainEdgeTypeHandle& type = door.edge_type;
			const int storey = door.storey;
			const bool flip = door.flip;

			ImGui::PushID( &type );
			if( type.IsNull() )
			{
				ImGuiEx::PushDisable();
				ImGui::Button( Utility::format( "({:d})", storey ).c_str(), ImVec2( -FLT_MIN, 0.0f ) );
				ImGuiEx::PopDisable();
				if( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) )
					ImGui::SetTooltip( "Edge Type: (null)\nStorey: %i\nFlip: %d", storey, flip );

			}
			else
			{
				const std::string label = Utility::format( "{} ({:d})", WstringToString( type->GetName() ).c_str(), storey );
				if( ImGui::Button( label.c_str(), ImVec2( -FLT_MIN, 0.0f ) ) )
					ToolsUtility::OpenInTextEditor( type.GetFilename().c_str() );

				if( ImGui::IsItemHovered() )
					ImGui::SetTooltip( "Edge Type: %s\nStorey: %i\nFlip: %d", WstringToString( type.GetFilename() ).c_str(), storey, flip );
			}
			ImGui::PopID();
		};

		ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2( 2, 2 ) );
		if( ImGui::BeginTable( "room_key", room_key.width + 2, ImGuiTableFlags_SizingStretchSame, ImVec2( -FLT_MIN, 0.0f ) ) )
		{
			RenderCorner();
			for( unsigned i = 0; i < room_key.width; ++i )
				RenderDoor( room_key.GetTopDoor( i ) );
			RenderCorner();

			const unsigned midx = ( room_key.width - 1 ) / 2;
			const unsigned midy = room_key.height / 2;

			for( unsigned j = room_key.height; j-- > 0; )
			{
				RenderDoor( room_key.GetLeftDoor( j ) );
				for( unsigned i = 0; i < room_key.width; ++i )
				{
					if( i == midx && j == midy )
						RenderCentre();
					else
						RenderNullButton();
				}
				RenderDoor( room_key.GetRightDoor( j ) );
			}

			RenderCorner();
			for( unsigned i = 0; i < room_key.width; ++i )
				RenderDoor( room_key.GetBottomDoor( i ) );
			RenderCorner();

			ImGui::EndTable();
		}

		ImGui::PopStyleVar();
	}

} //namespace ToolsUtility

#ifdef NO_OPTIMISE
#pragma optimize( "", on )
#endif

#endif //DEBUG_GUI_ENABLED
