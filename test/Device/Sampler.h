#pragma once

#include "Resource.h"

namespace Device
{
	
	enum class FilterType : unsigned
	{
		MIN_MAG_MIP_POINT,
		MIN_MAG_POINT_MIP_LINEAR,
		MIN_POINT_MAG_LINEAR_MIP_POINT,
		MIN_POINT_MAG_MIP_LINEAR,
		MIN_LINEAR_MAG_MIP_POINT,
		MIN_LINEAR_MAG_POINT_MIP_LINEAR,
		MIN_MAG_LINEAR_MIP_POINT,
		MIN_MAG_MIP_LINEAR,
		ANISOTROPIC,
		COMPARISON_MIN_MAG_MIP_POINT,
		COMPARISON_MIN_MAG_POINT_MIP_LINEAR,
		COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT,
		COMPARISON_MIN_POINT_MAG_MIP_LINEAR,
		COMPARISON_MIN_LINEAR_MAG_MIP_POINT,
		COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
		COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		COMPARISON_MIN_MAG_MIP_LINEAR,
		COMPARISON_ANISOTROPIC,
		MINIMUM_MIN_MAG_MIP_POINT,
		MINIMUM_MIN_MAG_POINT_MIP_LINEAR,
		MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT,
		MINIMUM_MIN_POINT_MAG_MIP_LINEAR,
		MINIMUM_MIN_LINEAR_MAG_MIP_POINT,
		MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
		MINIMUM_MIN_MAG_LINEAR_MIP_POINT,
		MINIMUM_MIN_MAG_MIP_LINEAR,
		MINIMUM_ANISOTROPIC,
		MAXIMUM_MIN_MAG_MIP_POINT,
		MAXIMUM_MIN_MAG_POINT_MIP_LINEAR,
		MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT,
		MAXIMUM_MIN_POINT_MAG_MIP_LINEAR,
		MAXIMUM_MIN_LINEAR_MAG_MIP_POINT,
		MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
		MAXIMUM_MIN_MAG_LINEAR_MIP_POINT,
		MAXIMUM_MIN_MAG_MIP_LINEAR,
		MAXIMUM_ANISOTROPIC,
		NumFilterType
	};

	enum class TextureAddress : unsigned
	{
		WRAP,
		MIRROR,
		CLAMP,
		BORDER,
		MIRRORONCE,
	};

	enum class ComparisonFunc : unsigned
	{
		NEVER,
		LESS,
		EQUAL,
		LESS_EQUAL,
		GREATER,
		NOT_EQUAL,
		GREATER_EQUAL,
		ALWAYS,
		NumComparisonFunc,
	};

	struct SamplerInfo
	{
		ComparisonFunc comparison_func;
		TextureAddress address_u;
		TextureAddress address_v;
		TextureAddress address_w;
		FilterType filter_type;
		uint32_t anisotropy_level;
		float lod_bias;
		bool is_from_settings;
	};

	class Sampler : public Resource
	{
		static inline std::atomic_uint count = { 0 };

	protected:
		Sampler(const Memory::DebugStringA<>& name);

	public:
		virtual ~Sampler();

		static Handle<Sampler> Create(const Memory::DebugStringA<>& name, IDevice* device, const SamplerInfo& info);

		static unsigned GetCount() { return count.load(); }
	};

}
