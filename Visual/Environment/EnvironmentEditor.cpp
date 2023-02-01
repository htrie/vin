#include "EnvironmentEditor.h"

#if DEBUG_GUI_ENABLED

#include <iomanip>
#include <algorithm>
#include <future>
#if defined(WIN32) && !defined(CONSOLE) && !defined( __APPLE__ )
#include <commdlg.h>
#endif

#include "ClientCommon/Game/EnvironmentMixer.h"
#include "ClientInstanceCommon/Game/GameObject.h"
#include "ClientInstanceCommon/Game/Components/Positioned.h"
#include "ClientInstanceCommon/Game/Components/Sector.h"
#include "ClientInstanceCommon/Game/GameWorld.h"
#include "Common/File/InputFileStream.h"
#include "Common/File/PathHelper.h"
#include "Common/File/FileSystem.h"
#include "Common/Loaders/CEFootstepAudio.h"
#include "Common/Utility/Format.h"
#include "Visual/Utility/CommonDialogs.h"
#include "Visual/Utility/JsonUtility.h"
#include "Visual/Utility/ScreenshotTool.h"
#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Utility/ToolsUtility.h"

#include "magic_enum/magic_enum.hpp"
#include "../GUI2/imgui/imgui_internal.h"
#include "../GUI2/Icons.h"
#include "../Utility/IMGUI/ToolGUI.h"

namespace Utility
{
	void GetFilenameAsync(const wchar_t *filetype, AsyncLoaderData *dst_data)
	{
		auto thread_func = [dst_data, filetype]()
		{
			std::wstring absolute_path = Utility::GetFilenameBox(filetype);
			if (absolute_path != L"")
			{
				dst_data->filename = PathHelper::RelativePath(absolute_path, FileSystem::GetCurrentDirectory());
				dst_data->is_loaded = true;
			}
		};
		std::thread async_thread(thread_func);
		async_thread.detach();
	}

	std::wstring GetFilename( const wchar_t* type )
	{
		static std::wstring last_opened_dir;
		return ToolsUtility::GetOpenFilename( type, last_opened_dir );
	}
}

namespace Environment
{
	EnvironmentEditorV2::EnvironmentEditorV2()
		: Device::GUI::GUIInstance( true, MsgProcCallbacks )
	{

	}

	void EnvironmentEditorV2::OnRender(EnvironmentSettingsV2 *settings, const float elapsed_time, const Game::EnvironmentMixer* env_mixer, const Game::GameObject* player )
	{
		if (!open || !settings)
			return;

		if( settings->GetLoadedData().size() == 0 )
		{
			current_action = Action::None;
			return;
		}

		if( settings->selected_environment.empty() )
			settings->selected_environment = settings->GetLoadedData().front()->name;

		LoadedEnvironment* current_environment = ( show_blended_environment && env_mixer ) ? nullptr : &settings->FetchData( settings->selected_environment );
		std::reference_wrapper< Data > current_data = current_environment ? current_environment->env_data : const_cast< Data& >( env_mixer->GetBlendedEnvironment() );

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_MenuBar;
		if( disable_docking )
			flags |= ImGuiWindowFlags_NoDocking;

		if( ImGui::Begin( "Environment Editor", &open, flags) )
		{
			is_focused = ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows );
			push_undo_state = false;

			const auto SelectEnvironment = [&]( const std::wstring& filename, const bool force_env = false )
			{
				current_environment = &settings->FetchData( filename );
				current_data = current_environment->env_data;
				settings->selected_environment = current_environment->name;
				settings->force_environment |= force_env;
				settings->auto_select_environment = false;
				show_blended_environment = false;
			};

			const auto NewEnvironment = [&]()
			{
				std::wstring new_env_filename;
				if( ToolsUtility::GetSaveFilename( L"Environment (*.env)\0*.env\0", new_env_filename, nullptr ) )
				{
					new_env_filename = PathHelper::RelativePath( new_env_filename, FileSystem::GetCurrentDirectory() );
					Environment::Data new_data;
					settings->settings_block.Clear( new_data );
					settings->SaveData( new_data, new_env_filename );
					SelectEnvironment( new_env_filename, true );
				}
			};

			const auto LoadEnvironment = [&]()
			{
				const auto browse = Utility::GetFilename( L"Environment (*.env)\0*.env\0" );
				if( !browse.empty() )
					SelectEnvironment( PathHelper::NormalisePath( PathHelper::RelativePath( browse, FileSystem::GetCurrentDirectory() ) ), true );
			};

			const auto SaveEnvironment = [&]()
			{
				settings->SaveData( current_data.get(), current_environment->name );
				current_environment->saved_data = current_environment->env_data;
				current_environment->has_unsaved_changes = false;
			};

			const auto SaveEnvironmentAs = [&]()
			{
				std::wstring new_env_filename = PathHelper::NormalisePath( FileSystem::GetCurrentDirectory() + L"\\" + current_environment->name, L'\\' );
				if( ToolsUtility::GetSaveFilename( L"Environment (*.env)\0*.env\0", new_env_filename, nullptr ) )
				{
					new_env_filename = PathHelper::RelativePath( new_env_filename, FileSystem::GetCurrentDirectory() );
					settings->SaveData( current_data.get(), new_env_filename );
					SelectEnvironment( new_env_filename, true );
				}
			};

			if( ImGui::BeginMenuBar() )
			{
				if( ImGui::BeginMenu( "File" ) )
				{
					if( ImGui::MenuItem( "New", "Ctrl+N" ) )
						NewEnvironment();

					if( ImGui::MenuItem( "Load", "Ctrl+O" ) )
						LoadEnvironment();
					
					ImGui::BeginDisabled( !current_environment );
					ImGui::BeginDisabled( settings->GetLoadedData().size() <= 1 || settings->selected_environment.empty() || settings->selected_environment == settings->GetLoadedData().front()->name );
					if( ImGui::MenuItem( "Remove" ) )
					{
						settings->RemoveData( settings->selected_environment );
						SelectEnvironment( settings->GetLoadedData().front()->name );
					}
					ImGui::EndDisabled();
					if( ImGui::MenuItem( "Reload", "Ctrl+R" ) )
						settings->ReloadData( settings->selected_environment );
					if( ImGui::MenuItem( "Save", "Ctrl+S" ) )
						SaveEnvironment();
					ImGui::EndDisabled();
					if( ImGui::MenuItem( "Save As", "Ctrl+Shift+S" ) )
						SaveEnvironmentAs();
					ImGui::EndMenu();
				}
				if( ImGui::BeginMenu( "Edit" ) )
				{
					ImGui::PushItemFlag( ImGuiItemFlags_SelectableDontClosePopup, true );
					ImGui::BeginDisabled( !current_environment || current_environment->current_undo_position == 0 );
					if( ImGui::MenuItem( "Undo", "Ctrl+Z" ) )
						current_environment->Undo();
					ImGui::EndDisabled();
					ImGui::BeginDisabled( !current_environment || current_environment->current_undo_position + 1 >= current_environment->undo_chain.size() );
					if( ImGui::MenuItem( "Redo", "Ctrl+Shift+Z" ) )
						current_environment->Redo();
					ImGui::EndDisabled();
					ImGui::PopItemFlag();
					if( ImGui::MenuItem( "Copy", "Ctrl+C" ) )
						ImGui::SetClipboardText( WstringToString( settings->CopyData( current_data.get() ) ).c_str() );
					ImGuiEx::SetItemTooltip( "Copies all set parameters from the current environment to the clipboard." );
					ImGui::BeginDisabled( !current_environment );
					if( ImGui::MenuItem( "Paste", "Ctrl+V" ) )
					{
						if( const char* clipboard_text = ImGui::GetClipboardText() )
							settings->PasteData( current_environment->env_data, StringToWstring( clipboard_text ) );
						current_environment->PushUndoState();
					}
					ImGuiEx::SetItemTooltip( "Pastes environment parameters from the clipboard, merging them into the curent environment." );
					if( ImGui::MenuItem( "Clear All" ) )
					{
						settings->settings_block.Clear( current_environment->env_data );
						current_environment->PushUndoState();
					}
					ImGuiEx::SetItemTooltip( "Removes all parameters from the current environment, resetting them to their default values." );
					ImGui::EndDisabled();
					ImGui::EndMenu();
				}
				if( ImGui::BeginMenu( "Settings" ) )
				{
					ImGui::PushItemFlag( ImGuiItemFlags_SelectableDontClosePopup, true );
					ImGui::MenuItem( "Disable Window Docking", nullptr, &disable_docking );
					ImGui::MenuItem( "Hide Unset Parameters", nullptr, &hide_unset_parameters );
					if( env_mixer )
						ImGui::MenuItem( "Show Environment Zones", nullptr, &show_environment_zones );

					ImGui::Separator();
					ImGui::DragFloat( "Minimum Column Width", &minimum_column_width, 0.1f, 0.1f, 100.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp );
					ImGui::PopItemFlag();
					ImGui::EndMenu();
				}

				ImGui::EndMenuBar();
			}

			for( const auto& loaded_env : settings->GetLoadedData() )
			{
				loaded_env->debug_text.clear();
				loaded_env->debug_weight = 0.0f;
			}

			if( env_mixer )
			{
				for( unsigned zone_index = 0; zone_index < env_mixer->zone_env_mixers.size(); ++zone_index )
				{
					const auto& zone = env_mixer->zone_env_mixers[ zone_index ];
					if( zone.weight <= 0.0f )
						continue;

					const std::string zone_text = Utility::safe_format( "Zone {}: {:.0f}%", zone_index + 1, zone.weight * 100.0f );

					for( unsigned i = 0; i < zone.zone_env.env_names.size(); ++i )
					{
						auto& loaded_env = settings->FetchData( zone.zone_env.env_names[ i ] );
						loaded_env.debug_weight = std::max( loaded_env.debug_weight, zone.weight * zone.zone_env.env_weights[ i ] );
						std::string transition_text = zone.zone_env.interpolator.type == Game::EnvInterpolator::Types::Constant ? "" : Utility::safe_format( " ({}: {:.0f}%)", magic_enum::enum_name( zone.zone_env.interpolator.type ), zone.zone_env.env_weights[ i ] * 100.0f );
						loaded_env.debug_text += Utility::safe_format( " [{} {}{}]", zone_text, env_mixer->use_zone_layers ? "override" : "base", transition_text );
					}
					for( const auto& env_override : zone.zone_env_overrides )
					{
						for( unsigned i = 0; i < env_override.override_env.env_names.size(); ++i )
						{
							auto& loaded_env = settings->FetchData( env_override.override_env.env_names[ i ] );
							loaded_env.debug_weight = std::max( loaded_env.debug_weight, zone.weight * env_override.curr_weight * zone.zone_env.env_weights[ i ] );
							std::string transition_text = env_override.override_env.interpolator.type == Game::EnvInterpolator::Types::Constant ? "" : Utility::safe_format( " ({}: {:.0f}%)", magic_enum::enum_name( env_override.override_env.interpolator.type ), env_override.override_env.env_weights[ i ] * 100.0f );
							loaded_env.debug_text += Utility::safe_format( " [{} override: {:.0f}%{}]", zone_text, env_override.curr_weight * 100.0f, transition_text );
						}
					}

					const auto& player_loc = player->GetPositioned().GetLocation();
					for( const auto& sector : player->game_world.GetSectorsAtLocation( player_loc ) )
					{
						if( sector->GetEnvironmentOverride().empty() )
							continue;

						const auto& sector_bound = sector->GetBound();
						const float env_weight = sector->CalculateEnvironmentWeight( player_loc );
						auto& loaded_env = settings->FetchData( sector->GetEnvironmentOverride() );
						loaded_env.debug_text += Utility::safe_format( " [Sector override: {:.0f}%]", env_weight * 100.0f );
					}
				}

				if( !settings->force_environment && !show_blended_environment && settings->auto_select_environment )
				{
					float best_weight = 0.0f;
					for( const auto& loaded_env : settings->GetLoadedData() )
					{
						if( loaded_env->debug_weight > best_weight )
						{
							best_weight = loaded_env->debug_weight;
							settings->selected_environment = loaded_env->name;
							current_environment = loaded_env.get();
							current_data = current_environment->env_data;
						}
					}
				}
			}

			for( const auto& loaded_env : settings->GetLoadedData() )
			{
				if( loaded_env->has_unsaved_changes )
					loaded_env->debug_text += " (Unsaved)";
			}

			const float min_box_height = ImGui::GetFrameHeightWithSpacing();
			static float current_box_height = min_box_height;
			current_box_height = std::max( current_box_height, show_environment_zones ? min_box_height * 4.0f : min_box_height );

			float remaining_height = ImGui::GetWindowHeight() - current_box_height;
			ImGuiEx::Splitter( false, 1.0f, &current_box_height, &remaining_height, min_box_height, 0.0f );

			constexpr float ActiveEnvThreshold = 1e-3f;
			const bool use_combo_box = current_box_height == min_box_height;
			ImGui::SetNextItemWidth( -FLT_MIN );
			if( use_combo_box && ImGui::BeginCombo( "##ActiveEnvironment", ( WstringToString( current_environment->name ) + current_environment->debug_text ).c_str() ) ||
				!use_combo_box && ImGui::BeginListBox( "##EnvironmentList", ImVec2( -FLT_MIN, current_box_height - ImGui::GetStyle().FramePadding.y ) ) )
			{
				if( env_mixer && show_environment_zones )
				{
					const auto disabled_text_colour = ImGui::GetColorU32( ImGuiCol_Text, 0.6f );
					const auto RenderEnvironmentTransition = [&]( const Game::EnvironmentTransition& env_transition, const char* label )
					{
						const bool tree_open = ImGui::TreeNodeEx( &env_transition, ImGuiTreeNodeFlags_DefaultOpen, "%s", label );

						if( env_transition.interpolator.type != Game::EnvInterpolator::Types::Constant && ImGui::IsItemHovered() )
						{
							ImGui::BeginTooltip();
							ImGui::Text( "Transition Type: %s", std::string( magic_enum::enum_name( env_transition.interpolator.type ) ).c_str() );
							for( unsigned i = 0; i < env_transition.interpolator.points.size(); ++i )
								ImGui::Text( "Point %u: (%d, %d)", i, env_transition.interpolator.points[ i ].x, env_transition.interpolator.points[ i ].y );
							if( env_transition.interpolator.start_flag.IsValid() )
								ImGui::Text( "Start Flag: %s", WstringToString( env_transition.interpolator.start_flag->GetId() ).c_str() );
							if( env_transition.interpolator.stop_flag.IsValid() )
								ImGui::Text( "Start Flag: %s", WstringToString( env_transition.interpolator.stop_flag->GetId() ).c_str() );
							ImGui::EndTooltip();
						}

						if( tree_open )
						{
							for( unsigned env_index = 0; env_index < env_transition.env_names.size(); ++env_index )
							{
								if( env_transition.env_weights[ env_index ] <= ActiveEnvThreshold )
									ImGui::PushStyleColor( ImGuiCol_Text, disabled_text_colour );
								const auto env_label = Utility::safe_format( "{} (Weight: {:.0f}%)###Env{}", WstringToString( env_transition.env_names[ env_index ] ), env_transition.env_weights[ env_index ] * 100.0f, env_index );
								if( ImGui::Selectable( env_label.c_str(), current_environment && current_environment->name == env_transition.env_names[ env_index ] ) )
								{
									SelectEnvironment( env_transition.env_names[ env_index ] );
								}
								if( env_transition.env_weights[ env_index ] <= ActiveEnvThreshold )
									ImGui::PopStyleColor();
								ToolsUtility::FileContextMenu( env_transition.env_names[ env_index ] );
							}
							ImGui::TreePop();
						}
					};

					for( unsigned zone_index = 0; zone_index < env_mixer->zone_env_mixers.size(); ++zone_index )
					{
						const auto& zone = env_mixer->zone_env_mixers[ zone_index ];
						if( env_mixer->use_zone_layers && zone_index > 0 && zone.layer > env_mixer->zone_env_mixers[ zone_index - 1 ].layer )
							ImGui::Separator();

						if( zone.weight <= ActiveEnvThreshold )
							ImGui::PushStyleColor( ImGuiCol_Text, disabled_text_colour );
						if( ImGui::TreeNodeEx( &zone, ImGuiTreeNodeFlags_DefaultOpen, "Zone #%u - Weight: %.0f%%", zone_index + 1, zone.weight * 100.0f ) )
						{
							const std::string env_label = Utility::safe_format( "Base Environment: {} ({})", WstringToString( zone.zone_env.environment_row->GetId() ), magic_enum::enum_name( zone.zone_env.interpolator.type ) );
							RenderEnvironmentTransition( zone.zone_env, env_label.c_str() );
							for( const auto& env_override : zone.zone_env_overrides )
							{
								if( env_override.curr_weight <= ActiveEnvThreshold )
									ImGui::PushStyleColor( ImGuiCol_Text, disabled_text_colour );
								const std::string override_label = Utility::safe_format( "Override Environment ({:.0f}%): {} ({})", env_override.curr_weight * 100.0f, WstringToString( env_override.override_env.environment_row->GetId() ), magic_enum::enum_name( env_override.override_env.interpolator.type ) );
								RenderEnvironmentTransition( env_override.override_env, override_label.c_str() );
								if( env_override.curr_weight <= ActiveEnvThreshold )
									ImGui::PopStyleColor();
							}
							ImGui::TreePop();
						}
						if( zone.weight <= ActiveEnvThreshold )
							ImGui::PopStyleColor();
					}

					// Sector Overrides
					if( player )
					{
						bool found = false;
						const auto& player_loc = player->GetPositioned().GetLocation();
						for( const auto& sector : player->game_world.GetSectorsAtLocation( player_loc ) )
						{
							if( sector->GetEnvironmentOverride().empty() )
								continue;

							if( !found )
							{
								found = true;
								if( env_mixer->use_zone_layers )
									ImGui::Separator();
							}

							const auto& sector_bound = sector->GetBound();
							const float env_weight = sector->CalculateEnvironmentWeight( player_loc );
							const auto env_label = Utility::safe_format( "Sector Override: {} (Weight: {:.0f}%)", WstringToString( sector->GetEnvironmentOverride() ), env_weight * 100.0f );
							if( ImGui::Selectable( env_label.c_str(), current_environment && current_environment->name == sector->GetEnvironmentOverride() ) )
							{
								SelectEnvironment( sector->GetEnvironmentOverride() );
							}
							if( ImGui::IsItemHovered() )
							{
								ImGui::BeginTooltip();
								ImGui::Text( "Object ID: %u", sector->GetObject().GetId() );
								ImGui::Text( "Sector Name: %s", WstringToString( sector->GetName() ).c_str() );
								ImGui::Text( "Bound: (%d, %d) - (%d, %d)", sector->GetBound().lower.x, sector->GetBound().lower.y, sector->GetBound().upper.x, sector->GetBound().upper.y );
								ImGui::Text( "Environment Margin: %u", sector->GetEnvironmentMargin() );
								ImGui::EndTooltip();
							}
							ToolsUtility::FileContextMenu( sector->GetEnvironmentOverride() );
						}
					}
				}
				else
				{
					for( const auto& loaded_env : settings->GetLoadedData() )
					{
						ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetColorU32( ImGuiCol_Text, !env_mixer || loaded_env->debug_weight > ActiveEnvThreshold ? 1.0f : 0.6f ) );
						if( ImGui::Selectable( ( WstringToString( loaded_env->name ) + loaded_env->debug_text ).c_str(), loaded_env.get() == current_environment && !show_blended_environment ) )
						{
							SelectEnvironment( loaded_env->name );
						}
						ImGui::PopStyleColor();
						ToolsUtility::FileContextMenu( loaded_env->name );
					}
				}

				if( use_combo_box )
					ImGui::EndCombo();
				else
					ImGui::EndListBox();
			}

			ImGui::Spacing();

			ImGui::Checkbox( "Force Active##Environment", &settings->force_environment );

			if( ImGui::IsItemHovered() )
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos( 35.0f * ImGui::GetFontSize() );
				ImGui::Text( "Force Active" );
				ImGui::Separator();
				ImGui::TextWrapped( "Forces the currently selected environment setting to be applied to the scene. If this is not active, you can select and edit different environment "
									"settings, but the scene will have it's default environment settings applied, no matter what setting is currently selected" );
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}

			if( env_mixer )
			{
				ImGui::BeginDisabled( settings->force_environment );
				ImGui::SameLine();
				ImGui::Checkbox( "Show Blended Environment", &show_blended_environment );

				ImGui::EndDisabled();
				ImGuiEx::SetItemTooltip( "Shows settings from the active environment, after applying any environment blends and overrides.\nEnvironment parameters cannot be modified while this is enabled.", "Show Blended Environment" );
			}
			if( settings->force_environment )
				show_blended_environment = false;

			ImGui::SameLine();
			if( ImGui::Button( "Add##Environment" ) || current_action == Action::Load )
			{
				LoadEnvironment();
			}
			ImGui::SameLine();
			ImGui::BeginDisabled( settings->GetLoadedData().size() <= 1 || settings->selected_environment.empty() || settings->selected_environment == settings->GetLoadedData().front()->name || !current_environment );
			if( ImGui::Button( "Remove##Environment" ) )
			{
				settings->RemoveData( settings->selected_environment );
				SelectEnvironment( settings->GetLoadedData().front()->name );
			}

			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled( !current_environment );
			if( ImGui::Button( "Reload##Environment" ) || ( current_environment && current_action == Action::Reload ) )
			{
				settings->ReloadData( settings->selected_environment );
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if( ImGui::Button( "Copy" ) || current_action == Action::Copy )
			{
				ImGui::SetClipboardText( WstringToString( settings->CopyData( current_data.get()) ).c_str() );
			}
			ImGui::SameLine();
			ImGui::BeginDisabled( !current_environment );
			if( ImGui::Button( "Paste" ) || ( current_environment && current_action == Action::Paste ) )
			{
				if( const char* clipboard_text = ImGui::GetClipboardText() )
				{
					settings->PasteData( current_environment->env_data, StringToWstring( clipboard_text ) );
					current_environment->PushUndoState();
				}
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled( !current_environment );
			if( ImGui::Button( "Clear" ) )
			{
				settings->settings_block.Clear( current_environment->env_data );
				current_environment->PushUndoState();
			}
			ImGuiEx::SetItemTooltip( "Removes all parameters from the current environment, resetting them to their default values." );
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled( !current_environment );
			if( ImGui::Button( "Save##Environment" ) || ( current_environment && current_action == Action::Save ) )
			{
				SaveEnvironment();
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if( ImGui::Button( "Save As##Environment" ) || current_action == Action::SaveAs )
				SaveEnvironmentAs();

			ImGui::SameLine();
			ImGui::Checkbox( "Hide Unset", &hide_unset_parameters );

			if( current_action == Action::New )
				NewEnvironment();
			else if( current_environment && current_action == Action::Undo )
				current_environment->Undo();
			else if( current_environment && current_action == Action::Redo )
				current_environment->Redo();

			current_action = Action::None;

			//code for mass converting environment files
			/*ImGui::SameLine();
			if (ImGui::Button("Convert All##Environment"))
			{
				using Block = Environment::SettingsBlock;
				using Params = Environment::EnvParamIds;

				namespace stdfs = std::experimental::filesystem;
				const stdfs::directory_iterator end{};

				std::vector<std::string> paths;
				paths.push_back("c:/SVNs/bin/Client/Metadata/EnvironmentSettings");
				paths.push_back("c:/SVNs/bin/Client/Metadata/EnvironmentSettings/Corrupted");
				paths.push_back("c:/SVNs/bin/Client/Metadata/EnvironmentSettings/ShaderGather");

				for (auto path : paths)
				{
					for (stdfs::directory_iterator iter{ path }; iter != end; ++iter)
					{
						std::wstring name = iter->path().wstring();
						std::wstring extension = std::wstring(name.data() + name.length() - 4, name.data() + name.length());

						if (extension != L".env")
							continue;
						//EnvironmentSettings new_env(name, Resource::resource_allocator);
						//new_env.ConvertToJson(name + L".converted.json");

						auto id = settings->FetchData(name);
						auto &data = settings->GetEnvironmentData(id);

						if (!data.Value(Params::Bool::DirectionalLightIsEnabled) && data.IsPresent(Params::Bool::DirectionalLightIsEnabled))
						{
							data.Value(Params::Float::DirectionalLightMultiplier) = 0.0f;
						}
						data.IsPresent(Params::Bool::DirectionalLightIsEnabled) = false;
						data.Value(Params::Bool::DirectionalLightIsEnabled) = true; //to make it default and not save

						if (data.IsPresent(Params::Vector3::DirectionalLightColour))
						{
							auto &color = data.Value(Params::Vector3::DirectionalLightColour);
							float mult = std::max(std::max(color.x, color.y), color.z);
							color = color * (1.0f / mult);
							data.Value(Params::Float::DirectionalLightMultiplier) *= mult;
						}

						if (!data.Value(Params::Bool::PlayerLightIsEnabled) && data.IsPresent(Params::Bool::PlayerLightIsEnabled))
						{
							data.Value(Params::Float::PlayerLightIntensity) = 0.0f;
						}
						data.IsPresent(Params::Bool::PlayerLightIsEnabled) = false;
						data.Value(Params::Bool::PlayerLightIsEnabled) = true; //to make it default and not save

						if (data.IsPresent(Params::Vector3::PlayerLightColour))
						{
							auto &color = data.Value(Params::Vector3::PlayerLightColour);
							float mult = std::max(std::max(color.x, color.y), color.z);
							color = color * (1.0f / mult);
							data.Value(Params::Float::PlayerLightIntensity) *= mult;
						}

						if (!data.Value(Params::Bool::PostTransformIsEnabled))
						{
							data.Value(Params::String::PostTransformTex).values[0] = L"";
							data.IsPresent(Params::Bool::PostTransformIsEnabled) = false;
							data.Value(Params::Bool::PostTransformIsEnabled) = false;
						}

						std::wstring dst_name = name;
						std::wstring src_folder = L"EnvironmentSettings";
						std::wstring dst_folder = L"ConvertedEnvironmentSettings";
						dst_name.replace(dst_name.find(src_folder), src_folder.length(), dst_folder);
						settings->SaveData(data, dst_name);
					}
				}
			}*/

			const int num_columns = Clamp( int( ImGui::GetWindowSize().x / ImGui::GetFontSize() / minimum_column_width ), 1, 4 );

			if( !ImGui::BeginTable( "##EnvironmentColumns", num_columns, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings, ImVec2( -FLT_MIN, -FLT_MIN ) ) )
			{
				ImGui::End();
				return;
			}

			using Block = Environment::SettingsBlock;
			using Params = Environment::EnvParamIds;

			int current_column = -1;
			int remaining_sections = 13;
			int remaining_sections_in_column = 0;
			int expand_collapse = 0;

			const auto BeginTable = [&]() -> Finally_p
			{
				if( ImGui::BeginTable( ( "##EnvironmentColumn" + std::to_string( current_column ) ).c_str(), 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings, ImVec2( -FLT_MIN, 0.0f ) ) )
				{
					ImGui::TableSetupColumn( "Is Set", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ImGui::GetFrameHeight() );
					ImGui::TableSetupColumn( "##EditWidget", ImGuiTableColumnFlags_WidthStretch );
					ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 8.0f );
					return make_finally( []()
					{
						ImGui::EndTable();
					} );
				}
				return nullptr;
			};

			const auto Section = [&]( const char* label, const wchar_t* section_id, const bool begin_table = true ) -> Finally_p
			{
				if( remaining_sections_in_column <= 0 )
				{
					++current_column;
					assert( current_column < std::size( expand_column ) );

					expand_collapse = 0;
					ImGui::TableNextColumn();
					if( expand_column[ current_column ] )
					{
						if( ImGui::Button( IMGUI_ICON_EXPAND " Expand All " IMGUI_ICON_EXPAND, ImVec2( -FLT_MIN, 0.0f ) ) )
							expand_collapse = 1;
					}
					else
					{
						if( ImGui::Button( IMGUI_ICON_COLLAPSE " Collapse All " IMGUI_ICON_COLLAPSE, ImVec2( -FLT_MIN, 0.0f ) ) )
							expand_collapse = -1;
					}

					expand_column[ current_column ] = true;
					remaining_sections_in_column = remaining_sections / ( num_columns - current_column );
				}
				--remaining_sections_in_column;
				--remaining_sections;

				if( hide_unset_parameters && section_id && !settings->settings_block.IsSectionPresent( current_data.get(), section_id ) )
					return nullptr;

				if( expand_collapse != 0 )
					ImGui::SetNextItemOpen( expand_collapse > 0, ImGuiCond_Always );

				const bool section_open = ImGui::CollapsingHeader( label );
				if( section_id && ImGui::BeginPopupContextItem( label ) )
				{
					if( ImGui::MenuItem( "Copy Section" ) )
						ImGui::SetClipboardText( WstringToString( settings->CopyData( current_data.get(), section_id ) ).c_str() );
					if( ImGui::MenuItem( "Clear Section" ) )
					{
						settings->settings_block.ClearSection( current_data.get(), section_id );
						push_undo_state = true;
					}
					ImGui::EndPopup();
				}
				if( section_open )
				{
					expand_column[ current_column ] = false;
					if( begin_table )
						return BeginTable();
					else
						return make_finally( [] {} );
				}

				return nullptr;
			};

			{
				if( const auto section = Section( "Directional Light", L"directional_light" ) )
				{
					RenderCheckbox( *settings, current_data.get(), current_environment, "Casts Shadows", Params::Bool::DirectionalLightShadowsEnabled );
					RenderSliderAngle( *settings, current_data.get(), current_environment, "Vertical angle", Params::Float::DirectionalLightPhi, -180.0f, 180.0f );
					RenderSliderAngle( *settings, current_data.get(), current_environment, "Horizontal angle", Params::Periodic::DirectionalLightTheta, -180.0f, 180.0f );
					RenderColorEdit( *settings, current_data.get(), current_environment, "Color", Params::Vector3::DirectionalLightColour );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Multiplier", Params::Float::DirectionalLightMultiplier, 0.0f, 5.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Desaturation", Params::Float::DirectionalLightDesaturation, 0.0f, 1.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Penumbra Radius", Params::Float::DirectionalLightPenumbraRadius, 0.5f, 5.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Penumbra Dist", Params::Float::DirectionalLightPenumbraDist, 0.0f, 1.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Dir Extra Z", Params::Float::DirectionalLightCamExtraZ, 0.0f, 5.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Dir Min Offset", Params::Float::DirectionalLightMinOffset, -1000.0f, 0.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Dir Max Offset", Params::Float::DirectionalLightMaxOffset, 0.0f, 1000.0f );
					RenderSliderAngle( *settings, current_data.get(), current_environment, "Rotation Speed", Params::Float::DirectionalLightDelta, -1000.0f, 1000.0f );
					if( RenderCheckbox( *settings, current_data.get(), current_environment, "Cinematic", Params::Bool::DirectionalLightCinematicMode ) )
					{
						if( !current_data.get().Value( Params::Bool::DirectionalLightCinematicMode ) )
						{
							current_data.get().IsPresent( Params::Float::DirectionalLightCamNearPercent ) = false;
							current_data.get().IsPresent( Params::Float::DirectionalLightCamFarPercent ) = false;
						}
					}
					if( current_data.get().Value( Params::Bool::DirectionalLightCinematicMode ) || current_data.get().IsPresent( Params::Float::DirectionalLightCamNearPercent ) )
						RenderSliderFloat( *settings, current_data.get(), current_environment, "Dir Near Percent", Params::Float::DirectionalLightCamNearPercent, 0.0f, 1.0f );
					if( current_data.get().Value( Params::Bool::DirectionalLightCinematicMode ) || current_data.get().IsPresent( Params::Float::DirectionalLightCamFarPercent ) )
						RenderSliderFloat( *settings, current_data.get(), current_environment, "Dir Far Percent", Params::Float::DirectionalLightCamFarPercent, 0.0f, 1.0f );
				}

				if( const auto section = Section( "Player Light", L"player_light" ) )
				{
					RenderCheckbox( *settings, current_data.get(), current_environment, "Casts Shadows", Params::Bool::PlayerLightShadowsEnabled );
					RenderColorEdit( *settings, current_data.get(), current_environment, "Color", Params::Vector3::PlayerLightColour );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Intensity", Params::Float::PlayerLightIntensity, 0.0f, 5.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Penumbra", Params::Float::PlayerLightPenumbra, 0.0f, 5.0f );
				}

				if( const auto section = Section( "Area", L"area" ) )
				{
					RenderSliderAngle( *settings, current_data.get(), current_environment, "Wind Direction", Params::Float::WindDirectionAngle, -180.0f, 180.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Wind Intensity", Params::Float::WindIntensity, 0.0f, 5000.0f );

					RenderCheckbox( *settings, current_data.get(), current_environment, "Lightning", Params::Bool::LightningEnabled );
					RenderColorEdit( *settings, current_data.get(), current_environment, "Dust Color", Params::Vector3::DustColor );

					RenderDragFloat( *settings, current_data.get(), current_environment, "Ground Scale", Params::Float::GroundScale, 0.0001f, 0.0f, 0.2f, "%.4f" );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Ground Scroll Speed X", Params::Float::GroundScrollSpeedX, -5.0f, 5.f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Ground Scroll Speed Y", Params::Float::GroundScrollSpeedY, -5.0f, 5.f );
					RenderFilenameBox( *settings, current_data.get(), current_environment, "Player Environment AO", Params::String::PlayerEnvAo, L"Animated Object (*.ao)\0*.ao", L".ao" );
					RenderFilenameBox( *settings, current_data.get(), current_environment, "Global Particle Effect", Params::String::GlobalParticleEffect, L"Particle Effect (*.pet)\0*.pet\0", L".pet" );
				}
			}

			{
				if( const auto section = Section( "Camera", L"camera", false ) )
				{
					ImGui::TextUnformatted( "WARNING: Don't change these without checking with someone first" );

					if( const auto table = BeginTable() )
					{
						RenderSliderFloat( *settings, current_data.get(), current_environment, "Camera Near Plane", Params::Float::CameraNearPlane, 75.0f, 200.0f );
						RenderSliderFloat( *settings, current_data.get(), current_environment, "Camera Far Plane", Params::Float::CameraFarPlane, 75.0f, 7000.0f );
						RenderSliderFloat( *settings, current_data.get(), current_environment, "Exposure", Params::Float::CameraExposure, 0.5f, 10.0f );
						RenderSliderFloat( *settings, current_data.get(), current_environment, "Z Rotation", Params::Float::CameraZRotation, -PI, PI );
					}
				}

				if( const auto section = Section( "Post Processing", L"post_processing" ) )
				{
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Original Intensity", Params::Float::PostProcessingOriginalIntensity, 0.0f, 1.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Bloom Intensity", Params::Float::PostProcessingBloomIntensity, 0.0f, 2.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Bloom Cutoff", Params::Float::PostProcessingBloomCutoff, 0.0f, 1.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Bloom Radius", Params::Float::PostProcessingBloomRadius, 0.2f, 3.0f );

					RenderCheckbox( *settings, current_data.get(), current_environment, "Depth Of Field", Params::Bool::PostProcessingDepthOfFieldIsEnabled );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "DoF Focus Distance", Params::Float::PostProcessingDoFFocusDistance, 0.0f, 5000.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "DoF Focus Range", Params::Float::PostProcessingDoFFocusRange, 0.0f, 3000.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "DoF Blur Scale Far", Params::Float::PostProcessingDoFBlurScaleBackground, 0.0f, 100.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "DoF Blur Scale Near", Params::Float::PostProcessingDoFBlurScaleForeground, 0.0f, 100.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "DoF Transition Far", Params::Float::PostProcessingDoFTransitionBackground, 0.0f, 5000.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "DoF Transition Near", Params::Float::PostProcessingDoFTransitionForeground, 0.0f, 5000.0f );
				}

				if( const auto section = Section( "Color Grading", L"post_transform" ) )
				{
					RenderParam( *settings, current_data.get(), current_environment, "Post Transform Tex", Params::String::PostTransformTex, [&]()
					{
						const std::wstring& volume_tex_filename = current_data.get().Value( Params::String::PostTransformTex ).Get();
						if( volume_tex_filename != L"" )
						{
							/*post_transform_tex_preview = ::Texture::Handle(::Texture::SRGBTextureDesc(volume_tex_filename)); //it loads 3d texture as 2d. but seems to work somehow
							ImGui::Image(post_transform_tex_preview->GetTexture(), ImVec2(64, 64));
							ImGui::SameLine();*/
							ImGui::Text( "%s", CacheString( volume_tex_filename ) );
						}

						bool changed = false;
						if( ImGui::Button( "Load Shot" ) && current_environment )
						{
							const std::wstring png_filename = Utility::GetFilename( L"PNG (*.png)\0*.png\0" );
							if( !png_filename.empty() )
							{
								settings->color_grading_conversion_task.is_updated = true;
								settings->color_grading_conversion_task.src_filename = PathHelper::RelativePath( png_filename, FileSystem::GetCurrentDirectory() );
								settings->color_grading_conversion_task.dst_data = &current_data.get();
								settings->color_grading_conversion_task.dst_filename = current_environment->name + L".post.dds";
								changed = true;
							}
						}

						ImGui::SameLine();
						if( !volume_tex_filename.empty() )
						{
							if( ImGui::Button( CacheString( L"Remove##PostTransformTex" ) ) )
							{
								current_data.get().Value( Environment::EnvParamIds::String::PostTransformTex ).Get().clear();
								changed = true;
							}
						}
						return changed;
					} );
				}
			}

			{
				if( const auto section = Section( "Environment Mapping", L"environment_mapping" ) )
				{
					if( RenderFilenameBox( *settings, current_data.get(), current_environment, "Diffuse Map", Params::String::EnvMappingDiffuseCubeTex, L"Direct Draw Surface (*.dds)\0*.dds\0", L".dds" ) )
					{
						if( current_data.get().IsPresent( Params::String::EnvMappingDiffuseCubeTex ) && !current_data.get().Value( Params::String::EnvMappingDiffuseCubeTex ).Get().empty() )
						{
							auto& diffuse_map = current_data.get().Value( Params::String::EnvMappingDiffuseCubeTex ).Get();
							StringReplace( diffuse_map, L"_specular", L"_diffuse" );
							std::wstring specular_map = diffuse_map;
							StringReplace( specular_map, L"_diffuse", L"_specular" );
							if( diffuse_map != specular_map && File::Exists( specular_map ) )
							{
								current_data.get().Value( Params::String::EnvMappingSpecularCubeTex ).Get() = specular_map;
								current_data.get().IsPresent( Params::String::EnvMappingSpecularCubeTex ) = true;
							}
						}
					}
					if( RenderFilenameBox( *settings, current_data.get(), current_environment, "Specular Map", Params::String::EnvMappingSpecularCubeTex, L"Direct Draw Surface (*.dds)\0*.dds\0", L".dds" ) )
					{
						if( current_data.get().IsPresent( Params::String::EnvMappingSpecularCubeTex ) && !current_data.get().Value( Params::String::EnvMappingSpecularCubeTex ).Get().empty() )
						{
							auto& specular_map = current_data.get().Value( Params::String::EnvMappingSpecularCubeTex ).Get();
							StringReplace( specular_map, L"_diffuse", L"_specular" );
							std::wstring diffuse_map = specular_map;
							StringReplace( diffuse_map, L"_specular", L"_diffuse" );
							if( diffuse_map != specular_map && File::Exists( diffuse_map ) )
							{
								current_data.get().Value( Params::String::EnvMappingDiffuseCubeTex ).Get() = diffuse_map;
								current_data.get().IsPresent( Params::String::EnvMappingDiffuseCubeTex ) = true;
							}
						}
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex( 1 );
					if( ImGui::Button( "Save cubemap" ) )
					{
						settings->render_env_cubemap_task.is_updated = true;
						settings->render_env_cubemap_task.include_transform = false;
					}

					ImGui::SameLine();
					if( ImGui::Button( "Save cubemap with transform" ) )
					{
						settings->render_env_cubemap_task.is_updated = true;
						settings->render_env_cubemap_task.include_transform = true;
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex( 1 );
					ImGui::SetNextItemWidth( -FLT_MIN );
					ImGui::SliderFloat( "##Cubemap Camera Lift", &settings->render_env_cubemap_task.offset, 0.0f, 500 );
					ImGui::TableNextColumn();
					ImGui::Text( "Cubemap Camera Lift" );

					RenderSliderFloat( *settings, current_data.get(), current_environment, "Solid Spec", Params::Float::EnvMappingSolidSpecularAttenuation, 0.0f, 1.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Water Spec", Params::Float::EnvMappingWaterSpecularAttenuation, 0.0f, 1.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Env Brightness", Params::Float::EnvMappingEnvBrightness, 0.0f, 3.5f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Gi Additional Env Light", Params::Float::EnvMappingGiAdditionalEnvLight, 0.0f, 1.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Direct Env Ratio", Params::Float::EnvMappingDirectLightEnvRatio, 0.0f, 1.0f );
					RenderSliderAngle( *settings, current_data.get(), current_environment, "Sky Horizontal Angle", Params::Float::EnvMappingHorAngle, -180.0f, 180.0f );
					RenderSliderAngle( *settings, current_data.get(), current_environment, "Sky Vertical Angle", Params::Float::EnvMappingVertAngle, -90.0f, 90.0f );
				}

				if( const auto section = Section( "Screenspace Fog", L"screenspace_fog" ) )
				{
					RenderColorEdit( *settings, current_data.get(), current_environment, "Color", Params::Vector3::SSFColor, ImGuiColorEditFlags_Float );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Color Alpha", Params::Float::SSFColorAlpha, 0.0f, 1.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Layer Count", Params::Float::SSFLayerCount, 0.0f, 10.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Thickness", Params::Float::SSFThickness, 0.0f, 50.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Turbulence", Params::Float::SSFTurbulence, 0.0f, 50.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Disperse Radius", Params::Float::SSFDisperseRadius, 0.0f, 1500.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Feathering", Params::Float::SSFFeathering, 0.0f, 1.0f );
				}

				if( const auto section = Section( "Global Illumination", L"global_illumination" ) )
				{
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Ambient Occlusion Power", Params::Float::GiAmbientOcclusionPower, 1.0f, 10.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Indirect Light Area", Params::Float::GiIndirectLightArea, 1.0f, 10.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Indirect Light Rampup", Params::Float::GiIndirectLightRampup, 0.0f, 10.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Thickness Angle", Params::Float::GiThicknessAngle, 5.0f, 20.0f );
				}
			}

			{
				if( const auto section = Section( "Water", L"water" ) )
				{
					RenderColorEdit( *settings, current_data.get(), current_environment, "Open Water Color", Params::Vector3::WaterOpenWaterColor );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Open Water Opacity", Params::Float::WaterOpenWaterMurkiness, 1e-3f, 0.4f, "%.4f", ImGuiSliderFlags_Logarithmic );
					RenderColorEdit( *settings, current_data.get(), current_environment, "Terrain Water Color", Params::Vector3::WaterTerrainWaterColor );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Terrain Water Opacity", Params::Float::WaterTerrainWaterMurkiness, 1e-3f, 0.4f, "%.4f", ImGuiSliderFlags_Logarithmic );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Subsurface Scattering", Params::Float::WaterSubsurfaceScattering, 0.0f, 4.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Caustics", Params::Float::WaterCausticsMult, 0.0f, 4.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Refraction Index", Params::Float::WaterRefractionIndex, 1.0f, 4.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Reflectiveness", Params::Float::WaterReflectiveness, 0.0f, 1.0f );
					RenderSliderAngle( *settings, current_data.get(), current_environment, "Wind Direction", Params::Float::WaterWindDirectionAngle, -180.0f, 180.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Wind Intensity", Params::Float::WaterWindIntensity, 0.0f, 4.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Wind Speed", Params::Float::WaterWindSpeed, 0.5f, 3.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Directionness", Params::Float::WaterDirectionness, 0.0f, 1.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Swell Intensity", Params::Float::WaterSwellIntensity, 0.0f, 2.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Swell Height", Params::Float::WaterSwellHeight, 0.0f, 200.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Swell Angle", Params::Float::WaterSwellAngle, -0.4e-3f, 0.4e-3f, "%.6f" );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Swell Period", Params::Float::WaterSwellPeriod, 1.0f / 35.0f, 1.0f / 15.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Flow Speed", Params::Float::WaterFlowIntensity, 0.0f, 2.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Flow Floam", Params::Float::WaterFlowFoam, 0.0f, 2.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Flow Turbulence(recreate)", Params::Float::WaterFlowTurbulence, 0.0f, 1.0f );
					RenderSliderInt( *settings, current_data.get(), current_environment, "Flowmap Resolution(recreate)", Params::Int::WaterFlowmapResolution, 1, 8 );
					RenderSliderInt( *settings, current_data.get(), current_environment, "Shoreline Resolution(recreate)", Params::Int::WaterShorelineResolution, 1, 8 );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Shoreline Offset(recreate)", Params::Float::WaterShorelineOffset, -300.0f, 300.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Height Offset", Params::Float::WaterHeightOffset, -200.0f, 200.0f );

					RenderCheckbox( *settings, current_data.get(), current_environment, "Wait for water init", Params::Bool::WaterWaitForInitialization );
					//RenderSliderFloat( *settings, current_data.get(), current_environment, "Flow Turbulence", Params::Float::FlowFoam, 0.0f, 1.0f );

					RenderFilenameBox( *settings, current_data.get(), current_environment, "Water Splash AO Override", Params::String::WaterSplashAo, L"Animated Object (*.ao)\0*.ao\0", L".ao" );
				}

				if( const auto section = Section( "Audio", L"audio" ) )
				{
					RenderInputText( *settings, current_data.get(), current_environment, "Music", Params::String::AudioMusic );
					RenderInputText( *settings, current_data.get(), current_environment, "Ambient Sound", Params::String::AudioAmbientSound );
					RenderInputText( *settings, current_data.get(), current_environment, "Ambient Bank", Params::String::AudioAmbientBank );
					RenderInputText( *settings, current_data.get(), current_environment, "Env Event", Params::String::AudioEnvEventName );
					RenderInputText( *settings, current_data.get(), current_environment, "Env Bank", Params::String::AudioEnvBankName );

					RenderFilenameBox( *settings, current_data.get(), current_environment, "Audio Environment AO", Params::String::AudioEnvAo, L"Animated Object (*.ao)\0*.ao\0", L".ao" );

					RenderParam( *settings, current_data.get(), current_environment, nullptr, Params::FootstepArray::FootstepAudio, [&]()
					{
						bool changed = false;
						if( const auto tree_lock = ImGuiEx::TreeNode( "Footsteps" ) )
						{
							std::vector<const char*> footstep_name_ptrs;

							const Loaders::FootstepAudioTable footsteps;
							for( const auto& row : footsteps )
							{
								footstep_name_ptrs.push_back( CacheString( Utility::safe_format( L"{} ({})##{}", row->GetId(), row->GetAudioID(), row->GetId() ) ) );
							}

							auto& values = current_data.get().Value( Params::FootstepArray::FootstepAudio );
							for( auto& value : values )
							{
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex( 1 );
								const auto material = PathHelper::PathTargetName( value.material.GetFilename() );
								ImGui::SetNextItemWidth( -FLT_MIN );
								if( ImGui::Combo( CacheString( ConcatString( L"##", value.material.GetFilename() ) ), &value.audio_index, footstep_name_ptrs.data(), ( int )footstep_name_ptrs.size() ) )
								{
									push_undo_state = true;
									changed = true;
								}
								ImGui::TableNextColumn();
								ImGui::Text( "%s", CacheString( material ) );
								if( ImGui::IsItemHovered() )
									ImGuiEx::SetTooltip( "%s", CacheString( value.material.GetFilename().c_str() ) );
							}
						}
						return changed;
					}, false );

					RenderParam( *settings, current_data.get(), current_environment, nullptr, Params::SoundParameterMap::SoundParameters, [&]()
					{
						bool changed = false;
						if( const auto tree_lock = ImGuiEx::TreeNode( "Sound Parameters" ) )
						{
							std::optional< std::pair< std::string, std::string > > to_rename;
							std::optional< std::string > to_delete;

							auto& values = current_data.get().Value( Params::SoundParameterMap::SoundParameters );
							for( auto& value : values )
							{
								const auto id_lock = ImGuiEx::PushID( &value.first );
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex( 1 );
								if( ImGui::Button( IMGUI_ICON_ABORT ) )
									to_delete = value.first.name;
								ImGui::SameLine( 0.0f, ImGui::GetStyle().ItemInnerSpacing.x );
								ImGui::SetNextItemWidth( -FLT_MIN );
								changed |= ImGui::DragFloat( "##SoundParameterValue", &value.second, 0.1f );
								push_undo_state |= ImGui::IsItemDeactivatedAfterEdit();

								ImGui::TableNextColumn();
								std::string param_name = value.first.name;
								ImGui::SetNextItemWidth( -FLT_MIN );
								if( ImGuiEx::InputString( "##SoundParameterName", param_name, ImGuiInputTextFlags_EnterReturnsTrue ) )
									to_rename = std::make_pair( value.first.name, param_name );
								if( ImGui::IsItemHovered() )
									ImGui::SetTooltip( "Hash: %llu", ( unsigned long long )Audio::CustomParameterHash( param_name ) );
							}
							if( to_rename && !to_rename->second.empty() )
							{
								const float value = values[ to_rename->first ];
								values.erase( to_rename->first );
								values[ to_rename->second ] = value;
								changed = true;
								push_undo_state = true;
							}
							if( to_delete )
							{
								values.erase( *to_delete );
								push_undo_state = true;
							}

							static std::string new_name;
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex( 2 );
							ImGui::SetNextItemWidth( -FLT_MIN );
							changed |= ImGuiEx::InputStringWithHint( "##NewSoundParameterName", "Add Parameter...", new_name );
							if( ImGui::IsItemDeactivatedAfterEdit() && !new_name.empty() )
							{
								values[ new_name ] = 0.0f;
								new_name.clear();
								push_undo_state = true;
							}
							if( ImGui::IsItemHovered() && !new_name.empty() )
								ImGui::SetTooltip( "Hash: %llu", ( unsigned long long )Audio::CustomParameterHash( new_name ) );
						}
						return changed;
					}, false );
				}

				if( const auto section = Section( "Rain", L"rain" ) )
				{
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Dist", Params::Float::RainDist, 0.0f, 2.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Amount", Params::Float::RainAmount, 0.0f, 2.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Intensity", Params::Float::RainIntensity, 0.0f, 2.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Vert angle", Params::Float::RainVertAngle, 0.0f, 3.1415f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Hor angle", Params::Float::RainHorAngle, 0.0f, 3.1415f * 2.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Turbulence", Params::Float::RainTurbulence, 0.0f, 2.0f );
				}

				if( const auto section = Section( "Clouds", L"clouds" ) )
				{
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Intensity", Params::Float::CloudsIntensity, 0.0f, 1.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Scale", Params::Float::CloudsScale, 0.1f, 1.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Midpoint", Params::Float::CloudsMidpoint, 0.1f, 0.9f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Sharpness", Params::Float::CloudsSharpness, 0.1f, 0.9f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Angle", Params::Float::CloudsAngle, -3.14f, 3.14f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Speed", Params::Float::CloudsSpeed, 0.0f, 500.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Fade radius", Params::Float::CloudsFadeRadius, 10.0f, 1000.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Pre fade", Params::Float::CloudsPreFade, 0.0f, 1.0f );
					RenderSliderFloat( *settings, current_data.get(), current_environment, "Post fade", Params::Float::CloudsPostFade, 0.0f, 1.0f );
				}
			}

			ImGui::EndTable();

			if( push_undo_state && current_environment )
				current_environment->PushUndoState();
		}

		ImGui::End();
	}

	const char* EnvironmentEditorV2::CacheString( std::string str )
	{
		return string_cache.insert( std::move( str ) ).first->c_str();
	}

	const char* EnvironmentEditorV2::CacheString( const std::wstring& str )
	{
		return CacheString( WstringToString( str ) );
	}

	bool EnvironmentEditorV2::RenderInputText( const EnvironmentSettingsV2& settings, Data& data, LoadedEnvironment* env, const char* label, const EnvParamIds::String param )
	{
		return RenderParam( settings, data, env, label, param, [&]() -> bool
		{
			std::wstring& value = data.Value( param ).Get();
			std::string string_value = WstringToString( value );
			ImGui::SetNextItemWidth( -FLT_MIN );
			if( ImGuiEx::InputString( "##Value", string_value, ImGuiInputTextFlags_EnterReturnsTrue ) )
			{
				value = StringToWstring( string_value );
				return true;
			}
			return false;
		} );
	}

	bool EnvironmentEditorV2::RenderFilenameBox( const EnvironmentSettingsV2& settings, Data& data, LoadedEnvironment* env, const char* label, const EnvParamIds::String param, const wchar_t* filter, const wchar_t* required_extension )
	{
		return RenderParam( settings, data, env, label, param, [&]() -> bool
		{
			bool changed = false;
			std::wstring& value = data.Value( param ).Get();
			if( ImGui::Button( "Browse" ) )
			{
				const auto new_value = Utility::GetFilename( filter );
				if( !new_value.empty() )
				{
					value = PathHelper::NormalisePath( PathHelper::RelativePath( new_value, FileSystem::GetCurrentDirectory() ) );
					changed = true;
					push_undo_state = true;
				}
			}
			std::string string_value = WstringToString( value );
			ImGui::SameLine( 0.0f, ImGui::GetStyle().ItemInnerSpacing.x );
			ImGui::SetNextItemWidth( -FLT_MIN );
			if( ImGuiEx::InputString( "##Value", string_value, ImGuiInputTextFlags_EnterReturnsTrue ) )
			{
				if( string_value.empty() )
				{
					value.clear();
					changed = true;
				}
				else
				{
					if( string_value.length() >= 2 && string_value.front() == '"' && string_value.back() == '"' )
						string_value = string_value.substr( 1, string_value.length() - 2 );
					std::wstring new_value = PathHelper::NormalisePath( PathHelper::RelativePath( StringToWstring( string_value ), FileSystem::GetCurrentDirectory() ) );
					if( File::Exists( new_value ) && ( !required_extension || EndsWith( new_value, required_extension ) ) )
					{
						value = new_value;
						changed = true;
					}
				}
			}
			return changed;
		} );
	}

	bool EnvironmentEditorV2::MsgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam)
	{
		if (!open || !is_focused || ImGui::GetIO().WantTextInput)
			return false;

		switch( umsg )
		{
		case WM_KEYDOWN:
		{
			if( wParam == VK_Z && ImGui::GetIO().KeyCtrl )
			{
				current_action = ImGui::GetIO().KeyShift ? Action::Redo : Action::Undo;
				return true;
			}
			if( wParam == VK_S && ImGui::GetIO().KeyCtrl )
			{
				current_action = ImGui::GetIO().KeyShift ? Action::SaveAs : Action::Save;
				return true;
			}
			if( wParam == VK_R && ImGui::GetIO().KeyCtrl )
			{
				current_action = Action::Reload;
				return true;
			}
			if( wParam == VK_C && ImGui::GetIO().KeyCtrl )
			{
				current_action = Action::Copy;
				return true;
			}
			if( wParam == VK_V && ImGui::GetIO().KeyCtrl )
			{
				current_action = Action::Paste;
				return true;
			}
			if( wParam == VK_N && ImGui::GetIO().KeyCtrl )
			{
				current_action = Action::New;
				return true;
			}
			if( wParam == VK_O && ImGui::GetIO().KeyCtrl )
			{
				current_action = Action::Load;
				return true;
			}
		}
		default: break;
		}

		return false;
	}

	void EnvironmentEditorV2::OnLoadSettings( Utility::ToolSettings* settings )
	{
		auto& doc = settings->GetDocument();
		const rapidjson::Value& env_settings = doc[ "environment_editor" ];
		if( env_settings.IsObject() )
		{
			JsonUtility::Read( env_settings, disable_docking, "disable_docking" );
			JsonUtility::Read( env_settings, minimum_column_width, "minimum_column_width" );
		}
	}

	void EnvironmentEditorV2::OnSaveSettings( Utility::ToolSettings* settings )
	{
		using namespace rapidjson;

		auto& doc = settings->GetDocument();
		auto& allocator = settings->GetAllocator();
		Value env_settings( kObjectType );
	
		JsonUtility::Write( env_settings, disable_docking, "disable_docking", allocator );
		JsonUtility::Write( env_settings, minimum_column_width, "minimum_column_width", allocator );
		JsonUtility::ReplaceMember( doc, std::move( env_settings ), "environment_editor", settings->GetAllocator() );
	}
}
#endif
