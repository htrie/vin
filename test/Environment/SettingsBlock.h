#pragma once

#include <vector>
#include <memory>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/error/en.h>
#include "EnvironmentParamIds.h"

#include "ClientInstanceCommon/Animation/AnimatedObjectType.h"
#include "Visual/Material/Material.h"

namespace Renderer
{
	namespace DrawCalls
	{
		class EffectInstance;
	}
}


namespace Environment
{
	using VolumeTex = int;
	using JsonDocument = rapidjson::GenericDocument<rapidjson::UTF16<> >;
	using JsonValue = JsonDocument::ValueType;
	using JsonAllocator = JsonDocument::AllocatorType;

	struct VolumeTexData;

	enum struct BlendTypes
	{
		Linear,
		Exclusive
	};

	struct FootstepData
	{
		MaterialHandle material;
		int audio_index = 0;
	};

	template<typename Type, int size>
	struct WeightedArray
	{
		WeightedArray() noexcept
		{
			for (size_t i = 0; i < size; i++)
			{
				weights[i] = (i == 0) ? 1.0f : 0.0f;
				values[i] = Type();
			}
		}
		Type& Get(size_t index = 0)
		{
			assert(index < size);
			return values[index];
		}
		const Type& Get(size_t index = 0) const
		{
			assert(index < size);
			return values[index];
		}
		std::array<Type, size> values;
		std::array<float, size> weights;
	};

	struct Data
	{
		Data() noexcept {}
		float& Value(typename EnvParamIds::Float id) { return float_data[size_t(id)].value; }
		const float Value(typename EnvParamIds::Float id) const { return float_data[size_t(id)].value; }
		bool& IsPresent(typename EnvParamIds::Float id) { return float_data[size_t(id)].is_present; }
		const bool IsPresent(typename EnvParamIds::Float id) const { return float_data[size_t(id)].is_present; }

		float& Value(typename EnvParamIds::Periodic id) { return periodic_data[size_t(id)].value; }
		const float Value(typename EnvParamIds::Periodic id) const { return periodic_data[size_t(id)].value; }
		bool& IsPresent(typename EnvParamIds::Periodic id) { return periodic_data[size_t(id)].is_present; }
		const bool IsPresent(typename EnvParamIds::Periodic id) const { return periodic_data[size_t(id)].is_present; }

		simd::vector3 &Value(typename EnvParamIds::Vector3 id) { return vector3_data[size_t(id)].value; }
		const simd::vector3 &Value(typename EnvParamIds::Vector3 id) const { return vector3_data[size_t(id)].value; }
		bool &IsPresent(typename EnvParamIds::Vector3 id) { return vector3_data[size_t(id)].is_present; }
		const bool IsPresent(typename EnvParamIds::Vector3 id) const { return vector3_data[size_t(id)].is_present; }

		bool &Value(typename EnvParamIds::Bool id) { return bool_data[size_t(id)].value; }
		const bool &Value(typename EnvParamIds::Bool id) const { return bool_data[size_t(id)].value; }
		bool &IsPresent(typename EnvParamIds::Bool id) { return bool_data[size_t(id)].is_present; }
		const bool IsPresent(typename EnvParamIds::Bool id) const { return bool_data[size_t(id)].is_present; }

		int& Value(typename EnvParamIds::Int id) { return int_data[size_t(id)].value; }
		const int& Value(typename EnvParamIds::Int id) const { return int_data[size_t(id)].value; }
		bool& IsPresent(typename EnvParamIds::Int id) { return int_data[size_t(id)].is_present; }
		const int IsPresent(typename EnvParamIds::Int id) const { return int_data[size_t(id)].is_present; }

		using WeightedStrings = WeightedArray<std::wstring, 2>;
		WeightedStrings &Value(typename EnvParamIds::String id) { return string_data[size_t(id)].value; }
		const WeightedStrings &Value(typename EnvParamIds::String id) const { return string_data[size_t(id)].value; }
		bool &IsPresent(typename EnvParamIds::String id) { return string_data[size_t(id)].is_present; }
		const bool IsPresent(typename EnvParamIds::String id) const { return string_data[size_t(id)].is_present; }

		std::vector<std::shared_ptr< Renderer::DrawCalls::EffectInstance > > &Value(typename EnvParamIds::EffectArray id) { return effect_array_data[size_t(id)].value; }
		const std::vector<std::shared_ptr< Renderer::DrawCalls::EffectInstance > > &Value(typename EnvParamIds::EffectArray id) const { return effect_array_data[size_t(id)].value; }
		bool &IsPresent(typename EnvParamIds::EffectArray id) { return effect_array_data[size_t(id)].is_present; }
		const bool IsPresent(typename EnvParamIds::EffectArray id) const { return effect_array_data[size_t(id)].is_present; }

		std::vector< ::Animation::AnimatedObjectTypeHandle > &Value(typename EnvParamIds::ObjectArray id) { return object_array_data[size_t(id)].value; }
		const std::vector< ::Animation::AnimatedObjectTypeHandle > &Value(typename EnvParamIds::ObjectArray id) const { return object_array_data[size_t(id)].value; }
		bool &IsPresent(typename EnvParamIds::ObjectArray id) { return object_array_data[size_t(id)].is_present; }
		const bool IsPresent(typename EnvParamIds::ObjectArray id) const { return object_array_data[size_t(id)].is_present; }

		std::vector< FootstepData >& Value( typename EnvParamIds::FootstepArray id ) { return footstep_array_data[ size_t( id ) ].value; }
		const std::vector< FootstepData >& Value( typename EnvParamIds::FootstepArray id ) const { return footstep_array_data[ size_t( id ) ].value; }
		bool& IsPresent( typename EnvParamIds::FootstepArray id ) { return footstep_array_data[ size_t( id ) ].is_present; }
		const bool IsPresent( typename EnvParamIds::FootstepArray id ) const { return footstep_array_data[ size_t( id ) ].is_present; }
	private:
		struct FloatValue
		{
			float value = 0;
			bool is_present = false;
		};
		FloatValue float_data[(unsigned)EnvParamIds::Float::Count];

		struct PeriodicValue
		{
			float value = 0;
			bool is_present = false;
		};
		PeriodicValue periodic_data[(unsigned)EnvParamIds::Periodic::Count];

		struct Vector3Value
		{
			simd::vector3 value = simd::vector3(0.0f, 0.0f, 0.0f);
			bool is_present = false;
		};
		Vector3Value vector3_data[(unsigned)EnvParamIds::Vector3::Count];

		struct BoolValue
		{
			bool value = false;
			bool is_present = false;
		};
		BoolValue bool_data[(unsigned)EnvParamIds::Bool::Count];

		struct IntValue
		{
			int value = 0;
			bool is_present = false;
		};
		IntValue int_data[(unsigned)EnvParamIds::Int::Count];

		struct StringValue
		{
			WeightedStrings value;
			bool is_present = false;
		};
		StringValue string_data[(unsigned)EnvParamIds::String::Count];

		struct EffectArrayValue
		{
			std::vector<std::shared_ptr< Renderer::DrawCalls::EffectInstance > > value;
			bool is_present = false;
		};
		EffectArrayValue effect_array_data[(unsigned)EnvParamIds::EffectArray::Count];

		struct ObjectArrayValue
		{
			std::vector< ::Animation::AnimatedObjectTypeHandle > value;
			bool is_present = false;
		};
		ObjectArrayValue object_array_data[(unsigned)EnvParamIds::ObjectArray::Count];

		struct FootstepArrayValue
		{
			std::vector< FootstepData > value;
			bool is_present = false;
		};
		FootstepArrayValue footstep_array_data[ (unsigned)EnvParamIds::FootstepArray::Count ];
	};

	struct SettingsBlock
	{
		void AddParam( EnvParamIds::Float id, std::wstring member_name, std::wstring section_name, float def_value );
		void AddParam( EnvParamIds::Periodic id, std::wstring member_name, std::wstring section_name, float def_value );
		void AddParam( EnvParamIds::Vector3 id, std::wstring member_name, std::wstring section_name, simd::vector3 def_value );
		void AddParam( EnvParamIds::Bool id, std::wstring member_name, std::wstring section_name, bool def_value );
		void AddParam( EnvParamIds::Int id, std::wstring member_name, std::wstring section_name, int def_value );
		void AddParam( EnvParamIds::String id, std::wstring member_name, std::wstring section_name );
		void AddParam( EnvParamIds::EffectArray id, std::wstring member_name, std::wstring section_name );
		void AddParam( EnvParamIds::ObjectArray id, std::wstring member_name, std::wstring section_name );
		void AddParam( EnvParamIds::FootstepArray id, std::wstring member_name, std::wstring section_name );

		const auto GetDefaultValue( EnvParamIds::Float id ) const { return float_params[ ( unsigned )id ]->default_value; }
		const auto GetDefaultValue( EnvParamIds::Periodic id ) const { return periodic_params[ ( unsigned )id ]->default_value; }
		const auto GetDefaultValue( EnvParamIds::Vector3 id ) const { return vector3_params[ ( unsigned )id ]->default_value; }
		const auto GetDefaultValue( EnvParamIds::Bool id ) const { return bool_params[ ( unsigned )id ]->default_value; }
		const auto GetDefaultValue( EnvParamIds::Int id ) const { return int_params[ ( unsigned )id ]->default_value; }
		const auto GetDefaultValue( EnvParamIds::String id ) const { return string_params[ ( unsigned )id ]->default_value; }
		const auto GetDefaultValue( EnvParamIds::EffectArray id ) const { return effect_array_params[ ( unsigned )id ]->default_value; }
		const auto GetDefaultValue( EnvParamIds::ObjectArray id ) const { return object_array_params[ ( unsigned )id ]->default_value; }
		const auto GetDefaultValue( EnvParamIds::FootstepArray id ) const { return footstep_array_params[ ( unsigned )id ]->default_value; }

		bool IsSectionPresent( const Data& data, const std::wstring_view section_name ) const;
		void Clear( Data& data ) const;
		void ClearSection( Data& data, const std::wstring_view section_name ) const;

		void Serialize(JsonValue &dst_value, const Data &src_data, JsonAllocator &allocator) const;
		void Deserialize(const JsonValue &src_value, Data &dst_data, const bool set_defaults);

		static void BlendData(Data **datum, float *weights, size_t datum_count, BlendTypes blend_type, Data *dst_data);
	private:
		static void Serialize( JsonValue& dst_value, const std::vector< FootstepData >& src_param, JsonAllocator& allocator );
		static void Deserialize( const JsonValue& src_value, std::vector< FootstepData >& dst_param );
		static void Serialize( JsonValue& dst_value, const std::vector< ::Animation::AnimatedObjectTypeHandle >& src_param, JsonAllocator& allocator );
		static void Deserialize( const JsonValue& src_value, std::vector< ::Animation::AnimatedObjectTypeHandle >& dst_param );
		static void Serialize( JsonValue& dst_value, const std::vector< std::shared_ptr< Renderer::DrawCalls::EffectInstance > >& src_param, JsonAllocator& allocator );
		static void Deserialize( const JsonValue& src_value, std::vector< std::shared_ptr< Renderer::DrawCalls::EffectInstance > >& dst_param );
		static void Serialize( JsonValue& dst_value, bool src_param, JsonAllocator& allocator );
		static void Deserialize( const JsonValue& src_value, bool& dst_param );
		static void Serialize( JsonValue& dst_value, int src_param, JsonAllocator& allocator );
		static void Deserialize( const JsonValue& src_value, int& dst_param );
		static void Serialize( JsonValue& dst_value, const simd::vector3& src_param, JsonAllocator& allocator );
		static void Deserialize( const JsonValue& src_value, simd::vector3& dst_param );
		static void Serialize(JsonValue& dst_value, const simd::vector4& src_param, JsonAllocator& allocator);
		static void Deserialize(const JsonValue& src_value, simd::vector4& dst_param);
		static void Serialize( JsonValue& dst_value, const std::wstring& src_param, JsonAllocator& allocator );
		static void Deserialize( const JsonValue& src_value, std::wstring& dst_param );
		static void Serialize( JsonValue& dst_value, const Data::WeightedStrings& src_param, JsonAllocator& allocator );
		static void Deserialize( const JsonValue& dst_value, Data::WeightedStrings& src_param );
		static void Serialize( JsonValue& dst_value, const float& src_param, JsonAllocator& allocator );
		static void Deserialize( const JsonValue& src_value, float& dst_param );

		struct BaseParameter
		{
			BaseParameter( std::wstring _section, std::wstring _member ) : section( std::move( _section ) ), member( std::move( _member ) ) {}
			std::wstring section;
			std::wstring member;

			virtual void Serialize( JsonValue& dst_value, const Data& src_data, JsonAllocator& allocator ) const = 0;
			virtual void Deserialize( const JsonValue& src_value, Data& src_data, const bool set_defaults ) const = 0;
			virtual bool IsPresent( const Data& data ) const = 0;
			virtual void Reset( Data& data ) const = 0;
		};

		template< typename IdType, typename ValueType >
		struct EnvironmentParameter : public BaseParameter
		{
			EnvironmentParameter( std::wstring section, std::wstring member, IdType param_id, ValueType default_value = ValueType() )
				: BaseParameter( std::move( section ), std::move( member ) )
				, param_id( param_id )
				, default_value( default_value )
			{
			}

			IdType param_id;
			ValueType default_value;

			void Serialize( JsonValue& dst_value, const Data& src_data, JsonAllocator& allocator ) const final
			{
				if( src_data.IsPresent( param_id ) )
					SettingsBlock::Serialize( dst_value, src_data.Value( param_id ), allocator );
			}

			void Deserialize( const JsonValue& src_value, Data& dst_data, const bool set_defaults ) const final
			{
				auto& is_present = dst_data.IsPresent( param_id );
				auto& value = dst_data.Value( param_id );
				if( src_value.HasMember( section.c_str() ) )
				{
					const JsonValue& section_value = src_value[ section.c_str() ];
					if( section_value.HasMember( member.c_str() ) )
					{
						const JsonValue& member_value = section_value[ member.c_str() ];
						SettingsBlock::Deserialize( member_value, value );
						is_present = true;
						return;
					}
				}
				if( set_defaults )
				{
					is_present = false;
					value = default_value;
				}
			}

			bool IsPresent( const Data& data ) const final
			{
				return data.IsPresent( param_id );
			}

			void Reset( Data& data ) const final
			{
				data.IsPresent( param_id ) = false;
				data.Value( param_id ) = default_value;
			}
		};

		using FloatParameter = EnvironmentParameter< EnvParamIds::Float, float >;
		using PeriodicParameter = EnvironmentParameter< EnvParamIds::Periodic, float >;
		using Vector3Parameter = EnvironmentParameter< EnvParamIds::Vector3, simd::vector3 >;
		using BoolParameter = EnvironmentParameter< EnvParamIds::Bool, bool >;
		using IntParameter = EnvironmentParameter< EnvParamIds::Int, int >;
		using StringParameter = EnvironmentParameter< EnvParamIds::String, Data::WeightedStrings >;
		using EffectArrayParameter = EnvironmentParameter< EnvParamIds::EffectArray, std::vector< std::shared_ptr< Renderer::DrawCalls::EffectInstance > > >;
		using ObjectArrayParameter = EnvironmentParameter< EnvParamIds::ObjectArray, std::vector< ::Animation::AnimatedObjectTypeHandle > >;
		using FootstepArrayParameter = EnvironmentParameter< EnvParamIds::FootstepArray, std::vector< FootstepData > >;

		std::unique_ptr< FloatParameter > float_params[(unsigned)EnvParamIds::Float::Count];
		std::unique_ptr< PeriodicParameter > periodic_params[(unsigned)EnvParamIds::Periodic::Count];
		std::unique_ptr< Vector3Parameter > vector3_params[(unsigned)EnvParamIds::Vector3::Count];
		std::unique_ptr< BoolParameter > bool_params[(unsigned)EnvParamIds::Bool::Count];
		std::unique_ptr< IntParameter > int_params[(unsigned)EnvParamIds::Int::Count];
		std::unique_ptr< StringParameter > string_params[(unsigned)EnvParamIds::String::Count];
		std::unique_ptr< EffectArrayParameter > effect_array_params[(unsigned)EnvParamIds::EffectArray::Count];
		std::unique_ptr< ObjectArrayParameter > object_array_params[(unsigned)EnvParamIds::ObjectArray::Count];
		std::unique_ptr< FootstepArrayParameter > footstep_array_params[(unsigned)EnvParamIds::FootstepArray::Count];

		std::vector< BaseParameter* > param_info;
	};

	struct EmptyParams
	{
		enum struct Float { Count = 1 };
		enum struct Periodic { Count = 1 };
		enum struct Vector3 { Count = 1 };
		enum struct Bool { Count = 1 };
		enum struct Int { Count = 1 };
		enum struct String { Count = 1 };
		enum struct VolumeTexData { Count = 1 };
		enum struct EffectArray { Count = 1 };
		enum struct ObjectArray { Count = 1 };
		enum struct FootstepArray { Count = 1 };
	};
}