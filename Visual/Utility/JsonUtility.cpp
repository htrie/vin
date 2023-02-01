#include <iomanip>

#include "JsonUtility.h"
#include "Common/Utility/StringManipulation.h"
#include "Visual/Utility/IMGUI/DebugGUIDefines.h"
#if DEBUG_GUI_ENABLED
#include "Visual/GUI2/imgui/imgui.h"
#endif

namespace JsonUtility
{
	void ReplaceMember( rapidjson::Value& object, rapidjson::Value value, const std::string_view name, rapidjson::Document::AllocatorType& allocator )
	{
		auto name_value = rapidjson::Value( name.data(), (unsigned)name.length(), allocator );
		auto it = object.FindMember( name_value );
		if( it == object.MemberEnd() )
			object.AddMember( std::move( name_value ), std::move( value ), allocator );
		else
			it->value = std::move( value );
	}

	void SortMembers( rapidjson::Value& object )
	{
		std::sort( object.MemberBegin(), object.MemberEnd(), []( const rapidjson::Value::Member& left, const rapidjson::Value::Member& right )
		{
			return StringCompare( left.name.GetString(), right.name.GetString() ) < 0;
		} );
	}

	void Read( const rapidjson::Value& object, int& var, const char* name )
	{
		if( const auto it = object.FindMember( name ); it != object.MemberEnd() )
			var = it->value.GetInt();
	}

	void Read( const rapidjson::Value& object, unsigned& var, const char* name )
	{
		if( const auto it = object.FindMember( name ); it != object.MemberEnd() )
			var = it->value.GetUint();
	}

	void Read( const rapidjson::Value& object, float& var, const char* name )
	{
		if( const auto it = object.FindMember( name ); it != object.MemberEnd() )
			var = it->value.GetFloat();
	}

	void Read( const rapidjson::Value& object, bool& var, const char* name )
	{
		if( const auto it = object.FindMember( name ); it != object.MemberEnd() )
			var = it->value.GetBool();
	}

	void Read( const rapidjson::Value& object, std::wstring& var, const char* name )
	{
		if( const auto it = object.FindMember( name ); it != object.MemberEnd() )
			var = StringToWstring( it->value.GetString() );
	}

	void Read( const rapidjson::Value& object, std::string& var, const char* name )
	{
		if( const auto it = object.FindMember( name ); it != object.MemberEnd() )
			var = it->value.GetString();
	}

	void Read( const rapidjson::Value& object, std::vector< std::wstring >& var, const char* name )
	{
		if( const auto it = object.FindMember( name ); it != object.MemberEnd() )
		{
			for( const auto& v : it->value.GetArray() )
			{
				var.push_back( StringToWstring( v.GetString() ) );
			}
		}
	}

	void Read( const rapidjson::Value& object, std::vector< std::string >& var, const char* name )
	{
		if( const auto it = object.FindMember( name ); it != object.MemberEnd() )
		{
			for( const auto& v : it->value.GetArray() )
			{
				var.push_back( v.GetString() );
			}
		}
	}

	void Read( const rapidjson::Value& object, simd::vector2& var, const char* name )
	{
		if( const auto it = object.FindMember( name ); it != object.MemberEnd() )
		{
			var.x = it->value.GetArray()[ 0 ].GetFloat();
			var.y = it->value.GetArray()[ 1 ].GetFloat();
		}
	}

	void Read( const rapidjson::Value& object, simd::vector3& var, const char* name )
	{
		if( const auto it = object.FindMember( name ); it != object.MemberEnd() )
		{
			var.x = it->value.GetArray()[ 0 ].GetFloat();
			var.y = it->value.GetArray()[ 1 ].GetFloat();
			var.z = it->value.GetArray()[ 2 ].GetFloat();
		}
	}

	void Read( const rapidjson::Value& object, simd::vector4& var, const char* name )
	{
		if( const auto it = object.FindMember( name ); it != object.MemberEnd() )
		{
			var.x = it->value.GetArray()[ 0 ].GetFloat();
			var.y = it->value.GetArray()[ 1 ].GetFloat();
			var.z = it->value.GetArray()[ 2 ].GetFloat();
			var.w = it->value.GetArray()[ 3 ].GetFloat();
		}
	}

	void Write( rapidjson::Value& object, const int var, const std::string_view name, rapidjson::Document::AllocatorType& allocator )
	{
		ReplaceMember( object, rapidjson::Value( var ), name, allocator );
	}

	void Write( rapidjson::Value& object, const unsigned var, const std::string_view name, rapidjson::Document::AllocatorType& allocator )
	{
		ReplaceMember( object, rapidjson::Value( var ), name, allocator );
	}

	void Write( rapidjson::Value& object, const float var, const std::string_view name, rapidjson::Document::AllocatorType& allocator )
	{
		ReplaceMember( object, rapidjson::Value( var ), name, allocator );
	}

	void Write( rapidjson::Value& object, const bool var, const std::string_view name, rapidjson::Document::AllocatorType& allocator )
	{
		ReplaceMember( object, rapidjson::Value( var ), name, allocator );
	}

	void Write( rapidjson::Value& object, const std::wstring& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator )
	{
		ReplaceMember( object, rapidjson::Value( WstringToString( var ).c_str(), allocator ), name, allocator );
	}

	void Write( rapidjson::Value& object, const std::string& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator )
	{
		ReplaceMember( object, rapidjson::Value( var.c_str(), allocator ), name, allocator );
	}

	void Write( rapidjson::Value& object, const std::vector<std::wstring>& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator )
	{
		rapidjson::Value arr( rapidjson::kArrayType );
		for( const std::wstring& str : var )
			arr.PushBack( rapidjson::Value().SetString( WstringToString( str ).c_str(), allocator ), allocator );
		ReplaceMember( object, std::move( arr ), name, allocator );
	}

	void Write( rapidjson::Value& object, const simd::vector2& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator )
	{
		rapidjson::Value vec( rapidjson::kArrayType );
		vec.PushBack( rapidjson::Value().SetFloat( var.x ), allocator );
		vec.PushBack( rapidjson::Value().SetFloat( var.y ), allocator );
		ReplaceMember( object, std::move( vec ), name, allocator );
	}

	void Write( rapidjson::Value& object, const simd::vector3& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator )
	{
		rapidjson::Value vec( rapidjson::kArrayType );
		vec.PushBack( rapidjson::Value().SetFloat( var.x ), allocator );
		vec.PushBack( rapidjson::Value().SetFloat( var.y ), allocator );
		vec.PushBack( rapidjson::Value().SetFloat( var.z ), allocator );
		ReplaceMember( object, std::move( vec ), name, allocator );
	}

	void Write( rapidjson::Value& object, const simd::vector4& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator )
	{
		rapidjson::Value vec( rapidjson::kArrayType );
		vec.PushBack( rapidjson::Value().SetFloat( var.x ), allocator );
		vec.PushBack( rapidjson::Value().SetFloat( var.y ), allocator );
		vec.PushBack( rapidjson::Value().SetFloat( var.z ), allocator );
		vec.PushBack( rapidjson::Value().SetFloat( var.w ), allocator );
		ReplaceMember( object, std::move( vec ), name, allocator );
	}

	bool ReadColor( const rapidjson::Value& object, simd::color& var, const char* name )
	{
		if( const auto it = object.FindMember( name ); it != object.MemberEnd() )
		{
			std::stringstream ss( it->value.GetString() );
			ss.ignore(); // ignore leading #
			unsigned color;
			ss >> std::hex >> color;
			if( ss.fail() )
				return false;

			var.r = ( color & 0xFF000000 ) >> 24;
			var.g = ( color & 0x00FF0000 ) >> 16;
			var.b = ( color & 0x0000FF00 ) >> 8;
			var.a = ( color & 0x000000FF );
			return true;
		}
		return false;
	}

	void WriteColor( rapidjson::Value& object, const simd::color& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator )
	{
		std::stringstream ss;
		ss << '#' << std::hex << std::setfill( '0' );
		ss << std::setw( 2 ) << static_cast< int >( var.r );
		ss << std::setw( 2 ) << static_cast< int >( var.g );
		ss << std::setw( 2 ) << static_cast< int >( var.b );
		ss << std::setw( 2 ) << static_cast< int >( var.a );
		ReplaceMember( object, rapidjson::Value( ss.str().c_str(), allocator ), name, allocator );
	}

#if DEBUG_GUI_ENABLED
	void Read( const rapidjson::Value& object, ImVec2& var, const char* name )
	{
		if( const auto it = object.FindMember( name ); it != object.MemberEnd() )
		{
			var.x = it->value.GetArray()[ 0 ].GetFloat();
			var.y = it->value.GetArray()[ 1 ].GetFloat();
		}
	}

	void Write( rapidjson::Value& object, const ImVec2& var, const std::string_view name, rapidjson::Document::AllocatorType& allocator )
	{
		rapidjson::Value vec( rapidjson::kArrayType );
		vec.PushBack( rapidjson::Value().SetFloat( var.x ), allocator );
		vec.PushBack( rapidjson::Value().SetFloat( var.y ), allocator );
		ReplaceMember( object, std::move( vec ), name, allocator );
	}
#endif
}