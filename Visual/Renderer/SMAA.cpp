#include "Visual/Device/Constants.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/State.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/Shader.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/Compiler.h"
#include "Visual/Device/Texture.h"

#include "SMAA.h"
#include "CachedHLSLShader.h"
#include "Utility.h"

namespace Renderer
{
	using namespace Device;

	namespace
	{
		const char* SMAA_QUALITY_MEDIUM = "SMAA_PRESET_LOW";
		const char* SMAA_QUALITY_HIGH = "SMAA_PRESET_ULTRA";

		const Device::VertexElements vertex_elements =
		{
			{ 0, 0, Device::DeclType::FLOAT4, Device::DeclUsage::POSITION, 0 },
			{ 0, sizeof(float) * 4, Device::DeclType::FLOAT2, Device::DeclUsage::TEXCOORD, 0 },
		};

#pragma pack( push, 1 )
		struct Vertex
		{
			simd::vector4 p;
			simd::vector2 t;
		};
#pragma pack( pop )

		const Vertex vertex_data[4] =
		{
			{ simd::vector4(-1.f, -1.f, 0.0f, 1.0f), simd::vector2(0.0f, 1.0f) },
			{ simd::vector4(-1.f, 1.f, 0.0f, 1.0f), simd::vector2(0.0f, 0.0f) },
			{ simd::vector4(1.f, -1.f, 0.0f, 1.0f), simd::vector2(1.0f, 1.0f) },
			{ simd::vector4(1.f, 1.f, 0.0f, 1.0f), simd::vector2(1.0f, 0.0f) }
		};

		std::array<const char*, SMAA::PassType::Count> VS_ENTRY     = { "EdgeDetectionVShad",      "BlendingWeightCalculationVShad",  "NeighborhoodBlendingVShad" };
		std::array<const char*, SMAA::PassType::Count> PS_ENTRY     = { "ColorEdgeDetectionPShad", "BlendingWeightCalculationPShad",  "NeighborhoodBlendingPShad" };
		std::array<const char*, SMAA::PassType::Count> NAME         = { "SMAA EdgeDetection",      "SMAA BlendingWeightCalculation",  "SMAA NeighborhoodBlending" };
		std::array<std::wstring, SMAA::PassType::Count> LNAME = { L"SMAA EdgeDetection",    L"SMAA BlendingWeightCalculation", L"SMAA NeighborhoodBlending" };
	}

	SMAA::PassKey::PassKey(Device::RenderTarget* render_target, Device::ClearTarget clear_target)
		: render_target(render_target), clear_target(clear_target)
	{}

	SMAA::PipelineKey::PipelineKey(Device::Pass* pass, PassType pass_type)
		: pass(pass), pass_type(pass_type)
	{}

	SMAA::BindingSetKey::BindingSetKey(Device::Shader * shader, uint32_t inputs_hash)
		: shader(shader), inputs_hash(inputs_hash)
	{}
	
	SMAA::DescriptorSetKey::DescriptorSetKey(Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set, uint32_t samplers_hash)
		: pipeline(pipeline), pixel_binding_set(pixel_binding_set), samplers_hash(samplers_hash)
	{}

	SMAA::SMAA()
		: vertex_declaration(vertex_elements)
	{}

	const std::array<std::wstring, SMAA::PassType::Count>& SMAA::PassDebugNames()
	{
		return LNAME;
	}

	void SMAA::Reload(Device::IDevice* device)
	{
		OnLostDevice();
		OnDestroyDevice();
		OnCreateDevice(device);
		OnResetDevice(device, enabled, high);
	}

	Device::RenderTarget* SMAA::GetRenderTarget(PassType pass_type, Device::RenderTarget* blend_weight, Device::RenderTarget* final_render_target)
	{
		switch (pass_type)
		{
		case PassType::EdgeDetection: return edges_render_target.get();
		case PassType::BlendingWeightCalculation: return blend_weight;
		case PassType::NeighborhoodBlending: return final_render_target;
		}
		return nullptr;
	}

	Device::ClearTarget SMAA::GetClearTarget(PassType pass_type)
	{
		if (pass_type == PassType::EdgeDetection)
			return (Device::ClearTarget)((UINT)Device::ClearTarget::COLOR | (UINT)Device::ClearTarget::STENCIL);
		return Device::ClearTarget::COLOR;
	}

	Device::StencilState SMAA::GetStencilState(PassType pass_type)
	{
		switch (pass_type)
		{
			case PassType::EdgeDetection: return Device::StencilState(true, 1, Device::CompareMode::ALWAYS, Device::StencilOp::REPLACE, Device::StencilOp::KEEP);
			case PassType::BlendingWeightCalculation: return Device::StencilState(true, 1, Device::CompareMode::EQUAL, Device::StencilOp::KEEP, Device::StencilOp::KEEP);
			case PassType::NeighborhoodBlending: return Device::DisableStencilState();
			default: return Device::DisableStencilState();
		}
	}

	void SMAA::Render(Device::IDevice* device,
		Device::CommandBuffer& command_buffer,
		Device::Texture *src_texture,
		simd::vector2 frame_to_dynamic_scale,
		Device::RenderTarget* depth_stencil_target,
		Device::RenderTarget* blend_weight_render_target,
		Device::RenderTarget* final_render_target)
	{
		RenderPass(device, command_buffer, src_texture, frame_to_dynamic_scale, depth_stencil_target, blend_weight_render_target, final_render_target, SMAA::PassType::EdgeDetection);
		RenderPass(device, command_buffer, nullptr, frame_to_dynamic_scale, depth_stencil_target, blend_weight_render_target, final_render_target, SMAA::PassType::BlendingWeightCalculation);
		RenderPass(device, command_buffer, src_texture, frame_to_dynamic_scale, depth_stencil_target, blend_weight_render_target, final_render_target, SMAA::PassType::NeighborhoodBlending);
	}

	void SMAA::RenderPass(
		Device::IDevice* device,
		Device::CommandBuffer& command_buffer,
		Device::Texture *src_texture,
		simd::vector2 frame_to_dynamic_scale,
		Device::RenderTarget* depth_stencil,
		Device::RenderTarget* blend_weight,
		Device::RenderTarget* final_render_target, 
		PassType pass_type)
	{
		auto* rt = GetRenderTarget(pass_type, blend_weight, final_render_target);
		const auto viewport_size = simd::vector2_int( static_cast< int >( rt->GetWidth() * frame_to_dynamic_scale.x ), static_cast< int >( rt->GetHeight() * frame_to_dynamic_scale.y));
		const auto clear_target = GetClearTarget(pass_type);
		auto* pass = passes.FindOrCreate(PassKey(rt, clear_target), [&]()
		{
			return Device::Pass::Create("SMAA", device, { rt }, depth_stencil, clear_target, simd::color(0), 1.0f);
		}).Get();
		command_buffer.BeginPass(pass, viewport_size, viewport_size);

		auto& shader = shaders[pass_type];

		auto* pipeline = pipelines.FindOrCreate(PipelineKey(pass, pass_type), [&]()
		{
			return Device::Pipeline::Create("SMAA", device, pass, Device::PrimitiveType::TRIANGLESTRIP, &vertex_declaration, shader.vertex_shader.Get(), shader.pixel_shader.Get(),
					Device::DisableBlendState(), Device::DefaultRasterizerState(), Device::DepthStencilState(Device::DisableDepthState(), GetStencilState(pass_type)));
		}).Get();
		if (command_buffer.BindPipeline(pipeline))
		{
			shader.pixel_uniform_buffer->SetInt("target_width", frame_width);
			shader.vertex_uniform_buffer->SetInt("target_width", frame_width);
			shader.pixel_uniform_buffer->SetInt("target_height", frame_height);
			shader.vertex_uniform_buffer->SetInt("target_height", frame_height);

			Device::DynamicBindingSet::Inputs inputs;
			switch (pass_type)
			{
			case PassType::EdgeDetection:
			{
				inputs.push_back({"colorTexGamma", src_texture});
				break;
			}
			case PassType::BlendingWeightCalculation:
			{
				inputs.push_back({ "edgesTex", edges_render_target->GetTexture().Get() });
				break;
			}
			case PassType::NeighborhoodBlending:
			{
				inputs.push_back({ "colorTex", src_texture });
				inputs.push_back({ "blendTex", blend_weight->GetTexture().Get() });
				break;
			}
			}
			inputs.push_back({ "areaTex", area_texture.GetTexture().Get() });
			inputs.push_back({ "searchTex", search_texture.GetTexture().Get() });
			auto* pixel_binding_set = binding_sets.FindOrCreate(BindingSetKey(shader.pixel_shader.Get(), inputs.Hash()), [&]()
			{
				return Device::DynamicBindingSet::Create("SMAA", device, shader.pixel_shader.Get(), inputs);
			}).Get();

			auto* descriptor_set = descriptor_sets.FindOrCreate(DescriptorSetKey(pipeline, pixel_binding_set, device->GetSamplersHash()), [&]()
			{
				return Device::DescriptorSet::Create("SMAA", device, pipeline, {}, { pixel_binding_set });
			}).Get();

			command_buffer.BindBuffers(nullptr, vb_post_process.Get(), 0, sizeof(Vertex));

			command_buffer.BindDescriptorSet(descriptor_set, {}, { shader.vertex_uniform_buffer.Get(), shader.pixel_uniform_buffer.Get() });

			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void SMAA::OnCreateDevice(Device::IDevice *device)
	{
		Device::MemoryDesc init_mem = { vertex_data, 0, 0 };
		vb_post_process = Device::VertexBuffer::Create("VB SMAA PostProcess", device, 4 * sizeof(Vertex), Device::UsageHint::IMMUTABLE, Device::Pool::DEFAULT, &init_mem);

		search_texture = ::Texture::Handle(::Texture::RawLinearUITextureDesc(L"Art/2DArt/Lookup/smaa_SearchTex.dds"));
		area_texture = ::Texture::Handle(::Texture::RawLinearUITextureDesc(L"Art/2DArt/Lookup/smaa_AreaTex.dds"));
	}

	void SMAA::OnResetDevice(Device::IDevice *device, bool enabled, bool high)
	{
		this->enabled = enabled;
		this->high = high;

		if (!Enabled())
			return;

		const auto frame_size = device->GetFrameSize();
		frame_width = frame_size.x;
		frame_height = frame_size.y;

		edges_render_target = Device::RenderTarget::Create("SMAA Edges", device, frame_width, frame_height, Device::PixelFormat::A8B8G8R8, Device::Pool::DEFAULT, false, false);

		Device::Macro quality_macro[] =
		{
			{ high ? SMAA_QUALITY_HIGH : SMAA_QUALITY_MEDIUM, "1" },
			{ NULL, NULL }
		};

		for (int i = 0; i < PassType::Count; i++)
		{
			shaders[i].vertex_shader = CreateCachedHLSLAndLoad(device, "Shaders/SMAA_main", LoadShaderSource(L"Shaders/SMAA_main.hlsl"), quality_macro, VS_ENTRY[i], Device::VERTEX_SHADER);
			shaders[i].pixel_shader = CreateCachedHLSLAndLoad(device, "Shaders/SMAA_main", LoadShaderSource(L"Shaders/SMAA_main.hlsl"), quality_macro, PS_ENTRY[i], Device::PIXEL_SHADER);

			shaders[i].vertex_uniform_buffer = Device::DynamicUniformBuffer::Create("SMAA", device, shaders[i].vertex_shader.Get());
			shaders[i].pixel_uniform_buffer = Device::DynamicUniformBuffer::Create("SMAA", device, shaders[i].pixel_shader.Get());
		}
	}
	
	void SMAA::OnLostDevice()
	{
		descriptor_sets.Clear();
		binding_sets.Clear();
		pipelines.Clear();
		passes.Clear();

		edges_render_target.reset();

		for (int i = 0; i < PassType::Count; i++)
		{
			shaders[i] = SMAAShader();
		}
	}
	
	void SMAA::OnDestroyDevice()
	{
		search_texture = ::Texture::Handle();
		area_texture = ::Texture::Handle();

		vb_post_process.Reset();
	}

}