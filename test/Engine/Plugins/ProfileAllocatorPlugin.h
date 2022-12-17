#pragma once

#include "Visual/Engine/PluginManager.h"
#include "Visual/Engine/Plugins/ProfilePlugin.h"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED && defined(PROFILING)
	class ProfileAllocatorPlugin final : public ProfilePlugin::Page
	{
	private:
		void RenderCategories(const Memory::Stats& stats);
		void RenderGroups(const Memory::Stats& stats);
		void RenderTags(const Memory::Stats& stats);
		void RenderBuckets(const Memory::Stats& stats);
		void RenderStats(const Memory::Stats& stats);
		void RenderTraces(const Memory::Stats& stats);

	public:
		ProfileAllocatorPlugin() : Page("Memory") {}
		void OnRender(float elapsed_time) override;
	};
#endif
}