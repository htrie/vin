
#if defined(_XBOX_ONE)

namespace Device
{

	static const UINT32 PageMaxCount = 512;
	static const UINT32 PageSize = 64 * 1024;

	XGMemoryLayoutEngine layout_engine;
	XGMemoryLayout* layout = nullptr;
	XGMemoryLayoutMapping mapping;

	UINT32 current_page = 0;


	bool IsDepthFormat(const D3D11_TEXTURE2D_DESC& desc)
	{
		return (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) > 0;
	}

	XG_TILE_MODE GetDepthTileMode(const D3D11_TEXTURE2D_DESC& desc)
	{
		XG_TILE_MODE depth_tile_mode;
		XG_TILE_MODE stencil_tile_mode;
		XGComputeOptimalDepthStencilTileModes(
			(XG_FORMAT&)desc.Format,
			desc.Width,
			desc.Height,
			desc.ArraySize,
			desc.SampleDesc.Count,
			TRUE,
			FALSE,
			FALSE,
			&depth_tile_mode,
			&stencil_tile_mode);
		assert(depth_tile_mode == stencil_tile_mode);
		return depth_tile_mode;
	}

	XG_TILE_MODE GetColorTileMode(const D3D11_TEXTURE2D_DESC& desc)
	{
		XG_TILE_MODE color_tile_mode = XGComputeOptimalTileMode(XG_RESOURCE_DIMENSION_TEXTURE2D,
			(XG_FORMAT&)desc.Format,
			desc.Width,
			desc.Height,
			desc.ArraySize,
			desc.SampleDesc.Count,
			XG_BIND_RENDER_TARGET);
		return color_tile_mode;
	}

	UINT32 GetPageCount(const D3D11_TEXTURE2D_DESC& desc, XG_TILE_MODE tile_mode)
	{
		XG_TEXTURE2D_DESC xg_desc =
		{
			desc.Width,
			desc.Height,
			desc.MipLevels,
			desc.ArraySize,
			(XG_FORMAT&)desc.Format,
			{
				desc.SampleDesc.Count,
				desc.SampleDesc.Quality,
			},
			(XG_USAGE&)desc.Usage,
			desc.BindFlags,
			desc.CPUAccessFlags,
			desc.MiscFlags,
			desc.ESRAMOffsetBytes,
			desc.ESRAMUsageBytes,
			tile_mode,
			0,
		};

		Microsoft::WRL::ComPtr<XGTextureAddressComputer> address_computer;
		auto hr = XGCreateTexture2DComputer(&xg_desc, address_computer.GetAddressOf());
		if (FAILED(hr))
			throw ExceptionD3D11("XGCreateTexture2DComputer", hr);

		XG_RESOURCE_LAYOUT resource_layout;
		hr = address_computer->GetResourceLayout(&resource_layout);
		if (FAILED(hr))
			throw ExceptionD3D11("GetResourceLayout", hr);

		return static_cast<UINT32>(ceil(resource_layout.SizeBytes / static_cast<float>(PageSize)));
	}

	ESRAM::ESRAM()
	{
#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)
		ScopedTimer timer(L"[STARTUP] ESRAM");
#endif
		wchar_t layoutName[128];
		_snwprintf_s(layoutName, _TRUNCATE, L"Layout");

		auto hr = layout_engine.InitializeWithPageCounts(PageMaxCount, PageMaxCount, 0);
		if (FAILED(hr))
			throw ExceptionD3D11("InitializeWithPageCounts", hr);
		hr = layout_engine.CreateMemoryLayout(layoutName, PageSize * PageMaxCount, 0, &layout);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateMemoryLayout", hr);

		hr = layout->CreateMapping(L"Default", &mapping);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateMapping", hr);
		hr = layout->MapSimple(&mapping, XG_ALL_REMAINING_PAGES, XG_ALL_REMAINING_PAGES);
		if (FAILED(hr))
			throw ExceptionD3D11("MapSimple", hr);
	}

	bool ESRAM::CreateTexture2D(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc, ID3D11Texture2D** texture)
	{
		const auto tile_mode = IsDepthFormat(desc) ? GetDepthTileMode(desc) : GetColorTileMode(desc);
		const auto page_count = GetPageCount(desc, tile_mode);

		if ((current_page + page_count) > PageMaxCount)
		{
			LOG_DEBUG(L"[ESRAM] No more space for texture: " << 
				std::to_wstring(desc.Width) << L" x " << std::to_wstring(desc.Height) << 
				L", " << std::to_wstring(page_count) << L" pages" << 
				L" (" << std::to_wstring(PageMaxCount - current_page) << L" pages left)");
			return false;
		}

		void* base_address = (BYTE*)layout->GetBaseAddress() + current_page * PageSize;
		current_page += page_count;

		const auto hr = ((ID3D11DeviceX*)device)->CreatePlacementTexture2D(&desc, tile_mode, 0, base_address, texture);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateShaderResourceView", hr);

		LOG_DEBUG(L"[ESRAM] Placed texture: " << 
				std::to_wstring(desc.Width) << L" x " << std::to_wstring(desc.Height) << L"," << 
				L", " << std::to_wstring(page_count) << L" pages" << 
				L" (" << std::to_wstring(PageMaxCount - current_page) << L" pages left)");
		return true;
	}

}

#endif
