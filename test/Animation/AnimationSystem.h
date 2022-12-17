#pragma once

#include "Common/Utility/System.h"

#include "Visual/Device/Resource.h"

namespace simd
{
	class matrix;
	class vector4;
}

namespace Device
{
	class IDevice;
	class StructuredBuffer;
}

namespace Animation
{

	typedef Memory::Vector<simd::matrix, Memory::Tag::AnimationPalette> MatrixPalette;
	typedef Memory::Vector<simd::vector4, Memory::Tag::AnimationPalette> Vector4Palette;

	struct Palette
	{
		std::shared_ptr<MatrixPalette> transform_palette;
		std::shared_ptr<Vector4Palette> uv_alpha_palette;
		simd::vector4 indices = 0.0f;
		uint64_t palette_id = 0;

		static inline std::atomic_uint64_t monotonic_id = 0;

		Palette(const std::shared_ptr<MatrixPalette>& transform_palette, const std::shared_ptr<Vector4Palette>& uv_alpha_palette)
			: transform_palette(transform_palette), uv_alpha_palette(uv_alpha_palette), palette_id(++monotonic_id) {}
	};

	struct Stats
	{
		size_t used_size = 0;
		size_t max_size = 0;
		size_t overflow_size = 0;
	};

	struct DetailedStorageID
	{
		uint64_t gen = 0;
		size_t id = 0;
	};

	struct DetailedStorageStatic
	{
		size_t frame_count = 0;
		size_t bone_count = 0;
		float elapsed_time_total = 0;
	};

	struct DetailedStorage
	{
		float elapsed_time = 0;
		Memory::Span<const simd::vector3> bones;
	};

	class Impl;

	class System : public ImplSystem<Impl, Memory::Tag::Animation>
	{
		System();

	public:
		static System& Get();

		void Swap() final;
		void GarbageCollect() final;
		void Clear() final;

		void OnCreateDevice(Device::IDevice* device);
		void OnResetDevice(Device::IDevice* device);
		void OnLostDevice();
		void OnDestroyDevice();

		Stats GetStats() const;

		void FrameMoveBegin();
		void FrameMoveEnd();

		void Add(const std::shared_ptr<Palette>& palette);
		void Remove(const std::shared_ptr<Palette>& palette);

		Device::Handle<Device::StructuredBuffer> GetBuffer();

		DetailedStorageID ReserveDetailedStorage(size_t bone_count, size_t frame_count, float elapsed_time);
		void PushDetailedStorage(const DetailedStorageID& id, size_t frame, float key, const Memory::Span<const simd::vector3>& bones);

		DetailedStorageStatic GetDetailedStorageStatic(const DetailedStorageID& id) const;
		DetailedStorage GetDetailedStorage(const DetailedStorageID& id, size_t frame) const;
	};

}
