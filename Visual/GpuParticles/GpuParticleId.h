#pragma once

namespace GpuParticles
{
	struct EmitterId
	{
		uint64_t v;

		EmitterId(uint64_t v = 0) : v(v) {}

		explicit operator bool() const { return v != 0; }
		bool operator==(const EmitterId& o) const { return v == o.v; }
		bool operator!=(const EmitterId& o) const { return v != o.v; }
	};
}