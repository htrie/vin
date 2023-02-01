#pragma once

#include "Visual/Engine/PluginManager.h"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED
	class HotkeyPlugin final : public Plugin
	{
	public:
		HotkeyPlugin();
		std::string GetName() const override { return "Hotkeys"; }
		void OnRender(float elapsed_time) override;
		void OnReloadSettings( const rapidjson::Value& settings ) override;
		void OnLoadSettings(const rapidjson::Value& settings) override;
		void OnSaveSettings(rapidjson::Value& settings, rapidjson::Document::AllocatorType& allocator) override;

		void RenderHotkeys();
	};
#endif
}