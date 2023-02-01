#pragma once
#include "Visual/GpuParticles/GpuParticleSystem.h"

namespace GpuParticles
{
	std::optional<System::Template> MakeTemplate(const EmitterTemplate& templ, float event_duration);
	std::optional<System::Desc> MakeDesc(const EmitterTemplate& templ, Device::CullMode cull_mode = Device::CullMode::CCW, bool async = true, const std::wstring_view& debug_name = L"");
	std::optional<System::Template> MakeTemplate(const Particles::EmitterTemplate& templ, float variance, float event_duration);
	std::optional<System::Desc> MakeDesc(const Particles::EmitterTemplate& templ, Device::CullMode cull_mode = Device::CullMode::CCW, bool async = true, const std::wstring_view& debug_name = L"");

	inline std::optional<System::Desc> MakeDesc(const EmitterTemplate& templ, bool async, const std::wstring_view& debug_name = L"") { return MakeDesc(templ, Device::CullMode::CCW, async, debug_name); }
	inline std::optional<System::Desc> MakeDesc(const Particles::EmitterTemplate& templ, bool async, const std::wstring_view& debug_name = L"") { return MakeDesc(templ, Device::CullMode::CCW, async, debug_name); }
	inline std::optional<System::Desc> MakeDesc(const EmitterTemplate& templ, const std::wstring_view& debug_name) { return MakeDesc(templ, Device::CullMode::CCW, true, debug_name); }
	inline std::optional<System::Desc> MakeDesc(const Particles::EmitterTemplate& templ, const std::wstring_view& debug_name) { return MakeDesc(templ, Device::CullMode::CCW, true, debug_name); }
}