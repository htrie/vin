#pragma once

#include <rapidjson/document.h>
#include "Visual/GUI2/imgui/imgui.h"

namespace JsonUtility
{
	void Read( const rapidjson::Value& object, int& var, const char* name );
	void Read( const rapidjson::Value& object, unsigned& var, const char* name );
	void Read( const rapidjson::Value& object, float& var, const char* name );
	void Read( const rapidjson::Value& object, bool& var, const char* name );
	void Read( const rapidjson::Value& object, std::wstring& var, const char* name );
	void Read( const rapidjson::Value& object, std::string& var, const char* name );
	void Read( const rapidjson::Value& object, std::vector< std::wstring >& var, const char* name );
	void Read( const rapidjson::Value& object, std::vector< std::string >& var, const char* name );
	void Read( const rapidjson::Value& object, ImVec2& var, const char* name );
	void Read( const rapidjson::Value& object, simd::vector2& var, const char* name );
	void Read( const rapidjson::Value& object, simd::vector3& var, const char* name );
	void Read( const rapidjson::Value& object, simd::vector4& var, const char* name );

	void Write( rapidjson::Value& object, const int var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const unsigned var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const float var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const bool var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const std::wstring& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const std::string& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const std::vector< std::wstring >& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const std::vector< std::string >& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const ImVec2& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const simd::vector2& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const simd::vector3& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );
	void Write( rapidjson::Value& object, const simd::vector4& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );

	// Colours are written as a string in the foramt #RRGGBBAA
	bool ReadColor( const rapidjson::Value& object, simd::color& var, const char* name );
	bool ReadColor( const rapidjson::Value& object, ImU32& var, const char* name );
	bool ReadColor( const rapidjson::Value& object, ImVec4& var, const char* name );

	void WriteColor( rapidjson::Value& object, const simd::color& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );
	void WriteColor( rapidjson::Value& object, const ImU32& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );
	void WriteColor( rapidjson::Value& object, const ImVec4& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator );
}