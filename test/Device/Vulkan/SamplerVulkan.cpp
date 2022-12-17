
namespace Device
{

	vk::Filter ConvertMagFilterModeVulkan(FilterType filter_type)
	{
		switch (filter_type)
		{
		case FilterType::MIN_MAG_MIP_POINT:							return vk::Filter::eNearest;
		case FilterType::MIN_MAG_POINT_MIP_LINEAR:					return vk::Filter::eNearest;
		case FilterType::MIN_POINT_MAG_LINEAR_MIP_POINT:			return vk::Filter::eLinear;
		case FilterType::MIN_POINT_MAG_MIP_LINEAR:					return vk::Filter::eLinear;
		case FilterType::MIN_LINEAR_MAG_MIP_POINT:					return vk::Filter::eNearest;
		case FilterType::MIN_LINEAR_MAG_POINT_MIP_LINEAR:			return vk::Filter::eNearest;
		case FilterType::MIN_MAG_LINEAR_MIP_POINT:					return vk::Filter::eLinear;
		case FilterType::MIN_MAG_MIP_LINEAR:						return vk::Filter::eLinear;
		case FilterType::ANISOTROPIC:								return vk::Filter::eLinear;
		case FilterType::COMPARISON_MIN_MAG_MIP_POINT:				return vk::Filter::eNearest;
		case FilterType::COMPARISON_MIN_MAG_POINT_MIP_LINEAR:		return vk::Filter::eNearest;
		case FilterType::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:	return vk::Filter::eLinear;
		case FilterType::COMPARISON_MIN_POINT_MAG_MIP_LINEAR:		return vk::Filter::eLinear;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_MIP_POINT:		return vk::Filter::eNearest;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:return vk::Filter::eNearest;
		case FilterType::COMPARISON_MIN_MAG_LINEAR_MIP_POINT:		return vk::Filter::eLinear;
		case FilterType::COMPARISON_MIN_MAG_MIP_LINEAR:				return vk::Filter::eLinear;
		case FilterType::COMPARISON_ANISOTROPIC:					return vk::Filter::eLinear;
		case FilterType::MINIMUM_MIN_MAG_MIP_POINT:					return vk::Filter::eNearest;
		case FilterType::MINIMUM_MIN_MAG_POINT_MIP_LINEAR:			return vk::Filter::eNearest;
		case FilterType::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return vk::Filter::eLinear;
		case FilterType::MINIMUM_MIN_POINT_MAG_MIP_LINEAR:			return vk::Filter::eLinear;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_MIP_POINT:			return vk::Filter::eNearest;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return vk::Filter::eNearest;
		case FilterType::MINIMUM_MIN_MAG_LINEAR_MIP_POINT:			return vk::Filter::eLinear;
		case FilterType::MINIMUM_MIN_MAG_MIP_LINEAR:				return vk::Filter::eLinear;
		case FilterType::MINIMUM_ANISOTROPIC:						return vk::Filter::eLinear;
		case FilterType::MAXIMUM_MIN_MAG_MIP_POINT:					return vk::Filter::eNearest;
		case FilterType::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:			return vk::Filter::eNearest;
		case FilterType::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return vk::Filter::eLinear;
		case FilterType::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:			return vk::Filter::eLinear;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:			return vk::Filter::eNearest;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return vk::Filter::eNearest;
		case FilterType::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:			return vk::Filter::eLinear;
		case FilterType::MAXIMUM_MIN_MAG_MIP_LINEAR:				return vk::Filter::eLinear;
		case FilterType::MAXIMUM_ANISOTROPIC:						return vk::Filter::eLinear;
		default:													throw ExceptionVulkan("Unsupported filter type");
		}
	}

	vk::Filter ConvertMinFilterModeVulkan(FilterType filter_type)
	{
		switch (filter_type)
		{
		case FilterType::MIN_MAG_MIP_POINT:							return vk::Filter::eNearest;
		case FilterType::MIN_MAG_POINT_MIP_LINEAR:					return vk::Filter::eNearest;
		case FilterType::MIN_POINT_MAG_LINEAR_MIP_POINT:			return vk::Filter::eNearest;
		case FilterType::MIN_POINT_MAG_MIP_LINEAR:					return vk::Filter::eNearest;
		case FilterType::MIN_LINEAR_MAG_MIP_POINT:					return vk::Filter::eLinear;
		case FilterType::MIN_LINEAR_MAG_POINT_MIP_LINEAR:			return vk::Filter::eLinear;
		case FilterType::MIN_MAG_LINEAR_MIP_POINT:					return vk::Filter::eLinear;
		case FilterType::MIN_MAG_MIP_LINEAR:						return vk::Filter::eLinear;
		case FilterType::ANISOTROPIC:								return vk::Filter::eLinear;
		case FilterType::COMPARISON_MIN_MAG_MIP_POINT:				return vk::Filter::eNearest;
		case FilterType::COMPARISON_MIN_MAG_POINT_MIP_LINEAR:		return vk::Filter::eNearest;
		case FilterType::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:	return vk::Filter::eNearest;
		case FilterType::COMPARISON_MIN_POINT_MAG_MIP_LINEAR:		return vk::Filter::eNearest;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_MIP_POINT:		return vk::Filter::eLinear;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:return vk::Filter::eLinear;
		case FilterType::COMPARISON_MIN_MAG_LINEAR_MIP_POINT:		return vk::Filter::eLinear;
		case FilterType::COMPARISON_MIN_MAG_MIP_LINEAR:				return vk::Filter::eLinear;
		case FilterType::COMPARISON_ANISOTROPIC:					return vk::Filter::eLinear;
		case FilterType::MINIMUM_MIN_MAG_MIP_POINT:					return vk::Filter::eNearest;
		case FilterType::MINIMUM_MIN_MAG_POINT_MIP_LINEAR:			return vk::Filter::eNearest;
		case FilterType::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return vk::Filter::eNearest;
		case FilterType::MINIMUM_MIN_POINT_MAG_MIP_LINEAR:			return vk::Filter::eNearest;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_MIP_POINT:			return vk::Filter::eLinear;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return vk::Filter::eLinear;
		case FilterType::MINIMUM_MIN_MAG_LINEAR_MIP_POINT:			return vk::Filter::eLinear;
		case FilterType::MINIMUM_MIN_MAG_MIP_LINEAR:				return vk::Filter::eLinear;
		case FilterType::MINIMUM_ANISOTROPIC:						return vk::Filter::eLinear;
		case FilterType::MAXIMUM_MIN_MAG_MIP_POINT:					return vk::Filter::eNearest;
		case FilterType::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:			return vk::Filter::eNearest;
		case FilterType::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return vk::Filter::eNearest;
		case FilterType::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:			return vk::Filter::eNearest;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:			return vk::Filter::eLinear;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return vk::Filter::eLinear;
		case FilterType::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:			return vk::Filter::eLinear;
		case FilterType::MAXIMUM_MIN_MAG_MIP_LINEAR:				return vk::Filter::eLinear;
		case FilterType::MAXIMUM_ANISOTROPIC:						return vk::Filter::eLinear;
		default:													throw ExceptionVulkan("Unsupported filter type");
		}
	}

	vk::SamplerMipmapMode ConvertMipFilterModeVulkan(FilterType filter_type)
	{
		switch (filter_type)
		{
		case FilterType::MIN_MAG_MIP_POINT:							return vk::SamplerMipmapMode::eNearest;
		case FilterType::MIN_MAG_POINT_MIP_LINEAR:					return vk::SamplerMipmapMode::eLinear;
		case FilterType::MIN_POINT_MAG_LINEAR_MIP_POINT:			return vk::SamplerMipmapMode::eNearest;
		case FilterType::MIN_POINT_MAG_MIP_LINEAR:					return vk::SamplerMipmapMode::eLinear;
		case FilterType::MIN_LINEAR_MAG_MIP_POINT:					return vk::SamplerMipmapMode::eNearest;
		case FilterType::MIN_LINEAR_MAG_POINT_MIP_LINEAR:			return vk::SamplerMipmapMode::eLinear;
		case FilterType::MIN_MAG_LINEAR_MIP_POINT:					return vk::SamplerMipmapMode::eNearest;
		case FilterType::MIN_MAG_MIP_LINEAR:						return vk::SamplerMipmapMode::eLinear;
		case FilterType::ANISOTROPIC:								return vk::SamplerMipmapMode::eLinear;
		case FilterType::COMPARISON_MIN_MAG_MIP_POINT:				return vk::SamplerMipmapMode::eNearest;
		case FilterType::COMPARISON_MIN_MAG_POINT_MIP_LINEAR:		return vk::SamplerMipmapMode::eLinear;
		case FilterType::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:	return vk::SamplerMipmapMode::eNearest;
		case FilterType::COMPARISON_MIN_POINT_MAG_MIP_LINEAR:		return vk::SamplerMipmapMode::eLinear;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_MIP_POINT:		return vk::SamplerMipmapMode::eNearest;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:return vk::SamplerMipmapMode::eLinear;
		case FilterType::COMPARISON_MIN_MAG_LINEAR_MIP_POINT:		return vk::SamplerMipmapMode::eNearest;
		case FilterType::COMPARISON_MIN_MAG_MIP_LINEAR:				return vk::SamplerMipmapMode::eLinear;
		case FilterType::COMPARISON_ANISOTROPIC:					return vk::SamplerMipmapMode::eLinear;
		case FilterType::MINIMUM_MIN_MAG_MIP_POINT:					return vk::SamplerMipmapMode::eNearest;
		case FilterType::MINIMUM_MIN_MAG_POINT_MIP_LINEAR:			return vk::SamplerMipmapMode::eLinear;
		case FilterType::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return vk::SamplerMipmapMode::eNearest;
		case FilterType::MINIMUM_MIN_POINT_MAG_MIP_LINEAR:			return vk::SamplerMipmapMode::eLinear;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_MIP_POINT:			return vk::SamplerMipmapMode::eNearest;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return vk::SamplerMipmapMode::eLinear;
		case FilterType::MINIMUM_MIN_MAG_LINEAR_MIP_POINT:			return vk::SamplerMipmapMode::eNearest;
		case FilterType::MINIMUM_MIN_MAG_MIP_LINEAR:				return vk::SamplerMipmapMode::eLinear;
		case FilterType::MINIMUM_ANISOTROPIC:						return vk::SamplerMipmapMode::eLinear;
		case FilterType::MAXIMUM_MIN_MAG_MIP_POINT:					return vk::SamplerMipmapMode::eNearest;
		case FilterType::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:			return vk::SamplerMipmapMode::eLinear;
		case FilterType::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return vk::SamplerMipmapMode::eNearest;
		case FilterType::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:			return vk::SamplerMipmapMode::eLinear;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:			return vk::SamplerMipmapMode::eNearest;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return vk::SamplerMipmapMode::eLinear;
		case FilterType::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:			return vk::SamplerMipmapMode::eNearest;
		case FilterType::MAXIMUM_MIN_MAG_MIP_LINEAR:				return vk::SamplerMipmapMode::eLinear;
		case FilterType::MAXIMUM_ANISOTROPIC:						return vk::SamplerMipmapMode::eLinear;
		default:													throw ExceptionVulkan("Unsupported filter type");
		}
	}

	vk::SamplerAddressMode ConvertTextureAddressVulkan(TextureAddress texture_address)
	{
		switch (texture_address)
		{
		case TextureAddress::WRAP:			return vk::SamplerAddressMode::eRepeat;
		case TextureAddress::MIRROR:		return vk::SamplerAddressMode::eMirroredRepeat;
		case TextureAddress::CLAMP:			return vk::SamplerAddressMode::eClampToEdge;
		case TextureAddress::BORDER:		return vk::SamplerAddressMode::eClampToBorder;
		case TextureAddress::MIRRORONCE:	return vk::SamplerAddressMode::eMirrorClampToEdge;
		default:							throw ExceptionVulkan("Unsupported texture address");
		}
	}

	vk::CompareOp ConvertComparisonFuncVulkan(ComparisonFunc comparison_func)
	{
		switch (comparison_func)
		{
		case ComparisonFunc::NEVER:			return vk::CompareOp::eNever;
		case ComparisonFunc::LESS:			return vk::CompareOp::eLess;
		case ComparisonFunc::EQUAL:			return vk::CompareOp::eEqual;
		case ComparisonFunc::LESS_EQUAL:	return vk::CompareOp::eLessOrEqual;
		case ComparisonFunc::GREATER:		return vk::CompareOp::eGreater;
		case ComparisonFunc::NOT_EQUAL:		return vk::CompareOp::eNotEqual;
		case ComparisonFunc::GREATER_EQUAL: return vk::CompareOp::eGreaterOrEqual;
		case ComparisonFunc::ALWAYS:		return vk::CompareOp::eAlways;
		default:							throw ExceptionVulkan("Unsupported comparision func");
		}
	}

	struct SamplerVulkan : public Sampler
	{
		vk::UniqueSampler sampler;

		SamplerVulkan(const Memory::DebugStringA<>& name, IDevice* device, const SamplerInfo& info);
	};

	SamplerVulkan::SamplerVulkan(const Memory::DebugStringA<>& name, IDevice* device, const SamplerInfo& info)
		: Sampler(name)
	{
		const auto sampler_info = vk::SamplerCreateInfo()
			.setMinFilter(ConvertMinFilterModeVulkan(info.filter_type))
			.setMagFilter(ConvertMagFilterModeVulkan(info.filter_type))
			.setMipmapMode(ConvertMipFilterModeVulkan(info.filter_type))
			.setAddressModeU(ConvertTextureAddressVulkan(info.address_u))
			.setAddressModeV(ConvertTextureAddressVulkan(info.address_v))
			.setAddressModeW(ConvertTextureAddressVulkan(info.address_w))
			.setMipLodBias(info.lod_bias)
			.setMinLod(0.0f)
			.setMaxLod(std::numeric_limits<float>::infinity())
			.setCompareEnable(info.comparison_func != ComparisonFunc::ALWAYS)
			.setMaxAnisotropy((float)info.anisotropy_level)
			.setAnisotropyEnable(info.anisotropy_level > 1)
			.setCompareOp(ConvertComparisonFuncVulkan(info.comparison_func));
		sampler = static_cast<IDeviceVulkan*>(device)->GetDevice().createSamplerUnique(sampler_info);
		static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)sampler->operator VkSampler(), VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, "Sampler");
	}

}
