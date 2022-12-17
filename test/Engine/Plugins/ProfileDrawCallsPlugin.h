#pragma once

#include "Visual/Engine/PluginManager.h"
#include "Visual/Engine/Plugins/ProfilePlugin.h"

#include "Visual/Device/Device.h"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED && defined(PROFILING)
	class ProfileDrawCallsPlugin final : public ProfilePlugin::Page
	{
	private:
		void RenderBlendModes(const Device::IDevice::Counters& counters);
		void RenderDrawCallInfo(const Device::IDevice::Counters& counters);
		void RenderDeviceInfo();
		void RenderGameInfo();

	public:
		ProfileDrawCallsPlugin() : Page("Draw Calls") {}
		void OnRender(float elapsed_time) override;
	};
#endif
}