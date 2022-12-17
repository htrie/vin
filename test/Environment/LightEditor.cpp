#include "LightEditor.h"

#if DEBUG_GUI_ENABLED

#include "Common/Utility/OsAbstraction.h"
#include "Visual/Animation/Components/FixedMeshComponent.h"
#include "Visual/Animation/Components/LightsComponent.h"
#include "Visual/Animation/Components/ClientAnimationControllerComponent.h"
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/Icons.h"
#include "Visual/Device/Line.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "magic_enum/magic_enum.hpp"

namespace LightEditor
{
	namespace
	{
		bool show_all_states = true;
		float transition_time = 0.0f;
	}

	bool RenderDynamicLight( Animation::Components::LightsStatic::DynamicLight& light, Animation::Components::Lights* lights_component, const unsigned idx, bool* open, bool& refresh, const StateChangeCallback& callback )
	{
		Animation::Components::LightsStatic::DynamicLight::LightInstance* light_instance = nullptr;
		if( lights_component && idx < lights_component->GetDynamicLights().size() )
			light_instance = &lights_component->GetDynamicLights().at( idx );

		std::string light_type;
		if( light.type == Renderer::Scene::Light::Type::Point )
			light_type = "Point Light";
		else if( light.type == Renderer::Scene::Light::Type::Spot )
			light_type = "Spot Light";
		else
			light_type = "Unknown";

		const bool disallow_editing = !lights_component || light.origin == Objects::Origin::Parent;
		const auto tree_open = ImGuiEx::TreeNode( light_type.c_str(), disallow_editing ? nullptr : open, ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen );

		if( !tree_open )
			return false;

		bool changed = false;

		ImGui::PushItemWidth( -ImGui::GetFontSize() * 4.0f );
		ImGuiEx::PushDisable( disallow_editing );

		if( light_instance && ( show_all_states || ImGui::BeginCombo( "State", light.states.at( light_instance->target_state ).name.c_str() ) ) )
		{
			ImGuiTableFlags flags = show_all_states ? ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersOuter : ImGuiTableFlags_None;
			if( ImGui::BeginTable( "States", 3, flags) )
			{
				ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch );
				ImGui::TableSetupColumn( "Default", ImGuiTableColumnFlags_WidthFixed );
				ImGui::TableSetupColumn( "Delete", ImGuiTableColumnFlags_WidthFixed );

				for( unsigned state_idx = 0; state_idx < light.states.size(); ++state_idx )
				{
					auto& state = light.states.at( state_idx );
					auto id_lock = ImGuiEx::PushID( state.name.c_str() );
					ImGui::TableNextColumn();
					ImGui::AlignTextToFramePadding();
					if( ImGui::Selectable( state.name.c_str(), state_idx == light_instance->target_state, ImGuiSelectableFlags_AllowItemOverlap | ImGuiSelectableFlags_SpanAllColumns ) )
					{
						lights_component->SetDynamicLightState( idx, state.name, transition_time );
						if( callback )
							callback( state.name, transition_time );
					}

					ImGui::TableNextColumn();
					if( light.default_state == state_idx )
					{
						ImGui::TextColored( ImVec4( 1.0f, 0.0f, 0.0f, 1.0f ), "(Default)" );
					}
					else
					{
						if( ImGui::Button( "Set Default" ) )
						{
							light.default_state = state_idx;
							changed = true;
						}
					}

					ImGui::TableNextColumn();
					if( light.states.size() > 1 )
					{
						if( ImGui::Button( IMGUI_ICON_ABORT ) )
						{
							light.states.erase( light.states.begin() + state_idx );
							if( light_instance->target_state == light.states.size() )
								light_instance->target_state = 0;
							if( light.default_state == light.states.size() )
								light.default_state = light_instance->target_state;
							changed = true;
							--state_idx;
						}
					}
				}

				ImGui::TableNextColumn();
				if( ImGui::Selectable( IMGUI_ICON_ADD "  Add State...", false, ImGuiSelectableFlags_SpanAllColumns ) )
				{
					std::string new_state_name;
					unsigned new_state_idx = 1;
					do
					{
						new_state_name = "NewState" + std::to_string( new_state_idx++ );
					} while( AnyOfIf( light.states, [&]( const auto& light_state ) { return light_state.name == new_state_name; } ) );
					light.states.push_back( light.states.at( light_instance->target_state ) );
					light.states.back().name = new_state_name;
					changed = true;
					light_instance->target_state = ( unsigned )light.states.size() - 1;
					if( callback )
						callback( new_state_name, 0 );
				}

				ImGui::EndTable();
			}

			if( !show_all_states )
				ImGui::EndCombo();
		}

		auto& state = light_instance ? light.states.at( light_instance->target_state ) : light.states.at( 0 );
		if( light_instance )
		{
			std::string name = state.name;

			if( ImGuiEx::InputString( "Name", name, ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter, []( ImGuiInputTextCallbackData* data )
			{
				if( data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter )
				{
					if( data->EventChar != '_' && !isalnum( data->EventChar ) )
						return 1;
				}
				return 0;
			}, &name ) )
			{
				if( NoneOfIf( light.states, [&]( const auto& light_state ) { return light_state.name == name; } ) )
				{
					state.name = name;
					refresh = true;
				}
				else
				{
					ImGui::TextColored( ImVec4( 0.8f, 0.1f, 0.1f, 1.0f ), IMGUI_ICON_WARNING );
					ImGui::SameLine();
					ImGui::Text( "A state with that name already exists!" );
				}
			}
		}

		static const char* xyz_formats[ 3 ] = { "X: %.2f", "Y: %.2f", "Z: %.2f" };
		static const char* rgb_formats[ 3 ] = { "R: %.3f", "G: %.3f", "B: %.3f" };

		{
			if( ImGui::Button( "Reset##position" ) )
			{
				state.position = simd::vector3( 0.0f, 0.0f, -125.0f );
				changed = true;
			}
			ImGui::SameLine( 0, ImGui::GetStyle().ItemInnerSpacing.x );
			if( ImGuiEx::DragFloat3( "Position", state.position.as_array(), 1.0f, 0.0f, 0.0f, xyz_formats ) )
				changed = true;
		}

		if( ImGui::Button( "Reset##direction" ) )
		{
			state.direction = simd::vector3( 0.0f, 0.0f, 0.0f );
			changed = true;
		}
		ImGui::SameLine( 0, ImGui::GetStyle().ItemInnerSpacing.x );
		if( ImGuiEx::DragFloat3( "Direction", state.direction.as_array(), 0.01f, 0.0f, 0.0f, xyz_formats ) )
			changed = true;
		if( ImGui::IsItemDeactivatedAfterEdit() && light.type != Renderer::Scene::Light::Type::Point )
			state.direction = state.direction.normalize();

		{
			if (ImGui::Button("Reset##penumbra_radius"))
			{
				state.penumbra_radius = 0.002f;
				changed = true;
			}
			ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);

			if (ImGui::SliderFloat("Pen. radius", &state.penumbra_radius, 0.0f, 0.01f, "%.3f"))
			{
				changed = true;
			}
		}

		{
			if (ImGui::Button("Reset##penumbra_dist"))
			{
				state.penumbra_dist = 0.0f;
				changed = true;
			}
			ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);

			if (ImGui::SliderFloat("Pen. dist", &state.penumbra_dist, 0.0f, 0.5f, "%.2f"))
			{
				changed = true;
			}
		}

		static bool show_intensity = true;

		ImGui::BeginGroup();
		if( !show_intensity )
		{
			if( ImGui::Button( "Reset##colour" ) )
			{
				state.colour = simd::vector3( 1.0f, 1.0f, 1.0f );
				changed = true;
			}
			ImGui::SameLine( 0, ImGui::GetStyle().ItemInnerSpacing.x );

			if( ImGuiEx::DragFloat3( "Colour", state.colour.as_array(), 0.01f, 0.0f, 100.0f, rgb_formats ) )
				changed = true;
		}
		else
		{
			float color_intensity = state.colour.dot( simd::vector3( 1.0f, 1.0f, 1.0f ) ) / 3;
			const float max_rgb = std::max( { state.colour.x, state.colour.y, state.colour.z } );
			const float multiplier = max_rgb <= 0.0f ? 0.0f : 1.0f / max_rgb;
			simd::vector3 color = state.colour * multiplier;
			bool changed_color = false;

			if( ImGui::Button( "Reset##colour" ) )
			{
				color = simd::vector3( 1.0f, 1.0f, 1.0f );
				changed_color = true;
			}
			ImGui::SameLine( 0, ImGui::GetStyle().ItemInnerSpacing.x );
			changed_color |= ImGuiEx::ColorEditSRGB< 3 >( "Colour", color.as_array(), ImGuiColorEditFlags_Float );

			if( ImGui::Button( "Reset##colour_intensity" ) )
			{
				color_intensity = 1.0f;
				changed_color = true;
			}
			ImGui::SameLine( 0, ImGui::GetStyle().ItemInnerSpacing.x );
			changed_color |= ImGui::DragFloat( "Intensity", &color_intensity, 0.001f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp );

			if( changed_color )
			{
				const float old_intensity = color.dot( simd::vector3( 1.0f, 1.0f, 1.0f ) ) / 3;
				state.colour = color * color_intensity / old_intensity;
				changed = true;
			}
		}
		ImGui::EndGroup();
		if( ImGui::IsItemHovered() )
		{
			ImGui::SetTooltip( "Middle click to toggle editing of colour intensity." );
			if( ImGui::IsMouseClicked( ImGuiMouseButton_Middle ) )
				show_intensity ^= 1;
		}

		{
			if( ImGui::Button( "Reset##radius" ) )
			{
				state.radius = 500.0f;
				changed = true;
			}
			ImGui::SameLine( 0, ImGui::GetStyle().ItemInnerSpacing.x );
			if( ImGui::DragFloat( "Radius", &state.radius, 1.0f, 0.0f, 100000.0f, "%.0f" ) )
				changed = true;
		}

		if( light.type == Renderer::Scene::Light::Type::Spot )
		{
			float penumbra_degrees = state.penumbra * ToDegreesMultiplier * 2;
			float umbra_degrees = state.umbra * ToDegreesMultiplier * 2;

			if( ImGui::DragFloat( "Penumbra", &penumbra_degrees, 0.05f, 0.0f, umbra_degrees, "%.2f\xC2\xB0" ) )
			{
				state.penumbra = penumbra_degrees * ToRadiansMultiplier / 2;
				changed = true;
			}
			if( ImGui::DragFloat( "Umbra", &umbra_degrees, 0.05f, penumbra_degrees, 179.0f, "%.2f\xC2\xB0" ) )
			{
				state.umbra = umbra_degrees * ToRadiansMultiplier / 2;
				changed = true;
			}
		}

		if( light.type == Renderer::Scene::Light::Type::Point )
		{
			if( ImGui::Button( "Reset##dist_threshold" ) )
			{
				state.dist_threshold = 0.0f;
				changed = true;
			}
			ImGui::SameLine( 0, ImGui::GetStyle().ItemInnerSpacing.x );
			if( ImGui::DragFloat( "Dist Threshold", &state.dist_threshold, 1.0f, 0.0f, 100000.0f ) )
				changed = true;

			if( ImGui::Button( "Reset##profile" ) )
			{
				light.profile = 0;
				refresh = true;
			}
			ImGui::SameLine( 0, ImGui::GetStyle().ItemInnerSpacing.x );
			const unsigned int profile_step = 1;
			if( ImGui::InputScalar( "Profile", ImGuiDataType_U32, &light.profile, &profile_step, &profile_step ) )
				refresh = true;
			ImGuiEx::HelpMarker( "IES lighting profiles describe how the light spreads in any given direction.\nClick here to open the wiki page for further details.", "Photometric Profile" );
#ifdef WIN32
			if( ImGui::IsItemClicked() )
				Utility::OpenURL( L"https://wiki.office.grindinggear.com/display/LD/Room+Editor+Layout#RoomEditorLayout-PointLightSettings" );
#endif

			if( light_instance )
			{
				if( ImGui::Button( "Reset##style" ) )
				{
					state.style = 0;
					state.custom_style.clear();
					changed = true;
				}
				ImGui::SameLine( 0, ImGui::GetStyle().ItemInnerSpacing.x );
				std::string current_style = state.style == Animation::Components::LightsStatic::DynamicLight::NumStyles ? "Custom" : std::string( magic_enum::enum_name( static_cast< Animation::Components::LightsStatic::DynamicLight::Style >( state.style ) ) );
				current_style += Utility::format( " ({:.3f})", light_instance->intensity );
				if( ImGui::BeginCombo( "Style", current_style.c_str() ) )
				{
					for( const auto [style, name] : magic_enum::enum_entries< Animation::Components::LightsStatic::DynamicLight::Style >() )
					{
						if( style == Animation::Components::LightsStatic::DynamicLight::NumStyles )
						{
							if( ImGui::Selectable( "Custom", state.style == style ) )
							{
								state.style = style;
								changed = true;
							}
						}
						else if( ImGui::Selectable( std::string( name ).c_str(), state.style == style ) )
						{
							state.style = style;
							state.custom_style.clear();
							changed = true;
						}
					}
					ImGui::EndCombo();
				}

				if( state.style == Animation::Components::LightsStatic::DynamicLight::NumStyles )
				{
					ImGuiEx::InputString( "##CustomStyle", state.custom_style, ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter, []( ImGuiInputTextCallbackData* data )
					{
						if( data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter )
						{
							if( !isalpha( data->EventChar ) )
								return 1;
						}
						return 0;
					}, &state.custom_style );

					ImGuiEx::HelpMarker( "A string of characters defining a looping pattern of brightness, where 'a' is fully dark and 'z' is full intensity.\n"
						"Lowercase characters are linearly interpolated to the next character. Uppercase characters are not interpolated.\n"
						"By default, each character represents a duration of 100 milliseconds.", "Custom Style" );
				}

				if( ImGui::Button( "Reset##style_speed" ) )
				{
					light.style_speed = 10.0f;
					changed = true;
				}
				ImGui::SameLine( 0, ImGui::GetStyle().ItemInnerSpacing.x );
				if( ImGui::DragFloat( "Style Speed", &light.style_speed, 0.01f, 0.01f, 1000.0f, "%.3f" ) )
					changed = true;

				if( ImGui::Checkbox( "Randomise Style Start Time", &light.randomise_start_time ) )
					changed = true;
			}

			ImGui::Text( "Usage Frequency" );
			ImGuiEx::HelpMarker( "Higher Frequency Lights require higher Lighting Quality to be displayed.\nWe assume Low Frequency lights will be placed infrequently, so they can show up on all Lighting Quality Settings.\n"
									"High Frequency lights are reserved for spammy effects or monsters, where performance is a concern.\n"
									"In the Room Editor, right click the " IMGUI_ICON_LIGHT " icon in the viewport to access Light Quality settings",
									"Frequency vs Quality" );
			const auto old_usage_freq = light.usage_frequency;
			if( ImGui::RadioButton( "Low", light.usage_frequency == Renderer::Scene::PointLight::UsageFrequency::Low ) )
				light.usage_frequency = Renderer::Scene::PointLight::UsageFrequency::Low;
			ImGui::SameLine();
			if( ImGui::RadioButton( "Medium", light.usage_frequency == Renderer::Scene::PointLight::UsageFrequency::Medium ) )
				light.usage_frequency = Renderer::Scene::PointLight::UsageFrequency::Medium;
			ImGui::SameLine();
			if( ImGui::RadioButton( "High", light.usage_frequency == Renderer::Scene::PointLight::UsageFrequency::High ) )
				light.usage_frequency = Renderer::Scene::PointLight::UsageFrequency::High;
			refresh |= old_usage_freq != light.usage_frequency;
		}
		else
		{
			if( ImGui::Checkbox( "Casts Shadow", &light.casts_shadow ) )
				refresh = true;
		}

		ImGuiEx::PopDisable();
		ImGui::PopItemWidth();

		return changed;
	}

	bool RenderLightEditor( Animation::AnimatedObject& ao, bool& refresh_ao, const StateChangeCallback& callback )
	{
		const auto* fixed_mesh = ao.GetComponent< Animation::Components::FixedMesh >();
		const auto* controller = ao.GetComponent< Animation::Components::ClientAnimationController >();
		auto* lights_component = ao.GetComponent< Animation::Components::Lights >();
		auto* lights_static = ao.GetType()->GetStaticComponent< Animation::Components::LightsStatic >();
		if( !lights_component || !lights_static )
			return false;

		std::vector< Animation::Components::LightsStatic::DynamicLight > mesh_lights;

		if( fixed_mesh )
		{
			for( const auto& mesh : fixed_mesh->GetFixedMeshes() )
			{
				for( const auto& light : mesh.mesh->GetLights() )
				{
					const auto& fixed_light = ( const FileReader::FMTReader::FixedLight& )light;
					if( fixed_light.type != FileReader::FMTReader::PointLight && fixed_light.type != FileReader::FMTReader::SpotLight )
						continue;

					mesh_lights.emplace_back();
					mesh_lights.back().states.emplace_back();
					mesh_lights.back().profile = fixed_light.profile;
					mesh_lights.back().casts_shadow = fixed_light.casts_shadow;
					mesh_lights.back().type = fixed_light.type == FileReader::FMTReader::PointLight ? Renderer::Scene::Light::Type::Point : Renderer::Scene::Light::Type::Spot;
					mesh_lights.back().usage_frequency = static_cast< Renderer::Scene::PointLight::UsageFrequency >( fixed_light.usage_frequency );
					mesh_lights.back().states.back().colour = fixed_light.colour;
					mesh_lights.back().states.back().position = fixed_light.position;
					mesh_lights.back().states.back().direction = fixed_light.direction;
					mesh_lights.back().states.back().radius = fixed_light.radius;
					mesh_lights.back().states.back().dist_threshold = fixed_light.dist_threshold;
					mesh_lights.back().states.back().penumbra = fixed_light.penumbra;
					mesh_lights.back().states.back().umbra = fixed_light.umbra;

					if( fixed_light.dist_threshold < 1e-5f )
						mesh_lights.back().states.back().dist_threshold = fixed_light.radius * 0.1f;
				}
			}
		}
		if( controller )
		{
			for( const auto& state : controller->GetCurrentLightState() )
			{
				mesh_lights.emplace_back();
				mesh_lights.back().states.emplace_back();
				mesh_lights.back().profile = state.profile;
				mesh_lights.back().casts_shadow = state.casts_shadow;
				mesh_lights.back().type = state.is_spotlight ? Renderer::Scene::Light::Type::Spot : Renderer::Scene::Light::Type::Point;
				mesh_lights.back().usage_frequency = state.usage_frequency;
				mesh_lights.back().states.back().colour = state.colour;
				mesh_lights.back().states.back().position = state.position;
				mesh_lights.back().states.back().direction = state.direction;
				mesh_lights.back().states.back().radius = state.radius_penumbra_umbra.x;
				mesh_lights.back().states.back().dist_threshold = state.dist_threshold;
				mesh_lights.back().states.back().penumbra = state.radius_penumbra_umbra.y;
				mesh_lights.back().states.back().umbra = state.radius_penumbra_umbra.z;

				if( state.dist_threshold < 1e-5f )
					mesh_lights.back().states.back().dist_threshold = state.radius_penumbra_umbra.x * 0.1f;
			}
		}

		bool toggle_override_mesh_lights = false;
		bool add_point_light = false;
		bool add_spot_light = false;

		ImGui::Button( IMGUI_ICON_ADD " Add Light" );
		if( ImGui::BeginPopupContextItem( "Add Light", ImGuiPopupFlags_MouseButtonLeft ) )
		{
			add_point_light = ImGui::MenuItem( IMGUI_ICON_LIGHT " Add Point Light" );
			add_spot_light = ImGui::MenuItem( IMGUI_ICON_CAMERA " Add Spot Light" );
			if( ImGui::IsItemHovered() )
			{
				ImGui::BeginTooltip();
				ImGui::TextColored( ImVec4( 0.75f, 0.25f, 0.25f, 1.0f ), "Spot lights incur a significantly greater performance cost compared to point lights.\nUse with caution!" );
				ImGui::EndTooltip();
			}
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		ImGui::Button( IMGUI_ICON_WRENCH " Editor Settings");
		if( ImGui::BeginPopupContextItem( "Editor Settings", ImGuiPopupFlags_MouseButtonLeft ) )
		{
			ImGui::PushItemWidth( ImGui::GetFontSize() * 4.0f );
			ImGui::Checkbox( "Show All States", &show_all_states );

			if( ImGuiEx::Checkbox( "Preview State Transitions", transition_time > 0.0f ) )
				transition_time = transition_time > 0.0f ? 0.0f : 2.0f;

			if( transition_time > 0.0f )
			{
				ImGui::SetNextItemWidth( -ImGui::GetFontSize() * 7.0f );
				ImGui::InputFloat( "Transition Length", &transition_time );
			}
			ImGui::PopItemWidth();
			ImGui::EndPopup();
		}

		if( !mesh_lights.empty() )
		{
			ImGui::SameLine();
			if( ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 8.0f )
				ImGui::NewLine();
			toggle_override_mesh_lights = ImGui::Checkbox( "Override Mesh Lights", &lights_static->disable_mesh_lights );
		}

		unsigned to_remove = -1;
		bool changed = false;

		for( unsigned idx = 0; idx < lights_static->dynamic_lights.size(); ++idx )
		{
			auto& light = lights_static->dynamic_lights[ idx ];
			auto id_lock = ImGuiEx::PushID( &light );

			if( idx >= lights_component->GetDynamicLights().size() )
				break;
				
			bool open = true;
			changed |= RenderDynamicLight( light, lights_component, idx, &open, refresh_ao, callback );
			if( !open )
				to_remove = idx;
		}

		if( !lights_static->disable_mesh_lights && !toggle_override_mesh_lights )
		{
			for( unsigned i = 0; i < mesh_lights.size(); ++i )
			{
				auto id_lock = ImGuiEx::PushID( i );
				RenderDynamicLight( mesh_lights[ i ], nullptr, -1, nullptr, refresh_ao, callback );
			}
		}

		if( add_point_light )
		{
			Animation::Components::LightsStatic::DynamicLight light;
			light.type = Renderer::Scene::Light::Type::Point;
			light.states.emplace_back();
			light.states.back().position = simd::vector3( 0.0f, 0.0f, -125.0f );
			light.states.back().colour = simd::vector3( 1.0f, 1.0f, 1.0f );
			light.states.back().radius = 500.0f;
			lights_static->dynamic_lights.push_back( std::move( light ) );
			refresh_ao = true;
		}

		if( add_spot_light )
		{
			Animation::Components::LightsStatic::DynamicLight light;
			light.type = Renderer::Scene::Light::Type::Spot;
			light.states.emplace_back();
			light.states.back().position = simd::vector3( 0.0f, 0.0f, -125.0f );
			light.states.back().direction = simd::vector3( 0.0f, -1.0f, 0.0f );
			light.states.back().colour = simd::vector3( 1.0f, 1.0f, 1.0f );
			light.states.back().radius = 1000.0f;
			light.states.back().penumbra = 0.0f;
			light.states.back().umbra = 0.25f;
			lights_static->dynamic_lights.push_back( std::move( light ) );
			refresh_ao = true;
		}

		if( toggle_override_mesh_lights )
		{
			if( lights_static->disable_mesh_lights )
			{
				for( const auto& light : mesh_lights )
					lights_static->dynamic_lights.push_back( light );
			}
			else
			{
				RemoveIf( lights_static->dynamic_lights, []( const Animation::Components::LightsStatic::DynamicLight& dynamic_light ) { return dynamic_light.origin != Objects::Origin::Parent; } );
			}
			refresh_ao = true;
		}

		if( to_remove != -1 )
		{
			lights_static->dynamic_lights.erase( lights_static->dynamic_lights.begin() + to_remove );
			refresh_ao = true;
		}

		return changed;
	}

	void RenderText( const simd::vector2 point, const simd::color& color, const std::string& text, const ImVec2 align = ImVec2( 0.5, 0.5 ) )
	{
		auto* viewport = ImGui::GetMainViewport();
		auto* drawlist = ImGui::GetBackgroundDrawList( viewport );

		const ImFont* font = ImGui::GetFont();
		const float font_size = font->FontSize * font->Scale * ImGui::GetIO().FontGlobalScale;
		ImVec2 p1 = ImVec2( point.x, point.y ) + viewport->Pos;
		ImVec2 text_size = font->CalcTextSizeA( font_size, FLT_MAX, 0.0f, text.c_str() );
		p1.x -= text_size.x * align.x;
		p1.y -= text_size.y * align.y;
		drawlist->AddText( font, font_size, p1 + ImVec2( 1.0f, 1.0f ), ImColor( 0, 0, 0, color.a / 2 ), text.c_str() );
		drawlist->AddText( font, font_size, p1, ImColor( color.r, color.g, color.b, color.a ), text.c_str() );
	}

	std::vector< simd::vector3 > GetCirclePoints( const simd::vector3 centre, const float radius, const simd::vector3& axis, const unsigned num_points = 72, const float start = 0.0f, const float end = PI2 )
	{
		const simd::vector3 unit_axis = axis.normalize();

		simd::vector3 a, b;

		if( abs( unit_axis.y ) > FLT_EPSILON || abs( unit_axis.z ) > FLT_EPSILON )
			a = unit_axis.cross( simd::vector3( 1, 0, 0 ) ).normalize();
		else
			a = unit_axis.cross( simd::vector3( 0, 1, 0 ) ).normalize();

		b = a.cross( unit_axis ).normalize();

		std::vector< simd::vector3 > points( num_points );
		for( unsigned i = 0; i < num_points; ++i )
		{
			float t = start + ( end - start ) * i / ( num_points - 1 );

			points[ i ] = centre + simd::vector3( radius * cosf( t ) * a.x + radius * sinf( t ) * b.x,
												  radius * cosf( t ) * a.y + radius * sinf( t ) * b.y,
												  radius * cosf( t ) * a.z + radius * sinf( t ) * b.z );
		}
		return points;
	}

	void RenderLightVisualisation( Device::Line& line )
	{
		const auto* camera = Renderer::Scene::System::Get().GetCamera();
		if( !camera )
			return;

		Renderer::Scene::PointLights point_lights = Renderer::Scene::System::Get().GetVisiblePointLights();
		Renderer::Scene::Lights lights, shadow_lights;
		std::tie( lights, shadow_lights ) = Renderer::Scene::System::Get().GetVisibleLightsNoPoint();

		const auto draw_usage_frequency = [&]( const simd::vector3 position, const simd::color& color, const Renderer::Scene::Light::UsageFrequency usage_frequency )
		{
			if( camera->IsPointBehindCamera( position ) )
				return;

			const simd::vector2 ss = camera->WorldToScreen( position );
			std::string usage_freq = IMGUI_ICON_LIGHT;

			if( usage_frequency == Renderer::Scene::Light::UsageFrequency::Low )
				usage_freq += "L";
			else if( usage_frequency == Renderer::Scene::Light::UsageFrequency::Medium )
				usage_freq += "M";
			else if( usage_frequency == Renderer::Scene::Light::UsageFrequency::High )
				usage_freq += "H";

			RenderText( ss, color, usage_freq );
		};

		// draw point lights
		for( const auto* light : point_lights )
		{
			const simd::vector3 origin = { light->GetPosition().x, light->GetPosition().y, light->GetPosition().z };
			const simd::vector3 direction = { light->GetDirection().x, light->GetDirection().y, light->GetDirection().z };
			const simd::vector4 col = light->GetColor();
			const float max_rgb = std::max( { col.x, col.y, col.z } );
			const float multiplier = max_rgb <= 0.0f ? 0.0f : 1.0f / max_rgb;
			const simd::color color( std::powf( col.x * multiplier, 1.0f / 2.2f ),
										std::powf( col.y * multiplier, 1.0f / 2.2f ),
										std::powf( col.z * multiplier, 1.0f / 2.2f ),
										1.0f );

			for( unsigned i = 0; i < light_visualisation_density; ++i )
			{
				if( light_visualisation_density == 3 )
				{
					// draw circle on each axis
					const auto points = GetCirclePoints( origin, light->GetMedianRadius(), simd::vector3( i == 0, i == 1, i == 2 ) );
					line.DrawTransform( &points[ 0 ], ( DWORD )points.size(), &camera->GetViewProjectionMatrix(), color );
				}
				else
				{
					// draw circles around z axis
					const float t = i * PI2 / light_visualisation_density;
					const simd::vector3 axis( cosf( t ), sinf( t ), 0 );
					const auto points = GetCirclePoints( origin, light->GetMedianRadius(), axis );
					line.DrawTransform( &points[ 0 ], ( DWORD )points.size(), &camera->GetViewProjectionMatrix(), color );
				}
			}

			if( direction.sqrlen() > 0.0f )
			{
				const simd::vector3 target = origin + direction * col.w;
				simd::vector3 points[] = { origin, target };
				line.DrawTransform( points, 2, &camera->GetViewProjectionMatrix(), color );
			}
			draw_usage_frequency( origin, color, light->GetLightFrequency() );
		}

		// draw spotlights
		for( const auto* light : lights )
		{
			if( light->GetType() != Renderer::Scene::Light::Type::Spot )
				continue;

			const simd::vector3 from = { light->GetPosition().x, light->GetPosition().y, light->GetPosition().z };
			const simd::vector3 direction = { light->GetDirection().x, light->GetDirection().y, light->GetDirection().z };
			const simd::vector3 to = from + direction * light->GetColor().w;
			const simd::vector4 col = light->GetColor();
			const float max_rgb = std::max( { col.x, col.y, col.z } );
			const float multiplier = max_rgb <= 0.0f ? 0.0f : 1.0f / max_rgb;
			const simd::color color( std::powf( col.x * multiplier, 1.0f / 2.2f ),
										std::powf( col.y * multiplier, 1.0f / 2.2f ),
										std::powf( col.z * multiplier, 1.0f / 2.2f ),
										1.0f );

			const Renderer::Scene::SpotLight* spotlight = static_cast< const Renderer::Scene::SpotLight* >( light );
			if( spotlight == nullptr )
				continue;

			{
				const auto points = GetCirclePoints( to, spotlight->cone_radius, direction );
				line.DrawTransform( &points[ 0 ], ( DWORD )points.size(), &camera->GetViewProjectionMatrix(), color );
			}

			for( const auto& point : GetCirclePoints( to, spotlight->cone_radius, direction, light_visualisation_density + 1 ) )
			{
				const simd::vector3 points[] = { from, point };
				line.DrawTransform( points, 2, &camera->GetViewProjectionMatrix(), color );
			}

			{
				const simd::vector3 points[] = { from, to };
				line.DrawTransform( points, 2, &camera->GetViewProjectionMatrix(), color );
			}

			draw_usage_frequency( to, color, light->GetLightFrequency() );
		}
	}
}

#endif