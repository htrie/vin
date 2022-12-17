#pragma once

#include "Visual/Engine/PluginManager.h"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED
	class TelemetryPlugin final : public Plugin
	{
	private:
		class Impl;

		Impl* impl;

	public:
		TelemetryPlugin();
		~TelemetryPlugin();
		std::string GetName() const override { return "Telemetry"; }
		void OnUpdate(float elapsed_time) override;
		void OnRender(float elapsed_time) override;
		void OnEnabled() override;
		void OnDisabled() override;
	};
#endif
}