
namespace Device
{

	D3D11_COMPARISON_FUNC ConvertComparisonFunc(ComparisonFunc value)
	{
		D3D11_COMPARISON_FUNC func = D3D11_COMPARISON_LESS_EQUAL;
		switch (value)
		{
		case ComparisonFunc::NEVER:			func = D3D11_COMPARISON_NEVER; break;
		case ComparisonFunc::LESS:			func = D3D11_COMPARISON_LESS; break;
		case ComparisonFunc::EQUAL:			func = D3D11_COMPARISON_EQUAL; break;
		case ComparisonFunc::LESS_EQUAL:	func = D3D11_COMPARISON_LESS_EQUAL; break;
		case ComparisonFunc::GREATER:		func = D3D11_COMPARISON_GREATER; break;
		case ComparisonFunc::NOT_EQUAL:		func = D3D11_COMPARISON_NOT_EQUAL; break;
		case ComparisonFunc::GREATER_EQUAL: func = D3D11_COMPARISON_GREATER_EQUAL; break;
		case ComparisonFunc::ALWAYS:		func = D3D11_COMPARISON_ALWAYS; break;
		}
		return func;
	}

	D3D11_TEXTURE_ADDRESS_MODE ConvertTextureAddress(TextureAddress value)
	{
		D3D11_TEXTURE_ADDRESS_MODE mode = D3D11_TEXTURE_ADDRESS_WRAP;
		switch (value)
		{
		case TextureAddress::WRAP:			mode = D3D11_TEXTURE_ADDRESS_WRAP; break;
		case TextureAddress::MIRROR:		mode = D3D11_TEXTURE_ADDRESS_MIRROR; break;
		case TextureAddress::CLAMP:			mode = D3D11_TEXTURE_ADDRESS_CLAMP; break;
		case TextureAddress::BORDER:		mode = D3D11_TEXTURE_ADDRESS_BORDER; break;
		case TextureAddress::MIRRORONCE:	mode = D3D11_TEXTURE_ADDRESS_MIRROR_ONCE; break;
		}
		return mode;
	}

	D3D11_FILTER ConvertFilterType(FilterType value)
	{
		D3D11_FILTER filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		switch (value)
		{
		case FilterType::MIN_MAG_MIP_POINT:							filter = D3D11_FILTER_MIN_MAG_MIP_POINT; break;
		case FilterType::MIN_MAG_POINT_MIP_LINEAR:					filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR; break;
		case FilterType::MIN_POINT_MAG_LINEAR_MIP_POINT:			filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT; break;
		case FilterType::MIN_POINT_MAG_MIP_LINEAR:					filter = D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR; break;
		case FilterType::MIN_LINEAR_MAG_MIP_POINT:					filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT; break;
		case FilterType::MIN_LINEAR_MAG_POINT_MIP_LINEAR:			filter = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR; break;
		case FilterType::MIN_MAG_LINEAR_MIP_POINT:					filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT; break;
		case FilterType::MIN_MAG_MIP_LINEAR:						filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; break;
		case FilterType::ANISOTROPIC:								filter = D3D11_FILTER_ANISOTROPIC; break;
		case FilterType::COMPARISON_MIN_MAG_MIP_POINT:				filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT; break;
		case FilterType::COMPARISON_MIN_MAG_POINT_MIP_LINEAR:		filter = D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR; break;
		case FilterType::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:	filter = D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT; break;
		case FilterType::COMPARISON_MIN_POINT_MAG_MIP_LINEAR:		filter = D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR; break;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_MIP_POINT:		filter = D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT; break;
		case FilterType::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:filter = D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR; break;
		case FilterType::COMPARISON_MIN_MAG_LINEAR_MIP_POINT:		filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; break;
		case FilterType::COMPARISON_MIN_MAG_MIP_LINEAR:				filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; break;
		case FilterType::COMPARISON_ANISOTROPIC:					filter = D3D11_FILTER_COMPARISON_ANISOTROPIC; break;
		case FilterType::MINIMUM_MIN_MAG_MIP_POINT:					filter = D3D11_FILTER_MINIMUM_MIN_MAG_MIP_POINT; break;
		case FilterType::MINIMUM_MIN_MAG_POINT_MIP_LINEAR:			filter = D3D11_FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR; break;
		case FilterType::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	filter = D3D11_FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT; break;
		case FilterType::MINIMUM_MIN_POINT_MAG_MIP_LINEAR:			filter = D3D11_FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR; break;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_MIP_POINT:			filter = D3D11_FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT; break;
		case FilterType::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	filter = D3D11_FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR; break;
		case FilterType::MINIMUM_MIN_MAG_LINEAR_MIP_POINT:			filter = D3D11_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT; break;
		case FilterType::MINIMUM_MIN_MAG_MIP_LINEAR:				filter = D3D11_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR; break;
		case FilterType::MINIMUM_ANISOTROPIC:						filter = D3D11_FILTER_MINIMUM_ANISOTROPIC; break;
		case FilterType::MAXIMUM_MIN_MAG_MIP_POINT:					filter = D3D11_FILTER_MAXIMUM_MIN_MAG_MIP_POINT; break;
		case FilterType::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:			filter = D3D11_FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR; break;
		case FilterType::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	filter = D3D11_FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT; break;
		case FilterType::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:			filter = D3D11_FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR; break;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:			filter = D3D11_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT; break;
		case FilterType::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	filter = D3D11_FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR; break;
		case FilterType::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:			filter = D3D11_FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT; break;
		case FilterType::MAXIMUM_MIN_MAG_MIP_LINEAR:				filter = D3D11_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR; break;
		case FilterType::MAXIMUM_ANISOTROPIC:						filter = D3D11_FILTER_MAXIMUM_ANISOTROPIC; break;
		}
		return filter;
	}

	struct SamplerD3D11 : public Sampler
	{
		std::unique_ptr<ID3D11SamplerState, Utility::GraphicsComReleaser> sampler_state;

		SamplerD3D11(const Memory::DebugStringA<>& name, IDevice* device, const SamplerInfo& info);
	};

	SamplerD3D11::SamplerD3D11(const Memory::DebugStringA<>& name, IDevice* device, const SamplerInfo& info)
		: Sampler(name)
	{
		D3D11_SAMPLER_DESC sampler_desc;
		ZeroMemory(&sampler_desc, sizeof(D3D11_SAMPLER_DESC));
		sampler_desc.Filter = ConvertFilterType(info.filter_type);
		sampler_desc.AddressU = ConvertTextureAddress(info.address_u);
		sampler_desc.AddressV = ConvertTextureAddress(info.address_v);
		sampler_desc.AddressW = ConvertTextureAddress(info.address_w);
		sampler_desc.ComparisonFunc = ConvertComparisonFunc(info.comparison_func);
		sampler_desc.MipLODBias = info.lod_bias;
		sampler_desc.MaxAnisotropy = info.anisotropy_level;
		sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

		ID3D11SamplerState* _sampler_state = nullptr;
		HRESULT hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateSamplerState(&sampler_desc, &_sampler_state);
		assert(SUCCEEDED(hr));

	#if defined(_XBOX_ONE)
		D3D11X_SAMPLER_DESC sampler_descX;
		_sampler_state->GetDescX(&sampler_descX);
		_sampler_state->Release();

		// TriJuicing:
		// Make trilinear sampling cost the same as bilinear would when the sampling occurs close to a MIP map. 
		// The higher the value specified by PerfMip, the more it does bilinear fetches instead of trilinear ones. 
		sampler_descX.PerfMip = 8;

		hr = ((ID3D11DeviceX*)static_cast<IDeviceD3D11*>(device)->GetDevice())->CreateSamplerStateX(&sampler_descX, &_sampler_state);
		assert(SUCCEEDED(hr));
	#endif

		sampler_state.reset(_sampler_state);
	}

}
