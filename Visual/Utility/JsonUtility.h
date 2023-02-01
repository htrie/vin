#pragma once

#include <vector>
#include <string>
#include <string_view>

#include <rapidjson/document.h>

#include "Common/Simd/Simd.h"
#include "Visual/Utility/IMGUI/DebugGUIDefines.h"

#if DEBUG_GUI_ENABLED
struct ImVec2;
#endif

namespace JsonUtility
{
	void ReplaceMember( rapidjson::Value& object, rapidjson::Value value, const std::string_view name, rapidjson::Document::AllocatorType& allocator );
	void SortMembers( rapidjson::Value& object );

	void Read( const rapidjson::Value& object, int& var, const char* name );
	void Read( const rapidjson::Value& object, unsigned& var, const char* name );
	void Read( const rapidjson::Value& object, float& var, const char* name );
	void Read( const rapidjson::Value& object, bool& var, const char* name );
	void Read( const rapidjson::Value& object, std::wstring& var, const char* name );
	void Read( const rapidjson::Value& object, std::string& var, const char* name );
	void Read( const rapidjson::Value& object, std::vector< std::wstring >& var, const char* name );
	void Read( const rapidjson::Value& object, std::vector< std::string >& var, const char* name );
	void Read( const rapidjson::Value& object, simd::vector2& var, const char* name );
	void Read( const rapidjson::Value& object, simd::vector3& var, const char* name );
	void Read( const rapidjson::Value& object, simd::vector4& var, const char* name );

	void Write( rapidjson::Value& object, const int var, const std::string_view name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const unsigned var, const std::string_view name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const float var, const std::string_view name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const bool var, const std::string_view name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const std::wstring& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const std::string& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const std::vector< std::wstring >& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const std::vector< std::string >& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const simd::vector2& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const simd::vector3& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const simd::vector4& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator );

	// Colours are written as a string in the foramt #RRGGBBAA
	bool ReadColor( const rapidjson::Value& object, simd::color& var, const char* name );
	void WriteColor( rapidjson::Value& object, const simd::color& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator );

#if DEBUG_GUI_ENABLED
	void Read( const rapidjson::Value& object, ImVec2& var, const char* name );
	void Write( rapidjson::Value& object, const ImVec2& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator );
#endif
}