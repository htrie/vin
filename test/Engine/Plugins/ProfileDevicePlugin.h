#pragma once

#include "Visual/Engine/PluginManager.h"
#include "Visual/Engine/Plugins/ProfilePlugin.h"

#include "Common/Simd/Simd.h"
#include "Visual/Animation/AnimationSystem.h"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED && defined(PROFILING)
	class ProfileDevicePlugin final : public ProfilePlugin::Page
	{
	private:
		uint64_t peak_animation_overflow = 0;

		void RenderAnimationInfo(const Animation::Stats& stats);

	public:
		ProfileDevicePlugin() : Page("Device") {}
		void OnRender(float elapsed_time) override;
	};
#endif
}