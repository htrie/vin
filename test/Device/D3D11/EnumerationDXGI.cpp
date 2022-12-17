
#if !defined(_XBOX_ONE)

namespace Device
{

DXGIEnumOutputInfo::DXGIEnumOutputInfo() 
	: AdapterOrdinal(0)
	, Output(0)
	, m_pOutput(nullptr)
{
	Desc = {};
}

DXGIEnumOutputInfo::~DXGIEnumOutputInfo() 
{
	SAFE_RELEASE( m_pOutput );
	displayModeList.clear();
}

DXGIEnumDeviceInfo::DXGIEnumDeviceInfo() 
	: AdapterOrdinal(0)
	, DeviceType(D3D_DRIVER_TYPE_UNKNOWN)
	, SelectedLevel((D3D_FEATURE_LEVEL)0)
	, MaxLevel((D3D_FEATURE_LEVEL)0)
	, ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x(false)
{
}

DXGIEnumDeviceInfo::~DXGIEnumDeviceInfo() 
{
}

DXGIEnumAdapterInfo::DXGIEnumAdapterInfo()
	: AdapterOrdinal(0)
	, m_pAdapter(nullptr)
	, bAdapterUnavailable(false)
{
	szUniqueDescription[0] = 0;
	AdapterDesc = {};
}

DXGIEnumAdapterInfo::~DXGIEnumAdapterInfo()
{
	outputInfoList.clear();
	deviceInfoList.clear();

	SAFE_RELEASE( m_pAdapter );
}


EnumerationDXGI::EnumerationDXGI()
{
    m_bHasEnumerated = false;
    //m_IsD3D11DeviceAcceptableFunc = NULL;
    //m_pIsD3D11DeviceAcceptableFuncUserContext = NULL;

    m_nMinWidth = 640;
    m_nMinHeight = 480;
    m_nMaxWidth = UINT_MAX;
    m_nMaxHeight = UINT_MAX;
    m_bEnumerateAllAdapterFormats = false;
	g_forceFL = static_cast<D3D_FEATURE_LEVEL>(0);

    m_nRefreshMin = 0;
    m_nRefreshMax = UINT_MAX;

    ResetPossibleDepthStencilFormats();
}


EnumerationDXGI::~EnumerationDXGI()
{
    ClearAdapterInfoList();
}


void EnumerationDXGI::ResetPossibleDepthStencilFormats()
{
	m_DepthStencilPossibleList.clear();
	m_DepthStencilPossibleList.push_back( DXGI_FORMAT_D32_FLOAT_S8X24_UINT );
	m_DepthStencilPossibleList.push_back( DXGI_FORMAT_D32_FLOAT );
	m_DepthStencilPossibleList.push_back( DXGI_FORMAT_D24_UNORM_S8_UINT );
	m_DepthStencilPossibleList.push_back( DXGI_FORMAT_D16_UNORM );
}

void EnumerationDXGI::SetEnumerateAllAdapterFormats( bool bEnumerateAllAdapterFormats )
{
	m_bEnumerateAllAdapterFormats = bEnumerateAllAdapterFormats;
}


HRESULT EnumerationDXGI::Enumerate()
//
// Enumerate for each adapter all of the supported display modes, 
// device types, adapter formats, back buffer formats, window/full screen support, 
// depth stencil formats, multisampling types/qualities, and presentations intervals.
//
// For each combination of device type (HAL/REF), adapter format, back buffer format, and
// IsWindowed it will call the app's ConfirmDevice callback.  This allows the app
// to reject or allow that combination based on its caps/etc.  It also allows the 
// app to change the BehaviorFlags.  The BehaviorFlags defaults non-pure HWVP 
// if supported otherwise it will default to SWVP, however the app can change this 
// through the ConfirmDevice callback.
//
{
	HRESULT hr;
    IDXGIFactory1* pFactory;
	CreateDXGIFactory1( __uuidof( IDXGIFactory1 ), ( LPVOID* )&pFactory );
    if( pFactory == NULL )
        return E_FAIL;

    m_bHasEnumerated = true;

    ClearAdapterInfoList();

	UINT index = 0;
	IDXGIAdapter* pAdapter = NULL;
 	while( true )
    {
		auto result = pFactory->EnumAdapters(index, &pAdapter);
		if( result == DXGI_ERROR_NOT_FOUND ) //No more devices to enumerate
			break;

		if( result != S_OK )
			throw std::runtime_error("Failed to EnumAdapters");

        auto pAdapterInfo = std::make_unique< DXGIEnumAdapterInfo >();
        if( pAdapterInfo.get() == nullptr )
        {
            SAFE_RELEASE( pAdapter );
            return E_OUTOFMEMORY;
        }
        pAdapterInfo->AdapterOrdinal = index;
        pAdapter->GetDesc( &pAdapterInfo->AdapterDesc );
        pAdapterInfo->m_pAdapter = pAdapter;

		LOG_INFO(L"Enumerated adapter: " << pAdapterInfo->AdapterDesc.Description);
 
        // Enumerate the device driver types on the adapter.
        hr = EnumerateDevices( pAdapterInfo.get() );
        if( FAILED( hr ) )
        {
			++index;
            continue;
        }
 
        hr = EnumerateOutputs( pAdapterInfo.get() );
        if( FAILED( hr ) || pAdapterInfo->outputInfoList.size() <= 0 )
        {
			++index;
            continue;
        }
 
        m_AdapterInfoList.push_back( std::move(pAdapterInfo) );
        ++index;
    }

    //  If we did not get an adapter then we should still enumerate WARP and Ref.
    if (m_AdapterInfoList.size() == 0) 
	{
		auto pAdapterInfo = std::make_unique< DXGIEnumAdapterInfo >();
		if( pAdapterInfo.get() == nullptr )
        {
            return E_OUTOFMEMORY;
        }
        pAdapterInfo->bAdapterUnavailable = true;

        hr = EnumerateDevices( pAdapterInfo.get() );
        if( !FAILED(hr) )
		{
			m_AdapterInfoList.push_back( std::move(pAdapterInfo) );
		}
    }

    //
    // Check for 2 or more adapters with the same name. Append the name
    // with some instance number if that's the case to help distinguish
    // them.
    //
    bool bUniqueDesc = true;
    DXGIEnumAdapterInfo* pAdapterInfo;
    for( int i = 0; i < m_AdapterInfoList.size(); i++ )
    {
        DXGIEnumAdapterInfo* pAdapterInfo1 = m_AdapterInfoList.at( i ).get();

        for( int j = i + 1; j < m_AdapterInfoList.size(); j++ )
        {
            DXGIEnumAdapterInfo* pAdapterInfo2 = m_AdapterInfoList.at( j ).get();
            if( wcsncmp( pAdapterInfo1->AdapterDesc.Description, pAdapterInfo2->AdapterDesc.Description, DXGI_MAX_DEVICE_IDENTIFIER_STRING ) == 0 )
            {
                bUniqueDesc = false;
                break;
            }
        }

        if( !bUniqueDesc )
            break;
    }

    for( int i = 0; i < m_AdapterInfoList.size(); i++ )
    {
        pAdapterInfo = m_AdapterInfoList.at( i ).get();

        wcscpy_s( pAdapterInfo->szUniqueDescription, 100, pAdapterInfo->AdapterDesc.Description );
        if( !bUniqueDesc )
        {
            WCHAR sz[100];
            swprintf_s( sz, 100, L" (#%d)", pAdapterInfo->AdapterOrdinal );
            wcscat_s( pAdapterInfo->szUniqueDescription, DXGI_MAX_DEVICE_IDENTIFIER_STRING, sz );
        }
    }

    return S_OK;
}


bool EnumerationDXGI::HasEnumerated() const
{ 
	return m_bHasEnumerated;  
}


const std::vector< std::unique_ptr< DXGIEnumAdapterInfo > >* EnumerationDXGI::GetAdapterInfoList() const
{ 
	return &m_AdapterInfoList; 
}


DXGIEnumAdapterInfo* EnumerationDXGI::GetAdapterInfo( UINT AdapterOrdinal ) const
{
	for( int iAdapter = 0; iAdapter < m_AdapterInfoList.size(); iAdapter++ )
	{
		DXGIEnumAdapterInfo* pAdapterInfo = m_AdapterInfoList.at( iAdapter ).get();
		if( pAdapterInfo->AdapterOrdinal == AdapterOrdinal )
		{
			return pAdapterInfo;
		}
	}

	return nullptr;
}


DXGIEnumDeviceInfo* EnumerationDXGI::GetDeviceInfo( UINT AdapterOrdinal, D3D_DRIVER_TYPE DeviceType ) const
{
	DXGIEnumAdapterInfo* pAdapterInfo = GetAdapterInfo( AdapterOrdinal );
	if( pAdapterInfo )
	{
		for( int iDeviceInfo = 0; iDeviceInfo < pAdapterInfo->deviceInfoList.size(); iDeviceInfo++ )
		{
			DXGIEnumDeviceInfo* pDeviceInfo = pAdapterInfo->deviceInfoList.at( iDeviceInfo ).get();
			if( pDeviceInfo->DeviceType == DeviceType )
			{
				return pDeviceInfo;
			}
		}
	}

	return nullptr;
}


DXGIEnumOutputInfo* EnumerationDXGI::GetOutputInfo( UINT AdapterOrdinal, UINT Output ) const
{
	DXGIEnumAdapterInfo* pAdapterInfo = GetAdapterInfo( AdapterOrdinal );
	if( pAdapterInfo && pAdapterInfo->outputInfoList.size() > int( Output ) )
	{
		return pAdapterInfo->outputInfoList.at( Output ).get();
	}

	return nullptr;
}

UINT EnumerationDXGI::GetOutputInfoSize(UINT AdapterOrdinal) const
{
	DXGIEnumAdapterInfo* pAdapterInfo = GetAdapterInfo(AdapterOrdinal);
	if (pAdapterInfo)
	{
		return (UINT)pAdapterInfo->outputInfoList.size();
	}
	return 0;
}


HRESULT EnumerationDXGI::EnumerateOutputs( DXGIEnumAdapterInfo* pAdapterInfo )
{
    IDXGIOutput* pOutput = nullptr;
    UINT i = 0;

	while (true)
	{
		const auto result = pAdapterInfo->m_pAdapter->EnumOutputs(i, &pOutput);
		if( result == DXGI_ERROR_NOT_FOUND )
			break;

		if( result != S_OK )
		{
			LOG_CRIT(L"Failed to enumerate output for adapter: " << pAdapterInfo->AdapterDesc.Description << L" with error code: " << result);
			return result;
		}
			

		auto pOutputInfo = std::make_unique< DXGIEnumOutputInfo >();
		if (!pOutputInfo)
		{
			SAFE_RELEASE(pOutput);
			return E_OUTOFMEMORY;
		}
		pOutput->GetDesc(&pOutputInfo->Desc);
		pOutputInfo->Output = i;
		pOutputInfo->m_pOutput = pOutput;

		EnumerateDisplayModes(pOutputInfo.get());
		if (pOutputInfo->displayModeList.size() <= 0)
		{
			LOG_WARN(L"Output for adapter " << pAdapterInfo->AdapterDesc.Description << L" of " << pOutputInfo->Desc.DeviceName << L" has no display modes. Skipping." );
			// If this output has no valid display mode, do not save it.
			continue;
		}

		LOG_INFO(L"Enumerated output for adapter " << pAdapterInfo->AdapterDesc.Description << L" of " << pOutputInfo->Desc.DeviceName);

		pAdapterInfo->outputInfoList.push_back(std::move(pOutputInfo));
		++i;
	}

	return S_OK;
}


HRESULT EnumerationDXGI::EnumerateDisplayModes( DXGIEnumOutputInfo* pOutputInfo )
{
    HRESULT hr = S_OK;
    DXGI_FORMAT allowedAdapterFormatArray[] =
    {
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,     //This is DXUT's preferred mode

        DXGI_FORMAT_R8G8B8A8_UNORM,			
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R10G10B10A2_UNORM
    };
    int allowedAdapterFormatArrayCount = sizeof( allowedAdapterFormatArray ) / sizeof( allowedAdapterFormatArray[0] );

    // Swap perferred modes for apps running in linear space
    DXGI_FORMAT RemoteMode = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    // The fast path only enumerates R8G8B8A8_UNORM_SRGB modes
    if( !m_bEnumerateAllAdapterFormats )
	{
        allowedAdapterFormatArrayCount = 1;
	}

    for( int f = 0; f < allowedAdapterFormatArrayCount; ++f )
    {
        // Fast-path: Try to grab at least 512 modes.
        //			  This is to avoid calling GetDisplayModeList more times than necessary.
        //			  GetDisplayModeList is an expensive call.
        UINT NumModes = 512;
        DXGI_MODE_DESC* pDesc = new DXGI_MODE_DESC[ NumModes ];
        assert( pDesc );
        if( !pDesc )
		{
            return E_OUTOFMEMORY;
		}

        hr = pOutputInfo->m_pOutput->GetDisplayModeList( allowedAdapterFormatArray[f],
                                                         DXGI_ENUM_MODES_SCALING,
                                                         &NumModes,
                                                         pDesc );
        if( DXGI_ERROR_NOT_FOUND == hr )
        {
            SAFE_DELETE_ARRAY( pDesc );
            NumModes = 0;
            break;
        }
        else if( MAKE_DXGI_HRESULT( 34 ) == hr && RemoteMode == allowedAdapterFormatArray[f] )
        {
            // DXGI cannot enumerate display modes over a remote session.  Therefore, create a fake display
            // mode for the current screen resolution for the remote session.
            if( 0 != GetSystemMetrics( 0x1000 ) ) // SM_REMOTESESSION
            {
                DEVMODE DevMode;
                DevMode.dmSize = sizeof( DEVMODE );
                if( EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, &DevMode ) )
                {
                    NumModes = 1;
                    pDesc[0].Width = DevMode.dmPelsWidth;
                    pDesc[0].Height = DevMode.dmPelsHeight;
                    pDesc[0].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    pDesc[0].RefreshRate.Numerator = 60;
                    pDesc[0].RefreshRate.Denominator = 1;
                    pDesc[0].ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
                    pDesc[0].Scaling = DXGI_MODE_SCALING_CENTERED;
                    hr = S_OK;
                }
            }
        }
        else if( DXGI_ERROR_MORE_DATA == hr )
        {
            // Slow path.  There were more than 512 modes.
            SAFE_DELETE_ARRAY( pDesc );
            hr = pOutputInfo->m_pOutput->GetDisplayModeList( allowedAdapterFormatArray[f],
                                                             DXGI_ENUM_MODES_SCALING,
                                                             &NumModes,
                                                             NULL );
            if( FAILED( hr ) )
            {
                NumModes = 0;
                break;
            }

            pDesc = new DXGI_MODE_DESC[ NumModes ];
            assert( pDesc );
            if( !pDesc )
                return E_OUTOFMEMORY;

            hr = pOutputInfo->m_pOutput->GetDisplayModeList( allowedAdapterFormatArray[f],
                                                             DXGI_ENUM_MODES_SCALING,
                                                             &NumModes,
                                                             pDesc );
            if( FAILED( hr ) )
            {
                SAFE_DELETE_ARRAY( pDesc );
                NumModes = 0;
                break;
            }
        }

        if( 0 == NumModes && 0 == f )
        {
            // No R8G8B8A8_UNORM_SRGB modes!
            // Abort the fast-path if we're on it
            allowedAdapterFormatArrayCount = sizeof( allowedAdapterFormatArray ) / sizeof
                ( allowedAdapterFormatArray[0] );
            SAFE_DELETE_ARRAY( pDesc );
            continue;
        }

        if( SUCCEEDED( hr ) )
        {
            for( UINT m = 0; m < NumModes; m++ )
            {
                pOutputInfo->displayModeList.push_back( pDesc[m] );
            }
        }

        SAFE_DELETE_ARRAY( pDesc );
    }

    return hr;
}


HRESULT EnumerationDXGI::EnumerateDevices( DXGIEnumAdapterInfo* pAdapterInfo )
{
    HRESULT hr;

    const D3D_DRIVER_TYPE devTypeArray[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE
    };

    const UINT devTypeArrayCount = sizeof( devTypeArray ) / sizeof( devTypeArray[0] );

    // Enumerate each Direct3D device type
    //for( UINT iDeviceType = 0; iDeviceType < devTypeArrayCount; iDeviceType++ )
    //{
		auto pDeviceInfo = std::make_unique< DXGIEnumDeviceInfo >();
        if( pDeviceInfo.get() == nullptr )
            return E_OUTOFMEMORY;

        // Fill struct w/ AdapterOrdinal and D3D_DRIVER_TYPE
        pDeviceInfo->AdapterOrdinal = pAdapterInfo->AdapterOrdinal;
        pDeviceInfo->DeviceType = D3D_DRIVER_TYPE_UNKNOWN;

        D3D_FEATURE_LEVEL FeatureLevels[] =
        {
            D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1
        };
        UINT NumFeatureLevels = ARRAYSIZE( FeatureLevels );

        // Call D3D11CreateDevice to ensure that this is a D3D11 device.
        ID3D11Device*        pd3dDevice = nullptr;
        ID3D11DeviceContext* pd3dDeviceContext = nullptr;
        //IDXGIAdapter*        pAdapter = nullptr;

        hr = D3D11CreateDevice( 
			pAdapterInfo->m_pAdapter,
			D3D_DRIVER_TYPE_UNKNOWN,
			( HMODULE )0,
			0,
			FeatureLevels,
			NumFeatureLevels,
			D3D11_SDK_VERSION,
			&pd3dDevice,
			&pDeviceInfo->MaxLevel,
			&pd3dDeviceContext );

		if ( (NumFeatureLevels > 1) && (hr == E_INVALIDARG) )
		{
			// Try again from 11.0.
			hr = D3D11CreateDevice(
				pAdapterInfo->m_pAdapter,
				D3D_DRIVER_TYPE_UNKNOWN,
				( HMODULE )0,
				0,
				&FeatureLevels[1],
				NumFeatureLevels-1,
				D3D11_SDK_VERSION,
				&pd3dDevice,
				&pDeviceInfo->MaxLevel,
				&pd3dDeviceContext );
		}
			
        if( FAILED( hr ) )
        {
			LOG_WARN(L"Failed to create device for adapter: " << pAdapterInfo->AdapterDesc.Description << ". Error code: " << hr );
			return hr;
        }
        
        if (g_forceFL == 0 || g_forceFL == pDeviceInfo->MaxLevel) 
		{ 
            pDeviceInfo->SelectedLevel = pDeviceInfo->MaxLevel;
        }
        else if (g_forceFL > pDeviceInfo->MaxLevel) 
		{
            SAFE_RELEASE( pd3dDevice );
            SAFE_RELEASE( pd3dDeviceContext );        
			LOG_CRIT( L"Forced feature level is not supported" );
			return E_FAIL;
        } 
		else 
		{
            // A device was created with a higher feature level that the user-specified feature level.
            SAFE_RELEASE( pd3dDevice );
            SAFE_RELEASE( pd3dDeviceContext );
            D3D_FEATURE_LEVEL rtFL;
            hr = D3D11CreateDevice( pAdapterInfo->m_pAdapter,
									D3D_DRIVER_TYPE_UNKNOWN,
                                    ( HMODULE )0,
                                    0,
                                    &g_forceFL,
                                    1,
                                    D3D11_SDK_VERSION,
                                    &pd3dDevice,
                                    &rtFL,
                                    &pd3dDeviceContext );

            if( !FAILED( hr ) && rtFL == g_forceFL )
			{    
                pDeviceInfo->SelectedLevel = g_forceFL;
            }
			else 
			{
                SAFE_RELEASE( pd3dDevice );
                SAFE_RELEASE( pd3dDeviceContext );        
				LOG_CRIT( L"Unable to create device at forced feature level" );
				return E_FAIL;
            }
        }

        IDXGIDevice1* pDXGIDev = NULL;
        hr = pd3dDevice->QueryInterface( __uuidof( IDXGIDevice1 ), ( LPVOID* )&pDXGIDev );
        if( SUCCEEDED( hr ) && pDXGIDev )
        {
            SAFE_RELEASE( pAdapterInfo->m_pAdapter );
            pDXGIDev->GetAdapter( &pAdapterInfo->m_pAdapter );
        }
        SAFE_RELEASE( pDXGIDev );

        D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS ho;
        pd3dDevice->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &ho, sizeof(ho));
        pDeviceInfo->ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x = ho.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x; 
        SAFE_RELEASE( pd3dDeviceContext );             
        SAFE_RELEASE( pd3dDevice );


		LOG_INFO(L"Enumerated device for adapter: " << pAdapterInfo->AdapterDesc.Description << ". Selected feature level: " << pDeviceInfo->SelectedLevel << L". Max feature level: " << pDeviceInfo->MaxLevel );

        pAdapterInfo->deviceInfoList.push_back( std::move(pDeviceInfo) );


		
    //}

    return S_OK;
}


void EnumerationDXGI::ClearAdapterInfoList()
{
    m_AdapterInfoList.clear();
}

}

#endif
