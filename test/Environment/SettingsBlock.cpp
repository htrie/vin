#include "SettingsBlock.h"
#include "Common/Loaders/CEFootstepAudio.h"

namespace Environment
{
	void SettingsBlock::AddParam( EnvParamIds::Float id, std::wstring member_name, std::wstring section_name, float def_value )
	{
		auto& param = float_params[ size_t( id ) ];
		param = std::make_unique< FloatParameter >( section_name, member_name, id, def_value );
		param_info.push_back( param.get() );
	}

	void SettingsBlock::AddParam( EnvParamIds::Periodic id, std::wstring member_name, std::wstring section_name, float def_value )
	{
		auto& param = periodic_params[ size_t( id ) ];
		param = std::make_unique< PeriodicParameter >( section_name, member_name, id, def_value );
		param_info.push_back( param.get() );
	}

	void SettingsBlock::AddParam( EnvParamIds::Vector3 id, std::wstring member_name, std::wstring section_name, simd::vector3 def_value )
	{
		auto& param = vector3_params[ size_t( id ) ];
		param = std::make_unique< Vector3Parameter >( section_name, member_name, id, def_value );
		param_info.push_back( param.get() );
	}

	void SettingsBlock::AddParam( EnvParamIds::Bool id, std::wstring member_name, std::wstring section_name, bool def_value )
	{
		auto& param = bool_params[ size_t( id ) ];
		param = std::make_unique< BoolParameter >( section_name, member_name, id, def_value );
		param_info.push_back( param.get() );
	}

	void SettingsBlock::AddParam( EnvParamIds::Int id, std::wstring member_name, std::wstring section_name, int def_value )
	{
		auto& param = int_params[ size_t( id ) ];
		param = std::make_unique< IntParameter >( section_name, member_name, id, def_value );
		param_info.push_back( param.get() );
	}

	void SettingsBlock::AddParam( EnvParamIds::String id, std::wstring member_name, std::wstring section_name )
	{
		auto& param = string_params[ size_t( id ) ];
		param = std::make_unique< StringParameter >( section_name, member_name, id );
		param_info.push_back( param.get() );
	}

	void SettingsBlock::AddParam( EnvParamIds::EffectArray id, std::wstring member_name, std::wstring section_name )
	{
		auto& param = effect_array_params[ size_t( id ) ];
		param = std::make_unique< EffectArrayParameter >( section_name, member_name, id );
		param_info.push_back( param.get() );
	}

	void SettingsBlock::AddParam( EnvParamIds::ObjectArray id, std::wstring member_name, std::wstring section_name )
	{
		auto& param = object_array_params[ size_t( id ) ];
		param = std::make_unique< ObjectArrayParameter >( section_name, member_name, id );
		param_info.push_back( param.get() );
	}

	void SettingsBlock::AddParam( EnvParamIds::FootstepArray id, std::wstring member_name, std::wstring section_name )
	{
		auto& param = footstep_array_params[ size_t( id ) ];
		param = std::make_unique< FootstepArrayParameter >( section_name, member_name, id );
		param_info.push_back( param.get() );
	}
	
	bool SettingsBlock::IsSectionPresent( const Data& data, const std::wstring_view section_name ) const
	{
		for( const auto& param : param_info )
			if( param->section == section_name && param->IsPresent( data ) )
				return true;
		return false;
	}

	void SettingsBlock::Clear( Data& data ) const
	{
		for( const auto& param : param_info )
			param->Reset( data );
	}

	void SettingsBlock::ClearSection( Data& data, const std::wstring_view section_name ) const
	{
		for( const auto& param : param_info )
			if( param->section == section_name )
				param->Reset( data );
	}

	void SettingsBlock::Serialize(JsonValue& dst_value, const Data& src_data, JsonAllocator& allocator) const
	{
		dst_value.SetObject();

		std::map<std::wstring, JsonValue> section_values;
		std::vector<std::wstring> section_names;

		auto GetSection = [&section_values, &section_names](std::wstring section_name) -> JsonValue&
		{
			auto it = section_values.find(section_name);
			if (it == section_values.end())
			{
				section_values[section_name].SetObject();
				section_names.push_back(section_name);
			}
			return section_values[section_name];
		};

		for (const auto& param : param_info)
		{
			JsonValue param_value;
			param->Serialize( param_value, src_data, allocator );
			if (!param_value.IsNull())
				GetSection( param->section.c_str()).AddMember(JsonValue().SetString( param->member.c_str(), allocator), param_value.Move(), allocator);
		}

		for (const auto& section_name : section_names)
		{
			const wchar_t *name = section_name.c_str();
			auto &section_value = GetSection(section_name);
			if (!section_value.Empty())
				dst_value.AddMember(JsonValue().SetString(name, allocator), section_value.Move(), allocator);
		}
	}
	void SettingsBlock::Deserialize(const JsonValue& src_value, Data& dst_data, const bool set_defaults)
	{
		if (src_value.IsNull())
		{
			dst_data = Data();
			return;
		}
		for (const auto& param : param_info)
		{
			param->Deserialize( src_value, dst_data, set_defaults );
		}
	}

	template<typename ParamType, typename ParamId>
	static void LerpParamBlend(Data ** datum, float * weights, size_t datum_count, Data *dst_data, ParamId param_id, BlendTypes blend_type)
	{
		ParamType sum_val = 0.0f;
		float sum_weight = 0.0f;

		switch (blend_type)
		{
			case BlendTypes::Linear:
			{
				dst_data->IsPresent(param_id) = false;
				for (size_t data_id = 0; data_id < datum_count; data_id++)
				{
					sum_val += datum[data_id]->Value(param_id) * weights[data_id];
					sum_weight += weights[data_id];
					dst_data->IsPresent(param_id) |= datum[data_id]->IsPresent(param_id);
				}
				dst_data->Value(param_id) = sum_val / (sum_weight + 1e-7f);
			}break;
			case BlendTypes::Exclusive:
			{
				for (size_t data_id = 0; data_id < datum_count; data_id++)
				{
					if (datum[data_id]->IsPresent(param_id))
					{
						dst_data->Value(param_id) =
							dst_data->Value(param_id) * (1.0f - weights[data_id]) +
							datum[data_id]->Value(param_id) * weights[data_id];
					}
				}
			}break;
		}
	}

	float frac(float v)
	{
		return v - floor(v);
	}
	float GetPeriodicDst(float ref_val, float dst_val, float period_min, float period_max)
	{
		float period = period_max - period_min;
		float norm_ref = (ref_val - period_min) / period;
		float norm_dst = (dst_val - period_min) / period;
		float delta = frac(norm_dst) - frac(norm_ref);
		if (delta > 0.5f) delta -= 1.0f;
		if (delta < -0.5f) delta += 1.0f;
		
		float norm_closest_dst = norm_ref + delta;
		return period_min + norm_closest_dst * period;
	}

	template<typename ParamId>
	static void LerpParamPeriodicBlend(Data ** datum, float * weights, size_t datum_count, Data *dst_data, ParamId param_id, BlendTypes blend_type)
	{
		float sum_val = 0.0f;
		float sum_weight = 0.0f;

		float ref_val = dst_data->Value(param_id);
		if (blend_type == BlendTypes::Linear && datum_count > 0)
		{
			ref_val = datum[0]->Value(param_id);
		}
		float period_min = -PI; //works only with radians for now
		float period_max = PI;

		switch (blend_type)
		{
			case BlendTypes::Linear:
			{
				dst_data->IsPresent(param_id) = false;
				for (size_t data_id = 0; data_id < datum_count; data_id++)
				{
					sum_val += GetPeriodicDst(ref_val, datum[data_id]->Value(param_id), period_min, period_max) * weights[data_id];
					sum_weight += weights[data_id];
					dst_data->IsPresent(param_id) |= datum[data_id]->IsPresent(param_id);
				}
				dst_data->Value(param_id) = sum_val / (sum_weight + 1e-7f);
			}break;
			case BlendTypes::Exclusive:
			{
				for (size_t data_id = 0; data_id < datum_count; data_id++)
				{
					if (datum[data_id]->IsPresent(param_id))
					{
						dst_data->Value(param_id) =
							dst_data->Value(param_id) * (1.0f - weights[data_id]) +
							GetPeriodicDst(ref_val, datum[data_id]->Value(param_id), period_min, period_max) * weights[data_id];
					}
				}
			}break;
		}
	}

	template<typename ParamType, size_t size, typename ParamId>
	static void LerpParamArrayBlend(Data ** datum, float * weights, size_t datum_count, Data *dst_data, ParamId param_id, BlendTypes blend_type)
	{
		if (blend_type == BlendTypes::Linear)
		{
			dst_data->IsPresent(param_id) = false;
			for (size_t data_id = 0; data_id < datum_count; data_id++)
			{
				dst_data->IsPresent(param_id) |= datum[data_id]->IsPresent(param_id);
			}
		}
		std::map<ParamType, float> param_weights; //todo: replace with faster container, size is usually 5..15

		float remaining_weight = 1.0f;
		for (size_t data_number = 0; data_number < datum_count; data_number++)
		{
			size_t data_id = datum_count - 1 - data_number;
			if (datum[data_id]->IsPresent(param_id) || blend_type == BlendTypes::Linear)
			{
				for (size_t index = 0; index < size; index++)
				{
					const auto &param_array = datum[data_id]->Value(param_id);
					param_weights[param_array.values[index]] += remaining_weight * weights[data_id] * param_array.weights[index];
				}
				if (blend_type == BlendTypes::Exclusive)
					remaining_weight *= (1.0f - weights[data_id]);
			}
		}
		if (blend_type == BlendTypes::Exclusive)
		{
			for (size_t index = 0; index < size; index++)
			{
				param_weights[dst_data->Value(param_id).Get(index)] += dst_data->Value(param_id).weights[index] * remaining_weight;
			}
		}

		struct ParamDatum
		{
			ParamType value{};
			float weight = 0.0f;
		};
		Memory::SmallVector< ParamDatum, 15, Memory::Tag::Game > param_data;
		for (const auto& it : param_weights)
		{
			ParamDatum param_datum;
			param_datum.value = it.first;
			param_datum.weight = it.second;
			param_data.push_back(std::move( param_datum ));
		}
		std::sort( param_data.begin(), param_data.end(), [](const ParamDatum& left, const ParamDatum& right) -> bool {return left.weight > right.weight; });

		float total_weight = 0.0f;
		for (size_t index = 0; index < size; index++)
		{
			if (index < param_data.size())
			{
				total_weight += param_data[index].weight;
				dst_data->Value(param_id).values[index] = param_data[index].value;
				dst_data->Value(param_id).weights[index] = param_data[index].weight;
			}
			else
			{
				dst_data->Value(param_id).values[index] = ParamType();
				dst_data->Value(param_id).weights[index] = 0.0f;
			}
		}
		for (size_t index = 0; index < size; index++)
		{
			dst_data->Value(param_id).weights[index] /= (total_weight + 1e-7f);
		}
	}

	template<typename ParamType, typename ParamId>
	static void DiscreteParamBlend(Data ** datum, float * weights, size_t datum_count, Data *dst_data, ParamId param_id, BlendTypes blend_type)
	{
		ParamType sum_val = ParamType(0);
		float sum_weight = 0.0f;

		switch (blend_type)
		{
		case BlendTypes::Linear:
		{
			dst_data->IsPresent(param_id) = false;
			size_t max_id = size_t(-1);
			for (size_t data_id = 0; data_id < datum_count; data_id++)
			{
				if (max_id == size_t(-1) || (weights[data_id] > weights[max_id]))
					max_id = data_id;
			}
			if (max_id != size_t(-1))
			{
				dst_data->Value(param_id) = datum[max_id]->Value(param_id);
				dst_data->IsPresent(param_id) = datum[max_id]->IsPresent(param_id);
			}
		}break;
		case BlendTypes::Exclusive:
		{
			for (size_t data_id = 0; data_id < datum_count; data_id++)
			{
				if (weights[data_id] > 0.5f && datum[data_id]->IsPresent(param_id))
				{
					dst_data->Value(param_id) = datum[data_id]->Value(param_id);
				}
			}
		}break;
		}
	}

	void SettingsBlock::BlendData(Data** datum, float* weights, size_t datum_count, BlendTypes blend_type, Data* dst_data)
	{
		for (size_t id = 0; id < size_t(EnvParamIds::Periodic::Count); id++)
		{
			LerpParamPeriodicBlend<EnvParamIds::Periodic>(datum, weights, datum_count, dst_data, EnvParamIds::Periodic(id), blend_type);
		}

		for (size_t id = 0; id < size_t(EnvParamIds::Float::Count); id++)
		{
			LerpParamBlend<float, EnvParamIds::Float>(datum, weights, datum_count, dst_data, EnvParamIds::Float(id), blend_type);
		}

		for (size_t id = 0; id < size_t(EnvParamIds::Vector3::Count); id++)
		{
			LerpParamBlend<simd::vector3, EnvParamIds::Vector3>(datum, weights, datum_count, dst_data, EnvParamIds::Vector3(id), blend_type);
		}

		for (size_t id = 0; id < size_t(EnvParamIds::Bool::Count); id++)
		{
			DiscreteParamBlend<bool, EnvParamIds::Bool>(datum, weights, datum_count, dst_data, EnvParamIds::Bool(id), blend_type);
		}
		for (size_t id = 0; id < size_t(EnvParamIds::Int::Count); id++)
		{
			DiscreteParamBlend<int, EnvParamIds::Int>(datum, weights, datum_count, dst_data, EnvParamIds::Int(id), blend_type);
		}
		for (size_t id = 0; id < size_t(EnvParamIds::String::Count); id++)
		{
			LerpParamArrayBlend<std::wstring, 2, EnvParamIds::String>(datum, weights, datum_count, dst_data, EnvParamIds::String(id), blend_type);
		}
		for (size_t id = 0; id < size_t(EnvParamIds::EffectArray::Count); id++)
		{
			DiscreteParamBlend<bool, EnvParamIds::EffectArray>(datum, weights, datum_count, dst_data, EnvParamIds::EffectArray(id), blend_type);
		}
		for (size_t id = 0; id < size_t(EnvParamIds::ObjectArray::Count); id++)
		{
			DiscreteParamBlend<bool, EnvParamIds::ObjectArray>(datum, weights, datum_count, dst_data, EnvParamIds::ObjectArray(id), blend_type);
		}
		for( size_t id = 0; id < size_t( EnvParamIds::FootstepArray::Count ); id++ )
		{
			DiscreteParamBlend<bool, EnvParamIds::FootstepArray>( datum, weights, datum_count, dst_data, EnvParamIds::FootstepArray( id ), blend_type );
		}
	}

	void SettingsBlock::Serialize(JsonValue& dst_value, bool src_param, JsonAllocator& allocator)
	{
		dst_value.SetBool(src_param);
	}
	void SettingsBlock::Deserialize(const JsonValue& src_value, bool& dst_param)
	{
		dst_param = src_value.GetBool();
	}

	void SettingsBlock::Serialize(JsonValue& dst_value, int src_param, JsonAllocator& allocator)
	{
		dst_value.SetInt(src_param);
	}
	void SettingsBlock::Deserialize(const JsonValue& src_value, int& dst_param)
	{
		dst_param = src_value.GetInt();
	}

	void SettingsBlock::Serialize(JsonValue& dst_value, const simd::vector3& src_param, JsonAllocator& allocator)
	{
		dst_value.SetArray()
			.PushBack(JsonValue(src_param.x).Move(), allocator)
			.PushBack(JsonValue(src_param.y).Move(), allocator)
			.PushBack(JsonValue(src_param.z).Move(), allocator);
	}
	void SettingsBlock::Deserialize(const JsonValue& src_value, simd::vector3& dst_param)
	{
		dst_param = simd::vector3(
			src_value[0].GetFloat(),
			src_value[1].GetFloat(),
			src_value[2].GetFloat());
	}
	void SettingsBlock::Serialize(JsonValue& dst_value, const simd::vector4& src_param, JsonAllocator& allocator)
	{
		dst_value.SetArray()
			.PushBack(JsonValue(src_param.x).Move(), allocator)
			.PushBack(JsonValue(src_param.y).Move(), allocator)
			.PushBack(JsonValue(src_param.z).Move(), allocator)
			.PushBack(JsonValue(src_param.w).Move(), allocator);
	}
	void SettingsBlock::Deserialize(const JsonValue& src_value, simd::vector4& dst_param)
	{
		dst_param = simd::vector4(
			src_value[0].GetFloat(),
			src_value[1].GetFloat(),
			src_value[2].GetFloat(),
			src_value[3].GetFloat());
	}
	void SettingsBlock::Serialize(JsonValue& dst_value, const std::wstring& src_param, JsonAllocator& allocator)
	{
		dst_value.SetString(src_param.c_str(), allocator);
	}
	void SettingsBlock::Deserialize(const JsonValue& src_value, std::wstring& dst_param)
	{
		dst_param = src_value.GetString();
	}
	void SettingsBlock::Serialize( JsonValue& dst_value, const Data::WeightedStrings& src_param, JsonAllocator& allocator )
	{
		dst_value.SetString( src_param.Get().c_str(), allocator );
	}
	void SettingsBlock::Deserialize( const JsonValue& src_value, Data::WeightedStrings& dst_param )
	{
		dst_param.Get() = src_value.GetString();
	}
	void SettingsBlock::Serialize(JsonValue& dst_value, const float& src_param, JsonAllocator& allocator)
	{
		dst_value.SetFloat(src_param);
	}
	void SettingsBlock::Deserialize(const JsonValue& src_value, float& dst_param)
	{
		dst_param = src_value.GetFloat();
	}
	void SettingsBlock::Serialize(JsonValue& dst_value, const std::vector<Animation::AnimatedObjectTypeHandle>& src_param, JsonAllocator& allocator)
	{
		dst_value.SetArray();
		for (const auto& object : src_param)
		{
			dst_value.PushBack(JsonValue().SetString(object.GetFilename().c_str(), allocator).Move(), allocator);
		}
	}
	void SettingsBlock::Deserialize(const JsonValue& src_value, std::vector<Animation::AnimatedObjectTypeHandle>& dst_param)
	{
		dst_param.clear();
		for (const auto &effect_name : src_value.GetArray())
		{
			dst_param.push_back(Animation::AnimatedObjectTypeHandle(effect_name.GetString()));
		}
	}
	void SettingsBlock::Serialize( JsonValue& dst_value, const std::vector< std::shared_ptr< Renderer::DrawCalls::EffectInstance > >& src_param, JsonAllocator& allocator )
	{
		// not implemented
	}
	void SettingsBlock::Deserialize( const JsonValue& src_value, std::vector< std::shared_ptr< Renderer::DrawCalls::EffectInstance > >& dst_param )
	{
		// not implemented
	}
	void SettingsBlock::Serialize( JsonValue& dst_value, const std::vector< FootstepData >& src_param, JsonAllocator& allocator )
	{
		dst_value.SetArray();
		for( const auto& data : src_param )
		{
			if( data.material.IsNull() || data.audio_index == 0 )
				continue;

			const auto audio = Loaders::FootstepAudio::GetDataRowByIndex( data.audio_index );
			if( audio.IsNull() )
				continue;

			JsonValue value;
			value.SetObject();
			JsonValue material_filename( data.material.GetFilename().c_str(), allocator );
			JsonValue audio_name( audio->GetId(), allocator );
			value.AddMember( L"material", material_filename, allocator );
			value.AddMember( L"audio_id", audio_name, allocator );
			
			dst_value.PushBack( value, allocator );
		}
	}
	void SettingsBlock::Deserialize( const JsonValue& src_value, std::vector< FootstepData >& dst_param )
	{
		dst_param.clear();
		for( const auto& effect : src_value.GetArray() )
		{
			const wchar_t* material_filename = effect[L"material"].GetString();
			const auto footstep_param = Loaders::FootstepAudio::GetDataRowByKey( effect[L"audio_id"].GetString() );
			const int audio_index = footstep_param.IsNull() ? 0 : ( int )Loaders::GetDataRowIndex( footstep_param );
			dst_param.push_back( { MaterialHandle( material_filename ), audio_index } );
		}
	}
}
