#pragma once

#include "Visual/Engine/PluginManager.h"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED
	class LoggingPlugin final : public Plugin
	{
	private:
		bool click_through = false;
		bool in_main_viewport = false;

	public:
		LoggingPlugin();
		std::string GetName() const override { return "Log"; }
		bool IsBlinking() const override;
		void OnRender(float elapsed_time) override;
		void OnRenderSettings(float elapsed_time) override;
		void OnEnabled() override;
		void OnDisabled() override;
		void OnLoadSettings(const rapidjson::Value& settings) override;
		void OnSaveSettings(rapidjson::Value& settings, rapidjson::Document::AllocatorType& allocator) override;

		void RenderNotification();
	};
#endif
}