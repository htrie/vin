
#if !defined(_XBOX_ONE) && !defined(PS4) && !defined( __APPLE__ ) && !defined( ANDROID )
	#define SCREENSHOT_TOOL_ENABLED
#endif

#include <assert.h>
#include <iomanip>
#include <sstream>

#include "Common/File/FileSystem.h"
#include "Common/File/PathHelper.h"
#include "Common/Utility/WindowsHeaders.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/FileReader/Vector4dByte.h"
#include "Common/Utility/StringManipulation.h"

#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Device/Constants.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/State.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/Texture.h"

#ifdef __APPLE__
#include "Common/Bridging/Bridge.h"
#endif

#include "ScreenshotTool.h"

namespace
{
	const wchar_t* const directory_name = L"Screenshots";
}

void ScreenshotTool::TakeScreenshot(Device::IDevice* device, Device::RenderTarget* rt, const bool add_postprocess_key, bool want_alpha)
{
	TakeScreenshot(device, rt, GetNextScreenshotName(), add_postprocess_key, want_alpha );
}

void ScreenshotTool::TakeScreenshot(Device::IDevice* device, Device::RenderTarget* rt, const std::wstring& screenshot_path, const bool add_postprocess_key, bool want_alpha)
{
#if defined(SCREENSHOT_TOOL_ENABLED)
	if( rt == nullptr || screenshot_path.empty() )
		return;

	if( !FileSystem::CreateDirectoryChain( PathHelper::ParentPath( screenshot_path ) ) )
		return;  // Failed to create the directory to put the screenshot inside

	try
	{

		Device::SurfaceDesc backbuffer_surface_desc;
		device->GetBackBufferDesc( 0, 0, &backbuffer_surface_desc );
		std::unique_ptr<Device::RenderTarget> shot = Device::RenderTarget::Create( "Screenshot", device, backbuffer_surface_desc.Width, backbuffer_surface_desc.Height, backbuffer_surface_desc.Format, Device::Pool::SYSTEMMEM, true, false );

		rt->CopyToSysMem( shot.get() );

		if( want_alpha && backbuffer_surface_desc.Format == Device::PixelFormat::A8R8G8B8 )
		{
			Device::LockedRect lock;
			shot->LockRect( &lock, Device::Lock::DEFAULT );
			for( unsigned int x = 0; x < backbuffer_surface_desc.Width; x++ )
				for( unsigned int y = 0; y < backbuffer_surface_desc.Height; y++ )
				{
					Vector4dByte& color = *(Vector4dByte*)( (char*)lock.pBits + y * lock.Pitch + x * 4 );
					if( !color.w )color.w = std::max( std::max( color.x, color.y ), color.z ); // if alpha is not available then set it from luminance (needed for bloom effect)
					if( color.w )color.setClamp( color.x * 255 / color.w, color.y * 255 / color.w, color.z * 255 / color.w, color.w );
				}
			shot->UnlockRect();
		}
		else if( add_postprocess_key )
		{
			LONG start_x = 16;
			LONG start_y = 16;
			Device::LockedRect lock;
			shot->LockRect( &lock, Device::Lock::DEFAULT );
			unsigned char* data = (unsigned char*)lock.pBits + lock.Pitch * start_y + start_x * 4;

			for( unsigned z_in = 0; z_in < 16; ++z_in )
			{
				const unsigned x_out_base = ( z_in % 4 ) * 16;
				const unsigned y_out_base = ( z_in / 4 ) * 16;

				for( unsigned y_in = 0; y_in < 16; ++y_in )
				{
					const auto y = y_out_base + y_in;
					auto* const row = (DWORD*)( data + lock.Pitch * y );
					for( unsigned x_in = 0; x_in < 16; ++x_in )
					{
						const auto x = x_out_base + x_in;
						DWORD colour;
						switch( Device::IDevice::GetType() )
						{
						case Device::Type::DirectX11:colour = simd::color::xrgb( 16 * z_in, 16 * y_in, 16 * x_in ).c(); break;
						default: throw std::runtime_error( "Unsupported" );
						}
						*( row + x ) = colour;
					}
				}
			}

			shot->UnlockRect();
		}

		shot->SaveToFile( screenshot_path.c_str(), Device::ImageFileFormat::PNG, NULL, want_alpha );
	}
	catch( const std::exception& e )
	{
		LOG_WARN( L"Failed to save screenshot: " << StringToWstring( e.what() ) );
	}
#endif
}

std::wstring ScreenshotTool::GetNextScreenshotName()
{
#if defined(SCREENSHOT_TOOL_ENABLED)
#ifdef __APPLE__macosx
    try
    {
        const auto result = Bridge::GetNextScreenshotName();

        if( result.empty() )
            throw std::runtime_error( "failed to get the next screenshot name" );

        return StringToWstring( result );
    }
#else
	try
	{
		std::vector< std::wstring > results;
		const std::wstring screenshot_dir = FileSystem::GetDocumentsFolderPathW() + L"\\" + GGG_GAME_FILE_PATH + L"\\" + directory_name;
		FileSystem::FindFiles( screenshot_dir, L"screenshot-*.png", false, false, results );

		unsigned screenshot_index = 0;

		for( size_t i = 0; i < results.size(); ++i )
		{
			const size_t dash_position = results[ i ].rfind( L'-' );
			const size_t dot_position = results[ i ].rfind( L'.' );

			if( dash_position == std::wstring::npos ||
				dot_position == std::wstring::npos )
				continue;

			std::wstringstream converter( results[ i ].substr( dash_position + 1, dot_position - dash_position - 1 ) );

			unsigned number;
			converter >> number;
			if( !converter.fail() && number >= screenshot_index )
				screenshot_index = number + 1;

		}

		std::wstringstream converter;
		converter << screenshot_dir << L"\\screenshot-" << std::setfill( L'0' ) << std::setw( 4 ) << screenshot_index << L".png";
		return converter.str();
	}
#endif
	catch( const std::exception& e )
	{
		LOG_WARN( L"Failed to get next screenshot name: " << StringToWstring( e.what() ) );
		return L"";
	}
#endif

	return L"";
}

void ScreenshotTool::ConvertScreenshotToVolumeTex(Device::IDevice* device, std::wstring screenshot_filename, std::wstring volume_tex_filename)
{
#if defined(SCREENSHOT_TOOL_ENABLED)
	const auto volume_texture = Device::Texture::CreateVolumeTexture("Screenshot Volume", device, 16, 16, 16, 1, Device::UsageHint::DEFAULT, Device::PixelFormat::A8R8G8B8, Device::Pool::SYSTEMMEM, false);

	const auto screenshot = Utility::CreateTexture(device, screenshot_filename, Device::UsageHint::DEFAULT, Device::Pool::MANAGED_WITH_SYSTEMMEM);

	Device::LockedBox volume_lock;
	volume_texture->LockBox(0, &volume_lock, Device::Lock::DEFAULT);

	LONG start_x = 16;
	LONG start_y = 16;
	Device::LockedRect lock;
	screenshot->LockRect(0, &lock, Device::Lock::READONLY);
	unsigned char* screenshot_data = (unsigned char*)lock.pBits + lock.Pitch * start_y + start_x * 4;

	for (unsigned z = 0; z < 16; ++z)
	{
		const unsigned x_in_base = (z % 4) * 16;
		const unsigned y_in_base = (z / 4) * 16;

		auto *const volume_slice = (unsigned char*)volume_lock.pBits + volume_lock.SlicePitch * z;

		for (unsigned y = 0; y < 16; ++y)
		{
			auto *const volume_row = (DWORD*)(volume_slice + volume_lock.RowPitch * y);
			auto *const input_row = (DWORD*)(screenshot_data + lock.Pitch * (y + y_in_base));

			memcpy(volume_row, input_row + x_in_base, 16 * sizeof(DWORD));
		}
	}

	screenshot->UnlockRect(0);

	volume_texture->UnlockBox(0);

	volume_texture->SaveToFile(volume_tex_filename.c_str(), Device::ImageFileFormat::DDS);
#endif
}
