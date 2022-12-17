#include "ToolSettings.h"

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>
#include <fstream>

#include "Common/File/FileSystem.h"
#include "Common/File/PathHelper.h"
#include "Common/Console/ConsoleCommon.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/StringManipulation.h"
#include "Visual/Device/WindowDX.h"

// Increment this number if you want to force clean settings!
#define SETTINGS_VERSION 1

namespace Utility
{
	namespace
	{
		std::string GetSettingsPath(const std::string& file)
		{
#ifdef CONSOLE
			return WstringToString(PathHelper::NormalisePath(Console::GetTemporaryDirectory() + L"/" + StringToWstring(file)));
#else
			return file;
#endif
		}
	}

	ToolSettings::ToolSettings()
	{
		settings_path = GetSettingsPath("tool_settings.json");

#ifdef CONSOLE
		constexpr unsigned MaxAttempts = 1;
#else
		constexpr unsigned MaxAttempts = 10;
#endif

		for( unsigned i = 0; i < MaxAttempts; ++i )
		{
			if( i > 0 )
				SleepMilli( 100 );

			std::ifstream file( settings_path );
			if( file.is_open() )
			{
				std::string json_code( std::istreambuf_iterator<char>( file ), {} );
				document.Parse<rapidjson::kParseTrailingCommasFlag>( json_code.c_str() );

				if( !document.HasParseError() )
					break;

				LOG_WARN( "Failed to parse tool_settings.json: " << rapidjson::GetParseError_En( document.GetParseError() ) );
			}
		}

		if( document.HasParseError() || document.IsNull() || document["settings_version"].IsNull() || document["settings_version"].GetInt() != SETTINGS_VERSION )
		{
#ifdef WIN32
			if( document.HasParseError() )
				Device::WindowDX::MessageBox( rapidjson::GetParseError_En( document.GetParseError() ), "Failed to parse tool_settings.json", MB_OK | MB_ICONWARNING );
			else if( FileSystem::FileExists( StringToWstring( settings_path ) ) )
				Device::WindowDX::MessageBox( "Failed to load tool_settings.json!\nResetting to default settings.", "Warning", MB_OK | MB_ICONWARNING );
			else
				LOG_WARN( L"tool_settings.json was not found. Resetting to default settings." );
#endif
			document.SetObject();
			document.AddMember( "settings_version", SETTINGS_VERSION, GetAllocator() );
		}
	}

	ToolSettings::~ToolSettings()
	{
		
	}

	void ToolSettings::Save()
	{
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		document.Accept(writer);

		FileSystem::CreateDirectoryChain(PathHelper::ParentPath(StringToWstring(settings_path)));
		std::ofstream file(settings_path);
		file << buffer.GetString();
	}

	void ToolSettings::SetConfigPath( const std::string& path )
	{
		imgui_ini_path = GetSettingsPath( path + "/imgui.ini" );
		FileSystem::CreateDirectoryChain( PathHelper::ParentPath( StringToWstring( imgui_ini_path ) ) );
	}

	rapidjson::Document::AllocatorType& ToolSettings::GetAllocator()
	{
		return document.GetAllocator();
	}

	rapidjson::Document& ToolSettings::GetDocument()
	{
		return document;
	}
}