#pragma once

namespace Device
{

	extern DXGI_FORMAT SWAP_CHAIN_FORMAT;

	class DeviceInfoDXGI : public DeviceInfo
	{
	public:
		DeviceInfoDXGI();
		virtual ~DeviceInfoDXGI();

		virtual HRESULT Enumerate() final;

		virtual std::vector< AdapterInfo >& GetAdapterInfoList() final;
		virtual UINT GetAdapterOrdinalByName(const std::wstring& adapter_name) final;
		virtual std::wstring GetAdapterNameByOrdinal(UINT adapter_ordinal) final;
		virtual HRESULT GetAdapterDisplayMode(UINT Adapter, UINT Output, DisplayMode* pMode) final;

		virtual UINT GetAdapterIndex(UINT Adapter) final;
		virtual HMONITOR GetAdapterMonitor(UINT Adapter, UINT Output) final;
		virtual std::wstring GetAdapterMonitorName( UINT Adapter, UINT Output ) final;
		virtual UINT GetCurrentAdapterMonitorIndex(UINT Adapter) final;
		virtual UINT GetCurrentAdapterOrdinal(UINT Adapter) final;

		virtual simd::vector2_int GetBestMatchingResolution(UINT adapter_ordinal, PixelFormat pixel_format, UINT width, UINT height) final;
		virtual std::vector<simd::vector2_int> DetermineSupportedResolutions() final;

		UINT GetAdapterModeCount(UINT Adapter, UINT Output, PixelFormat Format);

#if !defined(_XBOX_ONE)
		EnumerationDXGI* GetEnumeration() const { return _enumeration.get(); }

		std::unique_ptr<IDXGIFactory1, Utility::ComReleaser> _factory;
		std::unique_ptr<EnumerationDXGI> _enumeration;
		std::vector<AdapterInfo> unique_adapter_info_list;
#endif
		std::vector<AdapterInfo> adapter_info_list;
	};

	DeviceInfoDXGI::DeviceInfoDXGI()
	{
#if !defined(_XBOX_ONE)
		const auto hr = CreateDXGIFactory1(IID_GRAPHICS_PPV_ARGS(&_factory));
		if (FAILED(hr))
			throw ExceptionD3D11("CreateDXGIFactory1", hr);

		_enumeration = std::make_unique<EnumerationDXGI>();
#endif

		Enumerate();
	}

	DeviceInfoDXGI::~DeviceInfoDXGI()
	{
#if !defined(_XBOX_ONE)
		_factory.reset();
		_enumeration.reset();
#endif
	}

	UINT DeviceInfoDXGI::GetAdapterModeCount(UINT Adapter, UINT Output, PixelFormat Format)
	{
#if defined(_XBOX_ONE)
		return 0;
#else
		UINT count = _enumeration->GetOutputInfoSize(Adapter);

		assert(Output < count);

		DXGIEnumOutputInfo* output_info = _enumeration->GetOutputInfo(Adapter, Output);
		IDXGIOutput* output = output_info->m_pOutput;

		UINT mode_count = 0;
		output->GetDisplayModeList(ConvertPixelFormat(Format, false), 0, &mode_count, nullptr);

		return mode_count;
#endif
	}

	HRESULT DeviceInfoDXGI::Enumerate()
	{
#if defined(_XBOX_ONE)
		return S_OK;
#else
		return _enumeration->Enumerate();
#endif
	}
	
	std::wstring DeviceInfoDXGI::GetAdapterNameByOrdinal(UINT adapter_ordinal)
	{
#if defined(_XBOX_ONE)
		return L"default";
#else
		std::wstring adapter_name;
		GetAdapterInfoList();
		for (int i = 0; i < adapter_info_list.size(); ++i)
		{
			if (adapter_ordinal == adapter_info_list.at(i).index)
			{
				return adapter_info_list.at(i).unique_name.c_str();
			}
		}

		assert(false && "unique name not found");
		return adapter_name;
#endif
	}

	std::vector< AdapterInfo >& DeviceInfoDXGI::GetAdapterInfoList()
	{
#if defined(_XBOX_ONE)
		return adapter_info_list;
#else
		adapter_info_list.clear();
		unique_adapter_info_list.clear();

		const auto adapters = GetEnumeration()->GetAdapterInfoList();
		if (adapters != nullptr)
		{
			int adapter_index = 0;
			for (int i = 0; i < adapters->size(); ++i)
			{
				DXGIEnumAdapterInfo* enumerated_info = adapters->at(i).get();
				std::wstring adapter_name = enumerated_info->AdapterDesc.Description;
				const bool is_software_renderer = adapter_name.find(L"Microsoft Basic Render") != std::wstring::npos;
				const bool single_adapter = adapters->size() == 1;
				if (!is_software_renderer || single_adapter)
				{
					for (int j = 0; j < enumerated_info->outputInfoList.size(); ++j)
					{
						AdapterInfo info;
						info.display_name = adapter_name;

						static_assert(sizeof(info.luid) == sizeof(enumerated_info->AdapterDesc.AdapterLuid));
						memcpy(info.luid, &enumerated_info->AdapterDesc.AdapterLuid, sizeof(info.luid));

						std::wstring unique_name = std::wstring(enumerated_info->szUniqueDescription) + L"(#" + std::to_wstring(j) + L")";
						info.unique_name = unique_name;
						info.index = adapter_index; //output
						adapter_info_list.push_back(info);

						if (j == 0)
						{
							info.display_name = adapter_name;
							std::wstring unique_name = adapter_name;
							info.unique_name = unique_name;
							info.index = i; //output
							unique_adapter_info_list.push_back(info);

						}
						adapter_index++;
					}
				}

			}
		}

		return adapter_info_list;
#endif
	}

	UINT DeviceInfoDXGI::GetAdapterOrdinalByName( const std::wstring& adapter_name )
	{
#if defined(_XBOX_ONE)
		return 0;
#else
		assert( _factory );

		// If an adapter name is found in the settings, try to find it in the adapter list and get its ordinal/index.
		UINT adapter_ordinal = 0;
		GetAdapterInfoList();
		for (UINT i = 0; i < adapter_info_list.size(); ++i)
		{
			if (adapter_info_list.at(i).unique_name == adapter_name)
			{
				return adapter_info_list.at(i).index;
			}
		}
		
		return adapter_ordinal;
#endif
	}

	HRESULT DeviceInfoDXGI::GetAdapterDisplayMode( UINT Adapter, UINT Output, DisplayMode* pMode )
	{
#if defined(_XBOX_ONE)
		return S_OK;
#else
		assert(pMode);

		//dx11 does not have the corresponding call as dx9 for current display mode
		//so we are setting the adapter display format to what we use for swap chain
		DXGI_FORMAT format = SWAP_CHAIN_FORMAT;
		pMode->Format = UnconvertPixelFormat(format);

		WindowDX* window = WindowDX::GetWindow();
		if (window)
		{
			MonitorInfo monitor_info;
			monitor_info.cbSize = sizeof(MONITORINFO);
			bool success = window->FetchMonitorInfo(0, &monitor_info);
			if (success)
			{
				pMode->Width = monitor_info.rcMonitor.right;
				pMode->Height = monitor_info.rcMonitor.bottom;
			}
		}

		return S_OK;
#endif
	}

	UINT DeviceInfoDXGI::GetAdapterIndex(UINT Adapter)
	{
#if defined(_XBOX_ONE)
		return 0;
#else
		std::wstring adapter_unique_name = GetAdapterNameByOrdinal(Adapter);
		for (unsigned i = 0; i < unique_adapter_info_list.size(); ++i)
		{
			std::wstring display_name = unique_adapter_info_list.at(i).display_name;
			if (adapter_unique_name.find(display_name) != std::wstring::npos)
			{
				return i;
			}
		}
		return 0;
#endif
	}

	HMONITOR DeviceInfoDXGI::GetAdapterMonitor( UINT Adapter, UINT Output )
	{
#if defined(_XBOX_ONE)
		return 0;
#else
		UINT count = _enumeration->GetOutputInfoSize(Adapter);

		if (count > 0)
		{
			DXGIEnumOutputInfo* output_info = _enumeration->GetOutputInfo(Adapter, Output);
			if (output_info)
			{
				IDXGIOutput* output = output_info->m_pOutput;

				DXGI_OUTPUT_DESC desc;
				output->GetDesc(&desc);

				return desc.Monitor;
			}

			return nullptr;
		}
		return nullptr;
#endif
	}

	std::wstring DeviceInfoDXGI::GetAdapterMonitorName( UINT Adapter, UINT Output )
	{
#if defined(_XBOX_ONE)
		return L"";
#else
		UINT count = _enumeration->GetOutputInfoSize(Adapter);

		if (count > 0)
		{
			DXGIEnumOutputInfo* output_info = _enumeration->GetOutputInfo(Adapter, Output);
			if (output_info)
			{
				IDXGIOutput* output = output_info->m_pOutput;

				DXGI_OUTPUT_DESC desc;
				output->GetDesc(&desc);

				return desc.DeviceName;
			}

			return L"";
		}
		return L"";
#endif
	}

	UINT DeviceInfoDXGI::GetCurrentAdapterMonitorIndex(UINT Adapter)
	{
#if defined(_XBOX_ONE)
		return 0;
#else
		GetAdapterInfoList();

		AdapterInfo& adapter_info = adapter_info_list.at(Adapter);
		std::regex rgx(".*\\(#(.*)\\).*");// .unique_name
		std::smatch match;
		std::string adapter_unique_name = WstringToString(adapter_info.unique_name);
		if (std::regex_search(adapter_unique_name, match, rgx) && match.size() > 1)
		{
			std::string result = match.str(1);

			std::istringstream is(result);
			int output_index = 0;
			is >> output_index;

			if (is.fail())
			{
				return 0;
			}
			else
			{
				return output_index;
			}
		}

		return 0;
#endif
	}

	UINT DeviceInfoDXGI::GetCurrentAdapterOrdinal(UINT Adapter)
	{
#if defined(_XBOX_ONE)
		return 0;
#else
		GetAdapterInfoList();

		//get the graphics card name from the POE adapter list (list of concatenation of adapters and outputs)
		std::wstring adapter_name = L"";
		for (unsigned i = 0; i < adapter_info_list.size(); ++i)
		{
			if (adapter_info_list.at(i).index == Adapter)
			{
				adapter_name = adapter_info_list.at(i).display_name;
				break;
			}
		}

		//build a list of unique adapters (taking out the consideration of outputs), DX11 adapter
		std::vector<std::wstring> unique_adapter_list;
		for (unsigned i = 0; i < adapter_info_list.size(); ++i)
		{
			if (std::find(unique_adapter_list.begin(), unique_adapter_list.end(), adapter_info_list.at(i).display_name) == unique_adapter_list.end())
			{
				unique_adapter_list.push_back(adapter_info_list.at(i).display_name);
			}
		}

		//find the DX11 adapter index
		for (unsigned i = 0; i < unique_adapter_list.size(); ++i)
		{
			if (unique_adapter_list.at(i) == adapter_name)
			{
				return i;
			}
		}

		return 0;
#endif
	}

	simd::vector2_int DeviceInfoDXGI::GetBestMatchingResolution(UINT adapter_ordinal, PixelFormat pixel_format, UINT width, UINT height)
	{
#if defined(_XBOX_ONE)
		return simd::vector2_int(width, height);
#else
		IDXGIAdapter1 *adapter_ptr = nullptr;
		IDXGIOutput* output_ptr = nullptr;
		DXGI_FORMAT format = SWAP_CHAIN_FORMAT;
		UINT flags = DXGI_ENUM_MODES_INTERLACED;

		UINT best_width, best_height = 0;
		float best_mode_delta = 1000000000;

		UINT output = GetCurrentAdapterMonitorIndex(adapter_ordinal);
		UINT adapter = GetCurrentAdapterOrdinal(adapter_ordinal);
		for (unsigned i = 0; _factory->EnumAdapters1(i, &adapter_ptr) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			for (unsigned j = 0; adapter_ptr->EnumOutputs(j, &output_ptr) != DXGI_ERROR_NOT_FOUND; ++j)
			{
				unsigned num = GetAdapterModeCount(adapter, output, pixel_format);
				DXGI_MODE_DESC * desc = new DXGI_MODE_DESC[num];
				auto res = output_ptr->GetDisplayModeList(format, flags, &num, desc);

				for (unsigned k = 0; k < num; ++k)
				{
					float delta = abs((float)desc[k].Width - (float)width) + abs((float)desc[k].Height - (float)height);
					if (delta < best_mode_delta)
					{
						best_mode_delta = delta;
						best_width = desc[k].Width;
						best_height = desc[k].Height;
					}
				}
			}
		}

		return simd::vector2_int(best_width, best_height);
#endif
	}

	static bool SortResolutions(const simd::vector2_int& a, const simd::vector2_int& b)
	{
		return a.x < b.x;
	}

	static bool SameResolutions(const simd::vector2_int& a, const simd::vector2_int& b)
	{
		return (a.x == b.x) && (a.y == b.y);
	}

	std::vector<simd::vector2_int> DeviceInfoDXGI::DetermineSupportedResolutions()
	{
		std::vector<simd::vector2_int> resolutions;

#ifndef _XBOX_ONE
		IDXGIAdapter1 *adapter_ptr = nullptr;
		IDXGIOutput* output_ptr = nullptr;
		DXGI_FORMAT format = SWAP_CHAIN_FORMAT;
		UINT flags = DXGI_ENUM_MODES_INTERLACED;

		for (unsigned i = 0; _factory->EnumAdapters1(i, &adapter_ptr) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			for (unsigned j = 0; adapter_ptr->EnumOutputs(j, &output_ptr) != DXGI_ERROR_NOT_FOUND; ++j)
			{
				unsigned num = 0;
				HRESULT res = output_ptr->GetDisplayModeList(format, flags, &num, 0);

				auto desc = std::make_unique< DXGI_MODE_DESC[] >(num);
				res = output_ptr->GetDisplayModeList(format, flags, &num, desc.get());

				for (unsigned k = 0; k < num; ++k)
				{
					simd::vector2_int new_reso(desc[k].Width, desc[k].Height);
					bool add_it = true;

					if (new_reso.x < 800 || new_reso.y < 600)
						add_it = false;

					for (unsigned l = 0; l < resolutions.size(); l++)
					{
						if (SameResolutions(resolutions[l], new_reso))
						{
							add_it = false;
							break;
						}
					}

					if (add_it)
					{
						resolutions.push_back(new_reso);
					}
				}
			}
		}

		//Sort reso's in order of lowest to highest width
		sort(resolutions.begin(), resolutions.end(), SortResolutions);
#endif
		return resolutions;
	}

}
