#pragma once

#include "Visual/Engine/PluginManager.h"
#include "Visual/Engine/Plugins/ProfilePlugin.h"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED && defined(PROFILING)
	class ProfileResourcePlugin final : public ProfilePlugin::Page
	{
	public:
		ProfileResourcePlugin() : Page("Resources") {}
		void OnRender(float elapsed_time) override;
	};
#endif
}