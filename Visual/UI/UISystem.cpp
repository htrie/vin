
#include "Visual/Device/Device.h"
#include "Visual/Device/Font.h"
#include "Visual/Device/Line.h"

#include "UISystem.h"

namespace UI
{

	class Impl
	{
		std::unique_ptr<Device::Font> debug_font;
		std::unique_ptr<Device::Line> debug_line;

	public:
		void Init()
		{
		}

		void Swap()
		{
			if (debug_line)
				debug_line->Swap();
		}

		void Clear()
		{
		}

		void OnCreateDevice(Device::IDevice* device)
		{
			debug_font = Device::Font::Create(device);
			debug_line = Device::Line::Create(device);
		}

		void OnResetDevice(Device::IDevice* device)
		{
			if (debug_font)
				debug_font->OnResetDevice();
			if (debug_line)
				debug_line->OnResetDevice();
		}

		void OnLostDevice()
		{
			if (debug_font)
				debug_font->OnLostDevice();
			if (debug_line)
				debug_line->OnLostDevice();
		}

		void OnDestroyDevice()
		{
			debug_font.reset();
			debug_line.reset();
		}

		void Update(const float elapsed_time)
		{
		}

		void UpdateStats(Stats& stats)
		{
		}

		void DrawTextW(int size, const wchar_t* text, Device::Rect* rect, unsigned format, const simd::color& color, float scale)
		{
			if (debug_font)
				debug_font->DrawTextW(size, text, rect, format, color, scale);
		}

		void Begin()
		{
			if (debug_line)
				debug_line->Begin();
		}

		void End()
		{
			if (debug_line)
				debug_line->End();
		}

		void Draw(const simd::vector2* vertices, unsigned count, const simd::color& color, float thickness)
		{
			if (debug_line)
				debug_line->Draw(vertices, count, color, thickness);
		}

		void DrawTransform(const simd::vector3* vertices, unsigned count, const simd::matrix* transform, const simd::color& color, float thickness)
		{
			if (debug_line)
				debug_line->DrawTransform(vertices, count, transform, color, thickness);
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

	void System::OnCreateDevice(Device::IDevice* device) { impl->OnCreateDevice(device); }
	void System::OnResetDevice(Device::IDevice* device) { impl->OnResetDevice(device); }
	void System::OnLostDevice() { impl->OnLostDevice(); }
	void System::OnDestroyDevice() { impl->OnDestroyDevice(); }

	void System::Update(const float elapsed_time) { impl->Update(elapsed_time); }

	void System::DrawTextW(int size, const wchar_t* text, Device::Rect* rect, unsigned format, const simd::color& color, float scale) { impl->DrawTextW(size, text, rect, format, color, scale); }

	void System::Begin() { impl->Begin(); }
	void System::End() { impl->End(); }

	void System::Draw(const simd::vector2* vertices, unsigned count, const simd::color& color, float thickness) { impl->Draw(vertices, count, color, thickness); }
	void System::DrawTransform(const simd::vector3* vertices, unsigned count, const simd::matrix* transform, const simd::color& color, float thickness) { impl->DrawTransform(vertices, count, transform, color, thickness); }

	Stats System::GetStats()
	{
		Stats stats;
		impl->UpdateStats(stats);
		return stats;
	}

}
