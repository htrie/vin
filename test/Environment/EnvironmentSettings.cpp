#include <cmath>
#include <iomanip>

#include "EnvironmentSettings.h"

#include "Common/File/InputFileStream.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/uwofstream.h"
#include "Common/Utility/ContainerOperations.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>

// Environment audio versions
enum
{
	VER_AUDIO_FMOD_REVERB = 1,
	VER_AUDIO_FMOD_SNAPSHOT,
	VER_AUDIO_MAX
};
#define VER_AUDIO_LATEST  (VER_AUDIO_MAX - 1)

namespace Environment
{

	static float FixupZero(float f)
	{
		const bool subnormal = std::fpclassify(f) == FP_SUBNORMAL;
		const bool negative_or_tiny = f < 0.00001f;
		return (subnormal || negative_or_tiny) ? 0.0f : f;
	}

	static simd::vector3 ComputeDirection( const float phi, const float theta )
	{
		//phi is vertical, theta is horizontal
		const float sin_phi = sinf( phi );
		return -simd::vector3
		(
			cosf( theta ) * sin_phi,
			sinf( theta ) * sin_phi,
			cosf( phi )
		);
	}

	static simd::vector3 DesaturateColor(simd::vector3 rgb, float desaturation)
	{
		const simd::vector3 grayscale = simd::vector3(rgb.dot(simd::vector3(0.299f, 0.587f, 0.114f)));
		return Lerp(rgb, grayscale, desaturation);
	}

	static simd::vector3 FixupZero(const simd::vector3& c)
	{
		return simd::vector3(FixupZero(c.x), FixupZero(c.y), FixupZero(c.z));
	}

	EnvironmentSettingsV2::EnvironmentSettingsV2()
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Environment));

		{
			std::wstring block_name = L"directional_light";
			settings_block.AddParam(EnvParamIds::Bool::DirectionalLightShadowsEnabled, L"shadows_enabled", block_name, false);
			settings_block.AddParam(EnvParamIds::Bool::DirectionalLightCinematicMode, L"cinematic_mode", block_name, false);
			settings_block.AddParam(EnvParamIds::Vector3::DirectionalLightColour, L"colour", block_name, simd::vector3(1.0f, 1.0f, 1.0f));
			settings_block.AddParam(EnvParamIds::Float::DirectionalLightCamExtraZ, L"cam_extra_z", block_name, 1000.0f);
			settings_block.AddParam(EnvParamIds::Float::DirectionalLightCamFarPercent, L"cam_far_percent", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::DirectionalLightCamNearPercent, L"cam_near_percent", block_name, 0.0f);
			settings_block.AddParam(EnvParamIds::Float::DirectionalLightMinOffset, L"cam_min_offset", block_name, -200.0f);
			settings_block.AddParam(EnvParamIds::Float::DirectionalLightMaxOffset, L"cam_max_offset", block_name, 300.0f);
			settings_block.AddParam(EnvParamIds::Float::DirectionalLightMultiplier, L"multiplier", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::DirectionalLightPenumbraRadius, L"penumbra_radius", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::DirectionalLightPenumbraDist, L"penumbra_dist", block_name, 1e-5f);
			settings_block.AddParam(EnvParamIds::Float::DirectionalLightDesaturation, L"desaturation", block_name, 0.0f);
			settings_block.AddParam(EnvParamIds::Float::DirectionalLightPhi, L"phi", block_name, PI * 3.f / 4.f);
			settings_block.AddParam(EnvParamIds::Periodic::DirectionalLightTheta, L"theta", block_name, PI * 7.f / 4.f);
			settings_block.AddParam(EnvParamIds::Float::DirectionalLightDelta, L"delta", block_name, 0.0f );
		}

		{
			std::wstring block_name = L"player_light";
			settings_block.AddParam(EnvParamIds::Bool::PlayerLightShadowsEnabled, L"shadows_enabled", block_name, true);
			settings_block.AddParam(EnvParamIds::Vector3::PlayerLightColour, L"colour", block_name, simd::vector3(1.0f, 1.0f, 0.7f));
			settings_block.AddParam(EnvParamIds::Float::PlayerLightIntensity, L"intensity", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::PlayerLightPenumbra, L"penumbra", block_name, PI / 5);
		}

		{
			std::wstring block_name = L"post_processing";
			settings_block.AddParam(EnvParamIds::Float::PostProcessingBloomRadius, L"hdr_bloom_radius", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::PostProcessingBloomCutoff, L"hdr_bloom_cutoff", block_name, 0.0f);
			settings_block.AddParam(EnvParamIds::Float::PostProcessingBloomIntensity, L"hdr_bloom_intensity", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::PostProcessingOriginalIntensity, L"original_intensity", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::PostProcessingDoFFocusDistance, L"dof_focus_distance", block_name, 440.0f);
			settings_block.AddParam(EnvParamIds::Float::PostProcessingDoFFocusRange, L"dof_focus_range", block_name, 230.0f);
			settings_block.AddParam(EnvParamIds::Float::PostProcessingDoFBlurScaleBackground, L"dof_blur_scale", block_name, 50.0f);
			settings_block.AddParam(EnvParamIds::Float::PostProcessingDoFBlurScaleForeground, L"dof_blur_foreground_scale", block_name, 50.0f);
			settings_block.AddParam(EnvParamIds::Float::PostProcessingDoFTransitionBackground, L"dof_transition_background", block_name, 500.0f);
			settings_block.AddParam(EnvParamIds::Float::PostProcessingDoFTransitionForeground, L"dof_transition_foreground", block_name, 500.0f);
			settings_block.AddParam(EnvParamIds::Bool::PostProcessingDepthOfFieldIsEnabled, L"dof_enabled", block_name, false);
		}

		{
			std::wstring block_name = L"environment_mapping";
			settings_block.AddParam(EnvParamIds::String::EnvMappingDiffuseCubeTex, L"diffuse_cube", block_name);
			settings_block.AddParam(EnvParamIds::String::EnvMappingSpecularCubeTex, L"specular_cube", block_name);
			settings_block.AddParam(EnvParamIds::Float::EnvMappingDirectLightEnvRatio, L"direct_light_env_ratio", block_name, 0.0f);
			settings_block.AddParam(EnvParamIds::Float::EnvMappingEnvBrightness, L"env_brightness", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::EnvMappingGiAdditionalEnvLight, L"gi_additional_env_light", block_name, 0.3f);
			settings_block.AddParam(EnvParamIds::Float::EnvMappingHorAngle, L"hor_angle", block_name, 0.0f);
			settings_block.AddParam(EnvParamIds::Float::EnvMappingVertAngle, L"vert_angle", block_name, 0.0f);
			settings_block.AddParam(EnvParamIds::Float::EnvMappingSolidSpecularAttenuation, L"solid_specular_attenuation", block_name, 0.3f);
			settings_block.AddParam(EnvParamIds::Float::EnvMappingWaterSpecularAttenuation, L"water_specular_attenuation", block_name, 1.0f);
		}

		{
			std::wstring block_name = L"screenspace_fog";
			settings_block.AddParam(EnvParamIds::Vector3::SSFColor, L"ssf_color", block_name, simd::vector3(0.5f));
			settings_block.AddParam(EnvParamIds::Float::SSFColorAlpha, L"ssf_color_alpha", block_name, 0.5f);
			settings_block.AddParam(EnvParamIds::Float::SSFLayerCount, L"ssf_layer_count", block_name, 8.0f);
			settings_block.AddParam(EnvParamIds::Float::SSFThickness, L"ssf_thickness", block_name, 30.0f);
			settings_block.AddParam(EnvParamIds::Float::SSFTurbulence, L"ssf_turbulence", block_name, 40.0f);
			settings_block.AddParam(EnvParamIds::Float::SSFDisperseRadius, L"ssf_disperse_radius", block_name, 1000.0f);
			settings_block.AddParam(EnvParamIds::Float::SSFFeathering, L"ssf_feathering", block_name, 0.0f);
		}

		{
			std::wstring block_name = L"area";
			settings_block.AddParam(EnvParamIds::Vector3::DustColor, L"dust_color", block_name, simd::vector3(0.5f, 0.5f, 0.5f));
			settings_block.AddParam(EnvParamIds::Float::GroundScale, L"ground_scale", block_name, 0.005f);
			settings_block.AddParam(EnvParamIds::Float::GroundScrollSpeedX, L"ground_scroll_speedX", block_name, 0.f);
			settings_block.AddParam(EnvParamIds::Float::GroundScrollSpeedY, L"ground_scroll_speedY", block_name, 0.f);
			settings_block.AddParam(EnvParamIds::String::GlobalParticleEffect, L"global_particle_effect", block_name);
			settings_block.AddParam(EnvParamIds::String::PlayerEnvAo, L"player_environment_ao", block_name);
			settings_block.AddParam(EnvParamIds::EffectArray::GlobalEffects, L"global_effects", block_name);
			settings_block.AddParam(EnvParamIds::Bool::LightningEnabled, L"lightning", block_name, false);
			settings_block.AddParam( EnvParamIds::Float::WindDirectionAngle, L"wind_direction_angle", block_name, ToRadiansMultiplier * 180.0f );
			settings_block.AddParam( EnvParamIds::Float::WindIntensity, L"wind_intensity", block_name, 500.0f );
		}

		{
			std::wstring block_name = L"rain";
			settings_block.AddParam(EnvParamIds::Float::RainIntensity, L"rain_intensity", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::RainTurbulence, L"rain_turbulence", block_name, 0.2f);
			settings_block.AddParam(EnvParamIds::Float::RainHorAngle, L"rain_hor_angle", block_name, 0.0f);
			settings_block.AddParam(EnvParamIds::Float::RainVertAngle, L"rain_vert_angle", block_name, 0.0f);
			settings_block.AddParam(EnvParamIds::Float::RainAmount, L"rain_amount", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::RainDist, L"rain_dist", block_name, 0.5f);
		}

		{
			std::wstring block_name = L"clouds";
			settings_block.AddParam(EnvParamIds::Float::CloudsScale, L"clouds_scale", block_name, 0.5f);
			settings_block.AddParam(EnvParamIds::Float::CloudsIntensity, L"clouds_intensity", block_name, 0.9f);
			settings_block.AddParam(EnvParamIds::Float::CloudsMidpoint, L"clouds_midpoint", block_name, 0.5f);
			settings_block.AddParam(EnvParamIds::Float::CloudsSharpness, L"clouds_sharpness", block_name, 0.5f);
			settings_block.AddParam(EnvParamIds::Float::CloudsSpeed, L"clouds_speed", block_name, 50.0f);
			settings_block.AddParam(EnvParamIds::Float::CloudsAngle, L"clouds_angle", block_name, 0.5f);
			settings_block.AddParam(EnvParamIds::Float::CloudsFadeRadius, L"clouds_fade_radius", block_name, 800.0f);
			settings_block.AddParam(EnvParamIds::Float::CloudsPreFade, L"clouds_pre_fade", block_name, 0.1f);
			settings_block.AddParam(EnvParamIds::Float::CloudsPostFade, L"clouds_post_fade", block_name, 0.1f);
		}

		{
			std::wstring block_name = L"water";
			settings_block.AddParam(EnvParamIds::Float::WaterCausticsMult, L"caustics_mult", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::WaterDirectionness, L"directionness", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::WaterFlowFoam, L"flow_foam", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::WaterFlowIntensity, L"flow_intensity", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::WaterFlowTurbulence, L"flow_turbulence", block_name, 0.3f);
			settings_block.AddParam(EnvParamIds::Int::WaterFlowmapResolution, L"flow_resolution", block_name, 4);
			settings_block.AddParam(EnvParamIds::Int::WaterShorelineResolution, L"shoreline_resolution", block_name, 1);
			settings_block.AddParam(EnvParamIds::Bool::WaterWaitForInitialization, L"flow_wait_init", block_name, false);
			settings_block.AddParam(EnvParamIds::Vector3::WaterOpenWaterColor, L"open_water_color", block_name, simd::vector3(0.1f, 0.19f, 0.22f));
			settings_block.AddParam(EnvParamIds::Float::WaterOpenWaterMurkiness, L"open_water_murkiness", block_name, 0.01f);
			settings_block.AddParam(EnvParamIds::Float::WaterReflectiveness, L"reflectiveness", block_name, 0.03f);
			settings_block.AddParam(EnvParamIds::Float::WaterRefractionIndex, L"refraction_index", block_name, 1.33f);
			settings_block.AddParam(EnvParamIds::Float::WaterSubsurfaceScattering, L"subsurface_scattering", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::Float::WaterSwellAngle, L"swell_angle", block_name, 0.0f);
			settings_block.AddParam(EnvParamIds::Float::WaterSwellHeight, L"swell_height", block_name, 37.5f);
			settings_block.AddParam(EnvParamIds::Float::WaterSwellIntensity, L"swell_intensity", block_name, 0.0f);
			settings_block.AddParam(EnvParamIds::Float::WaterSwellPeriod, L"swell_period", block_name, 0.04f);
			settings_block.AddParam(EnvParamIds::Float::WaterShorelineOffset, L"shoreline_offset", block_name, 0.0f);
			settings_block.AddParam(EnvParamIds::Float::WaterHeightOffset, L"height_offset", block_name, 0.0f);
			settings_block.AddParam(EnvParamIds::Vector3::WaterTerrainWaterColor, L"terrain_water_color", block_name, simd::vector3(0.101503f, 0.091055f, 0.050373f));
			settings_block.AddParam(EnvParamIds::Float::WaterTerrainWaterMurkiness, L"terrain_water_murkiness", block_name, 0.13f);
			settings_block.AddParam(EnvParamIds::Float::WaterWindDirectionAngle, L"water_wind_direction_angle", block_name, 0.0f);
			settings_block.AddParam(EnvParamIds::Float::WaterWindIntensity, L"water_wind_intensity", block_name, 1.06f);
			settings_block.AddParam(EnvParamIds::Float::WaterWindSpeed, L"water_wind_speed", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::String::WaterSplashAo, L"water_splash_ao", block_name);
		}

		{
			std::wstring block_name = L"camera";
			settings_block.AddParam(EnvParamIds::Float::CameraExposure, L"exposure", block_name, 0.99f);
			settings_block.AddParam(EnvParamIds::Float::CameraFarPlane, L"far_plane", block_name, 3000.0f);
			settings_block.AddParam(EnvParamIds::Float::CameraNearPlane, L"near_plane", block_name, 75.0f);
			settings_block.AddParam(EnvParamIds::Float::CameraZRotation, L"z_rotation", block_name, 0.0f );
		}

		{
			std::wstring block_name = L"post_transform";
			settings_block.AddParam(EnvParamIds::String::PostTransformTex, L"texture", block_name);
		}

		{
			std::wstring block_name = L"effect_spawner";
			settings_block.AddParam(EnvParamIds::Float::EffectSpawnerRate, L"spawn_rate", block_name, 1.0f);
			settings_block.AddParam(EnvParamIds::ObjectArray::EffectSpawnerObjects, L"effect_objects", block_name);
		}

		{
			std::wstring block_name = L"audio";
			settings_block.AddParam(EnvParamIds::Float::AudioVersion, L"version", block_name, 2.0f);
			settings_block.AddParam(EnvParamIds::String::AudioEnvBankName, L"env_bank_name", block_name);
			settings_block.AddParam(EnvParamIds::String::AudioEnvEventName, L"env_event_name", block_name);
			settings_block.AddParam(EnvParamIds::String::AudioMusic, L"music", block_name);
			settings_block.AddParam(EnvParamIds::String::AudioAmbientSound, L"ambient_sound", block_name);
			settings_block.AddParam(EnvParamIds::String::AudioAmbientSound2, L"ambient_sound2", block_name);
			settings_block.AddParam(EnvParamIds::String::AudioAmbientBank, L"ambient_bank", block_name);
			settings_block.AddParam(EnvParamIds::Float::AudioListenerHeight, L"listener_height", block_name, 4.5f);
			settings_block.AddParam(EnvParamIds::Float::AudioListenerOffset, L"listener_offset", block_name, 0.0f);
			settings_block.AddParam(EnvParamIds::FootstepArray::FootstepAudio, L"footstep_materials", block_name);
			settings_block.AddParam(EnvParamIds::String::AudioEnvAo, L"audio_environment_ao", block_name);
		}

		{
			std::wstring block_name = L"global_illumination";
			settings_block.AddParam(EnvParamIds::Float::GiAmbientOcclusionPower, L"ambient_occlusion_power", block_name, 2.0f);
			settings_block.AddParam(EnvParamIds::Float::GiIndirectLightArea, L"indirect_light_area", block_name, 2.0f);
			settings_block.AddParam(EnvParamIds::Float::GiIndirectLightRampup, L"indirect_light_rampup", block_name, 2.0f);
			settings_block.AddParam(EnvParamIds::Float::GiThicknessAngle, L"thickness_angle", block_name, 15.0f);
			settings_block.AddParam(EnvParamIds::Bool::GiUseForcedScreenspaceShadows, L"use_forced_screenspace_shadows", block_name, false);
		}
	}

	EnvironmentSettingsV2::EnvironmentSettingsV2( const std::wstring& filename, Resource::Allocator& )
		: EnvironmentSettingsV2()
	{
		FetchData( filename );
	}

	void EnvironmentSettingsV2::ClearData()
	{
		loaded_environments.clear();
		loaded_environment_map.clear();
	}

	LoadedEnvironment& EnvironmentSettingsV2::FetchData( const std::wstring& filename )
	{
		const auto it = loaded_environment_map.find( filename );
		if( it != loaded_environment_map.end() )
			return *it->second;

		try
		{
			JsonDocument document;

			File::InputFileStream stream( filename, ".env" );

			std::wstring buffer;
			buffer.resize(stream.GetFileSize());
			stream.read(&buffer[0], buffer.size());
			document.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag | rapidjson::kParseFullPrecisionFlag>(&buffer[0]);

			if (document.HasParseError())
			{
				std::wstringstream err;
				err << StringToWstring(std::string(rapidjson::GetParseError_En(document.GetParseError()))) << " at offset " << document.GetErrorOffset();
				throw Resource::Exception(filename) << err.str();
			}

			JsonAllocator &allocator = document.GetAllocator();

			auto new_data = std::make_unique< LoadedEnvironment >();
			new_data->name = filename;
			settings_block.Deserialize( document, new_data->env_data, true );
			loaded_environments.emplace_back( std::move( new_data ) );
			loaded_environment_map[ filename ] = loaded_environments.back().get();

			return *loaded_environments.back();
		}
		catch (Resource::Exception& e)
		{
			e.SetFilename(filename);
			e << L"\nWhile reading environment settings file";
			throw;
		}
		catch (const File::FileNotFound& e)
		{
			Resource::DisplayException(e);
		}

		if( loaded_environments.empty() )
			loaded_environments.emplace_back( std::make_unique< LoadedEnvironment >() );
		loaded_environment_map[ filename ] = loaded_environments.front().get();
		return *loaded_environments.front();
	}

	void EnvironmentSettingsV2::RemoveData( const std::wstring filename )
	{
		RemoveIf( loaded_environments, [&]( const std::unique_ptr< LoadedEnvironment >& data ) { return data->name == filename; } );
		loaded_environment_map.erase( filename );

		if( selected_environment == filename )
		{
			selected_environment.clear();
			force_environment = false;
		}
	}

	void EnvironmentSettingsV2::ReloadData( const std::wstring& filename )
	{
		const auto it = loaded_environment_map.find( filename );
		if( it == loaded_environment_map.end() )
			return;

		if( File::Exists( filename ) )
		{
			JsonDocument document;

			File::InputFileStream stream( filename, ".env" );

			std::wstring buffer;
			buffer.resize( stream.GetFileSize() );
			stream.read( &buffer[ 0 ], buffer.size() );
			document.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag | rapidjson::kParseFullPrecisionFlag>( &buffer[ 0 ] );

			if( document.HasParseError() )
			{
				std::wstringstream err;
				err << StringToWstring( std::string( rapidjson::GetParseError_En( document.GetParseError() ) ) ) << " at offset " << document.GetErrorOffset();
				throw Resource::Exception( filename ) << err.str();
			}

			JsonAllocator& allocator = document.GetAllocator();

			settings_block.Deserialize( document, it->second->env_data, true );
		}
	}

	void EnvironmentSettingsV2::SaveData(const Data &src_data, const std::wstring& filename) const
	{
		JsonDocument out_doc;
		out_doc.SetObject();
		JsonAllocator& allocator = out_doc.GetAllocator();

		settings_block.Serialize(out_doc, src_data, allocator);

		using JsonEncoding = rapidjson::UTF16<>;
		using StringBuffer = rapidjson::GenericStringBuffer<JsonEncoding>;
		StringBuffer buffer;
		using PrettyWriter = rapidjson::PrettyWriter<StringBuffer, JsonEncoding>;
		PrettyWriter writer(buffer);
		writer.SetMaxDecimalPlaces(5);
		writer.SetIndent(L' ', 2);
		out_doc.Accept(writer);

		uwofstream out_stream(filename);
		out_stream << buffer.GetString();
		out_stream.close();
	}

	std::wstring EnvironmentSettingsV2::CopyData( const Data& src_data, const wchar_t* section_id ) const
	{
		JsonDocument out_doc;
		out_doc.SetObject();
		JsonAllocator& allocator = out_doc.GetAllocator();

		settings_block.Serialize( out_doc, src_data, allocator );

		for( auto it = out_doc.MemberBegin(); it != out_doc.MemberEnd(); )
		{
			if( !section_id || StringCompare( it->name.GetString(), section_id ) == 0 )
				++it;
			else
				it = out_doc.RemoveMember( it );
		}

		using JsonEncoding = rapidjson::UTF16<>;
		using StringBuffer = rapidjson::GenericStringBuffer<JsonEncoding>;
		StringBuffer buffer;
		using PrettyWriter = rapidjson::PrettyWriter<StringBuffer, JsonEncoding>;
		PrettyWriter writer( buffer );
		writer.SetMaxDecimalPlaces( 5 );
		writer.SetIndent( L' ', 2 );
		out_doc.Accept( writer );

		return buffer.GetString();
	}

	void EnvironmentSettingsV2::PasteData( Data& dst_data, const std::wstring& src_data )
	{
		JsonDocument document;
		document.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag | rapidjson::kParseFullPrecisionFlag>( src_data.c_str() );

		if( document.HasParseError() )
		{
			return;
		}

		JsonAllocator& allocator = document.GetAllocator();
		settings_block.Deserialize( document, dst_data, false );
	}

	void EnvironmentSettingsV2::BlendData(Data **datum, float *weights, size_t data_count, BlendTypes blend_type, Data *dst_data)
	{
		SettingsBlock::BlendData(datum, weights, data_count, blend_type, dst_data);
	}

	static float current_directional_light_delta = 0.0f;
	static float current_directional_light_angle = 0.0f;

	void EnvironmentSettingsV2::UpdateLightAngle(const Data& data, float dt)
	{
		const float delta = data.Value( EnvParamIds::Float::DirectionalLightDelta );
		if( delta != current_directional_light_delta )
		{
			current_directional_light_angle += ( delta - current_directional_light_delta );
			current_directional_light_delta = delta;
		}
		current_directional_light_angle += ( current_directional_light_delta * dt );
	}

	static simd::vector3 ComputeLightDirection(const Data& data)
	{
		const float phi = data.Value(EnvParamIds::Float::DirectionalLightPhi);
		const float theta = data.Value( EnvParamIds::Periodic::DirectionalLightTheta );
		return ComputeDirection( phi, theta + current_directional_light_angle );
	}

	static simd::vector3 ComputeLightColor(const Data& data)
	{
		auto color = data.Value(EnvParamIds::Vector3::DirectionalLightColour);
		color = DesaturateColor(color, data.Value(EnvParamIds::Float::DirectionalLightDesaturation));
		color *= data.Value(EnvParamIds::Float::DirectionalLightMultiplier);
		return color;
	}

	static simd::vector3 ComputeLightningDirection(const Data& data, double x)
	{
		const float phi = data.Value(EnvParamIds::Float::DirectionalLightPhi);
		const float theta = (int(x) * 100 + int(x) * int(x) * 823) / (3.14159f * 2.0f);
		return ComputeDirection(phi, theta);
	}

	static simd::vector3 ComputeLightningColor(const Data& data, double value)
	{
		return simd::vector3(float(value), float(value), float(value)) * 6.0f;
	}

	std::pair<simd::vector3, simd::vector3> EnvironmentSettingsV2::ComputeLightDirectionAndColour(const Data& data, const double time)
	{
		if (data.Value(EnvParamIds::Bool::LightningEnabled))
		{
			const double x = time * 2;
			const double value = sin(x) * sin(x * 3.1) * cos(x * 1.3) * cos(x * 4) * sin(x * 0.8) * sin(x * 1.1);
			if (value >= 0.4)
				return { ComputeLightningDirection(data, x), ComputeLightningColor(data, value) }; // Randomize direction over time, and use bright white.
		}

		return { ComputeLightDirection(data), ComputeLightColor(data) };
	}

	Audio::GlobalSettings EnvironmentSettingsV2::GetGlobalAudioSettings(const Data& data)
	{
		Audio::GlobalSettings settings;
		settings.env_bank_name = data.Value(EnvParamIds::String::AudioEnvBankName).Get();
		settings.env_event_name = data.Value(EnvParamIds::String::AudioEnvEventName).Get();
		settings.listener_height = data.Value(EnvParamIds::Float::AudioListenerHeight);
		settings.listener_offset = data.Value(EnvParamIds::Float::AudioListenerOffset);
		return settings;
	}
}
