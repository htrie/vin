#pragma once

#include <vector>
#include <memory>

#include "Common/Resource/Handle.h"
#include "Common/Utility/Geometric.h"
#include "ClientInstanceCommon/Animation/AnimatedObjectType.h"
#include "Visual/Particles/ParticleEffect.h"
#include "Visual/Utility/IMGUI/DebugGUIDefines.h"
#include "SettingsBlock.h"

namespace Audio
{
	struct GlobalSettings
	{
		float listener_height, listener_offset;
		std::wstring env_bank_name;
		std::wstring env_event_name;

		GlobalSettings()
			: env_bank_name(L"General_Environment.bank")
			, env_event_name(L"/Environment/FunctionNoReverb")
			, listener_height(4.5f)
			, listener_offset(0.0f)
		{}

		bool operator==(const GlobalSettings& other) const
		{
			return
				env_bank_name == other.env_bank_name &&
				env_event_name == other.env_event_name &&
				listener_height == other.listener_height &&
				listener_offset == other.listener_offset;
		}
	};
}

namespace Environment
{
	class EnvironmentSettingsV2;

	struct LoadedEnvironment
	{
		std::wstring name; //filename with extension
		Data env_data;

#if DEBUG_GUI_ENABLED
		std::string debug_text;
		float debug_weight = 0.0f;
#endif
	};

	class EnvironmentSettingsV2
	{
	public:
		EnvironmentSettingsV2();
		EnvironmentSettingsV2( const std::wstring& filename, Resource::Allocator& );

		SettingsBlock settings_block;

		void ClearData();
		LoadedEnvironment& FetchData( const std::wstring& filename );
		void RemoveData( const std::wstring filename );
		void ReloadData( const std::wstring& filename );
		void SaveData( const Data& src_data, const std::wstring& filename ) const;

		std::wstring CopyData( const Data& src_data, const wchar_t* section_id = nullptr ) const;
		void PasteData( Data& dst_data, const std::wstring& src_data );

		static void BlendData( Data** datum, float* weights, size_t count, Environment::BlendTypes blend_type, Data* dst_data );

		static void UpdateLightAngle( const Data& data, float dt );
		static std::pair<simd::vector3, simd::vector3> ComputeLightDirectionAndColour( const Data& data, const double time );

		static Audio::GlobalSettings GetGlobalAudioSettings( const Data& data );

		struct ColorGradingConversionTask
		{
			ColorGradingConversionTask() : is_updated( false ) {}
			std::wstring src_filename;
			std::wstring dst_filename;
			Data* dst_data;
			bool is_updated;
		} color_grading_conversion_task;

		struct RenderEnvCubemapTask
		{
			RenderEnvCubemapTask() : is_updated( false ) {}
			bool is_updated;
			bool include_transform;
			float offset = 140;
		} render_env_cubemap_task;

		const auto& GetLoadedData() const { return loaded_environments; }

		// Tools only
		std::wstring selected_environment;
		bool force_environment = false;
		bool auto_select_environment = true;

	private:
		std::vector< std::unique_ptr< LoadedEnvironment > > loaded_environments;
		Memory::FlatMap< std::wstring, LoadedEnvironment*, Memory::Tag::Environment > loaded_environment_map;
	};
}