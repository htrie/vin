#include "ToolGUI.h"

#if DEBUG_GUI_ENABLED

#include <stdlib.h>

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/error/en.h>

#include "Common/File/FileSystem.h"
#include "Common/File/PathHelper.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/LoadingScreen.h"

#include "ClientInstanceCommon/Terrain/TileMap.h"

#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/Icons.h"
#include "Visual/Environment/EnvironmentEditor.h"
#include "Visual/Renderer/Scene/ModelViewerCamera.h"
#include "Visual/Renderer/Scene/InGameCamera.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Renderer/Scene/SubsceneManager.h"
#include "Visual/Utility/JsonUtility.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/DynamicCulling.h"
#include "Visual/Engine/Plugins/LoggingPlugin.h"
#include "Visual/Engine/Plugins/PerformancePlugin.h"
#include "Visual/Device/WindowDX.h"
#include "Visual/GUI2/imgui/imgui_ex.h"

#define STYLE_MEMBERS \
	STYLE( AntiAliasedLines ) STYLE( AntiAliasedFill ) STYLE( WindowPadding ) STYLE( WindowMinSize ) STYLE( WindowTitleAlign ) \
	STYLE( FramePadding ) STYLE( ItemSpacing ) STYLE( ItemInnerSpacing ) STYLE( TouchExtraPadding ) STYLE( ButtonTextAlign ) \
	STYLE( SelectableTextAlign ) STYLE( DisplayWindowPadding ) STYLE( DisplaySafeAreaPadding ) STYLE( Alpha ) STYLE( WindowRounding ) \
	STYLE( WindowBorderSize ) STYLE( ChildRounding ) STYLE( ChildBorderSize ) STYLE( PopupRounding ) STYLE( PopupBorderSize ) \
	STYLE( FrameRounding ) STYLE( FrameBorderSize ) STYLE( IndentSpacing ) STYLE( ColumnsMinSpacing ) STYLE( ScrollbarSize ) \
	STYLE( ScrollbarRounding ) STYLE( GrabMinSize ) STYLE( GrabRounding ) STYLE( TabRounding ) STYLE( TabBorderSize ) \
	STYLE( MouseCursorScale ) STYLE( CurveTessellationTol ) STYLE( DisabledAlpha )


namespace Device
{
	namespace GUI
	{
		namespace {
			static void HelpMarker(const char* desc)
			{
				ImGui::TextDisabled("(?)");
				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
					ImGui::TextUnformatted(desc);
					ImGui::PopTextWrapPos();
					ImGui::EndTooltip();
				}
			}
		}

		bool EnableExperimentalUI = false;

		ToolGUI::ToolGUI( const char* _name, const bool force_use_python, const bool default_open_common )
			: GUIInstance( true )
			, name( _name )
			, open_common( default_open_common )
#if PYTHON_ENABLED == 1
			, python_interpreter( name, force_use_python )
#endif
		{
		#if defined(WIN32) && !defined(CONSOLE)
			if( std::ifstream env( "environment_variables.ini" ); env.is_open() )
			{
				std::string line;
				while( std::getline( env, line ) )
				{
					LOG_INFO_F( L"Setting environment variable: {}", StringToWstring( line ) );
					_putenv( line.c_str() );
				}
			}
		#endif

			ImGuiEx::StyleColorsGemini( &default_style );

			GUIResourceManager::GetGUIResourceManager()->SetConfigPath( "Tools/" + std::string( name ) );
		}

		ToolGUI::~ToolGUI()
		{
			
		}

		void ToolGUI::OnLoadSettings(Utility::ToolSettings* settings)
		{
			auto& doc = settings->GetDocument();
			ImGuiIO& io = ImGui::GetIO();

			auto* window = WindowDX::GetWindow();
			assert(window);

			const std::string save_name = name + std::string("_common");
			auto& saved_settings = doc[save_name.c_str()];
			if (!saved_settings.IsNull())
			{
			#if defined(WIN32) && !defined(CONSOLE)
				const rapidjson::Value& window_settings = saved_settings["window_settings"];
				if ( /* TEMP UNTIL FIXED */ false && !window_settings.IsNull() && name != "path_of_exile")
				{
					GetWindowStyle();

					int x = window_rect.left;
					int y = window_rect.top;
					int w = window_rect.right - x;
					int h = window_rect.bottom - y;

					auto& pos = window_settings["position"];
					if (!pos.IsNull())
					{
						x = pos.GetArray()[0].GetInt();
						y = pos.GetArray()[1].GetInt();
					}

					auto& size = window_settings["size"];
					if (!size.IsNull())
					{
						w = size.GetArray()[0].GetInt();
						h = size.GetArray()[1].GetInt();
					}

					auto& style = window_settings["style"];
					if (!style.IsNull())
						window_style = style.GetInt();

					::SetWindowPos(window->GetWnd(), HWND_NOTOPMOST, x, y, w, h, 0);
					window->SetWindowStyle(window_style);
				}
			#endif

				auto& show_help = saved_settings["show_help"];
				if (!show_help.IsNull())
					this->show_help = show_help.GetBool();
			}

		#if defined(WIN32) && !defined(CONSOLE)
			// Show window now!
			if (name != "path_of_exile")
			{
				window->ShowWindow(SW_SHOW);
				SetForegroundWindow(reinterpret_cast<HWND>(window->GetWnd()));
			}
		#endif

			const rapidjson::Value& imgui_settings = doc["imgui_settings"];
			if (!imgui_settings.IsNull())
			{
				opacity_display = imgui_settings["opacity"].GetInt();
				GUIResourceManager::GetGUIResourceManager()->opacity = Clamp(opacity_display / 100.0f, 0.01f, 0.9999f);
				if( auto& scale = imgui_settings[ "scale" ]; scale.IsFloat() )
					GUIResourceManager::GetGUIResourceManager()->desired_scale = Clamp( scale.GetFloat(), 0.5f, 2.0f );
				EnableExperimentalUI = imgui_settings["show_experimental_ui"].GetBool();
				this->style = imgui_settings["color_scheme"].GetInt();
				if (auto& move_titlebar_only = imgui_settings["move_titlebar_only"]; move_titlebar_only.IsBool())
					io.ConfigWindowsMoveFromTitleBarOnly = move_titlebar_only.GetBool();

				const auto& style_settings = imgui_settings["style_settings"];

			#define STYLE(name) JsonUtility::Read( style_settings, current_style.name, #name );
				STYLE_MEMBERS
			#undef STYLE

				JsonUtility::Read(imgui_settings, io.FontGlobalScale, "font_global_scale");
				JsonUtility::Read(imgui_settings, selectedFont, "used_font");

				const auto& fontList = imgui_settings["fonts"];
				ImFontAtlas* atlas = io.Fonts;
				if (atlas != nullptr)
				{
					if (fontList.IsArray())
					{
						for (int i = 0; i < atlas->Fonts.Size && i < (int)fontList.Size(); i++)
						{
							ImFont* font = atlas->Fonts[i];
							const auto& font_settings = fontList[i];
							JsonUtility::Read(font_settings, font->Scale, "scale");
						}
					}

					if (selectedFont < atlas->Fonts.Size)
						io.FontDefault = atlas->Fonts[selectedFont];
				}
			}

			ReloadStyle();

			environment_editor.OnLoadSettings( settings );
		}

		void ToolGUI::OnSaveSettings(Utility::ToolSettings* settings)
		{
			using namespace rapidjson;

			auto& doc = settings->GetDocument();
			auto& allocator = settings->GetAllocator();

			// App Settings
			{
				Value saved_settings(kObjectType);

				// Window settings
				if (strcmp(name, "path_of_exile") != 0)
				{
					Value window_settings(kObjectType);

					int x = window_rect.left;
					int y = window_rect.top;
					int w = window_rect.right - x;
					int h = window_rect.bottom - y;

					Value pos(kArrayType);
					pos.PushBack(x, allocator);
					pos.PushBack(y, allocator);
					window_settings.AddMember("position", pos, allocator);

					Value size(kArrayType);
					size.PushBack(w, allocator);
					size.PushBack(h, allocator);
					window_settings.AddMember("size", size, allocator);

					window_settings.AddMember("style", Value().SetInt(window_style), allocator);

					saved_settings.AddMember("window_settings", window_settings, allocator);
				}

				saved_settings.AddMember("show_help", show_help, allocator);

				const std::string save_name = name + std::string("_common");
				doc.RemoveMember(save_name.c_str());
				doc.AddMember(Value().SetString(save_name.c_str(), allocator), saved_settings, allocator);
			}

			// ImGui Settings
			{
				ImGuiIO& io = ImGui::GetIO();
				Value imgui_settings(kObjectType);

				imgui_settings.AddMember("opacity", opacity_display, allocator);
				imgui_settings.AddMember("scale", Device::GUI::GUIResourceManager::GetGUIResourceManager()->scale, allocator);
				imgui_settings.AddMember("show_experimental_ui", EnableExperimentalUI, allocator);
				imgui_settings.AddMember("color_scheme", this->style, allocator);
				imgui_settings.AddMember("move_titlebar_only", io.ConfigWindowsMoveFromTitleBarOnly, allocator);

				Value style_settings(kObjectType);

			#define STYLE(name) JsonUtility::Write( style_settings, current_style.name, #name, allocator );
				STYLE_MEMBERS
			#undef STYLE

				JsonUtility::Write(imgui_settings, io.FontGlobalScale, "font_global_scale", allocator);
				JsonUtility::Write(imgui_settings, selectedFont, "used_font", allocator);

				ImFontAtlas* atlas = io.Fonts;
				Value fontList(kArrayType);
				for (int i = 0; i < atlas->Fonts.Size; i++)
				{
					ImFont* font = atlas->Fonts[i];
					Value font_settings(kObjectType);
					JsonUtility::Write(font_settings, font->Scale, "scale", allocator);
					fontList.PushBack(font_settings, allocator);
				}

				imgui_settings.AddMember("fonts", fontList, allocator);

				imgui_settings.AddMember("style_settings", style_settings, allocator);

				const std::string save_name = "imgui_settings";
				doc.RemoveMember(save_name.c_str());
				doc.AddMember(Value().SetString(save_name.c_str(), allocator), imgui_settings, allocator);
			}

			environment_editor.OnSaveSettings( settings );
		}

		bool ToolGUI::MsgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam)
		{
			switch (umsg)
			{
			case WM_KEYDOWN:
				if (ImGui::GetIO().KeyShift && wParam == VK_F4)
				{
					Toggle( environment_editor.open );
					return true;
				}
			}

			return false;
		}

		void ToolGUI::OnRenderMenu( const float elapsed_time )
		{
			// 1.0f doesn't work
			// 0.0f crashes
			GUIResourceManager::GetGUIResourceManager()->opacity = Clamp( opacity_display / 100.0f, 0.01f, 0.9999f );

			// This could change at any point, so we should not store it
			auto* environment_settings = GetEnvironmentSettings();

			ImGui::PushItemWidth( -1 );
			ImGui::Text( "Framerate: %d", ( int )ImGui::GetIO().Framerate );

			ImGui::BeginGroup();
			if (ImGui::Button(IMGUI_ICON_SAVE "##SaveSettingsButton"))
				GUIResourceManager::GetGUIResourceManager()->SaveSettings();

			ImGui::SameLine();
			const float last_saved = GUIResourceManager::GetGUIResourceManager()->LastSaveTime();
			if (last_saved < 60.0f)
				ImGui::Text("Settings Saved: %.0f sec ago", last_saved);
			else if(last_saved < 3600.0f)
				ImGui::Text("Settings Saved: %.0f min ago", last_saved / 60.0f);
			else if(last_saved < 86400.0f)
				ImGui::Text("Settings Saved: %.0f hours ago", last_saved / 3600.0f);
			else if (last_saved < 31536000.0f)
				ImGui::Text("Settings Saved: %.0f days ago", last_saved / 86400.0f);
			else 
				ImGui::Text("Settings not saved");

			ImGui::EndGroup();
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(35 * ImGui::GetFontSize());
				ImGui::Text("Save Settings");
				ImGui::Separator();
				ImGui::TextWrapped( "Saves UI settings (like window positions, etc). These settings are saved automatically from time to time. Click this button if you want to save them right now.\n"
					"Note that these settings are shared between all tools and clients. To save these settings permanently, ensure that you have only one tool or client open." );
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}

			if( ImGui::CollapsingHeader( "Common", nullptr, open_common ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None ) )
			{
				if( ImGui::Button( "Reset Layout" ) )
					GUIResourceManager::GetGUIResourceManager()->ResetLayout();

				ImGui::SameLine();
				if( ImGui::Combo( "##Style", &style, "Gemini\0Classic\0Dark\0Light\0" ) )
					ReloadStyle();

				if( ImGui::BeginTable( "Checkboxes", 2 ) )
				{
					ImGui::TableNextColumn();

					ImGui::SetNextItemWidth( -FLT_MIN );
					ImGui::SliderInt( "##Opacity", &opacity_display, 0, 100, "Opacity: %d" );
					ImGui::Checkbox( "Show Environment Editor", &environment_editor.open );
					ImGui::Checkbox( "Show UI Debug GUI", &ui_debug_gui.open );

				#if PYTHON_ENABLED == 1
					ImGui::Checkbox( "Show Python Console", &python_interpreter.open );
				#endif

					ImGui::Checkbox( "Enable Experimental UI", &EnableExperimentalUI );
					ImGui::Checkbox( "Show Style Editor", &show_style_window );

					if (auto plugin = Engine::PluginManager::Get().GetPlugin<Engine::Plugins::LoggingPlugin>())
						plugin->RenderNotification();

					ImGui::TableNextColumn();

					ImGui::SetNextItemWidth( -FLT_MIN );
					ImGui::DragFloat( "##Scale", &GUIResourceManager::GetGUIResourceManager()->desired_scale, 0.01f, 0.5f, 2.0f, "Scale: %.2f", ImGuiSliderFlags_AlwaysClamp );
					ImGui::Checkbox( "Show About", &show_about_window );
					ImGui::Checkbox( "Show Metrics", &show_metrics_window );
					ImGui::Checkbox( "Show Demo", &show_gui_demo );
					ImGui::Checkbox( "Show Help", &show_help );
					ImGui::Checkbox( "Move with title bar", &ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly);

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(35 * ImGui::GetFontSize());
						ImGui::Text("Move windows with title bar only");
						ImGui::Separator();
						ImGui::TextWrapped("When enabled, windows can only be moved by clicking their title bar. \nWhen disabled, windows can be dragged by clicking anywhere inside them.");
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}

					ImGui::EndTable();
				}

			#ifdef PROFILING
				if (auto plugin = Engine::PluginManager::Get().GetPlugin<Engine::Plugins::PerformancePlugin>())
					plugin->RenderUI();
			#endif
			}

		#if defined(ENABLE_DEBUG_VIEWMODES) || defined(PROFILING)
			if (ImGui::CollapsingHeader("Debugging"))
			{
				ImGui::Indent();

			#ifdef PROFILING
				if (ImGui::CollapsingHeader("Loading Screen"))
				{
					const auto stats = LoadingScreen::GetStats();
					ImGui::Text("In Loading Screen: %s", LoadingScreen::IsActive() ? "Yes" : "No");
					ImGui::Text("Active State: %s", stats.ActiveState ? "true" : "false");
					ImGui::Text("Lock Count: %d", unsigned(stats.LockCount));
					ImGui::Text("Frame Delay: %d", unsigned(stats.FrameDelay));
					ImGui::Text("Last Duration: %u ms", unsigned(stats.LastDuration));
				}
			#else
				ImGui::Text("In Loading Screen: %s", LoadingScreen::IsActive() ? "Yes" : "No");
			#endif

			#ifdef ENABLE_DEBUG_VIEWMODES
				if (ImGui::CollapsingHeader("Dynamic Culling"))
				{
					auto GetOptionText = [](unsigned int i) -> std::pair<std::string, std::string>
					{
						if (i == 0)
							return std::make_pair("Disabled", "");

						switch ((DynamicCulling::ViewFilter)(i - 1))
						{
							case DynamicCulling::ViewFilter::Default:
								return std::make_pair("Enabled", "");
							case DynamicCulling::ViewFilter::NoCosmetic:
								return std::make_pair("No Cosmetic", "Shows only particles with 'Important' or 'Essential' priority.\nNote: The "
													  "difference between 'Worst Case' and 'No Cosmetic' is that with 'Worst Case', all important particles "
													  "above the priority threshold are culled as well");
							case DynamicCulling::ViewFilter::WorstCase:
								return std::make_pair("Worst Case", "Shows the effect as if you had really bad performance.\nNote: The "
													  "difference between 'Worst Case' and 'No Cosmetic' is that with 'Worst Case', all important particles "
													  "above the priority threshold are culled as well");
							case DynamicCulling::ViewFilter::CosmeticOnly:
								return std::make_pair("Cosmetic Only", "");
							case DynamicCulling::ViewFilter::ImportantOnly:
								return std::make_pair("Important Only", "");
							case DynamicCulling::ViewFilter::EssentialOnly:
								return std::make_pair("Essential Only", "");
							default:
								return std::make_pair("Error: Unknown Value", "");
						}
					};

					const float content_width = ImGui::GetContentRegionAvail().x;
					const float inner_spacing = ImGui::GetStyle().ItemInnerSpacing.x;
					const float button_size = ImGui::GetFrameHeight();

					ImGui::PushItemWidth(content_width);

					unsigned int dynamic_culling_selection = 0;
					if (Device::DynamicCulling::Get().Enabled())
						dynamic_culling_selection = 1 + (unsigned int)Device::DynamicCulling::Get().GetViewFilter();

					if (ImGui::BeginCombo("##DynamicCulling", ("Dynamic Culling: " + GetOptionText(dynamic_culling_selection).first).c_str()))
					{
						if (ImGui::Selectable("Disabled", dynamic_culling_selection == 0))
						{
							dynamic_culling_selection = 0;
							Device::DynamicCulling::Get().Enable(false);
							Device::DynamicCulling::Get().ForceActive(dynamic_culling_force_active = false);
						}

						for (unsigned int a = 0; a < (unsigned int)DynamicCulling::ViewFilter::NumViewFilters; a++)
						{
							const auto texts = GetOptionText(a + 1);
							if (ImGui::Selectable(texts.first.c_str(), dynamic_culling_selection == a + 1))
							{
								dynamic_culling_selection = a + 1;
								Device::DynamicCulling::Get().Enable(true);
								Device::DynamicCulling::Get().ForceActive(dynamic_culling_force_active = a > 0);
								Device::DynamicCulling::Get().SetViewFilter((DynamicCulling::ViewFilter)a);
							}

							if (ImGui::IsItemHovered() && !texts.second.empty())
							{
								ImGui::BeginTooltip();
								ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());

								ImGui::TextWrapped("%s", texts.first.c_str());
								ImGui::Separator();
								ImGui::TextWrapped("%s", texts.second.c_str());

								ImGui::PopTextWrapPos();
								ImGui::EndTooltip();
							}
						}

						ImGui::EndCombo();
					}

					if (Device::DynamicCulling::Get().Acitve())
						ImGui::Text("Active: true");
					else
						ImGui::Text("Active: false");

					ImGui::SameLine(content_width / 2, 0);
					if (dynamic_culling_selection != 1 + (unsigned int)DynamicCulling::ViewFilter::WorstCase)
					{
						if (ImGui::Checkbox("##OverwriteAggression", &dynamic_culling_overwrite_aggression))
						{
							if (dynamic_culling_overwrite_aggression)
								DynamicCulling::Get().SetForcedAggression(dynamic_culling_aggression / 100.0f);
							else
								DynamicCulling::Get().DisableForcedAggression();
						}

						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("Overwrite Aggression");

						ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
						if (dynamic_culling_overwrite_aggression)
						{
							ImGui::SetNextItemWidth((content_width / 2) - (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x));
							if (ImGui::SliderFloat("##ForcedAggression", &dynamic_culling_aggression, 0.0f, 100.0f, "Aggression: %.1f %%"))
							{
								dynamic_culling_aggression = std::min(100.0f, std::max(0.0f, dynamic_culling_aggression));
								DynamicCulling::Get().SetForcedAggression(dynamic_culling_aggression / 100.0f);
							}
						}
						else
							ImGui::Text("Aggression: %.1f %%", Device::DynamicCulling::Get().GetAggression() * 100.0f);
					}
					else
						ImGui::Text("Aggression: 100 %%");

					if (ImGui::Checkbox("Force Active", &dynamic_culling_force_active))
						Device::DynamicCulling::Get().ForceActive(dynamic_culling_force_active);

					ImGui::SameLine(content_width / 2, 0);
					ImGui::Text("Bucket Size: %2i", int(Device::DynamicCulling::Get().GetBucketSize()));

					ImGui::Text("Alive: %i", int(Device::DynamicCulling::Get().GetParticlesAlive()));
					ImGui::SameLine(content_width / 2, 0);
					ImGui::Text("Killed: %i", int(Device::DynamicCulling::Get().GetParticlesKilled()));

					// Heatmap:
					{
						const auto heatmap = Device::DynamicCulling::Get().GetHeatMap();
						const float w = content_width / heatmap.W;

					#ifdef DYNAMIC_CULLING_CONFIGURATION
						int mapW = int(heatmap.W);
						int mapH = int(heatmap.H);
						int mapD = int(heatmap.NumPerBucket);
						bool changed = false;
						if (ImGui::SliderInt("##HeatMapW", &mapW, 1, 16 * 15, "Num Buckets X: %i"))
							changed = true;

						if (ImGui::SliderInt("##HeatMapH", &mapH, 1, 9 * 15, "Num Buckets Y: %i"))
							changed = true;

						if (ImGui::SliderInt("##HeatMapD", &mapD, 0, 64, "Particles Per Bucket: %i"))
							changed = true;

						if (changed)
							Device::DynamicCulling::Get().SetHeatMapSize(size_t(mapW), size_t(mapH), size_t(mapD));

					#endif

						ImGui::Text("Heatmap:");
						ImGuiWindow* window = ImGui::GetCurrentWindow();
						ImGuiContext& g = *GImGui;
						const ImGuiStyle& style = g.Style;
						const ImGuiID id = window->GetID("##Heatmap");
						const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(content_width, w * heatmap.H));
						const ImRect total_bb(frame_bb.Min, frame_bb.Max);

						ImGui::ItemSize(total_bb, style.FramePadding.y);
						if (ImGui::ItemAdd(total_bb, id, &frame_bb))
						{
							auto draw_list = ImGui::GetWindowDrawList();
							const ImVec2 rctSize(w, w);
							for (size_t y = 0; y < heatmap.H; y++)
							{
								for (size_t x = 0; x < heatmap.W; x++)
								{
									const auto value = heatmap.Get(x, y);
									const ImColor color = ImColor(std::max(0.0f, std::min(value, 1.0f)), std::max(0.0f, std::min(2.0f - value, 1.0f)), 0.0f);

									const ImVec2 p = frame_bb.Min + ImVec2(x * w, y * w);
									draw_list->AddRectFilled(p, p + rctSize, color, 0.0f);
								}
							}
						}
					}

					ImGui::PopItemWidth();
				}
			#endif

				ImGui::Unindent();
			}
		#endif

			ImGui::PopItemWidth();

			GUIInstance::OnRenderMenu( elapsed_time );
		}

		void ToolGUI::OnRender( const float elapsed_time )
		{
			GetWindowStyle();

			if( show_style_window )
			{
				if( ImGui::Begin( "Style Editor", &show_style_window ) )
				{
					ShowStyleEditor( &default_style );
				}
				ImGui::End();
			}

			if( show_about_window )
				ImGui::ShowAboutWindow( &show_about_window );
			if( show_metrics_window )
				ImGui::ShowMetricsWindow( &show_metrics_window );
			if( show_gui_demo )
				ImGui::ShowDemoWindow( &show_gui_demo );
			if( show_help )
				ShowHelpWindow( elapsed_time );

		#ifdef PROFILING
			if (show_asset_info)
				ShowAssetStreamingInfo();
		#endif

			if( environment_editor.open )
				environment_editor.OnRender( GetEnvironmentSettings(), elapsed_time, GetEnvironmentMixer() );

			if( ui_debug_gui.open )
				ui_debug_gui.OnRender( elapsed_time );

			GUIInstance::OnRender( elapsed_time );
		}

		void ToolGUI::ShowAssetStreamingInfo()
		{
			// [TODO] Implement again.
		}

		void ToolGUI::ShowStyleEditor(ImGuiStyle* ref)
		{
			// You can pass in a reference ImGuiStyle structure to compare to, revert to and save to (else it compares to an internally stored reference)
			ImGuiStyle& style = current_style;
			static ImGuiStyle ref_saved_style;

			// Default to using internal storage as reference
			static bool init = true;
			if (init && ref == NULL)
				ref_saved_style = style;
			init = false;
			if (ref == NULL)
				ref = &ref_saved_style;

			bool changed = false;
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.50f);

			if (ImGui::Combo("Colors##Selector", &this->style, "Gemini\0Classic\0Dark\0Light\0"))
			{
				ReloadStyle();
				ref_saved_style = style;
			}

			ImGui::ShowFontSelector("Fonts##Selector");

			// Simplified Settings (expose floating-pointer border sizes as boolean representing 0.0f or 1.0f)
			if (ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f, "%.0f"))
			{
				style.GrabRounding = style.FrameRounding; // Make GrabRounding always the same value as FrameRounding
				changed = true;
			}
			{ bool border = (style.WindowBorderSize > 0.0f); if (ImGui::Checkbox("WindowBorder", &border)) { style.WindowBorderSize = border ? 1.0f : 0.0f; changed = true; } }
			ImGui::SameLine();
			{ bool border = (style.FrameBorderSize > 0.0f);  if (ImGui::Checkbox("FrameBorder", &border)) { style.FrameBorderSize = border ? 1.0f : 0.0f; changed = true; } }
			ImGui::SameLine();
			{ bool border = (style.PopupBorderSize > 0.0f);  if (ImGui::Checkbox("PopupBorder", &border)) { style.PopupBorderSize = border ? 1.0f : 0.0f; changed = true; } }

			// Save/Revert button
			if (ImGui::Button("Save Ref"))
				*ref = ref_saved_style = style;
			ImGui::SameLine();
			if (ImGui::Button("Revert Ref"))
			{
				style = *ref;
				changed = true;
			}

			ImGui::SameLine();
			HelpMarker(
				"Save/Revert in local non-persistent storage. Default Colors definition are not affected. "
				"Use \"Export\" below to save them somewhere.");

			ImGui::Separator();

			if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
			{
				if (ImGui::BeginTabItem("Sizes"))
				{
					ImGui::Text("Main");
					if (ImGui::SliderFloat2("WindowPadding", (float*)&style.WindowPadding, 0.0f, 20.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat2("FramePadding", (float*)&style.FramePadding, 0.0f, 20.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat2("CellPadding", (float*)&style.CellPadding, 0.0f, 20.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat2("ItemSpacing", (float*)&style.ItemSpacing, 0.0f, 20.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat2("ItemInnerSpacing", (float*)&style.ItemInnerSpacing, 0.0f, 20.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat2("TouchExtraPadding", (float*)&style.TouchExtraPadding, 0.0f, 10.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat("IndentSpacing", &style.IndentSpacing, 0.0f, 30.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat("ScrollbarSize", &style.ScrollbarSize, 1.0f, 20.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat("GrabMinSize", &style.GrabMinSize, 1.0f, 20.0f, "%.0f")) changed = true;
					ImGui::Text("Borders");
					if (ImGui::SliderFloat("WindowBorderSize", &style.WindowBorderSize, 0.0f, 1.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat("ChildBorderSize", &style.ChildBorderSize, 0.0f, 1.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat("PopupBorderSize", &style.PopupBorderSize, 0.0f, 1.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat("FrameBorderSize", &style.FrameBorderSize, 0.0f, 1.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat("TabBorderSize", &style.TabBorderSize, 0.0f, 1.0f, "%.0f")) changed = true;
					ImGui::Text("Rounding");
					if (ImGui::SliderFloat("WindowRounding", &style.WindowRounding, 0.0f, 12.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat("ChildRounding", &style.ChildRounding, 0.0f, 12.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat("PopupRounding", &style.PopupRounding, 0.0f, 12.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat("ScrollbarRounding", &style.ScrollbarRounding, 0.0f, 12.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat("GrabRounding", &style.GrabRounding, 0.0f, 12.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat("LogSliderDeadzone", &style.LogSliderDeadzone, 0.0f, 12.0f, "%.0f")) changed = true;
					if (ImGui::SliderFloat("TabRounding", &style.TabRounding, 0.0f, 12.0f, "%.0f")) changed = true;
					ImGui::Text("Alignment");
					if (ImGui::SliderFloat2("WindowTitleAlign", (float*)&style.WindowTitleAlign, 0.0f, 1.0f, "%.2f")) changed = true;
					int window_menu_button_position = style.WindowMenuButtonPosition + 1;
					if (ImGui::Combo("WindowMenuButtonPosition", (int*)&window_menu_button_position, "None\0Left\0Right\0"))
					{
						style.WindowMenuButtonPosition = window_menu_button_position - 1;
						changed = true;
					}
					if (ImGui::Combo("ColorButtonPosition", (int*)&style.ColorButtonPosition, "Left\0Right\0")) changed = true;
					if (ImGui::SliderFloat2("ButtonTextAlign", (float*)&style.ButtonTextAlign, 0.0f, 1.0f, "%.2f")) changed = true;
					ImGui::SameLine(); HelpMarker("Alignment applies when a button is larger than its text content.");
					if (ImGui::SliderFloat2("SelectableTextAlign", (float*)&style.SelectableTextAlign, 0.0f, 1.0f, "%.2f")) changed = true;
					ImGui::SameLine(); HelpMarker("Alignment applies when a selectable is larger than its text content.");
					ImGui::Text("Safe Area Padding");
					ImGui::SameLine(); HelpMarker("Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured).");
					if (ImGui::SliderFloat2("DisplaySafeAreaPadding", (float*)&style.DisplaySafeAreaPadding, 0.0f, 30.0f, "%.0f")) changed = true;
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Colors"))
				{
					static int output_dest = 0;
					static bool output_only_modified = true;
					if (ImGui::Button("Export"))
					{
						if (output_dest == 0)
							ImGui::LogToClipboard();
						else
							ImGui::LogToTTY();
						ImGui::LogText("ImVec4* colors = ImGui::GetStyle().Colors;" IM_NEWLINE);
						for (int i = 0; i < ImGuiCol_COUNT; i++)
						{
							const ImVec4& col = style.Colors[i];
							const char* name = ImGui::GetStyleColorName(i);
							if (!output_only_modified || memcmp(&col, &ref->Colors[i], sizeof(ImVec4)) != 0)
								ImGui::LogText("colors[ImGuiCol_%s]%*s= ImVec4(%.2ff, %.2ff, %.2ff, %.2ff);" IM_NEWLINE,
											   name, 23 - (int)strlen(name), "", col.x, col.y, col.z, col.w);
						}
						ImGui::LogFinish();
					}
					ImGui::SameLine(); ImGui::SetNextItemWidth(120); ImGui::Combo("##output_type", &output_dest, "To Clipboard\0To TTY\0");
					ImGui::SameLine(); ImGui::Checkbox("Only Modified Colors", &output_only_modified);

					static ImGuiTextFilter filter;
					filter.Draw("Filter colors", ImGui::GetFontSize() * 16);

					static ImGuiColorEditFlags alpha_flags = 0;
					if (ImGui::RadioButton("Opaque", alpha_flags == ImGuiColorEditFlags_None)) { alpha_flags = ImGuiColorEditFlags_None; } ImGui::SameLine();
					if (ImGui::RadioButton("Alpha", alpha_flags == ImGuiColorEditFlags_AlphaPreview)) { alpha_flags = ImGuiColorEditFlags_AlphaPreview; } ImGui::SameLine();
					if (ImGui::RadioButton("Both", alpha_flags == ImGuiColorEditFlags_AlphaPreviewHalf)) { alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf; } ImGui::SameLine();
					HelpMarker(
						"In the color list:\n"
						"Left-click on color square to open color picker,\n"
						"Right-click to open edit options menu.");

					ImGui::BeginChild("##colors", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NavFlattened);
					ImGui::PushItemWidth(-160);
					for (int i = 0; i < ImGuiCol_COUNT; i++)
					{
						const char* name = ImGui::GetStyleColorName(i);
						if (!filter.PassFilter(name))
							continue;
						ImGui::PushID(i);
						ImGui::ColorEdit4("##color", (float*)&style.Colors[i], ImGuiColorEditFlags_AlphaBar | alpha_flags);

						ImGui::BeginDisabled(memcmp(&style.Colors[i], &ref->Colors[i], sizeof(ImVec4)) == 0);
						ImGui::SameLine(0.0f, style.ItemInnerSpacing.x); if (ImGui::Button(IMGUI_ICON_SAVE "##Save")) { ref->Colors[i] = style.Colors[i]; } if (ImGui::IsItemHovered()) ImGui::SetTooltip(IMGUI_ICON_SAVE " Save");
						ImGui::SameLine(0.0f, style.ItemInnerSpacing.x); if (ImGui::Button(IMGUI_ICON_UNDO "##Revert")) { style.Colors[i] = ref->Colors[i]; } if (ImGui::IsItemHovered()) ImGui::SetTooltip(IMGUI_ICON_UNDO " Revert");
						ImGui::EndDisabled();

						ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
						ImGui::TextUnformatted(name);
						ImGui::PopID();
					}
					ImGui::PopItemWidth();
					ImGui::EndChild();

					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Fonts"))
				{
					ImGuiIO& io = ImGui::GetIO();
					ImFontAtlas* atlas = io.Fonts;
					HelpMarker("Read FAQ and docs/FONTS.md for details on font loading.");
					ImGui::ShowFontAtlas(atlas);

					// Post-baking font scaling. Note that this is NOT the nice way of scaling fonts, read below.
					// (we enforce hard clamping manually as by default DragFloat/SliderFloat allows CTRL+Click text to get out of bounds).
					const float MIN_SCALE = 0.3f;
					const float MAX_SCALE = 2.0f;
					HelpMarker(
						"Those are old settings provided for convenience.\n"
						"However, the _correct_ way of scaling your UI is currently to reload your font at the designed size, "
						"rebuild the font atlas, and call style.ScaleAllSizes() on a reference ImGuiStyle structure.\n"
						"Using those settings here will give you poor quality results.");
					static float window_scale = 1.0f;
					ImGui::PushItemWidth(ImGui::GetFontSize() * 8);
					if (ImGui::DragFloat("window scale", &window_scale, 0.005f, MIN_SCALE, MAX_SCALE, "%.2f", ImGuiSliderFlags_AlwaysClamp)) // Scale only this window
					{
						ImGui::SetWindowFontScale(window_scale);
						changed = true;
					}
					if (ImGui::DragFloat("global scale", &io.FontGlobalScale, 0.005f, MIN_SCALE, MAX_SCALE, "%.2f", ImGuiSliderFlags_AlwaysClamp)) changed = true; // Scale everything
					ImGui::PopItemWidth();

					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Rendering"))
				{
					if (ImGui::Checkbox("Anti-aliased lines", &style.AntiAliasedLines)) changed = true;
					ImGui::SameLine();
					HelpMarker("When disabling anti-aliasing lines, you'll probably want to disable borders in your style as well.");

					if (ImGui::Checkbox("Anti-aliased lines use texture", &style.AntiAliasedLinesUseTex)) changed = true;
					ImGui::SameLine();
					HelpMarker("Faster lines using texture data. Require backend to render with bilinear filtering (not point/nearest filtering).");

					if (ImGui::Checkbox("Anti-aliased fill", &style.AntiAliasedFill)) changed = true;
					ImGui::PushItemWidth(ImGui::GetFontSize() * 8);
					if (ImGui::DragFloat("Curve Tessellation Tolerance", &style.CurveTessellationTol, 0.02f, 0.10f, 10.0f, "%.2f")) changed = true;
					if (style.CurveTessellationTol < 0.10f) style.CurveTessellationTol = 0.10f;

					// When editing the "Circle Segment Max Error" value, draw a preview of its effect on auto-tessellated circles.
					if (ImGui::DragFloat("Circle Tessellation Max Error", &style.CircleTessellationMaxError, 0.005f, 0.10f, 5.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) changed = true;
					if (ImGui::IsItemActive())
					{
						ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
						ImGui::BeginTooltip();
						ImGui::TextUnformatted("(R = radius, N = number of segments)");
						ImGui::Spacing();
						ImDrawList* draw_list = ImGui::GetWindowDrawList();
						const float min_widget_width = ImGui::CalcTextSize("N: MMM\nR: MMM").x;
						for (int n = 0; n < 8; n++)
						{
							const float RAD_MIN = 5.0f;
							const float RAD_MAX = 70.0f;
							const float rad = RAD_MIN + (RAD_MAX - RAD_MIN) * (float)n / (8.0f - 1.0f);

							ImGui::BeginGroup();

							ImGui::Text("R: %.f\nN: %d", rad, draw_list->_CalcCircleAutoSegmentCount(rad));

							const float canvas_width = std::max(min_widget_width, rad * 2.0f);
							const float offset_x = floorf(canvas_width * 0.5f);
							const float offset_y = floorf(RAD_MAX);

							const ImVec2 p1 = ImGui::GetCursorScreenPos();
							draw_list->AddCircle(ImVec2(p1.x + offset_x, p1.y + offset_y), rad, ImGui::GetColorU32(ImGuiCol_Text));
							ImGui::Dummy(ImVec2(canvas_width, RAD_MAX * 2));

							/*
							const ImVec2 p2 = ImGui::GetCursorScreenPos();
							draw_list->AddCircleFilled(ImVec2(p2.x + offset_x, p2.y + offset_y), rad, ImGui::GetColorU32(ImGuiCol_Text));
							ImGui::Dummy(ImVec2(canvas_width, RAD_MAX * 2));
							*/

							ImGui::EndGroup();
							ImGui::SameLine();
						}
						ImGui::EndTooltip();
					}
					ImGui::SameLine();
					HelpMarker("When drawing circle primitives with \"num_segments == 0\" tesselation will be calculated automatically.");

					if (ImGui::DragFloat("Global Alpha", &style.Alpha, 0.005f, 0.20f, 1.0f, "%.2f")) changed = true; // Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets). But application code could have a toggle to switch between zero and non-zero.
					if (ImGui::DragFloat("Disabled Alpha", &style.DisabledAlpha, 0.005f, 0.0f, 1.0f, "%.2f")) changed = true; ImGui::SameLine(); HelpMarker("Additional alpha multiplier for disabled items (multiply over current value of Alpha).");
					ImGui::PopItemWidth();

					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}

			ImGui::PopItemWidth();

			GUIResourceManager::GetGUIResourceManager()->SetImGuiStyle(current_style);
			if (changed)
				ImGui::GetIO().WantSaveIniSettings = true;
		}

		void ToolGUI::ShowHelpWindow( const float elapsed_time )
		{
			if( ImGui::Begin( "Help", &show_help, ImGuiWindowFlags_NoFocusOnAppearing ) )
				OnRenderHelp( elapsed_time );
			ImGui::End();
		}

		void ToolGUI::ReloadStyle()
		{
			switch( style )
			{
			default:
			case 0: ImGuiEx::StyleColorsGemini( &current_style );
				break;
			case 1: ImGui::StyleColorsClassic( &current_style );
				break;
			case 2: ImGui::StyleColorsDark( &current_style );
				break;
			case 3: ImGui::StyleColorsLight( &current_style );
				break;
			}
			GUIResourceManager::GetGUIResourceManager()->SetImGuiStyle( current_style );
		}

		void ToolGUI::GetWindowStyle()
		{
			if( auto* window = WindowDX::GetWindow() )
			{
			#if defined(WIN32) && !defined(CONSOLE)
				::GetWindowRect( window->GetWnd(), reinterpret_cast<::RECT*>( &window_rect ) );
			#endif
				window_style = window->GetWindowStyle();
			}
		}

		void ToolGUI::ItemToolTip( const char* desc )
		{
			if( ImGui::IsItemHovered() )
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );
				ImGui::TextUnformatted( desc );
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
		}

		void ToolGUI::OnResetLayout( const DockBuilder& dock_builder )
		{
			ImGui::DockBuilderDockWindow( "Tools", dock_builder.GetTray( Trays::LeftTray ) );
			ImGui::DockBuilderDockWindow( "Environment Editor", dock_builder.GetTray( Trays::TopTray ) );
			ImGui::DockBuilderDockWindow( "Python", dock_builder.GetTray( Trays::BottomTray2 ) );
			ImGui::DockBuilderDockWindow( "ImGui Demo", dock_builder.GetTray( Trays::BottomTray1 ) );
		}

		bool ToolGUI::RenderCameraSection( Renderer::Scene::Camera* current, Renderer::Scene::ModelViewerCamera* model, Renderer::Scene::InGameCamera* ingame, int& mode_out, void(*PutInClipboard)(const std::wstring& text), bool(*GetFromClipboard)(std::wstring& output) )
		{
#ifdef ENABLE_DEBUG_CAMERA
			if( ImGui::CollapsingHeader( "Camera", nullptr ) )
			{
				return RenderCameraSettings( current, model, ingame, mode_out, PutInClipboard, GetFromClipboard );
			}
#endif
			return false;
		}

		bool ToolGUI::RenderCameraSettings( Renderer::Scene::Camera* current, Renderer::Scene::ModelViewerCamera* model, Renderer::Scene::InGameCamera* ingame, int& mode_out, void(*PutInClipboard)(const std::wstring& text), bool(*GetFromClipboard)(std::wstring& output) )
		{
#ifdef ENABLE_DEBUG_CAMERA
			{
				ImGui::Text( "Camera Mode:" );
				ImGui::SameLine();
				ImGui::RadioButton( "Arcball", &mode_out, Renderer::Scene::ModelViewerCamera::ArcballCamera );
				ImGui::SameLine();
				ImGui::RadioButton( "FPS", &mode_out, Renderer::Scene::ModelViewerCamera::FirstPersonCamera );
				ImGui::SameLine();
				ImGui::RadioButton( "Maya", &mode_out, Renderer::Scene::ModelViewerCamera::MayaOrbitCamera );

				if( ingame )
				{
					ImGui::SameLine();
					ImGui::RadioButton( "InGame", &mode_out, Renderer::Scene::ModelViewerCamera::MaxCameras );
				}

				BaseCamera* camera = nullptr;
				simd::vector3 eye = 0.f, look_at = 0.f, up = 0.f;
				float fov = 0.f, radius = 0.f;
				bool show_radius = false;

				if( mode_out == Renderer::Scene::ModelViewerCamera::MaxCameras )
				{
					fov = ingame->GetFOV();
					fov *= ToDegreesMultiplier;

					if( ingame->IsOrthographicCamera() )
					{
						float orthographic_scale = 1.0f / ingame->GetOrthographicScale();
						if( ImGui::DragFloat( "Scale", &orthographic_scale, 0.01f, 0.01f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp ) )
							ingame->SetOrthographicScale( 1.0f / orthographic_scale );
					}
					else
					{
						if( ImGui::DragFloat( "FOV", &fov, ToDegreesMultiplier * 0.0025f, ToDegreesMultiplier * 0.1f, ToDegreesMultiplier * 2.0f ) )
							ingame->SetFOV( fov * ToRadiansMultiplier );
					}

					if( ImGuiEx::Checkbox( "Enable Orthographic", ingame->IsOrthographicCamera() ) )
					{
						ingame->SetOrthographicCamera( !ingame->IsOrthographicCamera() );
					}
				}
				else if( mode_out == Renderer::Scene::ModelViewerCamera::ArcballCamera )
				{
					camera = &model->GetArcballCamera();
					radius = static_cast<RotationCamera*>(camera)->GetRadius();
					show_radius = true;
					
				}
				else if( mode_out == Renderer::Scene::ModelViewerCamera::FirstPersonCamera )
				{
					camera = &model->GetFirstPersonCamera();
				}
				else if( mode_out == Renderer::Scene::ModelViewerCamera::MayaOrbitCamera )
				{
					camera = &model->GetMayaCamera();
					radius = static_cast<RotationCamera*>(camera)->GetRadius();
					show_radius = true;
				}

				if (camera)
				{
					fov = model->GetFOV();
					fov *= ToDegreesMultiplier;
					if( ImGui::DragFloat( "FOV", &fov, ToDegreesMultiplier * 0.0025f, ToDegreesMultiplier * 0.1f, ToDegreesMultiplier * 2.0f ) )
						model->SetFOV( fov * ToRadiansMultiplier );

					bool setparams = false;
					eye = *camera->GetEyePt();
					look_at = *camera->GetLookAtPt();
					up = *camera->GetUpPt();

					setparams |= ImGui::DragFloat3("Eye", eye.as_array());
					setparams |= ImGui::DragFloat3("Look At", look_at.as_array());
					setparams |= ImGui::DragFloat3("Up", up.as_array());
					if (show_radius)
					{
						auto* rotation_cam = static_cast<RotationCamera*>(camera);
						if (ImGui::SliderFloat("Radius", &radius, rotation_cam->GetMinRadius(), rotation_cam->GetMaxRadius()))
							rotation_cam->SetRadiusOnly(radius);
					}

					ImGui::Checkbox("Enable Gizmo", &BaseCamera::IsGizmoEnabled);
					ImGui::SameLine();

					if(ImGuiEx::Checkbox("Enable Orthographic", camera->IsOrthographicCamera() ) )
					{
						camera->SetOrthographicCamera( !camera->IsOrthographicCamera() );
					}

					if( camera->IsOrthographicCamera() )
					{
						ImGui::SameLine();

						ImGui::PushItemWidth(50.f);
						if (ImGui::BeginCombo("##OrthoViewMode", BaseCamera::ortho_viewmodes[BaseCamera::ortho_viewmode].c_str()))
						{
							for (unsigned index = 0; index < BaseCamera::NumOrthoViewMode; ++index)
							{
								bool is_selected = (BaseCamera::ortho_viewmode == index);
								if (ImGui::Selectable(BaseCamera::ortho_viewmodes[index].c_str(), is_selected))
								{
									BaseCamera::ortho_viewmode = BaseCamera::OrthoViewMode(index);
									if (BaseCamera::ortho_viewmode == BaseCamera::Side)
									{
										eye.x = eye.z = 0.f;
										eye.y = -500.f;
										look_at = simd::vector3(0.f, 1.f, 0.f);
										up = simd::vector3(0.f, 0.f, -1.f);					
									}
									else
									{
										eye.x = eye.y = 0.f;
										eye.z = -500.f;
										look_at = simd::vector3(0.f, 0.f, 1.f);
										up = simd::vector3(0.f, -1.f, 0.f);
									}
									setparams = true;
								}
								if (is_selected)
									ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
						ImGui::PopItemWidth();

						if( mode_out != Renderer::Scene::ModelViewerCamera::MayaOrbitCamera )
						{
							float orthographic_scale = 1.0f / camera->GetOrthographicScale();
							if( ImGui::DragFloat( "Scale", &orthographic_scale, 0.01f, 0.01f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp ) )
								camera->SetOrthographicScale( 1.0f / orthographic_scale );
						}
					}

					if (PutInClipboard && ImGui::Button("Copy"))
					{
						std::wostringstream stream;
						stream << mode_out << L"\n";
						stream << eye.x << L" " << eye.y << L" " << eye.z << L"\n";
						stream << look_at.x << L" " << look_at.y << L" " << look_at.z << L"\n";
						stream << up.x << L" " << up.y << L" " << up.z << L"\n";
						stream << fov << L" " << radius << L"\n";
						PutInClipboard(stream.str());
					}

					if( GetFromClipboard )
					{
						ImGui::SameLine();
						if( ImGui::Button( "Paste" ) )
						{
							std::wstring data;
							if( GetFromClipboard( data ) )
							{
								std::wistringstream stream( data );
								int temp_mode = 0;
								stream >> temp_mode;
								stream >> eye.x >> eye.y >> eye.z;
								stream >> look_at.x >> look_at.y >> look_at.z;
								stream >> up.x >> up.y >> up.z;
								stream >> fov >> radius;
								if( !stream.fail() )
								{
									mode_out = temp_mode;
									if( mode_out == Renderer::Scene::ModelViewerCamera::MaxCameras )
										ingame->SetFOV( fov );
									else if( mode_out == Renderer::Scene::ModelViewerCamera::ArcballCamera )
									{
										camera = &model->GetArcballCamera();
										static_cast<RotationCamera*>( camera )->SetRadiusOnly( radius );
									}
									else if( mode_out == Renderer::Scene::ModelViewerCamera::FirstPersonCamera )
										camera = &model->GetFirstPersonCamera();
									else if( mode_out == Renderer::Scene::ModelViewerCamera::MayaOrbitCamera )
									{
										camera = &model->GetMayaCamera();
										static_cast<RotationCamera*>( camera )->SetRadiusOnly( radius );
									}

									if( camera )
										camera->SetFOV( fov );

									setparams = true;
								}
							}
						}
					}

					if (setparams)
					{
						camera->Reset();
						camera->SetViewParams(&eye, &look_at, &up);
					}
				} //if( camera)

				return true;
			}
#endif
			return false;
		}

		bool ToolGUI::RenderSubsceneSettings( Renderer::Scene::SubsceneManager& subscene_manager, Terrain::TileMap& tile_map )
		{
#ifdef ENABLE_DEBUG_CAMERA
			auto& subscenes = subscene_manager.GetSubscenes();
			bool modified = false;
			ImGui::AlignTextToFramePadding();
			ImGui::Text( "Subscenes: %d", subscenes.size() );
			
			if( subscenes.empty() )
				ImGuiEx::PushDisable();
			
			ImGui::SameLine();
			if( ImGui::Button( "Reset" ) )
			{
				subscenes = subscene_manager.backup;
				modified = true;
			}

			ImGui::SameLine();
			if( ImGuiEx::Checkbox( "Render", subscene_manager.render_subscene_bbs ) )
				subscene_manager.render_subscene_bbs = !subscene_manager.render_subscene_bbs;
			
			ImGui::SameLine();
			if( ImGuiEx::Checkbox( "Disable", subscene_manager.disable_subscenes ) )
			{
				subscene_manager.disable_subscenes = !subscene_manager.disable_subscenes;
				modified = true;
			}

			if( !subscene_manager.disable_subscenes )
			{
				if( subscene_manager.GetLastPlayerSubscene() < subscenes.size() )
				{
					ImGui::SameLine();
					ImGui::Text( "Player Subscene: %d (%s)", subscene_manager.GetLastPlayerSubscene(), subscenes[ subscene_manager.GetLastPlayerSubscene() ].name.c_str() );
				}
			}

			for( unsigned i = 0; i < subscenes.size(); ++i )
			{
				const auto id = ImGuiEx::PushID( i );
				Renderer::Scene::SubsceneDetails& ss = subscenes[ i ];
				if( ImGui::CollapsingHeader( fmt::format( "Subscene {}: {}", i, ss.name ).c_str(), nullptr ) )
				{
					ImGui::PushItemWidth( ImGui::GetFontSize() * -2.3f );
					AABB aabb = ss.scene_bounds.ToAABB();
					modified |= ImGui::SliderFloat3( "Centre##Subscene", aabb.center.as_array(), 0.0f, 10000.0f );
					modified |= ImGui::SliderFloat3( "Extent##Subscene", aabb.extents.as_array(), 0.0f, 10000.0f );
					ss.scene_bounds = aabb;

					modified |= ImGui::SliderFloat( "Scale##Subscene", &ss.scale, 0.1f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic );
					modified |= ImGui::SliderFloat3( "Offset##Subscene", ss.offset.as_array(), -5000.0f, 5000.0f );
					tile_map.UpdateSubscene( i, ss.offset, ss.scale );
				}
			}
			if( subscenes.empty() )
				ImGuiEx::PopDisable();
			
			if( modified )
			{
				subscene_manager.Dirty();
				tile_map.RecheckActivationZones();
			}
#endif
			return false;
		}
	}
}

#endif
