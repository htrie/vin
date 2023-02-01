#pragma once

#include "Visual/Utility/IMGUI/DebugGUIDefines.h"

#if DEBUG_GUI_ENABLED

#include "EnvironmentSettings.h"
#include "Common/Utility/Event.h"
#include "Visual/GUI2/GUIResourceManager.h"
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/Icons.h"
#include <set>

namespace Utility
{
	struct AsyncLoaderData
	{
		std::atomic<bool> is_loaded = false;
		std::wstring filename;
	};

	void GetFilenameAsync( const wchar_t* filetype, AsyncLoaderData* dst_data );
}

namespace Game
{
	class EnvironmentMixer;
}

namespace Environment
{
	class EnvironmentEditorV2 : public Device::GUI::GUIInstance
	{
	public:
		EnvironmentEditorV2( );

		using MessageCallback = std::function<void(const std::wstring& message)>;
		void OnRender(EnvironmentSettingsV2 *settings, const float elapsed_time, const Game::EnvironmentMixer* env_mixer = nullptr, const Game::GameObject* player = nullptr );
		virtual bool MsgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) override;

		void SetCallback(MessageCallback _callback) { callback = _callback; }
		const MessageCallback& GetCallback() { return callback; }

		virtual void OnLoadSettings( Utility::ToolSettings* settings ) override;
		virtual void OnSaveSettings( Utility::ToolSettings* settings ) override;

	private:
		bool hide_unset_parameters = false;
		bool show_environment_zones = false;
		bool show_blended_environment = false;
		bool disable_docking = false;
		bool expand_column[ 4 ] = {};
		bool is_focused = false;
		bool push_undo_state = false;
		float minimum_column_width = 20.0f;

		enum class Action
		{
			None,
			New,
			Load,
			Reload,
			Save,
			SaveAs,
			Undo,
			Redo,
			Copy,
			Paste,
		} current_action = Action::None;

		::Texture::Handle post_transform_tex_preview;

		MessageCallback callback = nullptr;

		const char* CacheString( std::string str );
		const char* CacheString( const std::wstring& str );
		std::set< std::string > string_cache;

		template< typename T >
		bool RenderParam( const EnvironmentSettingsV2& settings, Data& data, LoadedEnvironment* env, const char* label, const T param, const std::function<bool()>& func, const bool push_id = true )
		{
			auto& is_present = data.IsPresent( param );
			if( hide_unset_parameters && !is_present )
				return false;

			ImGui::PushID( &is_present );
			bool changed = false;
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex( 0 );
			if( ImGui::Checkbox( "##IsPresent", &is_present ) )
			{
				if( !is_present )
					data.Value( param ) = settings.settings_block.GetDefaultValue( param );
				changed = true;
				push_undo_state = true;
			}

			ImGui::TableSetColumnIndex( 2 );
			if( env && ( is_present != env->saved_data.IsPresent( param ) || data.Value( param ) != env->saved_data.Value( param ) ) )
			{
				if( ImGui::Button( IMGUI_ICON_UNDO ) )
				{
					is_present = env->saved_data.IsPresent( param );
					data.Value( param ) = env->saved_data.Value( param );
					changed = true;
					push_undo_state = true;
				}
				if( label )
					ImGui::SameLine();
			}
			if( label )
				ImGui::Text( "%s", label );
			if( !push_id )
				ImGui::PopID();

			ImGui::TableSetColumnIndex( 1 );
			ImGui::SetNextItemWidth( -FLT_MIN );
			const auto& style_lock = ImGuiEx::PushStyleVar( ImGuiStyleVar_Alpha, is_present ? ImGui::GetStyle().Alpha : ImGui::GetStyle().Alpha * 0.5f );
			if( func() )
			{
				is_present = true;
				changed = true;
			}
			push_undo_state |= ImGui::IsItemDeactivatedAfterEdit();
			if( push_id )
				ImGui::PopID();
			return changed;
		}

		bool RenderCheckbox( const EnvironmentSettingsV2& settings, Data& data, LoadedEnvironment* env, const char* label, const EnvParamIds::Bool param )
		{
			return RenderParam( settings, data, env, label, param, [&]()
			{
				//ImGui::SameLine( ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() );
				return ImGui::Checkbox( "##Value", &data.Value( param ) );
			} );
		}

		bool RenderSliderInt( const EnvironmentSettingsV2& settings, Data& data, LoadedEnvironment* env, const char* label, const EnvParamIds::Int param, const int min_value, const int max_value )
		{
			return RenderParam( settings, data, env, label, param, [&]() { return ImGui::SliderInt( "##Value", &data.Value( param ), min_value, max_value ); } );
		}

		template< typename T >
		bool RenderSliderFloat( const EnvironmentSettingsV2& settings, Data& data, LoadedEnvironment* env, const char* label, const T param, const float min_value, const float max_value, const char* format = "%.3f", const ImGuiSliderFlags flags = 0 )
		{
			return RenderParam( settings, data, env, label, param, [&]() { return ImGui::SliderFloat( "##Value", &data.Value( param ), min_value, max_value, format, flags ); } );
		}

		template< typename T >
		bool RenderDragFloat( const EnvironmentSettingsV2& settings, Data& data, LoadedEnvironment* env, const char* label, const T param, const float speed, const float min_value = 0.0f, const float max_value = 0.0f, const char* format = "%.3f", const ImGuiSliderFlags flags = 0 )
		{
			return RenderParam( settings, data, env, label, param, [&]() { return ImGui::DragFloat( "##Value", &data.Value( param ), speed, min_value, max_value, format, flags ); } );
		}

		template< typename T >
		bool RenderSliderAngle( const EnvironmentSettingsV2& settings, Data& data, LoadedEnvironment* env, const char* label, const T param, const float min_value, const float max_value )
		{
			return RenderParam( settings, data, env, label, param, [&]()
			{
				// Modified version of ImGui::SliderAngle that doesn't modify the value unless changed to avoid floating-point loss
				float v_deg = data.Value( param ) * 360.0f / ( 2 * IM_PI );
				if( ImGui::SliderFloat( label, &v_deg, min_value, max_value, "%.0f deg" ) )
				{
					data.Value( param ) = v_deg * ( 2 * IM_PI ) / 360.0f;
					return true;
				}
				return false;
			} );
		}

		bool RenderColorEdit( const EnvironmentSettingsV2& settings, Data& data, LoadedEnvironment* env, const char* label, const EnvParamIds::Vector3 param, ImGuiColorEditFlags flags = 0 )
		{
			return RenderParam( settings, data, env, label, param, [&]() { return ImGuiEx::ColorEditSRGB<3>( "##Value", &data.Value( param ).x, flags ); } );
		}

		bool RenderInputText( const EnvironmentSettingsV2& settings, Data& data, LoadedEnvironment* env, const char* label, const EnvParamIds::String param );
		bool RenderFilenameBox( const EnvironmentSettingsV2& settings, Data& data, LoadedEnvironment* env, const char* label, const EnvParamIds::String param, const wchar_t* filter, const wchar_t* required_extension = nullptr );
	};
}

#endif
