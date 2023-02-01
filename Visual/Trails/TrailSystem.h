#pragma once

#include "Common/Utility/System.h"
#include "Common/Utility/Geometric.h"
#include "Visual/Device/Resource.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Include/magic_enum/magic_enum.hpp"

namespace Device
{
	class IDevice;
	class IndexBuffer;
	class VertexBuffer;
	class StructuredBuffer;
}

namespace Trails
{
	extern const Device::VertexElements vertex_elements;

	struct Trail;
	class Impl;

	enum VertexType
	{
		VERTEX_TYPE_DEFAULT,
		VERTEX_TYPE_DEFAULT_NO_LIGHTING,
		VERTEX_TYPE_CAM_FACING,
		VERTEX_TYPE_CAM_FACING_NO_LIGHTING,
	};

	struct TrailDraw
	{
		unsigned num_vertices = 0;
		unsigned num_primitives = 0;
		unsigned segment_offset = 0;
		unsigned segment_stride = 0;
	};

	struct Stats
	{
		unsigned num_segments = 0;
		unsigned num_vertices = 0;
		unsigned num_merged_trails = 0;
		uint64_t vertex_buffer_size = 0;
		uint64_t vertex_buffer_used = 0;
		uint64_t vertex_buffer_desired = 0;
	};

	class System : public ImplSystem<Impl, Memory::Tag::Trail>
	{
		System();

	public:
		System(const System&) = delete;
		System& operator=(const System&) = delete;

		static System& Get();

		void Init();

		void Swap() override final;
		void Clear() override final;

		Stats GetStats() const;

		Device::Handle<Device::StructuredBuffer> GetBuffer() const;

		void OnCreateDevice(Device::IDevice* device);
		void OnResetDevice(Device::IDevice* device);
		void OnLostDevice();
		void OnDestroyDevice();

		TrailDraw Merge(const Memory::Span<const Trail*>& trails);

		void FrameMoveBegin(float elapsed_time);
		void FrameMoveEnd();
	};
}