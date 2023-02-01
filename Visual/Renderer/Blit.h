#pragma once

#include "Visual/Device/Resource.h"
#include "Visual/Device/State.h"
#include "Visual/Device/VertexDeclaration.h"

namespace Device
{
	class IDevice;
	class Shader;
	class Texture;
	class IndexBuffer;
	class VertexBuffer;
	class RenderTarget;
}

namespace Renderer
{
	struct CopySubResourceRect
	{
		UINT dst_x;
		UINT dst_y;
		UINT src_x;
		UINT src_y;
		UINT width;
		UINT height;

		CopySubResourceRect(UINT dst_x, UINT dst_y, UINT src_x, UINT src_y, UINT width, UINT height) :
			dst_x(dst_x), dst_y(dst_y), src_x(src_x), src_y(src_y), width(width), height(height)
		{}
	};

	struct CopySubResourcesInfo
	{
		Device::Texture* pSrcResource;
		std::vector<CopySubResourceRect> rects;

		CopySubResourcesInfo(Device::Texture* pSrcResource) :
			pSrcResource(pSrcResource)
		{}

		void Add(UINT dst_x, UINT dst_y, UINT src_x, UINT src_y, UINT width, UINT height)
		{
			rects.push_back(CopySubResourceRect(dst_x, dst_y, src_x, src_y, width, height));
		}
	};

	class Blit
	{
		static const unsigned QuadVertexCount = 6;
		static const unsigned QuadMaxCount = 1024;
		static const unsigned VertexMaxCount = QuadMaxCount * QuadVertexCount;

		struct Vertex
		{
			simd::vector2 position;
			simd::vector2 texcoord;
		};

		Device::VertexDeclaration vertex_declaration;

		Device::Handle<Device::Shader> vertex_shader;
		Device::Handle<Device::Shader> pixel_shader;

	#pragma pack(push)
	#pragma pack(1)
		struct PassKey
		{
			Device::PointerID<Device::RenderTarget> render_target;

			PassKey() {}
			PassKey(Device::RenderTarget* render_target);
		};
	#pragma pack(pop)
		Device::Cache<PassKey, Device::Pass> passes;

	#pragma pack(push)	
	#pragma pack(1)
		struct PipelineKey
		{
			Device::PointerID<Device::Pass> pass;

			PipelineKey() {}
			PipelineKey(Device::Pass* pass);
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

		Device::Handle<Device::VertexBuffer> vertex_buffer;

		Device::IDevice* device;

		static void GenerateQuad(Device::RenderTarget& render_target, Vertex* out, const Device::SurfaceDesc& src_desc, const CopySubResourceRect& rect);

		bool GenerateQuads(Device::RenderTarget& render_target, const Memory::Span<const CopySubResourcesInfo> infos);
		void DrawQuads(Device::RenderTarget& render_target, const Memory::Span<const CopySubResourcesInfo> infos);

	public:
		Blit(Device::IDevice* device);
		
		void CopySubResources(Device::RenderTarget& render_target, const Memory::Span<const CopySubResourcesInfo> infos);

	};

}
