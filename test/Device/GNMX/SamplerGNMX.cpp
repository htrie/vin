
namespace Device
{

	Gnm::DepthCompare ConvertComparisonFunc(ComparisonFunc comparison_func)
	{
		switch (comparison_func)
		{
		case ComparisonFunc::NEVER:			return Gnm::kDepthCompareNever;
		case ComparisonFunc::LESS:			return Gnm::kDepthCompareLess;
		case ComparisonFunc::EQUAL:			return Gnm::kDepthCompareEqual;
		case ComparisonFunc::LESS_EQUAL:	return Gnm::kDepthCompareLessEqual;
		case ComparisonFunc::GREATER:		return Gnm::kDepthCompareGreater;
		case ComparisonFunc::NOT_EQUAL:		return Gnm::kDepthCompareNotEqual;
		case ComparisonFunc::GREATER_EQUAL: return Gnm::kDepthCompareGreaterEqual;
		case ComparisonFunc::ALWAYS:		return Gnm::kDepthCompareAlways;
		default:							throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::WrapMode ConvertTextureAddress(TextureAddress texture_address)
	{
		switch (texture_address)
		{
		case TextureAddress::WRAP:			return Gnm::kWrapModeWrap;
		case TextureAddress::MIRROR:		return Gnm::kWrapModeMirror;
		case TextureAddress::CLAMP:			return Gnm::kWrapModeClampLastTexel;
		case TextureAddress::BORDER:		return Gnm::kWrapModeClampBorder;
		case TextureAddress::MIRRORONCE:	return Gnm::kWrapModeMirrorOnceLastTexel;
		default:							throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::FilterMode ConvertMagFilterMode(FilterType filter_type)
	{
		switch (filter_type)
		{
		case FilterType::MIN_MAG_MIP_POINT:							return Gnm::kFilterModePoint;
		case FilterType::MIN_MAG_POINT_MIP_LINEAR:					return Gnm::kFilterModePoint;
		case FilterType::MIN_POINT_MAG_LINEAR_MIP_POINT:			return Gnm::kFilterModeBilinear;
		case FilterType::MIN_POINT_MAG_MIP_LINEAR:					return Gnm::kFilterModeBilinear;
		case FilterType::MIN_LINEAR_MAG_MIP_POINT:					return Gnm::kFilterModePoint;
		case FilterType::MIN_LINEAR_MAG_POINT_MIP_LINEAR:			return Gnm::kFilterModePoint;
		case FilterType::MIN_MAG_LINEAR_MIP_POINT:					return Gnm::kFilterModeBilinear;
		case FilterType::MIN_MAG_MIP_LINEAR:						return Gnm::kFilterModeBilinear;
		case FilterType::ANISOTROPIC:								return Gnm::kFilterModeAnisoBilinear;
		case FilterType::COMPARISON_MIN_MAG_MIP_POINT:				return Gnm::kFilterModePoint;
		case FilterType::COMPARISON_MIN_MAG_POINT_MIP_LINEAR:		return Gnm::kFilterModePoint;
		case FilterType::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:	return Gnm::kFilterModeBilinear;
		case FilterType::COMPARISON_MIN_POINT_MAG_MIP_LINEAR:		return Gnm::kFilterModeBilinear;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_MIP_POINT:		return Gnm::kFilterModePoint;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:return Gnm::kFilterModePoint;
		case FilterType::COMPARISON_MIN_MAG_LINEAR_MIP_POINT:		return Gnm::kFilterModeBilinear;
		case FilterType::COMPARISON_MIN_MAG_MIP_LINEAR:				return Gnm::kFilterModeBilinear;
		case FilterType::COMPARISON_ANISOTROPIC:					return Gnm::kFilterModeAnisoBilinear;
		case FilterType::MINIMUM_MIN_MAG_MIP_POINT:					return Gnm::kFilterModePoint;
		case FilterType::MINIMUM_MIN_MAG_POINT_MIP_LINEAR:			return Gnm::kFilterModePoint;
		case FilterType::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return Gnm::kFilterModeBilinear;
		case FilterType::MINIMUM_MIN_POINT_MAG_MIP_LINEAR:			return Gnm::kFilterModeBilinear;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_MIP_POINT:			return Gnm::kFilterModePoint;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return Gnm::kFilterModePoint;
		case FilterType::MINIMUM_MIN_MAG_LINEAR_MIP_POINT:			return Gnm::kFilterModeBilinear;
		case FilterType::MINIMUM_MIN_MAG_MIP_LINEAR:				return Gnm::kFilterModeBilinear;
		case FilterType::MINIMUM_ANISOTROPIC:						return Gnm::kFilterModeAnisoBilinear;
		case FilterType::MAXIMUM_MIN_MAG_MIP_POINT:					return Gnm::kFilterModePoint;
		case FilterType::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:			return Gnm::kFilterModePoint;
		case FilterType::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return Gnm::kFilterModeBilinear;
		case FilterType::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:			return Gnm::kFilterModeBilinear;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:			return Gnm::kFilterModePoint;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return Gnm::kFilterModePoint;
		case FilterType::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:			return Gnm::kFilterModeBilinear;
		case FilterType::MAXIMUM_MIN_MAG_MIP_LINEAR:				return Gnm::kFilterModeBilinear;
		case FilterType::MAXIMUM_ANISOTROPIC:						return Gnm::kFilterModeAnisoBilinear;
		default:													throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::FilterMode ConvertMinFilterMode(FilterType filter_type)
	{
		switch (filter_type)
		{
		case FilterType::MIN_MAG_MIP_POINT:							return Gnm::kFilterModePoint;
		case FilterType::MIN_MAG_POINT_MIP_LINEAR:					return Gnm::kFilterModePoint;
		case FilterType::MIN_POINT_MAG_LINEAR_MIP_POINT:			return Gnm::kFilterModePoint;
		case FilterType::MIN_POINT_MAG_MIP_LINEAR:					return Gnm::kFilterModePoint;
		case FilterType::MIN_LINEAR_MAG_MIP_POINT:					return Gnm::kFilterModeBilinear;
		case FilterType::MIN_LINEAR_MAG_POINT_MIP_LINEAR:			return Gnm::kFilterModeBilinear;
		case FilterType::MIN_MAG_LINEAR_MIP_POINT:					return Gnm::kFilterModeBilinear;
		case FilterType::MIN_MAG_MIP_LINEAR:						return Gnm::kFilterModeBilinear;
		case FilterType::ANISOTROPIC:								return Gnm::kFilterModeAnisoBilinear;
		case FilterType::COMPARISON_MIN_MAG_MIP_POINT:				return Gnm::kFilterModePoint;
		case FilterType::COMPARISON_MIN_MAG_POINT_MIP_LINEAR:		return Gnm::kFilterModePoint;
		case FilterType::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:	return Gnm::kFilterModePoint;
		case FilterType::COMPARISON_MIN_POINT_MAG_MIP_LINEAR:		return Gnm::kFilterModePoint;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_MIP_POINT:		return Gnm::kFilterModeBilinear;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:return Gnm::kFilterModeBilinear;
		case FilterType::COMPARISON_MIN_MAG_LINEAR_MIP_POINT:		return Gnm::kFilterModeBilinear;
		case FilterType::COMPARISON_MIN_MAG_MIP_LINEAR:				return Gnm::kFilterModeBilinear;
		case FilterType::COMPARISON_ANISOTROPIC:					return Gnm::kFilterModeAnisoBilinear;
		case FilterType::MINIMUM_MIN_MAG_MIP_POINT:					return Gnm::kFilterModePoint;
		case FilterType::MINIMUM_MIN_MAG_POINT_MIP_LINEAR:			return Gnm::kFilterModePoint;
		case FilterType::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return Gnm::kFilterModePoint;
		case FilterType::MINIMUM_MIN_POINT_MAG_MIP_LINEAR:			return Gnm::kFilterModePoint;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_MIP_POINT:			return Gnm::kFilterModeBilinear;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return Gnm::kFilterModeBilinear;
		case FilterType::MINIMUM_MIN_MAG_LINEAR_MIP_POINT:			return Gnm::kFilterModeBilinear;
		case FilterType::MINIMUM_MIN_MAG_MIP_LINEAR:				return Gnm::kFilterModeBilinear;
		case FilterType::MINIMUM_ANISOTROPIC:						return Gnm::kFilterModeAnisoBilinear;
		case FilterType::MAXIMUM_MIN_MAG_MIP_POINT:					return Gnm::kFilterModePoint;
		case FilterType::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:			return Gnm::kFilterModePoint;
		case FilterType::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return Gnm::kFilterModePoint;
		case FilterType::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:			return Gnm::kFilterModePoint;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:			return Gnm::kFilterModeBilinear;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return Gnm::kFilterModeBilinear;
		case FilterType::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:			return Gnm::kFilterModeBilinear;
		case FilterType::MAXIMUM_MIN_MAG_MIP_LINEAR:				return Gnm::kFilterModeBilinear;
		case FilterType::MAXIMUM_ANISOTROPIC:						return Gnm::kFilterModeAnisoBilinear;
		default:													throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::MipFilterMode ConvertMipFilterMode(FilterType filter_type)
	{
		switch (filter_type)
		{
		case FilterType::MIN_MAG_MIP_POINT:							return Gnm::kMipFilterModePoint;
		case FilterType::MIN_MAG_POINT_MIP_LINEAR:					return Gnm::kMipFilterModeLinear;
		case FilterType::MIN_POINT_MAG_LINEAR_MIP_POINT:			return Gnm::kMipFilterModePoint;
		case FilterType::MIN_POINT_MAG_MIP_LINEAR:					return Gnm::kMipFilterModeLinear;
		case FilterType::MIN_LINEAR_MAG_MIP_POINT:					return Gnm::kMipFilterModePoint;
		case FilterType::MIN_LINEAR_MAG_POINT_MIP_LINEAR:			return Gnm::kMipFilterModeLinear;
		case FilterType::MIN_MAG_LINEAR_MIP_POINT:					return Gnm::kMipFilterModePoint;
		case FilterType::MIN_MAG_MIP_LINEAR:						return Gnm::kMipFilterModeLinear;
		case FilterType::ANISOTROPIC:								return Gnm::kMipFilterModeLinear;
		case FilterType::COMPARISON_MIN_MAG_MIP_POINT:				return Gnm::kMipFilterModePoint;
		case FilterType::COMPARISON_MIN_MAG_POINT_MIP_LINEAR:		return Gnm::kMipFilterModeLinear;
		case FilterType::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:	return Gnm::kMipFilterModePoint;
		case FilterType::COMPARISON_MIN_POINT_MAG_MIP_LINEAR:		return Gnm::kMipFilterModeLinear;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_MIP_POINT:		return Gnm::kMipFilterModePoint;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:return Gnm::kMipFilterModeLinear;
		case FilterType::COMPARISON_MIN_MAG_LINEAR_MIP_POINT:		return Gnm::kMipFilterModePoint;
		case FilterType::COMPARISON_MIN_MAG_MIP_LINEAR:				return Gnm::kMipFilterModeLinear;
		case FilterType::COMPARISON_ANISOTROPIC:					return Gnm::kMipFilterModeLinear;
		case FilterType::MINIMUM_MIN_MAG_MIP_POINT:					return Gnm::kMipFilterModePoint;
		case FilterType::MINIMUM_MIN_MAG_POINT_MIP_LINEAR:			return Gnm::kMipFilterModeLinear;
		case FilterType::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return Gnm::kMipFilterModePoint;
		case FilterType::MINIMUM_MIN_POINT_MAG_MIP_LINEAR:			return Gnm::kMipFilterModeLinear;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_MIP_POINT:			return Gnm::kMipFilterModePoint;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return Gnm::kMipFilterModeLinear;
		case FilterType::MINIMUM_MIN_MAG_LINEAR_MIP_POINT:			return Gnm::kMipFilterModePoint;
		case FilterType::MINIMUM_MIN_MAG_MIP_LINEAR:				return Gnm::kMipFilterModeLinear;
		case FilterType::MINIMUM_ANISOTROPIC:						return Gnm::kMipFilterModeLinear;
		case FilterType::MAXIMUM_MIN_MAG_MIP_POINT:					return Gnm::kMipFilterModePoint;
		case FilterType::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:			return Gnm::kMipFilterModeLinear;
		case FilterType::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return Gnm::kMipFilterModePoint;
		case FilterType::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:			return Gnm::kMipFilterModeLinear;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:			return Gnm::kMipFilterModePoint;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return Gnm::kMipFilterModeLinear;
		case FilterType::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:			return Gnm::kMipFilterModePoint;
		case FilterType::MAXIMUM_MIN_MAG_MIP_LINEAR:				return Gnm::kMipFilterModeLinear;
		case FilterType::MAXIMUM_ANISOTROPIC:						return Gnm::kMipFilterModeLinear;
		default:													throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::ZFilterMode ConvertZFilterMode(FilterType filter_type) // Based on MAG filter.
	{
		switch (filter_type)
		{
		case FilterType::MIN_MAG_MIP_POINT:							return Gnm::kZFilterModePoint;
		case FilterType::MIN_MAG_POINT_MIP_LINEAR:					return Gnm::kZFilterModePoint;
		case FilterType::MIN_POINT_MAG_LINEAR_MIP_POINT:			return Gnm::kZFilterModeLinear;
		case FilterType::MIN_POINT_MAG_MIP_LINEAR:					return Gnm::kZFilterModeLinear;
		case FilterType::MIN_LINEAR_MAG_MIP_POINT:					return Gnm::kZFilterModePoint;
		case FilterType::MIN_LINEAR_MAG_POINT_MIP_LINEAR:			return Gnm::kZFilterModePoint;
		case FilterType::MIN_MAG_LINEAR_MIP_POINT:					return Gnm::kZFilterModeLinear;
		case FilterType::MIN_MAG_MIP_LINEAR:						return Gnm::kZFilterModeLinear;
		case FilterType::ANISOTROPIC:								return Gnm::kZFilterModeLinear;
		case FilterType::COMPARISON_MIN_MAG_MIP_POINT:				return Gnm::kZFilterModePoint;
		case FilterType::COMPARISON_MIN_MAG_POINT_MIP_LINEAR:		return Gnm::kZFilterModePoint;
		case FilterType::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:	return Gnm::kZFilterModeLinear;
		case FilterType::COMPARISON_MIN_POINT_MAG_MIP_LINEAR:		return Gnm::kZFilterModeLinear;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_MIP_POINT:		return Gnm::kZFilterModePoint;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:return Gnm::kZFilterModePoint;
		case FilterType::COMPARISON_MIN_MAG_LINEAR_MIP_POINT:		return Gnm::kZFilterModeLinear;
		case FilterType::COMPARISON_MIN_MAG_MIP_LINEAR:				return Gnm::kZFilterModeLinear;
		case FilterType::COMPARISON_ANISOTROPIC:					return Gnm::kZFilterModeLinear;
		case FilterType::MINIMUM_MIN_MAG_MIP_POINT:					return Gnm::kZFilterModePoint;
		case FilterType::MINIMUM_MIN_MAG_POINT_MIP_LINEAR:			return Gnm::kZFilterModePoint;
		case FilterType::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return Gnm::kZFilterModeLinear;
		case FilterType::MINIMUM_MIN_POINT_MAG_MIP_LINEAR:			return Gnm::kZFilterModeLinear;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_MIP_POINT:			return Gnm::kZFilterModePoint;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return Gnm::kZFilterModePoint;
		case FilterType::MINIMUM_MIN_MAG_LINEAR_MIP_POINT:			return Gnm::kZFilterModeLinear;
		case FilterType::MINIMUM_MIN_MAG_MIP_LINEAR:				return Gnm::kZFilterModeLinear;
		case FilterType::MINIMUM_ANISOTROPIC:						return Gnm::kZFilterModeLinear;
		case FilterType::MAXIMUM_MIN_MAG_MIP_POINT:					return Gnm::kZFilterModePoint;
		case FilterType::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:			return Gnm::kZFilterModePoint;
		case FilterType::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return Gnm::kZFilterModeLinear;
		case FilterType::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:			return Gnm::kZFilterModeLinear;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:			return Gnm::kZFilterModePoint;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return Gnm::kZFilterModePoint;
		case FilterType::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:			return Gnm::kZFilterModeLinear;
		case FilterType::MAXIMUM_MIN_MAG_MIP_LINEAR:				return Gnm::kZFilterModeLinear;
		case FilterType::MAXIMUM_ANISOTROPIC:						return Gnm::kZFilterModeLinear;
		default:													throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::AnisotropyRatio ConvertAnisotropyRatio(uint32_t max_anisotropy)
	{
		switch (max_anisotropy)
		{
		case 2: return Gnm::AnisotropyRatio::kAnisotropyRatio2;
		case 4: return Gnm::AnisotropyRatio::kAnisotropyRatio4;
		case 8: return Gnm::AnisotropyRatio::kAnisotropyRatio8;
		case 16: return Gnm::AnisotropyRatio::kAnisotropyRatio16;
		default: return Gnm::AnisotropyRatio::kAnisotropyRatio1;
		}
	}

	struct SamplerGNMX : public Sampler
	{
		Gnm::Sampler sampler;

		SamplerGNMX(const Memory::DebugStringA<>& name, IDevice* device, const SamplerInfo& info);
	};

	SamplerGNMX::SamplerGNMX(const Memory::DebugStringA<>& name, IDevice* device, const SamplerInfo& info)
		: Sampler(name)
	{
		sampler.init();
		sampler.setBorderColor(Gnm::BorderColor::kBorderColorTransBlack);
		sampler.setDepthCompareFunction(ConvertComparisonFunc(info.comparison_func));
		sampler.setWrapMode(ConvertTextureAddress(info.address_u), ConvertTextureAddress(info.address_v), ConvertTextureAddress(info.address_w));
		sampler.setXyFilterMode(ConvertMagFilterMode(info.filter_type), ConvertMinFilterMode(info.filter_type));
		sampler.setMipFilterMode(ConvertMipFilterMode(info.filter_type));
		sampler.setZFilterMode(ConvertZFilterMode(info.filter_type));
		sampler.setLodBias(Gnmx::convertF32ToS6_8(info.lod_bias), Gnmx::convertF32ToS2_4(0.0f));
		sampler.setAnisotropyRatio(ConvertAnisotropyRatio(info.anisotropy_level));
	}

}
