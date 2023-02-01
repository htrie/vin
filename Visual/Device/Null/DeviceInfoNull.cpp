
namespace Device
{

	class DeviceInfoNull : public DeviceInfo
	{
		std::vector<AdapterInfo> adapter_info_list;

	public:
		DeviceInfoNull();
		virtual ~DeviceInfoNull();

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
	};

	DeviceInfoNull::DeviceInfoNull()
	{
	}

	DeviceInfoNull::~DeviceInfoNull()
	{
	}

	HRESULT DeviceInfoNull::Enumerate()
	{
		return S_OK;
	}

	UINT DeviceInfoNull::GetAdapterOrdinalByName(const std::wstring& adapter_name)
	{
		return 0;
	}

	std::wstring DeviceInfoNull::GetAdapterNameByOrdinal(UINT adapter_ordinal)
	{
		std::wstring adapter_name;
		return adapter_name;
	}

	std::vector<AdapterInfo>& DeviceInfoNull::GetAdapterInfoList()
	{
		adapter_info_list.clear();
		return adapter_info_list;
	}

	HRESULT DeviceInfoNull::GetAdapterDisplayMode( UINT Adapter, UINT Output, DisplayMode* pMode )
	{
		return S_OK;
	}

	UINT DeviceInfoNull::GetAdapterIndex(UINT Adapter)
	{
		return 0;
	}

	HMONITOR DeviceInfoNull::GetAdapterMonitor( UINT Adapter, UINT Output )
	{
		return 0;
	}

	std::wstring DeviceInfoNull::GetAdapterMonitorName(UINT Adapter, UINT Output)
	{
		return std::wstring();
	}

	UINT DeviceInfoNull::GetCurrentAdapterMonitorIndex(UINT Adapter)
	{
		return 0;
	}

	UINT DeviceInfoNull::GetCurrentAdapterOrdinal(UINT Adapter)
	{
		//abstraction to work correspondingly with DX11
		return Adapter;
	}

	simd::vector2_int DeviceInfoNull::GetBestMatchingResolution(UINT adapter_ordinal, PixelFormat pixel_format, UINT width, UINT height)
	{
		return simd::vector2_int(width, height);
	}

	std::vector<simd::vector2_int> DeviceInfoNull::DetermineSupportedResolutions()
	{
		return {};
	}

}
