
#include "Common/Utility/Logger/Logger.h"
#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Renderer/CachedHLSLShader.h"
#include "Visual/Renderer/Utility.h"
#include "Visual/Device/State.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/Shader.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/Device.h"

#include "Blit.h"

namespace Renderer
{

	static Device::VertexElements vertex_def =
	{
		{ 0, 0, Device::DeclType::FLOAT2, Device::DeclUsage::POSITION, 0 },
		{ 0, 8, Device::DeclType::FLOAT2, Device::DeclUsage::TEXCOORD, 0 },
	};

	Blit::PassKey::PassKey(Device::RenderTarget* render_target)
		: render_target(render_target)
	{}

	Blit::PipelineKey::PipelineKey(Device::Pass* pass)
		: pass(pass)
	{}

	Blit::BindingSetKey::BindingSetKey(Device::Shader* shader, uint32_t inputs_hash)
		: shader(shader), inputs_hash(inputs_hash)
	{}

	Blit::DescriptorSetKey::DescriptorSetKey(Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set, uint32_t samplers_hash)
		: pipeline(pipeline), pixel_binding_set(pixel_binding_set), samplers_hash(samplers_hash)
	{}

	Blit::Blit(Device::IDevice* device)
		: device(device)
		, vertex_declaration(vertex_def)
	{
		vertex_buffer = Device::VertexBuffer::Create("VB Blit", device, (unsigned)sizeof(Vertex) * VertexMaxCount, (Device::UsageHint)((unsigned)Device::UsageHint::DYNAMIC | (unsigned)Device::UsageHint::WRITEONLY), Device::Pool::DEFAULT, nullptr);

		vertex_shader = Renderer::CreateCachedHLSLAndLoad(device, "Blit_Vertex", Renderer::LoadShaderSource(L"Shaders/Blit_Vertex.hlsl"), nullptr, "main", Device::ShaderType::VERTEX_SHADER);
		pixel_shader = Renderer::CreateCachedHLSLAndLoad(device, "Blit_Pixel", Renderer::LoadShaderSource(L"Shaders/Blit_Pixel.hlsl"), nullptr, "main", Device::ShaderType::PIXEL_SHADER);
	}

	void Blit::GenerateQuad(Device::RenderTarget& render_target, Vertex* out, const Device::SurfaceDesc& src_desc, const CopySubResourceRect& rect)
	{
		const float src_left = rect.src_x / (float)src_desc.Width;
		const float src_top = rect.src_y / (float)src_desc.Height;
		const float src_right = (rect.src_x + rect.width) / (float)src_desc.Width;
		const float src_bottom = (rect.src_y + rect.height) / (float)src_desc.Height;

		const float dst_left = rect.dst_x / (float)render_target.GetWidth();
		const float dst_top = rect.dst_y / (float)render_target.GetHeight();
		const float dst_right = (rect.dst_x + rect.width) / (float)render_target.GetWidth();
		const float dst_bottom = (rect.dst_y + rect.height) / (float)render_target.GetHeight();

		const auto src_0 = simd::vector2(src_left, src_top);
		const auto src_1 = simd::vector2(src_right, src_top);
		const auto src_2 = simd::vector2(src_left, src_bottom);
		const auto src_3 = simd::vector2(src_right, src_bottom);
		
		const auto dst_0 = simd::vector2(dst_left, 1.0f - dst_top) * 2.0f - 1.0f;
		const auto dst_1 = simd::vector2(dst_right, 1.0f - dst_top) * 2.0f - 1.0f;
		const auto dst_2 = simd::vector2(dst_left, 1.0f - dst_bottom) * 2.0f - 1.0f;
		const auto dst_3 = simd::vector2(dst_right, 1.0f - dst_bottom) * 2.0f - 1.0f;

		out[0].position = dst_0;
		out[1].position = dst_1;
		out[2].position = dst_2;
		out[3].position = dst_1;
		out[4].position = dst_3;
		out[5].position = dst_2;
		
		out[0].texcoord = src_0;
		out[1].texcoord = src_1;
		out[2].texcoord = src_2;
		out[3].texcoord = src_1;
		out[4].texcoord = src_3;
		out[5].texcoord = src_2;
	}

	bool Blit::GenerateQuads(Device::RenderTarget& render_target, const Memory::Span<const CopySubResourcesInfo> infos)
	{
		Vertex* vertex_data = nullptr;
		vertex_buffer->Lock(0, 0, (void**)(&vertex_data), Device::Lock::DISCARD);

		unsigned index = 0;
		bool all_quads_generated = true;

		for (auto& info : infos)
		{
			if (info.pSrcResource)
			{
				Device::SurfaceDesc src_desc;
				info.pSrcResource->GetLevelDesc(0, &src_desc);

				for(auto& rect : info.rects)
				{
					if (index < QuadMaxCount)
						GenerateQuad(render_target, &vertex_data[index * QuadVertexCount], src_desc, rect);
					else
						all_quads_generated = false;

					index++;
				}
			}
		}

		vertex_buffer->Unlock();

		return all_quads_generated;
	}
	
	void Blit::DrawQuads(Device::RenderTarget& render_target, const Memory::Span<const CopySubResourcesInfo> infos)
	{
		auto& command_buffer = *device->GetCurrentUICommandBuffer();

		auto* pass = passes.FindOrCreate(PassKey(&render_target), [&]()
		{
			return Device::Pass::Create("Blit", device, {&render_target}, nullptr, Device::ClearTarget::COLOR, simd::color(0), 0.0f);
		}).Get();
		command_buffer.BeginPass(pass, render_target.GetSize(), render_target.GetSize());

		auto* pipeline = pipelines.FindOrCreate(PipelineKey(pass), [&]()
		{
			return Device::Pipeline::Create("Blit", device, pass, Device::PrimitiveType::TRIANGLELIST, &vertex_declaration, vertex_shader.Get(), pixel_shader.Get(), 
				Device::DisableBlendState(), Device::RasterizerState(Device::CullMode::NONE, Device::FillMode::SOLID, 0, 0), Device::DisableDepthStencilState());
		}).Get();
		if (command_buffer.BindPipeline(pipeline))
		{
			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));

			unsigned index = 0;
		
			for (auto& info : infos)
			{
				Device::DynamicBindingSet::Inputs inputs;
				inputs.push_back({ "tex_sampler", info.pSrcResource });
				auto* pixel_binding_set = binding_sets.FindOrCreate(BindingSetKey(pixel_shader.Get(), inputs.Hash()), [&]()
				{
					return Device::DynamicBindingSet::Create("Blit", device, pixel_shader.Get(), inputs);
				}).Get();

				auto* descriptor_set = descriptor_sets.FindOrCreate(DescriptorSetKey(pipeline, pixel_binding_set, device->GetSamplersHash()), [&]()
				{
					return Device::DescriptorSet::Create("Blit", device, pipeline, {}, { pixel_binding_set });
				}).Get();
				command_buffer.BindDescriptorSet(descriptor_set);

				for(auto& rect : info.rects)
				{
					if (index < QuadMaxCount)
						command_buffer.Draw(QuadVertexCount, index * QuadVertexCount);

					index++;
				}
			}
		}

		command_buffer.EndPass();
	}

	void Blit::CopySubResources(Device::RenderTarget& render_target, const Memory::Span<const CopySubResourcesInfo> infos)
	{
		const auto all_quads_generated = GenerateQuads(render_target, infos);

		if (!all_quads_generated)
			LOG_CRIT(L"Error: Blit vertex buffer size exceeded. Some draw calls skipped.");

		DrawQuads(render_target, infos);
	}

}
