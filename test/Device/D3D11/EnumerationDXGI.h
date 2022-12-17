#pragma once

#if !defined(_XBOX_ONE)

namespace Device
{

	#define DXGI_MAX_DEVICE_IDENTIFIER_STRING 128


	class DXGIEnumOutputInfo
	{
	public:
		DXGIEnumOutputInfo();
		~DXGIEnumOutputInfo();

		UINT AdapterOrdinal;
		UINT Output;
		IDXGIOutput* m_pOutput;
		DXGI_OUTPUT_DESC Desc;

		std::vector<DXGI_MODE_DESC > displayModeList; // Array of supported DisplayModes
	};


	class DXGIEnumDeviceInfo
	///
	/// A class describing a Direct3D11 device that contains a 
	/// unique supported driver type
	///
	{
	public:
		DXGIEnumDeviceInfo();
		~DXGIEnumDeviceInfo();

		UINT AdapterOrdinal;
		D3D_DRIVER_TYPE DeviceType;
		D3D_FEATURE_LEVEL SelectedLevel;
		D3D_FEATURE_LEVEL MaxLevel;
		BOOL ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x;
	};

	
	class DXGIEnumAdapterInfo
	///
	/// A class describing an adapter which contains a unique adapter ordinal 
	/// that is installed on the system
	///
	{
	public:
		DXGIEnumAdapterInfo();
		~DXGIEnumAdapterInfo();

		UINT AdapterOrdinal;
		DXGI_ADAPTER_DESC AdapterDesc;
		WCHAR szUniqueDescription[DXGI_MAX_DEVICE_IDENTIFIER_STRING];
		IDXGIAdapter *m_pAdapter;
		bool bAdapterUnavailable;

		std::vector< std::unique_ptr< DXGIEnumOutputInfo > > outputInfoList;
		std::vector< std::unique_ptr< DXGIEnumDeviceInfo > > deviceInfoList;
		
		//// List of DXGIEnumDeviceSettingsCombo* with a unique set of BackBufferFormat, and Windowed
		//std::vector<DXGIEnumDeviceSettingsCombo*> deviceSettingsComboList;
	};


	class EnumerationDXGI
	///
	/// Enumerates available Direct3D11 adapters, devices, modes, etc.
	///
	{
	private:
		bool m_bHasEnumerated;
		std::vector<DXGI_FORMAT> m_DepthStencilPossibleList;
		UINT m_nMinWidth;
		UINT m_nMaxWidth;
		UINT m_nMinHeight;
		UINT m_nMaxHeight;
		UINT m_nRefreshMin;
		UINT m_nRefreshMax;
		UINT m_nMultisampleQualityMax;
		bool m_bEnumerateAllAdapterFormats;
		D3D_FEATURE_LEVEL g_forceFL;
		std::vector< std::unique_ptr< DXGIEnumAdapterInfo > > m_AdapterInfoList;

	public:
		EnumerationDXGI();
		~EnumerationDXGI();

		void    ResetPossibleDepthStencilFormats();
		void    SetEnumerateAllAdapterFormats(bool bEnumerateAllAdapterFormats);
		HRESULT Enumerate();

		bool HasEnumerated() const;

		const std::vector< std::unique_ptr< DXGIEnumAdapterInfo > >* GetAdapterInfoList() const;
		DXGIEnumAdapterInfo* GetAdapterInfo(UINT AdapterOrdinal) const;
		DXGIEnumDeviceInfo* GetDeviceInfo(UINT AdapterOrdinal, D3D_DRIVER_TYPE DeviceType) const;
		DXGIEnumOutputInfo* GetOutputInfo(UINT AdapterOrdinal, UINT Output) const;
		UINT GetOutputInfoSize(UINT AdapterOrdinal) const;

	private:
		HRESULT EnumerateOutputs(DXGIEnumAdapterInfo *pAdapterInfo);
		HRESULT EnumerateDevices(DXGIEnumAdapterInfo *pAdapterInfo);
		HRESULT EnumerateDisplayModes(DXGIEnumOutputInfo *pOutputInfo);

		void ClearAdapterInfoList();
	};

}

#endif
