#pragma once

#include "WindowsUtility.h"

namespace simd
{
	class vector3;
}

namespace Utility
{
	///Function that displays a dialog box asking for a filename of a given type.
	///@return the filename that was selected. Returns the empty string if the user cancelled or there was some other problem.
	std::wstring GetFilenameBox( const wchar_t* filter, const wchar_t* initial_dir = nullptr, const bool add_to_recently_used = true );
	std::wstring GetFilenameBox( const wchar_t* filter, std::wstring& last_open_dir, const bool add_to_recently_used = true );

	///Same as GetFilenameBox but allows selection of multiple files
	std::vector< std::wstring > GetFilenamesBox( const wchar_t* filter, const wchar_t* initial_dir = nullptr, const bool add_to_recently_used = true );
	std::vector< std::wstring > GetFilenamesBox( const wchar_t* filter, std::wstring& last_open_dir, const bool add_to_recently_used = true );

	///Same as GetFilenameBox but returns the path relative to the current directory.
	std::wstring GetFilenameRelativeBox( const wchar_t* filter, const wchar_t* initial_dir = nullptr );

	///Reformats the string for long file paths with "..." (For displaying paths on path text ui's)
	void GetFilenameDisplayString( std::wstring& output, const std::wstring& filename );

	///Function that displays a dialog box asking for a filepath to save a file into
	///@return the filepath / name that was selected. Returns the empty string if the user cancelled or there was some other problem.
	std::wstring SaveFilenameBox( const wchar_t* filter, const wchar_t* initial_dir = nullptr, const bool warn_on_overwrite = true );
	bool SaveFilenameBox( const wchar_t* filter, std::wstring& filename_in_out, const wchar_t* initial_dir = nullptr, const bool warn_on_overwrite = true );

	std::vector< std::wstring > SaveFilenamesBox( const wchar_t* filter, const wchar_t* initial_dir = nullptr, const bool warn_on_overwrite = true );

	///Puts up a choose folder dialog with the specified window title.
	///@return the empty string if the user cancels the operation otherwise the chosen directory with trailing slash.
	std::wstring GetDirectoryBox( const std::wstring window_title, const std::wstring& start_folder = L"" );

	///Function that displays a dialog box asking for a colour
	bool PickColour(simd::vector3& colour_in_out );
}
