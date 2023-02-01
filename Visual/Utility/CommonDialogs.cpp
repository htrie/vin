#include "CommonDialogs.h"
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
#include <CommDlg.h>
#include <shlobj.h>
#include <ShObjIdl.h>
#endif
#include "Common/File/FileSystem.h"
#include "Common/File/PathHelper.h"
#include "Common/Utility/StringManipulation.h"

#ifdef __APPLE__
#include "Common/Bridging/ApplicationBridge.h"
#endif

namespace Utility
{
	constexpr const wchar_t* GetExtension( const wchar_t* filter )
	{
		//assume typical OPENFILENAME filter format: L"DISPLAYNAME\0*.EXTENSION\0"
		//e.g. L"Hideout File (*.hideout)\0*.hideout\0"
		//the string must be terminated by TWO null chars
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
		const size_t display_name_length = StringLength( filter );
		const wchar_t* extension = filter + display_name_length + 1;
		if( extension[ 0 ] == L'*' && extension[ 1 ] == L'.' && !StringContains( extension, L";" ) )
			return extension + 2;
#endif
		return nullptr; //not found
	}

	HWND GetHWND()
	{
		const Device::WindowDX* window = Device::WindowDX::GetWindow();
		return window ? window->GetWnd() : NULL;
	}

	std::wstring GetFilenameBox( const wchar_t* filter, const wchar_t* initial_dir, const bool add_to_recently_used )
	{
		std::wstring last_open_dir;
		if( initial_dir )
			last_open_dir = initial_dir;
		return GetFilenameBox( filter, last_open_dir, add_to_recently_used );
	}

	std::wstring GetFilenameBox( const wchar_t* filter, std::wstring& last_open_dir, const bool add_to_recently_used )
	{
#if !defined( CONSOLE ) && !defined( __APPLE__ )  && !defined( ANDROID )
		Memory::WriteLock lock(FileSystem::GetMutex()); // Lock to avoid issue with other file opening routines.

		//Save the old working directory so its not clobbered by GetOpenFilename
		const std::wstring current_directory = FileSystem::GetCurrentDirectory();
		PathHelper::NormalisePathInPlace( last_open_dir, L'\\' );

		OPENFILENAME ofn;
		wchar_t filename[ 1024 ] = { L'\0' };

		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFile = filename;
		ofn.nMaxFile = ARRAYSIZE( filename );
		ofn.lpstrFilter = filter;
		ofn.lpstrInitialDir = !last_open_dir.empty() ? last_open_dir.c_str() : nullptr;
		ofn.lpstrDefExt = GetExtension( filter );
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
		if( !add_to_recently_used )
			ofn.Flags |= OFN_DONTADDTORECENT;

		const bool success = !!GetOpenFileNameW(&ofn);

		if( success )
			last_open_dir = FileSystem::GetCurrentDirectory();
		FileSystem::SetCurrentDirectory( current_directory );

		if( !success )
			return L"";

		return PathHelper::NormalisePath( filename );
#elif defined( __APPLE__macosx )
        const std::string folder = WstringToString( last_open_dir );
        const std::string filter_str = WstringToString( filter );
        const std::string file_name = Bridge::GetOpenFileName( folder, filter_str );
        return StringToWstring( file_name );
#else
		return L""; //unimplemented
#endif
	}

	std::vector< std::wstring > GetFilenamesBox( const wchar_t* filter, const wchar_t* initial_dir, const bool add_to_recently_used )
	{
		std::wstring last_open_dir;
		if( initial_dir )
			last_open_dir = initial_dir;
		return GetFilenamesBox( filter, last_open_dir, add_to_recently_used );
	}

	std::vector< std::wstring > GetFilenamesBox( const wchar_t* filter, std::wstring& last_open_dir, const bool add_to_recently_used )
	{
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
		Memory::WriteLock lock( FileSystem::GetMutex() ); // Lock to avoid issue with other file opening routines.

		std::vector< std::wstring > results;
		const std::wstring current_directory = FileSystem::GetCurrentDirectory();
		PathHelper::NormalisePathInPlace( last_open_dir, L'\\' );

		wchar_t filenames_buffer[ 16 * 1024 ] = { L'\0' };

		//Give all options for GetOpenFilename default values
		OPENFILENAME ofn{};
		ZeroMemory( &ofn, sizeof( ofn ) );

		//Set all options required
		ofn.lStructSize = sizeof( ofn );
		ofn.hwndOwner = Utility::GetHWND();
		ofn.lpstrFile = filenames_buffer;
		ofn.nMaxFile = ARRAYSIZE( filenames_buffer );
		ofn.lpstrFilter = filter;
		ofn.lpstrInitialDir = !last_open_dir.empty() ? last_open_dir.c_str() : nullptr;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
		if( !add_to_recently_used )
			ofn.Flags |= OFN_DONTADDTORECENT;
		
		if( GetOpenFileNameW( &ofn ) )
		{
			//Check if only a single file is selected
			if( filenames_buffer[ ofn.nFileOffset - 1 ] != NULL )
			{
				results.push_back( PathHelper::NormalisePath( filenames_buffer ) );
			}
			else
			{
				//Extract the directory that all of the multi-selected files are in
				const std::wstring files_directory = filenames_buffer;

				//Find the start of the first filename in the list
				const wchar_t* file_start_location = filenames_buffer + ( files_directory.size() + 1 );

				//While there is another filename in the list
				while( *file_start_location != NULL )
				{
					//Extract the current file
					const std::wstring filename = file_start_location;
					results.push_back( PathHelper::NormalisePath( files_directory + L'/' + filename ) );

					//Move to the next file in the list
					file_start_location += filename.size() + 1;
				}
			}
			last_open_dir = FileSystem::GetCurrentDirectory();
		}

		FileSystem::SetCurrentDirectory( current_directory );
		return results;
#else
		return {}; //unimplemented
#endif
	}

	std::wstring GetFilenameRelativeBox( const wchar_t* filter, const wchar_t* initial_dir )
	{
#if !defined( CONSOLE ) && !defined( __APPLE__ )
		const std::wstring current_directory = FileSystem::GetCurrentDirectory();

		const std::wstring result = GetFilenameBox( filter, initial_dir );
		if( result.empty() )
			return std::wstring();

		if( !PathHelper::PathIsInside( result, current_directory ) )
			return std::wstring();

		return PathHelper::RelativePath( result, current_directory );
#else 
		return L""; //unimplemented
#endif
	}

	void GetFilenameDisplayString( std::wstring& output, const std::wstring& filename )
	{
#if !defined( CONSOLE ) && !defined( __APPLE__ )
		output.clear();
		const size_t max_display_length = 27;
		const size_t filename_size = filename.size();

		if (filename.empty())
		{
			output = L"{not set}";
			return;
		}

		if (filename_size> max_display_length)
		{
			const auto location = filename.find_first_of('\\', filename_size - max_display_length);
			if (location == std::wstring::npos)
			{
				output = L"..." + filename.substr(filename_size - max_display_length);
			}
			else
			{
				output = L"..." + filename.substr(location);
			}
		}
		else
		{
			output = filename;
		}
#endif
	}

	namespace
	{
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
		static COLORREF colours[ 16 ] = 
		{ 
			RGB( 255, 255, 255 ), RGB( 255, 255, 255 ),RGB( 255, 255, 255 ),RGB( 255, 255, 255 ),
			RGB( 255, 255, 255 ), RGB( 255, 255, 255 ),RGB( 255, 255, 255 ),RGB( 255, 255, 255 ),
			RGB( 255, 255, 255 ), RGB( 255, 255, 255 ),RGB( 255, 255, 255 ),RGB( 255, 255, 255 ),
			RGB( 255, 255, 255 ), RGB( 255, 255, 255 ),RGB( 255, 255, 255 ),RGB( 255, 255, 255 ),
		};
#endif
	}

	std::wstring SaveFilenameBox( const wchar_t* filter, const wchar_t* initial_dir, const bool warn_on_overwrite )
	{
		std::wstring filename;
		if( SaveFilenameBox( filter, filename, initial_dir, warn_on_overwrite ) )
			return filename;
		else
			return {};
	}

	bool SaveFilenameBox( const wchar_t* filter, std::wstring& filename_in_out, const wchar_t* initial_dir, const bool warn_on_overwrite )
	{
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
		Memory::WriteLock lock(FileSystem::GetMutex()); // Lock to avoid issue with other file opening routines.

		//Save the old working directory so its not clobbered by GetSaveFilename
		const std::wstring current_directory = FileSystem::GetCurrentDirectory();

		wchar_t filename[ 1024 ] = { L'\0' };
		CopyString( filename, initial_dir ? PathHelper::PathTargetName( filename_in_out ) : filename_in_out );
		PathHelper::NormalisePathInPlace( filename, L'\\' );

		OPENFILENAME ofn;
		ZeroMemory( &ofn, sizeof( ofn ) );
		ofn.lStructSize = sizeof( ofn );
		ofn.hwndOwner = Utility::GetHWND();
		ofn.lpstrFile = filename;
		ofn.nMaxFile = ARRAYSIZE( filename );
		ofn.lpstrFilter = filter;
		ofn.lpstrDefExt = GetExtension( filter );
		ofn.Flags = OFN_PATHMUSTEXIST;
		if( warn_on_overwrite )
			ofn.Flags |= OFN_OVERWRITEPROMPT;

		std::wstring input_dir = PathHelper::NormalisePath( initial_dir ? initial_dir : L"", L'\\' );
		if( input_dir.size() )
			ofn.lpstrInitialDir = input_dir.c_str();
		
		const bool success = !!GetSaveFileNameW( &ofn );
		FileSystem::SetCurrentDirectory( current_directory );
		if( !success )
			return false;

		filename_in_out = PathHelper::NormalisePath( filename );
		return true;
#elif defined( __APPLE__macosx )
		const std::string folder = initial_dir ? WstringToString( initial_dir ) : std::string();
        const std::string filter_str = WstringToString( filter );
        const std::string file_name = Bridge::GetSaveFileName( folder, filter_str );
		filename_in_out = StringToWstring( file_name );
		return true;
#else
		return false; //unimplemented
#endif
	}

	std::vector< std::wstring > SaveFilenamesBox( const wchar_t* filter, const wchar_t* initial_dir, const bool warn_on_overwrite )
		{
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
		Memory::WriteLock lock( FileSystem::GetMutex() ); // Lock to avoid issue with other file opening routines.

		std::vector< std::wstring > results;
		const std::wstring current_directory = FileSystem::GetCurrentDirectory();

		wchar_t filenames_buffer[ 16 * 1024 ] = { L'\0' };

		//Give all options for GetOpenFilename default values
		OPENFILENAME ofn{};
		ZeroMemory( &ofn, sizeof( ofn ) );

		//Set all options required
		ofn.lStructSize = sizeof( ofn );
		ofn.hwndOwner = Utility::GetHWND();
		ofn.lpstrFile = filenames_buffer;
		ofn.nMaxFile = ARRAYSIZE( filenames_buffer );
		ofn.lpstrFilter = filter;
		ofn.lpstrDefExt = GetExtension( filter );
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
		if( warn_on_overwrite )
			ofn.Flags |= OFN_OVERWRITEPROMPT;

		std::wstring input_dir = PathHelper::NormalisePath( initial_dir ? initial_dir : L"", L'\\' );
		if( input_dir.size() )
			ofn.lpstrInitialDir = input_dir.c_str();

		if( GetSaveFileNameW( &ofn ) )
		{
			//Check if only a single file is selected
			if( filenames_buffer[ ofn.nFileOffset - 1 ] != NULL )
			{
				results.push_back( PathHelper::NormalisePath( filenames_buffer ) );
		}
			else
			{
				//Extract the directory that all of the multi-selected files are in
				const std::wstring files_directory = filenames_buffer;

				//Find the start of the first filename in the list
				const wchar_t* file_start_location = filenames_buffer + ( files_directory.size() + 1 );

				//While there is another filename in the list
				while( *file_start_location != NULL )
				{
					//Extract the current file
					const std::wstring filename = file_start_location;
					results.push_back( PathHelper::NormalisePath( files_directory + L'/' + filename ) );

					//Move to the next file in the list
					file_start_location += filename.size() + 1;
				}
			}
		}

		FileSystem::SetCurrentDirectory( current_directory );
		return results;
#else
		return {}; //unimplemented
#endif
	}

	std::wstring GetDirectoryBox( const std::wstring window_title, const std::wstring& start_folder_path)
	{
#if !defined(_XBOX_ONE) && !defined(PS4) &&!defined( __APPLE__ ) && !defined( ANDROID )
		Memory::WriteLock lock( FileSystem::GetMutex() ); // Lock to avoid issue with other file opening routines.

		IShellItem* start_folder = NULL;
		if (start_folder_path.length() > 0)
		{
			if (FAILED(SHCreateItemFromParsingName(start_folder_path.c_str(), NULL, IID_PPV_ARGS(&start_folder))))
				start_folder = NULL;
		}

		std::wstring folder_path;

		IFileOpenDialog* pfd = NULL;
		if( SUCCEEDED( CoCreateInstance( CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &pfd ) ) ) )
		{
			DWORD dwFlags;
			if( SUCCEEDED( pfd->GetOptions( &dwFlags ) ) &&
				SUCCEEDED( pfd->SetOptions( dwFlags | FOS_PICKFOLDERS ) ) &&
				SUCCEEDED( pfd->SetTitle( window_title.c_str() ) ) &&
			    (start_folder == NULL || SUCCEEDED(pfd->SetFolder(start_folder))) &&
				SUCCEEDED( pfd->Show( Utility::GetHWND() ) ) )
			{
				IShellItem* psiResult;
				if( SUCCEEDED( pfd->GetResult( &psiResult ) ) )
				{
					PWSTR pszFilePath = NULL;
					if( SUCCEEDED( psiResult->GetDisplayName( SIGDN_FILESYSPATH, &pszFilePath ) ) )
					{
						folder_path = pszFilePath;
						CoTaskMemFree( pszFilePath );
					}
					psiResult->Release();
				}
			}
			pfd->Release();
		}

		if (start_folder != NULL)
			start_folder->Release();

		if( folder_path.empty() )
			return L"";

		folder_path += L'\\';
		return folder_path;
#else
		return L"";
#endif
	}

	bool PickColour( simd::vector3& colour_in_out )
	{
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
		DWORD colour = simd::color( colour_in_out.x, colour_in_out.y, colour_in_out.z, 1.0f ).c();

		CHOOSECOLOR choose_color;
		ZeroMemory( &choose_color, sizeof( CHOOSECOLOR ) );
		choose_color.lStructSize = sizeof( CHOOSECOLOR );

		choose_color.hwndOwner = Utility::GetHWND();
		choose_color.rgbResult = RGB
			( 
			((colour&0x00ff0000)>>16), 
			((colour&0x0000ff00)>>8 ), 
			((colour&0x000000ff)    ) 
			);
		choose_color.lpCustColors = colours;
		choose_color.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT;
		if( !ChooseColor( &choose_color ) )
			return false;
		colour_in_out = simd::vector3( GetRValue( choose_color.rgbResult ) / 255.0f, GetGValue( choose_color.rgbResult ) / 255.0f, GetBValue( choose_color.rgbResult ) / 255.f );
#endif
		return true;
	}

}
