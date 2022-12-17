#include <iomanip>

#include "JsonUtility.h"
#include "Common/Utility/StringManipulation.h"

namespace JsonUtility
{
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

	void Read( const rapidjson::Value& object, ImVec2& var, const char* name )
	{
		if( const auto it = object.FindMember( name ); it != object.MemberEnd() )
		{
			var.x = it->value.GetArray()[ 0 ].GetFloat();
			var.y = it->value.GetArray()[ 1 ].GetFloat();
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

	void Write( rapidjson::Value& object, const int var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator )
	{
		object.AddMember( name, var, allocator );
	}

	void Write( rapidjson::Value& object, const unsigned var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator )
	{
		object.AddMember( name, var, allocator );
	}

	void Write( rapidjson::Value& object, const float var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator )
	{
		object.AddMember( name, var, allocator );
	}

	void Write( rapidjson::Value& object, const bool var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator )
	{
		object.AddMember( name, var, allocator );
	}

	void Write( rapidjson::Value& object, const std::wstring& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator )
	{
		object.AddMember( name, rapidjson::Value().SetString( WstringToString( var ).c_str(), allocator ), allocator );
	}

	void Write( rapidjson::Value& object, const std::string& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator )
	{
		object.AddMember( name, rapidjson::Value().SetString( var.c_str(), allocator ), allocator );
	}

	void Write( rapidjson::Value& object, const std::vector<std::wstring>& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator )
	{
		rapidjson::Value arr( rapidjson::kArrayType );
		for( const std::wstring& str : var )
			arr.PushBack( rapidjson::Value().SetString( WstringToString( str ).c_str(), allocator ), allocator );
		object.AddMember( name, arr, allocator );
	}

	void Write( rapidjson::Value& object, const ImVec2& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator )
	{
		rapidjson::Value vec( rapidjson::kArrayType );
		vec.PushBack( rapidjson::Value().SetFloat( var.x ), allocator );
		vec.PushBack( rapidjson::Value().SetFloat( var.y ), allocator );
		object.AddMember( name, vec, allocator );
	}

	void Write( rapidjson::Value& object, const simd::vector2& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator )
	{
		rapidjson::Value vec( rapidjson::kArrayType );
		vec.PushBack( rapidjson::Value().SetFloat( var.x ), allocator );
		vec.PushBack( rapidjson::Value().SetFloat( var.y ), allocator );
		object.AddMember( name, vec, allocator );
	}

	void Write( rapidjson::Value& object, const simd::vector3& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator )
	{
		rapidjson::Value vec( rapidjson::kArrayType );
		vec.PushBack( rapidjson::Value().SetFloat( var.x ), allocator );
		vec.PushBack( rapidjson::Value().SetFloat( var.y ), allocator );
		vec.PushBack( rapidjson::Value().SetFloat( var.z ), allocator );
		object.AddMember( name, vec, allocator );
	}

	void Write( rapidjson::Value& object, const simd::vector4& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator )
	{
		rapidjson::Value vec( rapidjson::kArrayType );
		vec.PushBack( rapidjson::Value().SetFloat( var.x ), allocator );
		vec.PushBack( rapidjson::Value().SetFloat( var.y ), allocator );
		vec.PushBack( rapidjson::Value().SetFloat( var.z ), allocator );
		vec.PushBack( rapidjson::Value().SetFloat( var.w ), allocator );
		object.AddMember( name, vec, allocator );
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

			var.r = (color & 0xFF000000) >> 24;
			var.g = (color & 0x00FF0000) >> 16;
			var.b = (color & 0x0000FF00) >> 8;
			var.a = (color & 0x000000FF);
			return true;
		}
		return false;
	}

	bool ReadColor( const rapidjson::Value& object, ImU32& var, const char* name )
	{
		simd::color color;
		if( !ReadColor( object, color, name ) )
			return false;
		var = ImColor( color.r, color.g, color.b, color.a );
		return true;
	}

	bool ReadColor( const rapidjson::Value& object, ImVec4& var, const char* name )
	{
		simd::color color;
		if( !ReadColor( object, color, name ) )
			return false;
		var = ImColor( color.r, color.g, color.b, color.a );
		return true;
	}

	void WriteColor( rapidjson::Value& object, const simd::color& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator )
	{
		std::stringstream ss;
		ss << '#' << std::hex << std::setfill( '0' );
		ss << std::setw( 2 ) << static_cast<int>(var.r);
		ss << std::setw( 2 ) << static_cast<int>(var.g);
		ss << std::setw( 2 ) << static_cast<int>(var.b);
		ss << std::setw( 2 ) << static_cast<int>(var.a);
		object.AddMember( name, rapidjson::Value().SetString( ss.str().c_str(), allocator ), allocator );
	}

	void WriteColor( rapidjson::Value& object, const ImU32& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator )
	{
		WriteColor( object, ImGui::ColorConvertU32ToFloat4( var ), name, allocator );
	}

	void WriteColor( rapidjson::Value& object, const ImVec4& var, const rapidjson::GenericStringRef<char>& name, rapidjson::Document::AllocatorType& allocator )
	{
		WriteColor( object, simd::color( var.x, var.y, var.z, var.w ), name, allocator );
	}
}