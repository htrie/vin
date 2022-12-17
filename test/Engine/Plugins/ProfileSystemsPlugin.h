#pragma once

#include "Visual/Engine/PluginManager.h"
#include "Visual/Engine/Plugins/ProfilePlugin.h"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED && defined(PROFILING)
	class ProfileSystemPlugin final : public ProfilePlugin::Page
	{
	private:
		static constexpr size_t MaxSceneStats = 64;

		float current_time = 0.0f;

		struct SceneEndStats
		{
			uint64_t loaded_bundles = 0;
			uint64_t loaded_bytes = 0;
		};

		Memory::RingBuffer<SceneEndStats, MaxSceneStats> scene_stats;

	public:
		ProfileSystemPlugin() : Page("Systems") {}
		void OnRender(float elapsed_time) override;
		void OnGarbageCollect() override;
	};
#endif
}