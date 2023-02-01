#pragma once

#include "DebugGUIDefines.h"
#if DEBUG_GUI_ENABLED

#include "Visual/GUI2/GUIResourceManager.h"
#include "Visual/Environment/EnvironmentEditor.h"
#include "Visual/Utility/IMGUI/UIDebugGUI.h"
#include "Visual/Python/Interpreter.h"
#include "Visual/GUI2/imgui/imgui.h"

namespace Sessions
{
	class ClientOnlyInstanceSession;
}

namespace Terrain
{
	class TileMap;
}

namespace Game
{
	class EnvironmentMixer;
}

namespace Renderer
{
	namespace Scene
	{
		class Camera;
		class ModelViewerCamera;
		class InGameCamera;
		class SubsceneManager;
	}
}

namespace Device
{
	namespace GUI
	{
		extern bool EnableExperimentalUI;

		class ToolGUI : public Device::GUI::GUIInstance
		{
		public:
			ToolGUI( const char* name, const bool force_use_python = false, const bool default_open_common = true );
			~ToolGUI();

			Environment::EnvironmentEditorV2& GetEnvironmentEditor() { return environment_editor; }
			Debug::UIDebugGUI& GetUIDebugGUI() { return ui_debug_gui; }

			bool RenderCameraSection( Renderer::Scene::Camera* current, Renderer::Scene::ModelViewerCamera* model, Renderer::Scene::InGameCamera* ingame, int& mode_out, void(*PutInClipboard)(const std::wstring& text) = nullptr, bool(*GetFromClipboard)(std::wstring& output) = nullptr );
			bool RenderCameraSettings( Renderer::Scene::Camera* current, Renderer::Scene::ModelViewerCamera* model, Renderer::Scene::InGameCamera* ingame, int& mode_out, void(*PutInClipboard)(const std::wstring& text) = nullptr, bool(*GetFromClipboard)(std::wstring& output) = nullptr );
			bool RenderSubsceneSettings( Renderer::Scene::SubsceneManager& subscene_manager, Terrain::TileMap& tile_map );

			bool IsHelpWindowOpen() const { return show_help; }
			void ShowHelp() { show_help = true; }
			void ToggleHelp() { show_help = !show_help; }

		protected:
			// This should be overridden by any program that wants to change environment settings
			virtual Environment::EnvironmentSettingsV2* GetEnvironmentSettings() { return nullptr; };
			virtual const Game::EnvironmentMixer* GetEnvironmentMixer() const { return nullptr; };
			virtual const Game::GameObject* GetPlayerObject() const { return nullptr; };

			// Helper function for adding quick tooltips
			void ItemToolTip( const char* desc );

			virtual void OnResetLayout( const DockBuilder& dock_builder ) override;
			virtual void OnReloadSettings( const rapidjson::Value& settings ) override;
			virtual void OnLoadSettings(Utility::ToolSettings* settings) override;
			virtual void OnSaveSettings(Utility::ToolSettings* settings) override;

			const char* name;

			virtual void OnRenderMenu( const float elapsed_time ) override;
			virtual void OnRender( const float elapsed_time ) override;
			virtual bool MsgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) override;

			void ShowStyleEditor(ImGuiStyle* ref);
			void ShowAssetStreamingInfo();

			//By default, clicking the "Show Help" checkbox just disables it again.
			//Override this function to enable a proper Help window for your tool.
			virtual void OnRenderHelp( const float elapsed_time ) { show_help = false; }

		private:
			void ReloadStyle();
			void GetWindowStyle();

			void ShowHelpWindow( const float elapsed_time );

			Environment::EnvironmentEditorV2 environment_editor;
			Debug::UIDebugGUI ui_debug_gui;

			int opacity_display = 100;
			int style = 0;
			int selectedFont = 0;
		#ifdef ENABLE_DEBUG_VIEWMODES
			bool dynamic_culling_force_active = false;
			bool dynamic_culling_overwrite_aggression = false;
			float dynamic_culling_aggression = 0.0f;
		#endif

#if PYTHON_ENABLED == 1
			Python::Interpreter python_interpreter;
#endif

			ImGuiStyle current_style;
			ImGuiStyle default_style;
			ImGuiTextFilter asset_info_filter;
			bool show_asset_info = false;
			bool show_about_window = false;
			bool show_style_window = false;
			bool show_metrics_window = false;
			bool show_gui_demo = false;
			bool show_help = true;
			bool open_common = true;
		};
	}
}

#endif