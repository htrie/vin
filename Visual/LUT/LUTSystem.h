#pragma once

#include "Common/Utility/System.h"
#include "Common/Simd/Curve.h"
#include "Common/Simd/Simd.h"
#include "Visual/Device/Resource.h"

namespace Device
{
	class IDevice;
	class Texture;
}

namespace LUT
{
	typedef simd::GenericCurve<8> Desc;
	typedef uint32_t Id;

	class Impl;

	struct Stats
	{
		size_t num_entries = 0;
		size_t num_unique = 0;
	};

	class System : public ImplSystem<Impl, Memory::Tag::Render>
	{
		System();

	protected:
		System(const System&) = delete;
		System& operator=(const System&) = delete;

	public:
		static System& Get();

		void Init();

		void Swap() override final;
		void Clear() override final;

		Stats GetStats() const;

		Id Add(const Desc& desc);
		Id Add(Id id);
		void Remove(Id id);
		float Fetch(Id id);

		void OnCreateDevice(Device::IDevice* device);
		void OnResetDevice(Device::IDevice* device);
		void OnLostDevice();
		void OnDestroyDevice();

		void Update(const float elapsed_time);

		Device::Handle<Device::Texture> GetBuffer();
		const float& GetBufferWidth();
	};

	class Handle
	{
	private:
		Id id;

	public:
		Handle() : id(0) {}
		Handle(const Desc& desc) : id(System::Get().Add(desc)) {}
		Handle(const Handle& o) : id(System::Get().Add(o.id)) {}
		Handle(Handle&& o) noexcept : id(o.id) { o.id = 0; }

		~Handle() { System::Get().Remove(id); }

		Handle& operator=(const Handle& o)
		{
			if (id == o.id)
				return *this;

			System::Get().Remove(id);
			id = System::Get().Add(o.id);
			return *this;
		}

		Handle& operator=(Handle&& o) noexcept 
		{
			System::Get().Remove(id);
			id = o.id;
			o.id = 0;
			return *this;
		}

		Id GetId() const { return id; }
		void Reset() { System::Get().Remove(id); id = 0; }
		float Get() const { return System::Get().Fetch(id); }
		float operator*() const { return Get(); }
	};

	class Spline
	{
	private:
		Handle handle[4];

		bool UpdateHandle(size_t i, const Desc& d)
		{
			handle[i] = Handle(d);
			return handle[i].Get() < 1.0f;
		}

		bool UpdateImpl()
		{
			if (!UpdateHandle(0, r)) return false;
			if (!UpdateHandle(1, g)) return false;
			if (!UpdateHandle(2, b)) return false;
			if (!UpdateHandle(3, a)) return false;
			return true;
		}

	public:
		Desc r;
		Desc g;
		Desc b;
		Desc a;

		Handle& operator[](size_t i) { return handle[i]; }
		const Handle& operator[](size_t i) const { return handle[i]; }
		simd::vector4 Fetch() const { return simd::vector4(handle[0].Get(), handle[1].Get(), handle[2].Get(), handle[3].Get()); }

		void Update()
		{
			if (!UpdateImpl())
				Reset();
		}

		void Reset()
		{
			for (auto& a : handle)
				a.Reset();
		}
	};
}