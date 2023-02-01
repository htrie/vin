#pragma once

#include "Visual/Engine/PluginManager.h"
#include "Visual/Engine/Plugins/ProfilePlugin.h"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED && defined(PROFILING)
	class ProfileFileAccessPlugin final : public ProfilePlugin::Page
	{
	private:
		void RenderDriveAccess();
		void RenderShaderAccess();

	public:
		ProfileFileAccessPlugin() : Page("Access") {}
		void OnRender(float elapsed_time) override;
	};
#endif
}