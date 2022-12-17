#pragma once

#include "Common/Utility/System.h"

namespace Device
{
	class IDevice;
}

namespace Video
{
	class Impl;

	class System : public ImplSystem<Impl, Memory::Tag::Video>
	{
		System();

	protected:
		System(const System&) = delete;
		System& operator=(const System&) = delete;

	public:
		static System& Get();

		void Init(bool enable_video);

		void Swap() final;
		void GarbageCollect() final;
		void Clear() final;

		void OnCreateDevice(Device::IDevice* device);
		void OnResetDevice(Device::IDevice* device);
		void OnLostDevice();
		void OnDestroyDevice();

		uint64_t Add(std::wstring_view filename, std::wstring_view url);
		void Remove(uint64_t id);

		void Render(uint64_t id);

		void SetPosition(uint64_t id, float left, float top, float right, float bottom); // [0.0 1.0]
		void SetLoop(uint64_t id, bool loop);
		void SetVolume(uint64_t id, float volume); // [0.0 2.0]
		void SetAlpha(uint64_t id, float alpha);
		void SetColourMultiply(uint64_t id, float r, float g, float b);

		void SetAdditiveBlendMode(uint64_t id);
		void ResetBlendMode(uint64_t id);

		float GetAspectRatio(uint64_t id);
		bool IsPlaying(uint64_t id);

		unsigned int GetNextFrame(uint64_t id);
		void SetNextFrame(uint64_t id, unsigned int frame);

		float GetPlaceByPercent(uint64_t id);
		void SetPlaceByPercent(uint64_t id, float percent);
	};

}
