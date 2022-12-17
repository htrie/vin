#include "Include/magic_enum/magic_enum.hpp"

#include "Visual/Device/Device.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/VertexBuffer.h"

#include "TrailsTemplate.h"
#include "TrailsTrail.h"
#include "TrailSegment.h"
#include "TrailSystem.h"

namespace Trails
{
	const Device::VertexElements vertex_elements =
	{
		// Empty
	};

	struct alignas(16) TrailsVertexStructured
	{
		simd::vector3 position;					// [ 0; 12]
		float width;							// [12; 16]
		simd::vector3 pivot_position;			// [16; 28]
		simd::vector2_half scale_spline;		// [28; 32]
		simd::vector4_half colour;				// [32; 40]
		float distance_from_tip;				// [40; 44]
		float distance_since_start;				// [44; 48]
		simd::vector2_half gradient_variance;	// [48; 52]
		simd::vector2_half v_stretch;			// [52; 56]
		float time_since_start;					// [56; 60]
		float phase;							// [60; 64]
	};

	static_assert(sizeof(TrailsVertexStructured) == 64);

	namespace
	{
	#if defined(MOBILE)
		constexpr unsigned int StructureBufferMaxSize = 1 * Memory::MB;
	#elif defined(CONSOLE)
		constexpr unsigned int StructureBufferMaxSize = 2 * Memory::MB;
	#else
		constexpr unsigned int StructureBufferMaxSize = 4 * Memory::MB;
	#endif

		constexpr unsigned int StructuredBufferCount = StructureBufferMaxSize / sizeof(TrailsVertexStructured);
		constexpr unsigned int StructuredBufferSize = sizeof(TrailsVertexStructured) * StructuredBufferCount;
	}

	class Impl
	{
		Device::Handle<Device::StructuredBuffer> structured_buffer;
		std::atomic<uint64_t> structured_pos;
		uint8_t* structured_data = nullptr;

		uint64_t vertex_buffer_used = 0;
		uint64_t vertex_buffer_desired = 0;
		unsigned num_segments = 0;
		unsigned num_vertices = 0;
		unsigned num_merged_trails = 0;

		std::atomic<unsigned> segments_counter = 0;
		std::atomic<unsigned> vertices_counter = 0;
		std::atomic<unsigned> merged_trail_counter = 0;
		std::atomic<uint64_t> desired_buffer_counter = 0;

		struct TransformedSegmentVertex
		{
			simd::vector3 offset;
			float v;
			float width;
		};

		static unsigned GetNumMergedSegments(const Memory::Span<const Trail*>& trails)
		{
			unsigned result = 0;
			for (size_t a = 0; a < trails.size(); a++)
			{
				trails[a]->ForEachPiece([&](const Trail::Piece& fraction, bool first, bool last)
				{
					const auto count = fraction.NumSplineSegments();
					if (count > 0)
						result += count + 2; // Add empty segments for merging
				});
			}

			return result;
		}

		static unsigned WriteMergedSegment(TrailsVertexStructured* buffer, const Trail::Piece& piece, const Segment& segment, unsigned vertices_per_segment, const TransformedSegmentVertex* vertices, const simd::vector3& pivot)
		{
			for (unsigned a = 0; a < vertices_per_segment; a++)
			{
				buffer[a].colour = simd::vector4_half(segment.colour.x, segment.colour.y, segment.colour.z, segment.colour.w);
				buffer[a].phase = std::max(0.0f, segment.phase);
				buffer[a].pivot_position = pivot;
				buffer[a].position = vertices[a].offset;
				buffer[a].gradient_variance = simd::vector2_half(segment.gradient, segment.emitter_variance);
				buffer[a].scale_spline = simd::vector2_half(std::max(0.0f, segment.scale), piece.GetTrail()->trail_template->spline_strength);
				buffer[a].width = vertices[a].width;
				buffer[a].v_stretch = simd::vector2_half(vertices[a].v, piece.GetTrail()->trail_template->texture_stretch);
				buffer[a].distance_since_start = segment.distance_since_start;
				buffer[a].time_since_start = segment.time_since_start;	
				buffer[a].distance_from_tip = segment.distance_from_tip;	
			}

			return vertices_per_segment;
		}

		static unsigned WriteMergedSegmentEmpty(TrailsVertexStructured* buffer, const Trail::Piece& piece, const Segment& segment, unsigned vertices_per_segment, const TransformedSegmentVertex* vertices, const simd::vector3& pivot)
		{
			WriteMergedSegment(buffer, piece, segment, vertices_per_segment, vertices, pivot);

			for (unsigned a = 0; a < vertices_per_segment; a++)
			{
				buffer[a].position = pivot;
				buffer[a].scale_spline = simd::vector2_half(0.0f, 1.0f);
				buffer[a].phase = -FLT_MAX;
				buffer[a].colour = simd::vector4_half(0.0f, 0.0f, 0.0f, 0.0f);
				buffer[a].width = 0.0f;
			}

			return vertices_per_segment;
		}

		static std::pair<unsigned, unsigned> WriteMergedPiece(TrailsVertexStructured* buffer, const Trail::Piece& piece, unsigned vertices_per_segment, bool first_piece)
		{
			const auto num_segments = piece.NumSplineSegments();
			if (num_segments == 0)
				return std::make_pair(0, 0);

			const auto num_vertices = std::min(piece.GetTrail()->NumVerticesPerSegment(), vertices_per_segment);
			const auto& world = piece.GetTrail()->GetTransform();
			unsigned processed = 0;
			unsigned segments = 0;
			TransformedSegmentVertex* vertices = (TransformedSegmentVertex*)alloca(sizeof(TransformedSegmentVertex) * vertices_per_segment);
			
			piece.ForEachSplineSegment([&](const Segment& segment, bool is_last, unsigned index)
			{
				const auto pivot = piece.GetTrail()->trail_template->locked_to_parent ? world.mulpos(segment.position) : segment.position;

				for (unsigned a = 0; a < num_vertices; a++)
				{
					auto p = (segment.GetVertices()[a].offset * std::max(0.0f, segment.scale)) + segment.position;
					if (piece.GetTrail()->trail_template->locked_to_parent)
						p = world.mulpos(p);

					vertices[a].offset = p;
					vertices[a].v = segment.GetVertices()[a].v;
					vertices[a].width = segment.GetVertices()[a].width;
				}

				for (unsigned a = num_vertices; a < vertices_per_segment; a++)
				{
					vertices[a].offset = pivot;
					vertices[a].v = 0.0f;
					vertices[a].width = 0.0f;
				}

				if (processed == 0)
				{
					processed += WriteMergedSegmentEmpty(buffer + processed, piece, segment, vertices_per_segment, vertices, pivot); // Empty Segment for merging
					segments++;
				}

				processed += WriteMergedSegment(buffer + processed, piece, segment, vertices_per_segment, vertices, pivot); // Actual segment
				segments++;

				if (is_last)
				{
					processed += WriteMergedSegmentEmpty(buffer + processed, piece, segment, vertices_per_segment, vertices, pivot); // Empty Segment for merging
					segments++;
				}
			});

			if (segments <= 3)
				return std::make_pair(0, 0);

			if (!first_piece)
				segments++; // Vertices to merge with previous data

			const auto tesellation = (1 + piece.GetTrail()->trail_template->GetTessellationU()) * (1 + piece.GetTrail()->trail_template->GetTessellationV()) * (piece.GetTrail()->trail_template->double_trail ? 2 : 1);
			const auto quad_count = (segments - 3) * tesellation;
			return std::make_pair(processed, quad_count);
		}

		static std::pair<unsigned, unsigned> WriteMergedTrail(TrailsVertexStructured* buffer, const Trail* trail, unsigned vertices_per_segment, bool first_trail)
		{
			unsigned processed = 0;
			unsigned quads = 0;
			trail->ForEachPiece([&](const Trail::Piece& piece, bool first, bool last) 
			{
				const auto r = WriteMergedPiece(buffer + processed, piece, vertices_per_segment, processed == 0 && first_trail);
				processed += r.first;
				quads += r.second;
			});

			return std::make_pair(processed, quads);
		}

		static std::pair<unsigned, unsigned> WriteMergedBatch(TrailsVertexStructured* buffer, const Memory::Span<const Trail*>& trails, unsigned vertices_per_segment)
		{
			unsigned offset = 0;
			unsigned quads = 0;
			for (size_t a = 0; a < trails.size(); a++)
			{
				const auto r = WriteMergedTrail(buffer + offset, trails[a], vertices_per_segment, offset == 0);
				offset += r.first;
				quads += r.second;
			}

			return std::make_pair(offset, quads);
		}

		static bool IsCameraFacing(VertexType t)
		{
			switch (t)
			{
				case VertexType::VERTEX_TYPE_CAM_FACING:
				case VertexType::VERTEX_TYPE_CAM_FACING_NO_LIGHTING:
					return true;
				default:
					return false;
			}
		}

	public:
		void Init()
		{
			structured_pos.store(0, std::memory_order_release);
		}

		void Swap()
		{
			vertex_buffer_used = structured_pos.exchange(0, std::memory_order_acq_rel);
		
			num_merged_trails = merged_trail_counter.exchange(0, std::memory_order_relaxed);
			num_segments = segments_counter.exchange(0, std::memory_order_relaxed);
			num_vertices = vertices_counter.exchange(0, std::memory_order_relaxed);
			vertex_buffer_desired = desired_buffer_counter.exchange(0, std::memory_order_relaxed);
		}

		void Clear()
		{
			
		}

		Stats GetStats() const
		{
			Stats stats;
			stats.num_merged_trails = num_merged_trails;
			stats.vertex_buffer_desired = vertex_buffer_desired * sizeof(TrailsVertexStructured);
			stats.vertex_buffer_size = StructuredBufferCount * sizeof(TrailsVertexStructured);
			stats.vertex_buffer_used = vertex_buffer_used * sizeof(TrailsVertexStructured);
			stats.num_segments = num_segments;
			stats.num_vertices = num_vertices;

			return stats;
		}

		void OnCreateDevice(Device::IDevice* device)
		{
			structured_buffer = Device::StructuredBuffer::Create("Trail Buffer", device, sizeof(TrailsVertexStructured), StructuredBufferCount, Device::FrameUsageHint(), Device::Pool::MANAGED, nullptr, false);
		}

		void OnResetDevice(Device::IDevice* _device) {}

		void OnLostDevice() {}

		void OnDestroyDevice()
		{
			structured_buffer.Reset();
		}

		TrailDraw Merge(const Memory::Span<const Trail*>& trails)
		{
			if (trails.empty())
				return TrailDraw();

			TrailDraw trail_draw;

			const auto vertex_type = trails[0]->GetVertexType();
			const auto vertex_per_segment = IsCameraFacing(vertex_type) ? 2 : trails[0]->NumVerticesPerSegment();
			const auto segment_stride = IsCameraFacing(vertex_type) ? 1 : vertex_per_segment;

			const auto num_segments = GetNumMergedSegments(trails);
			if (num_segments < 2)
				return trail_draw;

			const auto num_vertices = num_segments * segment_stride;

			desired_buffer_counter.fetch_add(num_vertices, std::memory_order_relaxed);

			auto offset = structured_pos.load(std::memory_order_acquire);
			do {
				if (offset + num_vertices > StructuredBufferCount)
					return trail_draw;

			} while (!structured_pos.compare_exchange_weak(offset, offset + num_vertices, std::memory_order_acq_rel));

			const auto processed = WriteMergedBatch(reinterpret_cast<TrailsVertexStructured*>(structured_data) + offset, trails, segment_stride);
			assert(processed.first <= num_vertices);
			assert(processed.first % segment_stride == 0);

			if (processed.second == 0)
				return trail_draw;

			segments_counter.fetch_add(processed.first, std::memory_order_relaxed);
			vertices_counter.fetch_add((vertex_per_segment - 1) * processed.second * 6, std::memory_order_relaxed);

			trail_draw.num_primitives = (vertex_per_segment - 1) * processed.second * 2;
			trail_draw.num_vertices = (vertex_per_segment - 1) * processed.second * 6;
			trail_draw.segment_offset = unsigned(offset);
			trail_draw.segment_stride = segment_stride;

			merged_trail_counter.fetch_add(1, std::memory_order_relaxed);
			return trail_draw;
		}

		void FrameMoveBegin(float elapsed_time)
		{
			structured_buffer->Lock(0, StructuredBufferSize, (void**)&structured_data, Device::Lock::DISCARD);
		}

		void FrameMoveEnd()
		{
			structured_buffer->Unlock();
		}

		Device::Handle<Device::StructuredBuffer> GetBuffer() const
		{
			return structured_buffer; 
		}
	};

	System::System() : ImplSystem()
	{}

	System& System::Get()
	{
		static System instance;
		return instance;
	}

	void System::Init() { impl->Init(); }

	void System::Swap() { impl->Swap(); }
	void System::Clear() { impl->Clear(); }

	Stats System::GetStats() const { return impl->GetStats(); }

	Device::Handle<Device::StructuredBuffer> System::GetBuffer() const { return impl->GetBuffer(); }

	void System::OnCreateDevice(Device::IDevice* device) { impl->OnCreateDevice(device); }
	void System::OnResetDevice(Device::IDevice* device) { impl->OnResetDevice(device); }
	void System::OnLostDevice() { impl->OnLostDevice(); }
	void System::OnDestroyDevice() { impl->OnDestroyDevice(); }

	TrailDraw System::Merge(const Memory::Span<const Trail*>& trails) { return impl->Merge(trails); }

	void System::FrameMoveBegin(float elapsed_time) { impl->FrameMoveBegin(elapsed_time); }
	void System::FrameMoveEnd() { impl->FrameMoveEnd(); }
}

