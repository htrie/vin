#pragma once

#include "Common/Utility/System.h"

namespace Device
{
	class IDevice;
	struct Rect;
}

namespace UI
{
	class Impl;

	struct Stats
	{
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

		void Swap() final;
		void Clear() final;

		void OnCreateDevice(Device::IDevice* device);
		void OnResetDevice(Device::IDevice* device);
		void OnLostDevice();
		void OnDestroyDevice();

		void Update(const float elapsed_time);

		void DrawTextW(int size, const wchar_t* text, Device::Rect* rect, unsigned format, const simd::color& color, float scale = 1.0f);

		void Begin();
		void End();

		void Draw(const simd::vector2* vertices, unsigned count, const simd::color& color, float thickness = 1.0f);
		void DrawTransform(const simd::vector3* vertices, unsigned count, const simd::matrix* transform, const simd::color& color, float thickness = 1.0f);

		Stats GetStats();
	};
}