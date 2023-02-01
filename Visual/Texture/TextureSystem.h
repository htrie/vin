#pragma once

#include "Common/Utility/System.h"

#include "Visual/Device/Resource.h"


namespace Device
{
	class IDevice;
	class Texture;
}

namespace Texture
{
	enum class Type : unsigned
	{
		Default,
		Cube,
		Volume,
		Count
	};

	enum Flags
	{
		Srgb = 1,
		Raw = 2,
		Readable = 4,
		FromDisk = 8,
		NoFilter = 16,
		PremultiplyAlpha = 32,
	};

	struct Desc
	{
		unsigned char flags = 0;
		std::wstring filename;
		Type type = Type::Default;
		uint64_t hash = 0;
		bool is_interface = false;
		bool is_async = false;
		bool allow_skip = false;

		Desc() noexcept {}
		Desc(const std::wstring& filename, Type type, unsigned char flags);
	};


	struct Infos
	{
		unsigned width = 0;
		unsigned height = 0;
		size_t size = 0;

		Infos() noexcept {}
		Infos(unsigned width, unsigned height, size_t size)
			: width(width), height(height), size(size) {}

		operator bool() const { return size > 0; }
	};

	static inline Desc SRGBTextureDesc(const std::wstring& filename) { return Desc(filename, Type::Default, Flags::Srgb); }
	static inline Desc LinearTextureDesc(const std::wstring& filename) { return Desc(filename, Type::Default, 0); }
	static inline Desc RawSRGBTextureDesc(const std::wstring& filename) { return Desc(filename, Type::Default, Flags::Srgb | Flags::Raw); }
	static inline Desc RawLinearTextureDesc(const std::wstring& filename) { return Desc(filename, Type::Default, Flags::Raw); }
	static inline Desc RawLinearUITextureDesc(const std::wstring& filename) { return Desc(filename, Type::Default, Flags::Raw | Flags::NoFilter); }
	static inline Desc ReadableLinearTextureDesc(const std::wstring& filename) { return Desc(filename, Type::Default, Flags::Readable); }
	static inline Desc UITextureDesc(const std::wstring& filename) { return Desc(filename, Type::Default, Flags::Srgb | Flags::FromDisk | Flags::NoFilter); }
	static inline Desc UIPremultiplyAlphaTextureDesc(const std::wstring& filename) { return Desc(filename, Type::Default, Flags::Srgb | Flags::FromDisk | Flags::NoFilter | Flags::PremultiplyAlpha); }
	static inline Desc CubeTextureDesc(const std::wstring& filename, bool srgb = true) { return Desc(filename, Type::Cube, srgb ? Flags::Srgb : 0); }
	static inline Desc VolumeTextureDesc(const std::wstring& filename, bool srgb = true) { return Desc(filename, Type::Volume, srgb ? Flags::Srgb : 0); }


	class Handle
	{
		Desc desc;
		Infos infos;
		mutable float pixels = 0.0f; // [TODO] Avoid mutable.

	public:
		Handle() noexcept {}
		Handle(const Desc& desc);

		void SetOnScreenSize(float pixels) const { this->pixels = pixels; }

		Device::Handle<Device::Texture> GetTexture() const;

		const std::wstring& GetFilename() const { return desc.filename; }
		unsigned char GetFlags() const { return desc.flags; }
		uint64_t GetHash() const { return desc.hash; }

		unsigned GetWidth() const { return infos.width; }
		unsigned GetHeight() const { return infos.height; }

		bool IsNull() const { return !infos; }
		bool IsReady() const;

		operator bool() const { return !!infos; }
	};


	struct Stats
	{
		unsigned level_count = 0;
		unsigned active_level_count = 0;
		unsigned touched_level_count = 0;
		unsigned created_level_count = 0;

		size_t budget = 0;
		size_t usage = 0;

		size_t levels_total_size = 0;
		size_t levels_active_size = 0;
		size_t levels_interface_size = 0;
		size_t levels_interface_active_size = 0;
		std::array<float, 16> total_size_histogram;

		float level_qos_min = 0.0f;
		float level_qos_avg = 0.0f;
		float level_qos_max = 0.0f;
		std::array<float, 10> level_qos_histogram;

		unsigned frame_upload_count = 0;
		size_t frame_upload_size = 0;

		bool enable_async = false;
		bool potato_mode = false;
		bool enable_throttling = false;
		bool enable_budget = false;
	};


	class Impl;

	class System : public ImplSystem<Impl, Memory::Tag::Texture>
	{
		System();

	protected:
		System(const System&) = delete;
		System& operator=(const System&) = delete;

	public:
		static System& Get();

		void SetAsync(bool enable);
		void SetThrottling(bool enable);
		void SetBudget(bool enable);
		void SetThrow(bool enable);
		void SetPotato(bool enable);
		void DisableAsync(unsigned frame_count);

		void Init();
		void Swap() final;
		void GarbageCollect() final;
		void Clear() final;

		void OnCreateDevice(Device::IDevice* device);
		void OnResetDevice(Device::IDevice* device);
		void OnLostDevice();
		void OnDestroyDevice();

		bool Update(const float elapsed_time, size_t budget);

		void Invalidate(const std::wstring_view filename);

		void ReloadHigh();

		Infos Gather(const Desc& desc);
		Device::Handle<Device::Texture> Fetch(const Desc& desc, float pixels);
		bool IsReady(const Desc& desc);

		Stats GetStats();
	};
}

