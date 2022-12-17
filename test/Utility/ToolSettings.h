#pragma once

#include "rapidjson/document.h"

#include <map>
#include <string>

namespace Utility
{
	class ToolSettings
	{
	public:
		ToolSettings();
		~ToolSettings();

		rapidjson::Document::AllocatorType& GetAllocator();
		rapidjson::Document& GetDocument();

		void Save();

		const auto& GetFileName() const { return settings_path; }
		const auto& GetImGuiINI() const { return imgui_ini_path; }
		void SetConfigPath( const std::string& path );

	private:
		rapidjson::Document document;
		std::string settings_path;
		std::string imgui_ini_path;
	};
}