#pragma once

#include "Visual/Device/Constants.h"

namespace simd
{
	class vector2_int;
}

namespace Device
{
	class IDevice;

	enum class PixelFormat : unsigned;

	enum class DeviceType : unsigned
	{
		HAL,
		REF,
		SW,
		NULLREF,
	};

	struct DisplayMode
	{
		UINT Width;
		UINT Height;
		UINT RefreshRate;
		PixelFormat Format;
	};

	struct DeviceCaps
	{
		DeviceType DeviceType;
		UINT AdapterOrdinal;
		DWORD MaxAnisotropy;
	};

	struct MonitorInfo
	{
		DWORD cbSize;
		Rect rcMonitor;
		Rect rcWork;
		DWORD dwFlags;
	};

	struct AdapterInfo
	{
		unsigned int index;
		std::wstring display_name;
		std::wstring unique_name;
		uint8_t luid[8];
	};

	class DeviceInfo
	{
	public:
		virtual ~DeviceInfo() {};

		static std::unique_ptr<DeviceInfo> Create();

		virtual HRESULT Enumerate() = 0;

		virtual std::vector< AdapterInfo >& GetAdapterInfoList() = 0;
		virtual UINT GetAdapterOrdinalByName(const std::wstring& adapter_name) = 0;
		virtual std::wstring GetAdapterNameByOrdinal(UINT adapter_ordinal) = 0;
		virtual HRESULT GetAdapterDisplayMode(UINT Adapter, UINT Output, DisplayMode* pMode) = 0;

		virtual UINT GetAdapterIndex(UINT Adapter) = 0;
		virtual HMONITOR GetAdapterMonitor(UINT Adapter, UINT Output) = 0;
		virtual std::wstring GetAdapterMonitorName( UINT Adapter, UINT Output ) = 0;
		virtual UINT GetCurrentAdapterMonitorIndex(UINT Adapter) = 0;
		virtual UINT GetCurrentAdapterOrdinal(UINT Adapter) = 0;

		virtual simd::vector2_int GetBestMatchingResolution(UINT adapter_ordinal, PixelFormat pixel_format, UINT width, UINT height) = 0;
		virtual std::vector<simd::vector2_int> DetermineSupportedResolutions() = 0;

		static inline bool ForceLockableResource = false;
	};
}
