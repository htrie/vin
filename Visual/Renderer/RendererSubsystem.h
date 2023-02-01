#pragma once

#include "Common/Utility/System.h"

#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/ShaderSystem.h" // [TODO] Remove.

namespace Device
{
	class Pass;
}

namespace Utility
{
	enum class ViewMode : unsigned;

	class GraphReloader;
}

namespace Environment
{
	struct Data;
}

namespace Renderer
{
	class Settings;
	class TargetResampler;
	class PostProcess;
	class Pipeline;
	class DrawCallType;
	struct Pass; // [TODO] Remove.

	namespace Scene
	{
		class Camera;
		struct OceanData;
		struct RiverData;
		struct FogBlockingData;	
	}

	class Base
	{
		Shader::Base shader_base;
		Device::VertexDeclaration vertex_declaration;
		Device::PrimitiveType primitive_type = Device::PrimitiveType::TRIANGLELIST;
		Device::CullMode cull_mode = Device::CullMode::CCW;

	public:
		Base& SetShaderBase(const Shader::Base& shader_base);
		Base& SetVertexElements(const Memory::Span<const Device::VertexElement>& vertex_elements);
		Base& SetPrimitiveType(Device::PrimitiveType primitive_type);
		Base& SetCullMode(Device::CullMode cull_mode);

		Device::PrimitiveType GetPrimitiveType() const { return primitive_type; }
		const Device::VertexDeclaration& GetVertexDeclaration() const { return vertex_declaration; }
		const DrawCalls::EffectGraphGroups& GetEffectGraphs() const { return shader_base.GetEffectGraphs(); }
		DrawCalls::BlendMode GetBlendMode() const { return shader_base.GetBlendMode(); }
		Device::CullMode GetCullMode() const { return cull_mode; }
	};

	class Desc
	{
		Shader::Desc shader_desc;
		Device::Handle<Device::Pass> pass;
		Device::VertexDeclaration vertex_declaration;
		Device::PrimitiveType primitive_type = Device::PrimitiveType::TRIANGLELIST;
		Device::CullMode cull_mode = Device::CullMode::CCW;
		bool inverted_culling = false;
		float depth_bias = 0.0f;
		float slope_scale = 0.0f;
		unsigned hash = 0;
		int64_t start_time = 0;

	public:
		friend class Pipeline;

		Desc() {}
		Desc(const Shader::Desc& shader_desc,
			const Device::Handle<Device::Pass>& pass,
			Device::VertexDeclaration vertex_declaration,
			Device::PrimitiveType primitive_type,
			Device::CullMode cull_mode,
			bool inverted_culling,
			float depth_bias,
			float slope_scale);

		const Shader::Desc& GetShaderDesc() const { return shader_desc; }
		unsigned GetHash() const { return hash; }
	};


	struct Stats
	{
		size_t budget = 0;
		size_t usage = 0;

		size_t device_binding_set_count = 0;
		size_t device_descriptor_set_count = 0;
		size_t device_pipeline_count = 0;

		size_t device_binding_set_frame_count = 0;
		size_t device_descriptor_set_frame_count = 0;
		size_t device_pipeline_frame_count = 0;

		size_t pipeline_count = 0;
		size_t warmed_pipeline_count = 0;
		size_t active_pipeline_count = 0;
		size_t type_count = 0;
		size_t warmed_type_count = 0;
		size_t active_type_count = 0;

		size_t cpass_fetches = 0;

		float pipeline_qos_min = 0.0f;
		float pipeline_qos_avg = 0.0f;
		float pipeline_qos_max = 0.0f;
		std::array<float, 10> pipeline_qos_histogram;

		float type_qos_min = 0.0f;
		float type_qos_avg = 0.0f;
		float type_qos_max = 0.0f;
		std::array<float, 10> type_qos_histogram;

		std::array<std::pair<unsigned, unsigned>, (unsigned)DrawCalls::Type::Count> type_counts;

		bool enable_async = false;
		bool enable_budget = false;
		bool enable_warmup = false;
		bool enable_skip = false;
		bool enable_throttling = false;
		bool enable_instancing = false;
		bool potato_mode = false;
	};

	class Impl;

	class System : public ImplSystem<Impl, Memory::Tag::Render>
	{
		System();

	protected:
		friend struct MetaPass;
		friend class Pipeline;
		friend class DrawCallType;

		Device::Handle<Device::BindingSet> FindBindingSet(Device::IDevice* device, Device::Shader* vertex_shader, Device::Shader* pixel_shader, const Device::BindingSet::Inputs& inputs, uint32_t inputs_hash);
		Device::Handle<Device::BindingSet> FindBindingSet(Device::IDevice* device, Device::Shader* compute_shader, const Device::BindingSet::Inputs& inputs, uint32_t inputs_hash);
		Device::Handle<Device::DescriptorSet> FindDescriptorSet(Device::IDevice* device, Device::Pipeline* pipeline, Device::BindingSet* pass_binding_set, Device::BindingSet* pipeline_binding_set, Device::BindingSet* object_binding_set);
		Device::Handle<Device::Pipeline> FindPipeline(Device::IDevice* device, Device::Pass* pass, Device::PrimitiveType primitive_type,
			const Device::VertexDeclaration* vertex_declaration, Device::Shader* vertex_shader, Device::Shader* pixel_shader,
			const Device::BlendState& blend_state, const Device::RasterizerState& rasterizer_state, const Device::DepthStencilState& depth_stencil_state, 
			uint32_t blend_state_hash, uint32_t rasterizer_state_hash, uint32_t depth_stencil_state_hash, bool wait);
		Device::Handle<Device::Pipeline> FindPipeline(Device::IDevice* device, Device::Shader* compute_shader, bool wait);
		Device::Handle<Device::CommandBuffer> FindCommandBuffer(unsigned index);

	public:
		static System& Get();

	public:
		void Init(const Settings& settings, bool disable_render, bool enable_debug_layer, bool tight_buffers);

		void SetAsync(bool enable);
		void SetBudget(bool enable);
		void SetWait(bool enable);
		void SetWarmup(bool enable);
		void SetSkip(bool enable);
		void SetThrottling(bool enable);
		void SetInstancing(bool enable);
		void SetPotato(bool enable);
		void DisableAsync(unsigned frame_count);

		void Swap() final;
		void Clear() final;

		Stats GetStats() const;

		void OnCreateDevice(Device::IDevice* const device);
		void OnResetDevice(Device::IDevice* const device);
		void OnLostDevice();
		void OnDestroyDevice();

		void StartDevice(); // [TODO] Remove.
		void StopDevice(); // [TODO] Remove.

		void SetEnvironmentSettings(const Environment::Data& settings, const float time_scale, const simd::vector2* tile_scroll_override );

		Memory::Object<DrawCalls::DrawCall> CreateDrawCall(const Entity::Desc& desc);

		void Warmup(Base& base);

		std::shared_ptr<Pipeline> Fetch(const Desc& desc, bool warmed_up, bool async);

		bool Update(const float frame_length, size_t budget);
		void FrameRenderBegin(simd::color clear_color, std::function<void()>* callback_render_to_texture = nullptr, std::function<void(Device::Pass*)>* callback_render = nullptr);
		void FrameRenderEnd();

		void Render(Device::IDevice* device, Device::CommandBuffer& command_buffer, Pass& pass);

		void Invalidate(const std::wstring_view filename);

		void ReloadShaders();

		void SetRendererSettings(const Settings& settings);
		const Settings& GetRendererSettings() const;

		void ToggleGlobalIllumination();

		void SetPostProcessCamera( Scene::Camera* camera );
		void SetSceneData(simd::vector2 scene_size, simd::vector2_int scene_tiles_count, const Scene::OceanData& ocean_data, const Scene::RiverData& river_data, const Scene::FogBlockingData& fog_blocking_data);
		simd::vector4 GetSceneData(simd::vector2 planar_pos);
		void RenderDebugInfo(const simd::matrix & view_projection);

		Device::RenderTarget* GetBackBuffer() const; // [TODO] Make private.
		TargetResampler& GetResampler(); // [TODO] Make private.
		PostProcess& GetPostProcess(); // [TODO] Make private.
		Device::IDevice* GetDevice();

		Utility::GraphReloader* GetGraphReloader() const { return graph_reloader; } // [TODO] Remove graph reloader and do it in system.
		void SetGraphReloader(Utility::GraphReloader* _graph_reloader) { graph_reloader = _graph_reloader; }

	private:
		Utility::GraphReloader* graph_reloader = nullptr; // TODO: remove.
	};

}

