#pragma once

#include "Visual/Device/Shader.h"
#include "Visual/Utility/DXHelperFunctions.h"

namespace Device
{
	class VertexDeclaration;
	class Shader;
	class VertexBuffer;
	struct DepthState;
}

namespace Renderer
{
	// Implementation of subpixel morphological antialiasing
	// https://github.com/iryoku/smaa
	class SMAA
	{
	public:
		SMAA();

		enum PassType : int
		{
			EdgeDetection,
			BlendingWeightCalculation,
			NeighborhoodBlending,
			Count
		};

		static const std::array<std::wstring, SMAA::PassType::Count>& PassDebugNames();

		bool Enabled() const { return enabled; }

		void Render(Device::IDevice* device,
			Device::CommandBuffer& command_buffer,
			Device::Texture *src_texture,
			simd::vector2 frame_to_dynamic_scale,
			Device::RenderTarget* depth_stencil_target,
			Device::RenderTarget* blend_weight_render_target,
			Device::RenderTarget* final_render_target
		);

		void Reload(Device::IDevice* device);

		void OnCreateDevice( Device::IDevice *device);
		void OnResetDevice(Device::IDevice* device, bool enabled, bool high);
		void OnLostDevice();
		void OnDestroyDevice();
	
	private:
		void RenderPass(
			Device::IDevice* device,
			Device::CommandBuffer& command_buffer,
			Device::Texture *src_texture,
			simd::vector2 frame_to_dynamic_scale,
			Device::RenderTarget* depth_stencil_target,
			Device::RenderTarget* blend_weight_render_target,
			Device::RenderTarget* final_render_target, 
			PassType pass_type
		);

		struct SMAAShader
		{
			Device::Handle<Device::Shader> vertex_shader;
			Device::Handle<Device::Shader> pixel_shader;

			Device::Handle<Device::DynamicUniformBuffer> vertex_uniform_buffer;
			Device::Handle<Device::DynamicUniformBuffer> pixel_uniform_buffer;
		};

		Device::RenderTarget* GetRenderTarget(PassType pass_type, Device::RenderTarget* blend_weight, Device::RenderTarget* final_render_target);
		Device::ClearTarget GetClearTarget(PassType pass_type);
		Device::StencilState GetStencilState(PassType pass_type);

	private:
		bool enabled = false;
		bool high = false;

		int frame_width;
		int frame_height;

		Device::VertexDeclaration vertex_declaration;

		Device::Handle<Device::VertexBuffer> vb_post_process;

		std::unique_ptr<Device::RenderTarget> edges_render_target;

		std::array<SMAAShader, PassType::Count> shaders;

	#pragma pack(push)
	#pragma pack(1)
		struct PassKey
		{
			Device::PointerID<Device::RenderTarget> render_target;
			Device::ClearTarget clear_target = Device::ClearTarget::NONE;

			PassKey() {}
			PassKey(Device::RenderTarget* render_target, Device::ClearTarget clear_target);
		};
	#pragma pack(pop)
		Device::Cache<PassKey, Device::Pass> passes;

	#pragma pack(push)	
	#pragma pack(1)
		struct PipelineKey
		{
			Device::PointerID<Device::Pass> pass;
			PassType pass_type;

			PipelineKey() {}
			PipelineKey(Device::Pass* pass, PassType pass_type);
		};
	#pragma pack(pop)
		Device::Cache<PipelineKey, Device::Pipeline> pipelines;

	#pragma pack(push)
	#pragma pack(1)
		struct BindingSetKey
		{
			Device::PointerID<Device::Shader> shader;
			uint32_t inputs_hash = 0;

			BindingSetKey() {}
			BindingSetKey(Device::Shader* shader, uint32_t inputs_hash);
		};
	#pragma pack(pop)
		Device::Cache<BindingSetKey, Device::DynamicBindingSet> binding_sets;

	#pragma pack(push)
	#pragma pack(1)
		struct DescriptorSetKey
		{
			Device::PointerID<Device::Pipeline> pipeline;
			Device::PointerID<Device::DynamicBindingSet> pixel_binding_set;
			uint32_t samplers_hash = 0;

			DescriptorSetKey() {}
			DescriptorSetKey(Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set, uint32_t samplers_hash);
		};
	#pragma pack(pop)
		Device::Cache<DescriptorSetKey, Device::DescriptorSet> descriptor_sets;

		::Texture::Handle search_texture;
		::Texture::Handle area_texture;
	};

}