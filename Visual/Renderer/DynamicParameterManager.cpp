#include "Visual/Renderer/DrawCalls/EffectGraphNode.h"
#include "Visual/Renderer/DrawCalls/EffectGraph.h"
#include "Visual/Renderer/EffectGraphSystem.h"

#include "DynamicParameterManager.h"
#include "DynamicParameterFunctions.h"
#include "DynamicParameterRegistration.h"

namespace Renderer
{
	DynamicParameterInfo::DynamicParameterInfo(const std::string& name, DrawCalls::GraphType data_type, const DynamicParameterInfo::Flags& flags )
		: name(name), data_type(data_type), flags( flags )
	{
		
	}

	void DynamicParameterManager::RegisterFunctions(const DynamicParamFunction* begin, const DynamicParamFunction* end)
	{
		for (auto f = begin; f != end; ++f)
		{
			const unsigned hash = DrawCalls::EffectGraphUtil::HashString(f->name);
			const auto found = dynamic_parameter_infos.find(hash);
			if (found != dynamic_parameter_infos.end())
				found->second.func = f->func;
		}
	}

	const DynamicParameterInfo* DynamicParameterManager::GetParamInfo( unsigned hash ) const
	{
		const auto found = dynamic_parameter_infos.find(hash);
		if (found != dynamic_parameter_infos.end())
			return &found->second;
		return nullptr;
	}

	void DynamicParameterManager::AddParamInfo(const std::string& name, const DrawCalls::GraphType type, const DynamicParameterInfo::Flags& flags )
	{
		const unsigned hash = DrawCalls::EffectGraphUtil::HashString(name);
		dynamic_parameter_infos.emplace( hash, DynamicParameterInfo( name, type, flags ) );
	}

	DynamicParameterManager& GetDynamicParameterManager()
	{
		static DynamicParameterManager manager;
		return manager;
	}

	// register all the dynamic parameters here
	DynamicParameterManager::DynamicParameterManager()
	{
#define X( NAME, DRAWCALL, DATA, ... ) AddParamInfo( #NAME, DRAWCALL, Network::BitsetWithBitSet< DynamicParameterInfo::NumFlags >( __VA_ARGS__ ) );
		CORE_FUNCTIONS
		CLIENT_FUNCTIONS
#undef X
		RegisterFunctions( Game::core_functions, Game::core_functions_end );
	}


	uint64_t DynamicParameter::ToIdHash() const
	{
		return Device::IdHash(id, 0);
	}

	DrawCalls::Uniform DynamicParameter::ToUniform() const
	{
		switch (type)
		{
		case DrawCalls::GraphType::Bool: return *param->AsBool();
		case DrawCalls::GraphType::Int: return *param->AsInt(); break;
		case DrawCalls::GraphType::UInt: return unsigned(std::max(0, *param->AsInt())); break;
		case DrawCalls::GraphType::Float: return *param->AsFloat(); break;
		case DrawCalls::GraphType::Float2:
		case DrawCalls::GraphType::Float3:
		case DrawCalls::GraphType::Float4: return *param->AsVector(); break;
		case DrawCalls::GraphType::Float4x4: return *param->AsMatrix(); break;
		default: throw std::runtime_error("Invalid dynamic parameter type");
		}
	}

	void DynamicParameterLocalStorage::Append(Renderer::DynamicParameters& dynamic_parameters, const DrawCalls::EffectGraph& graph)
	{
		graph.ProcessDynamicParams([&](const auto& info)
		{
			auto found = param_infos.find(info->data_id);
			if (found == param_infos.end())
			{
				assert( info->func );
				auto new_info = ParamInfo(info);
				found = param_infos.emplace(info->data_id, std::move(new_info)).first;
			}

			const auto found_param = std::find_if(dynamic_parameters.begin(), dynamic_parameters.end(), [&](const auto& parameter) { return parameter.id == found->second.info->data_id; });
			if (found_param == dynamic_parameters.end())
				dynamic_parameters.emplace_back(found->second.info->data_id, found->second.info->data_type, &found->second.param);
		});

		graph.ProcessCustomDynamicParams([&](const auto& id)
		{
			auto found = custom_param_infos.find(id);
			if (found == custom_param_infos.end())
			{
				auto new_info = CustomParamInfo(id);
				found = custom_param_infos.emplace(id, std::move(new_info)).first;
			}

			const auto found_param = std::find_if(dynamic_parameters.begin(), dynamic_parameters.end(), [&](const auto& parameter) { return parameter.id == found->second.id; });
			if (found_param == dynamic_parameters.end())
				dynamic_parameters.emplace_back(found->second.id, DrawCalls::GraphType::Float, &found->second.param);
		});
	}

} //namespace Renderer