#pragma once

#include "Visual/Engine/PluginManager.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Entity/Component.h"

namespace Utility
{
	enum class ViewMode : unsigned
	{
		DefaultViewmode = 0,

		LIGHT_BEGIN,
		LightComplexity = LIGHT_BEGIN,
		TiledLights,
		LightModel,
		DetailLighting,
		LightingOnly,
		LIGHT_END = LightingOnly,

		MipLevel,
		SceneIndex,
		Overdraw,
		DoubleSided,
		ShaderComplexity,
		PerformanceOverlay,
		Polygons,
		TrailWireframe,

		SHADING_BEGIN,
		Albedo = SHADING_BEGIN,
		Gloss,
		Roughness,
		Normal,
		Normal2,
		SurfaceNormal,
		Specular,
		Indirect,
		SubSurface,
		Glow,
		SHADING_END = Glow,

		NumViewModes
	};

	std::string_view GetViewModeName( const ViewMode mode );

#ifdef ENABLE_DEBUG_VIEWMODES
	std::wstring_view GetViewModeGraph( const ViewMode mode, Renderer::DrawCalls::Type type);
	ViewMode GetCurrentViewMode();
#endif
}

namespace Engine::Plugins
{
#ifdef PROFILING
	class PerformancePlugin final : public Plugin
	{
	public:
		typedef Memory::BitSet<(unsigned)Renderer::DrawCalls::Type::Count> CullMask;

	private:
		CullMask culling_mask;
		size_t max_overlays = 32;
		bool show_all_overlays = false;

	public:
		PerformancePlugin();
		std::string GetName() const override { return "Performance"; }
		void OnRender(float elapsed_time) override;
		void OnRenderSettings(float elapsed_time) override;
		void OnLoadSettings(const rapidjson::Value& settings) override;
		void OnSaveSettings(rapidjson::Value& settings, rapidjson::Document::AllocatorType& allocator) override;

		bool IsCulled(Renderer::DrawCalls::Type type) const;
		
		void RenderUI();

		bool SetViewMode( const Utility::ViewMode mode );
	};
#endif
}